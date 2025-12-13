/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

#include <iterator>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// The set of traits that describes the behavior of MemoryCache.
constexpr base::MemoryConsumerTraits kMemoryCacheTraits = {
    .supports_memory_limit =
        base::MemoryConsumerTraits::SupportsMemoryLimit::kYes,
    .in_process = base::MemoryConsumerTraits::InProcess::kYes,
    .estimated_memory_usage =
        base::MemoryConsumerTraits::EstimatedMemoryUsage::kMedium,
    .release_memory_cost =
        base::MemoryConsumerTraits::ReleaseMemoryCost::kRequiresTraversal,
    .recreate_memory_cost = base::MemoryConsumerTraits::RecreateMemoryCost::kNA,
    .information_retention =
        base::MemoryConsumerTraits::InformationRetention::kLossless,
    .memory_release_behavior =
        base::MemoryConsumerTraits::MemoryReleaseBehavior::kIdempotent,
    .execution_type = base::MemoryConsumerTraits::ExecutionType::kAsynchronous,
    .release_gc_references =
        base::MemoryConsumerTraits::ReleaseGCReferences::kYes,
    .garbage_collects_v8_heap =
        base::MemoryConsumerTraits::GarbageCollectsV8Heap::kNo,
};

// Use function-local statics to cache the feature parameters. This avoids
// global constructors and ensures the .Get() call happens only once.
double GetFrequencyWeight() {
  static const double kWeight =
      features::kMemoryCacheIntelligentPruningFreqWeight.Get();
  return kWeight;
}

double GetCostWeight() {
  static const double kWeight =
      features::kMemoryCacheIntelligentPruningCostWeight.Get();
  return kWeight;
}

double GetTypeWeight() {
  static const double kWeight =
      features::kMemoryCacheIntelligentPruningTypeWeight.Get();
  return kWeight;
}

int GetResourceTypePriority(ResourceType type) {
  switch (type) {
    // --- Highest Priority ---
    // These are typically render-blocking and critical for the first paint.
    // Keeping them in cache has the highest impact on LCP.
    case ResourceType::kCSSStyleSheet:
    case ResourceType::kFont:
      return 10;

    // --- High Priority ---
    // Resources that are typically essential for page functionality and initial
    // rendering.
    case ResourceType::kScript:
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kRaw:
      return 8;

    // --- Medium Priority ---
    // Visible content that contributes to LCP but is often non-blocking.
    case ResourceType::kImage:
    case ResourceType::kSVGDocument:
    case ResourceType::kManifest:
    // For testing purposes only; not a real resource type.
    case ResourceType::kMock:
      return 5;

    // --- Low Priority ---
    // Media or other content that is often loaded later or is not critical
    // for the initial user experience.
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kTextTrack:
      return 2;

    // --- Lowest Priority ---
    // Speculative fetches that may or may not be used.
    case ResourceType::kLinkPrefetch:
    case ResourceType::kSpeculationRules:
    case ResourceType::kDictionary:
      return 1;

    default:
      NOTREACHED();
  }
}

}  // namespace

static Persistent<MemoryCache>* g_memory_cache;

static constexpr base::TimeDelta kDefaultStrongReferencePruneDelay =
    base::Minutes(5);

// Feature to control the duration for which a strong reference may remain
// in the MemoryCache after its last access.
BASE_FEATURE(kMemoryCacheChangeStrongReferencePruneDelay,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Parameter defining the delay after which a strong reference is removed
// from the MemoryCache after its last access.
BASE_FEATURE_PARAM(base::TimeDelta,
                   kMemoryCacheStrongReferencePruneDelay,
                   &kMemoryCacheChangeStrongReferencePruneDelay,
                   "strong_reference_prune_delay",
                   kDefaultStrongReferencePruneDelay);

ScopedMemoryCacheForTesting::ScopedMemoryCacheForTesting(
    Persistent<MemoryCache> cache) {
  if (!g_memory_cache) {
    g_memory_cache = new Persistent<MemoryCache>(std::move(cache));
    return;
  }

  stored_cache_ = std::exchange(*g_memory_cache, std::move(cache));
}

ScopedMemoryCacheForTesting::~ScopedMemoryCacheForTesting() {
  if (stored_cache_) {
    *g_memory_cache = std::move(stored_cache_);
  } else {
    delete g_memory_cache;
    g_memory_cache = nullptr;
  }
  blink::ThreadState::Current()->CollectAllGarbageForTesting();
}

void MemoryCacheEntry::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      MemoryCacheEntry, &MemoryCacheEntry::ClearResourceWeak>(this);
}

void MemoryCacheEntry::ClearResourceWeak(const LivenessBroker& info) {
  if (!resource_ || info.IsHeapObjectAlive(resource_))
    return;
  MemoryCache::Get()->Remove(resource_.Get());
  resource_.Clear();
}

// static
MemoryCache* MemoryCache::Get() {
  DCHECK(IsMainThread());
  if (!g_memory_cache) {
    g_memory_cache = new Persistent<MemoryCache>(
        MakeGarbageCollected<MemoryCache>(Thread::MainThread()->GetTaskRunner(
            MainThreadTaskRunnerRestricted())));
  }
  return g_memory_cache->Get();
}

MemoryCache::MemoryCache(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : memory_pressure_listener_registration_(
          FROM_HERE,
          base::MemoryPressureListenerTag::kMemoryCache,
          this),
      memory_consumer_registration_(
          "MemoryCache",
          kMemoryCacheTraits,
          this,
          MemoryConsumerRegistration::CheckUnregister::kDisabled,
          MemoryConsumerRegistration::CheckRegistryExists::kDisabled),
      strong_references_max_size_(
          features::kMemoryCacheStrongReferenceTotalSizeThresholdParam.Get()),
      strong_references_prune_duration_(
          kMemoryCacheStrongReferencePruneDelay.Get()),
      task_runner_(std::move(task_runner)) {
  MemoryCacheDumpProvider::Instance()->SetMemoryCache(this);
}

MemoryCache::~MemoryCache() = default;

void MemoryCache::Trace(Visitor* visitor) const {
  visitor->Trace(resource_maps_);
  visitor->Trace(strong_references_);
  visitor->Trace(tiered_strong_references_);
  MemoryCacheDumpClient::Trace(visitor);
}

void MemoryCache::Dispose() {
  memory_pressure_listener_registration_.Dispose();
  memory_consumer_registration_.Dispose();
}

KURL MemoryCache::RemoveFragmentIdentifierIfNeeded(const KURL& original_url) {
  if (!original_url.HasFragmentIdentifier())
    return original_url;
  // Strip away fragment identifier from HTTP URLs. Data URLs must be
  // unmodified. For file and custom URLs clients may expect resources to be
  // unique even when they differ by the fragment identifier only.
  if (!original_url.ProtocolIsInHTTPFamily())
    return original_url;
  KURL url = original_url;
  url.RemoveFragmentIdentifier();
  return url;
}

String MemoryCache::DefaultCacheIdentifier() {
  return g_empty_string;
}

MemoryCache::ResourceMap* MemoryCache::EnsureResourceMap(
    const String& cache_identifier) {
  if (!resource_maps_.Contains(cache_identifier)) {
    ResourceMapIndex::AddResult result = resource_maps_.insert(
        cache_identifier, MakeGarbageCollected<ResourceMap>());
    CHECK(result.is_new_entry);
  }
  return resource_maps_.at(cache_identifier);
}

void MemoryCache::Add(Resource* resource) {
  DCHECK(resource);
  ResourceMap* resources = EnsureResourceMap(resource->CacheIdentifier());
  AddInternal(resources, MakeGarbageCollected<MemoryCacheEntry>(resource));
  RESOURCE_LOADING_DVLOG(1)
      << "MemoryCache::add Added " << resource->Url().GetString()
      << ", resource " << resource;
}

void MemoryCache::AddInternal(ResourceMap* resource_map,
                              MemoryCacheEntry* entry) {
  DCHECK(IsMainThread());
  DCHECK(resource_map);

  Resource* resource = entry->GetResource();
  if (!resource)
    return;
  DCHECK(resource->Url().IsValid());

  KURL url = RemoveFragmentIdentifierIfNeeded(resource->Url());
  ResourceMap::iterator it = resource_map->find(url);
  if (it != resource_map->end()) {
    Resource* old_resource = it->value->GetResource();
    CHECK_NE(old_resource, resource);
    Update(old_resource, old_resource->size(), 0);
    strong_references_.erase(old_resource);
  }
  resource_map->Set(url, entry);
  Update(resource, 0, resource->size());
}

void MemoryCache::Remove(Resource* resource) {
  DCHECK(IsMainThread());
  DCHECK(resource);
  // Resources can be created with garbage urls in error cases. These Resources
  // should never be added to the cache (AddInternal() DCHECKs that the url is
  // valid). Null urls will crash if we attempt to hash them, so early exit.
  if (resource->Url().IsNull())
    return;

  RESOURCE_LOADING_DVLOG(1) << "Evicting resource " << resource << " for "
                            << resource->Url().GetString() << " from cache";
  TRACE_EVENT1("blink", "MemoryCache::evict", "resource",
               resource->Url().GetString().Utf8());

  const auto resource_maps_it =
      resource_maps_.find(resource->CacheIdentifier());
  if (resource_maps_it == resource_maps_.end())
    return;
  ResourceMap* resources = resource_maps_it->value.Get();

  KURL url = RemoveFragmentIdentifierIfNeeded(resource->Url());
  ResourceMap::iterator it = resources->find(url);
  if (it == resources->end() || it->value->GetResource() != resource)
    return;
  RemoveInternal(resources, it);
}

void MemoryCache::RemoveInternal(ResourceMap* resource_map,
                                 const ResourceMap::iterator& it) {
  DCHECK(IsMainThread());
  DCHECK(resource_map);

  Resource* resource = it->value->GetResource();
  DCHECK(resource);

  Update(resource, resource->size(), 0);
  resource_map->erase(it);
  if (base::FeatureList::IsEnabled(features::kMemoryCacheIntelligentPruning)) {
    // If intelligent pruning is on, the resource can only be in the new
    // tiered vector. We perform a "lazy" remove for performance.
    size_t index = tiered_strong_references_.Find(resource);
    if (index != kNotFound) {
      tiered_strong_references_[index] = nullptr;
    }
  } else {
    // Otherwise, the resource can only be in the original strong references
    // set.
    strong_references_.erase(resource);
  }
}

bool MemoryCache::Contains(const Resource* resource) const {
  if (!resource || resource->Url().IsEmpty())
    return false;

  const auto resource_maps_it =
      resource_maps_.find(resource->CacheIdentifier());
  if (resource_maps_it == resource_maps_.end())
    return false;
  const ResourceMap* resources = resource_maps_it->value.Get();

  KURL url = RemoveFragmentIdentifierIfNeeded(resource->Url());
  const auto resources_it = resources->find(url);
  if (resources_it == resources->end())
    return false;
  return resource == resources_it->value->GetResource();
}

Resource* MemoryCache::ResourceForURLForTesting(
    const KURL& resource_url) const {
  return ResourceForURL(resource_url, DefaultCacheIdentifier());
}

Resource* MemoryCache::ResourceForURL(const KURL& resource_url,
                                      const String& cache_identifier) const {
  DCHECK(IsMainThread());
  if (!resource_url.IsValid() || resource_url.IsNull())
    return nullptr;
  DCHECK(!cache_identifier.IsNull());

  const auto resource_maps_it = resource_maps_.find(cache_identifier);
  if (resource_maps_it == resource_maps_.end()) {
    return nullptr;
  }
  const ResourceMap* resources = resource_maps_it->value.Get();

  KURL url = RemoveFragmentIdentifierIfNeeded(resource_url);
  const auto resources_it = resources->find(url);
  if (resources_it == resources->end()) {
    return nullptr;
  }

  Resource* resource = resources_it->value->GetResource();
  if (resource &&
      base::FeatureList::IsEnabled(features::kMemoryCacheIntelligentPruning)) {
    resource->UpdateMemoryCacheLastAccessedTime();
  }

  return resource;
}

HeapVector<Member<Resource>> MemoryCache::ResourcesForURL(
    const KURL& resource_url) const {
  DCHECK(IsMainThread());
  KURL url = RemoveFragmentIdentifierIfNeeded(resource_url);
  HeapVector<Member<Resource>> results;
  for (const auto& resource_maps_it : resource_maps_) {
    const auto resources_it = resource_maps_it.value->find(url);
    if (resources_it == resource_maps_it.value->end())
      continue;
    Resource* resource = resources_it->value->GetResource();
    DCHECK(resource);
    results.push_back(resource);
  }
  return results;
}

void MemoryCache::Update(Resource* resource, size_t old_size, size_t new_size) {
  ptrdiff_t delta = new_size - old_size;
  if (Contains(resource)) {
    DCHECK(delta >= 0 || size_ >= static_cast<size_t>(-delta));
    size_ += delta;
  }
  if (strong_references_.Contains(resource)) {
    PruneStrongReferences();
  }
}

void MemoryCache::RemoveURLFromCache(const KURL& url) {
  HeapVector<Member<Resource>> resources = ResourcesForURL(url);
  for (Resource* resource : resources)
    Remove(resource);
}

void MemoryCache::TypeStatistic::AddResource(Resource* o) {
  count++;
  size += o->size();
  decoded_size += o->DecodedSize();
  encoded_size += o->EncodedSize();
  overhead_size += o->OverheadSize();
  code_cache_size += o->CodeCacheSize();
  encoded_size_duplicated_in_data_urls +=
      o->Url().ProtocolIsData() ? o->EncodedSize() : 0;
}

MemoryCache::Statistics MemoryCache::GetStatistics() const {
  Statistics stats;
  for (const auto& resource_map_iter : resource_maps_) {
    for (const auto& resource_iter : *resource_map_iter.value) {
      Resource* resource = resource_iter.value->GetResource();
      DCHECK(resource);
      switch (resource->GetType()) {
        case ResourceType::kImage:
          stats.images.AddResource(resource);
          break;
        case ResourceType::kCSSStyleSheet:
          stats.css_style_sheets.AddResource(resource);
          break;
        case ResourceType::kScript:
          stats.scripts.AddResource(resource);
          break;
        case ResourceType::kXSLStyleSheet:
          stats.xsl_style_sheets.AddResource(resource);
          break;
        case ResourceType::kFont:
          stats.fonts.AddResource(resource);
          break;
        default:
          stats.other.AddResource(resource);
          break;
      }
    }
  }
  return stats;
}

void MemoryCache::EvictResources() {
  for (auto resource_map_iter = resource_maps_.begin();
       resource_map_iter != resource_maps_.end();) {
    RemoveAllResourcesFromMap(resource_map_iter->value.Get());
    resource_maps_.erase(resource_map_iter);
    resource_map_iter = resource_maps_.begin();
  }
  ClearStrongReferences();
}

void MemoryCache::EvictResourcesForCacheIdentifier(
    const String& cache_identifier) {
  const auto& resource_map_iter = resource_maps_.find(cache_identifier);
  // Not all cache identifiers will end up in the resource map (e.g. a failed
  // fetch or a dataURL)
  if (resource_map_iter == resource_maps_.end()) {
    return;
  }

  RemoveAllResourcesFromMap(resource_map_iter->value.Get());
  resource_maps_.erase(resource_map_iter);
}

void MemoryCache::RemoveAllResourcesFromMap(ResourceMap* resources) {
  for (auto resource_iter = resources->begin();
       resource_iter != resources->end(); resource_iter = resources->begin()) {
    DCHECK(resource_iter.Get());
    DCHECK(resource_iter->value.Get());
    DCHECK(resource_iter->value->GetResource());
    Resource* resource = resource_iter->value->GetResource();
    DCHECK(resource);
    RemoveInternal(resources, resource_iter);
  }
}

bool MemoryCache::OnMemoryDump(WebMemoryDumpLevelOfDetail level_of_detail,
                               WebProcessMemoryDump* memory_dump) {
  if (level_of_detail == WebMemoryDumpLevelOfDetail::kBackground) {
    Statistics stats = GetStatistics();
    WebMemoryAllocatorDump* dump1 =
        memory_dump->CreateMemoryAllocatorDump("web_cache/Image_resources");
    dump1->AddScalar("size", "bytes",
                     stats.images.encoded_size + stats.images.overhead_size);
    WebMemoryAllocatorDump* dump2 = memory_dump->CreateMemoryAllocatorDump(
        "web_cache/CSS stylesheet_resources");
    dump2->AddScalar("size", "bytes",
                     stats.css_style_sheets.encoded_size +
                         stats.css_style_sheets.overhead_size);
    WebMemoryAllocatorDump* dump3 =
        memory_dump->CreateMemoryAllocatorDump("web_cache/Script_resources");
    dump3->AddScalar("size", "bytes",
                     stats.scripts.encoded_size + stats.scripts.overhead_size);
    WebMemoryAllocatorDump* dump4 = memory_dump->CreateMemoryAllocatorDump(
        "web_cache/XSL stylesheet_resources");
    dump4->AddScalar("size", "bytes",
                     stats.xsl_style_sheets.encoded_size +
                         stats.xsl_style_sheets.overhead_size);
    WebMemoryAllocatorDump* dump5 =
        memory_dump->CreateMemoryAllocatorDump("web_cache/Font_resources");
    dump5->AddScalar("size", "bytes",
                     stats.fonts.encoded_size + stats.fonts.overhead_size);
    WebMemoryAllocatorDump* dump6 =
        memory_dump->CreateMemoryAllocatorDump("web_cache/Code_cache");
    dump6->AddScalar("size", "bytes", stats.scripts.code_cache_size);
    WebMemoryAllocatorDump* dump7 = memory_dump->CreateMemoryAllocatorDump(
        "web_cache/Encoded_size_duplicated_in_data_urls");
    dump7->AddScalar("size", "bytes",
                     stats.other.encoded_size +
                         stats.other.encoded_size_duplicated_in_data_urls);
    WebMemoryAllocatorDump* dump8 =
        memory_dump->CreateMemoryAllocatorDump("web_cache/Other_resources");
    dump8->AddScalar("size", "bytes",
                     stats.other.encoded_size + stats.other.overhead_size);
    return true;
  }

  for (const auto& resource_map_iter : resource_maps_) {
    for (const auto& resource_iter : *resource_map_iter.value) {
      Resource* resource = resource_iter.value->GetResource();
      resource->OnMemoryDump(level_of_detail, memory_dump);
    }
  }
  return true;
}

void MemoryCache::OnMemoryPressure(base::MemoryPressureLevel level) {
  if (level == base::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kReleaseResourceStrongReferencesOnMemoryPressure)) {
    ClearStrongReferences();
  }
}

void MemoryCache::OnReleaseMemory() {
  if (base::FeatureList::IsEnabled(features::kMemoryCacheStrongReference)) {
    PruneStrongReferences();
  }
}

void MemoryCache::OnUpdateMemoryLimit() {
  // It is important to not do any memory management in this function. The max
  // size is updated to the requested limit without calling
  // PruneStrongReferences().
  strong_references_max_size_ =
      features::kMemoryCacheStrongReferenceTotalSizeThresholdParam.Get() *
      memory_limit_ratio();
}

bool MemoryCache::HasStrongReferenceForTesting(Resource* resource) const {
  return strong_references_.Contains(resource) ||
         tiered_strong_references_.Contains(resource);
}

void MemoryCache::SaveTieredStrongReference(Resource* resource) {
  if (tiered_strong_references_.Contains(resource)) {
    return;
  }

  // Just append. The list will be sorted later in PruneTieredStrongReferences.
  tiered_strong_references_.push_back(resource);
}

void MemoryCache::SaveStrongReference(Resource* resource) {
  DCHECK(base::FeatureList::IsEnabled(features::kMemoryCacheStrongReference));
  resource->UpdateMemoryCacheLastAccessedTime();
  if (base::FeatureList::IsEnabled(features::kMemoryCacheIntelligentPruning)) {
    CHECK(strong_references_.empty());
    SaveTieredStrongReference(resource);
  } else {
    CHECK(tiered_strong_references_.empty());
    strong_references_.AppendOrMoveToLast(resource);
  }
  PruneStrongReferences();
}

void MemoryCache::PruneTieredStrongReferences() {
  // Monitor the performance of this new value-based pruning logic to ensure
  // the O(N log N) sorting step is not a bottleneck in production.
  SCOPED_UMA_HISTOGRAM_TIMER("MemoryCache.PruneTieredStrongReferences.Time");

  const size_t max_threshold = strong_references_max_size_;

  // Enforce a maximum lifetime for all strong references.
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta max_lifetime = strong_references_prune_duration_;

  EraseIf(tiered_strong_references_, [&](const Member<Resource>& resource) {
    // Erase the resource if it's null (due to lazy removal by
    // `RemoveInternal`) or if it has expired
    return !resource ||
           (now - resource->MemoryCacheLastAccessed() > max_lifetime);
  });

  size_t current_total_size = 0;
  for (Resource* resource : tiered_strong_references_) {
    CHECK(resource, base::NotFatalUntil::M145);
    current_total_size += resource->size();
  }

  //  Early exit if already under budget
  if (current_total_size <= max_threshold) {
    return;
  }

  // Sort the vector from HIGHEST value to LOWEST value. This allows us to
  // efficiently remove the lowest-value items from the end.
  // We only pay O(N log N) cost when we know we have to evict.
  // The sorting is "Just-In-Time" for the eviction decisions.
  std::sort(tiered_strong_references_.begin(), tiered_strong_references_.end(),
            [this](const Member<Resource>& a, const Member<Resource>& b) {
              CHECK(a, base::NotFatalUntil::M145);
              CHECK(b, base::NotFatalUntil::M145);
              // Note: `>` sorts in descending order (highest value first).
              return CalculateResourceValue(a.Get()) >
                     CalculateResourceValue(b.Get());
            });

  // Evict the lowest-value items from the end of the sorted vector until we are
  // within budget. This is very fast.
  while (current_total_size > max_threshold) {
    if (tiered_strong_references_.empty()) {
      break;
    }

    Resource* resource_to_evict = tiered_strong_references_.back().Get();
    current_total_size -= resource_to_evict->size();
    tiered_strong_references_.pop_back();
  }
}

void MemoryCache::PruneStrongReferences() {
  if (base::FeatureList::IsEnabled(features::kMemoryCacheIntelligentPruning)) {
    PruneTieredStrongReferences();
    return;
  }
  // Measures the execution time of the original pruning logic.
  SCOPED_UMA_HISTOGRAM_TIMER("MemoryCache.PruneStrongReferences.Time");

  DCHECK(base::FeatureList::IsEnabled(features::kMemoryCacheStrongReference));
  const size_t max_threshold = strong_references_max_size_;

  base::TimeTicks last_ticks;
  size_t strong_reference_total_size = 0;
  for (Resource* resource : strong_references_) {
    // Sanity check on data structure.
    CHECK(resource->MemoryCacheLastAccessed() >= last_ticks);
    last_ticks = resource->MemoryCacheLastAccessed();
    strong_reference_total_size += resource->size();
  }

  while (strong_reference_total_size > max_threshold) {
    CHECK(!strong_references_.empty());
    Resource* front_resource = strong_references_.front();
    strong_references_.erase(strong_references_.begin());
    size_t resource_size = front_resource->size();
    CHECK_GE(strong_reference_total_size, resource_size);
    strong_reference_total_size -= resource_size;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  while (!strong_references_.empty()) {
    Resource* front_resource = strong_references_.front();
    base::TimeTicks next_expiry = front_resource->MemoryCacheLastAccessed() +
                                  strong_references_prune_duration_;
    if (next_expiry > now) {
      if (strong_references_prune_time_ < now ||
          strong_references_prune_time_ > next_expiry) {
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(&MemoryCache::PruneStrongReferences,
                           WrapWeakPersistent(this)),
            next_expiry - now);
        strong_references_prune_time_ = next_expiry;
      }
      break;
    }
    strong_references_.erase(strong_references_.begin());
  }
}

void MemoryCache::ClearStrongReferences() {
  strong_references_.clear();
  tiered_strong_references_.clear();
}

double MemoryCache::CalculateResourceValue(const Resource* resource) const {
  double cost_score = resource->EncodedSize() * GetCostWeight();
  // Use log1p to apply diminishing returns to the hit count. This prevents a
  // high frequency from dominating the resource's score and is numerically
  // stable for low hit counts.
  double frequency_score =
      std::log1p(resource->MemoryCacheHitCount()) * GetFrequencyWeight();
  double type_score =
      GetResourceTypePriority(resource->GetType()) * GetTypeWeight();

  return frequency_score + cost_score + type_score;
}

}  // namespace blink
