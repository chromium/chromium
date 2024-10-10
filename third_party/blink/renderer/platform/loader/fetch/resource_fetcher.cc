/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
    rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

    This class provides all functionality needed for loading images, style
    sheets and html pages from the web. It has a memory cache for these objects.
*/

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/auto_reset.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink.h"
#include "third_party/abseil-cpp/absl/cleanup/cleanup.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/lcp_critical_path_predictor_util.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink-forward.h"
#include "third_party/blink/public/mojom/timing/resource_timing.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/scheduler/web_scoped_virtual_time_pauser.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/instance_counters.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/traced_value.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/back_forward_cache_loader_helper.h"
#include "third_party/blink/renderer/platform/loader/fetch/console_logger.h"
#include "third_party/blink/renderer/platform/loader/fetch/detachable_use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_client_settings_object_snapshot.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/render_blocking_behavior.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher_properties.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_observer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loading_log.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_utils.h"
#include "third_party/blink/renderer/platform/loader/fetch/stale_revalidation_resource_client.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle_list.h"
#include "third_party/blink/renderer/platform/loader/fetch/unique_identifier.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"
#include "third_party/blink/renderer/platform/mojo/mojo_binding_context.h"
#include "third_party/blink/renderer/platform/network/encoded_form_data.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/agent_group_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/weborigin/known_ports.h"
#include "third_party/blink/renderer/platform/weborigin/origin_access_entry.h"
#include "third_party/blink/renderer/platform/weborigin/reporting_disposition.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/weborigin/security_policy.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

constexpr uint32_t ResourceFetcher::kKeepaliveInflightBytesQuota;

namespace {

constexpr base::TimeDelta kKeepaliveLoadersTimeout = base::Seconds(30);

// Timeout for link preloads to be used after window.onload
static constexpr base::TimeDelta kUnusedPreloadTimeout = base::Seconds(3);

static constexpr char kCrossDocumentCachedResource[] =
    "Blink.MemoryCache.CrossDocumentCachedResource2";

static constexpr char kEarlyHintsInitiatorType[] = "early-hints";

#define RESOURCE_HISTOGRAM_PREFIX "Blink.MemoryCache.RevalidationPolicy."

#define RESOURCE_TYPE_NAME(name) \
  case ResourceType::k##name: {  \
    return #name;                \
    break;                       \
  }

const std::string ResourceTypeName(ResourceType type) {
  // `ResourceType` variants in
  // tools/metrics/histograms/metadata/blink/histograms.xml
  // should be updated when you update the followings.
  switch (type) {
    RESOURCE_TYPE_NAME(Image)             // 1
    RESOURCE_TYPE_NAME(CSSStyleSheet)     // 2
    RESOURCE_TYPE_NAME(Script)            // 3
    RESOURCE_TYPE_NAME(Font)              // 4
    RESOURCE_TYPE_NAME(Raw)               // 5
    RESOURCE_TYPE_NAME(SVGDocument)       // 6
    RESOURCE_TYPE_NAME(XSLStyleSheet)     // 7
    RESOURCE_TYPE_NAME(LinkPrefetch)      // 8
    RESOURCE_TYPE_NAME(TextTrack)         // 9
    RESOURCE_TYPE_NAME(Audio)             // 10
    RESOURCE_TYPE_NAME(Video)             // 11
    RESOURCE_TYPE_NAME(Manifest)          // 12
    RESOURCE_TYPE_NAME(SpeculationRules)  // 13
    RESOURCE_TYPE_NAME(Mock)              // 14
    RESOURCE_TYPE_NAME(Dictionary)        // 15
  }
}

ResourceLoadPriority TypeToPriority(ResourceType type) {
  switch (type) {
    case ResourceType::kCSSStyleSheet:
    case ResourceType::kFont:
      // Also parser-blocking scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kVeryHigh;
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kRaw:
    case ResourceType::kScript:
      // Also visible resources/images (set explicitly in loadPriority)
      return ResourceLoadPriority::kHigh;
    case ResourceType::kManifest:
    case ResourceType::kMock:
      // Also late-body scripts and stylesheets discovered by the
      // preload scanner (set explicitly in loadPriority)
      return ResourceLoadPriority::kMedium;
    case ResourceType::kImage:
    case ResourceType::kTextTrack:
    case ResourceType::kAudio:
    case ResourceType::kVideo:
    case ResourceType::kSVGDocument:
      // Also async scripts (set explicitly in loadPriority)
      return ResourceLoadPriority::kLow;
    case ResourceType::kLinkPrefetch:
    case ResourceType::kSpeculationRules:
    case ResourceType::kDictionary:
      return ResourceLoadPriority::kVeryLow;
  }

  NOTREACHED_IN_MIGRATION();
  return ResourceLoadPriority::kUnresolved;
}

bool ShouldResourceBeAddedToMemoryCache(const FetchParameters& params,
                                        Resource* resource) {
  return IsMainThread() &&
         params.GetResourceRequest().HttpMethod() == http_names::kGET &&
         params.Options().data_buffering_policy != kDoNotBufferData &&
         !IsRawResource(*resource) &&
         // Always create a new resource for SVG resource documents since they
         // are tied to the requesting document. There's a document-scoped cache
         // in-front of the ResourceFetcher that will handle reuse (see
         // SVGResourceDocumentContent::Fetch()).
         resource->GetType() != ResourceType::kSVGDocument;
}

bool ShouldResourceBeKeptStrongReferenceByType(
    Resource* resource,
    const SecurityOrigin* settings_object_origin) {
  // Image, fonts, stylesheets and scripts are the most commonly reused scripts.

  return resource->GetType() == ResourceType::kScript ||
         resource->GetType() == ResourceType::kFont ||
         resource->GetType() == ResourceType::kCSSStyleSheet ||
         resource->GetType() == ResourceType::kMock;  // For tests.
}

bool ShouldResourceBeKeptStrongReference(
    Resource* resource,
    const SecurityOrigin* settings_object_origin) {
  return IsMainThread() && resource->IsLoaded() &&
         resource->GetResourceRequest().HttpMethod() == http_names::kGET &&
         resource->Options().data_buffering_policy != kDoNotBufferData &&
         ShouldResourceBeKeptStrongReferenceByType(resource,
                                                   settings_object_origin) &&
         !resource->GetResponse().CacheControlContainsNoCache() &&
         !resource->GetResponse().CacheControlContainsNoStore();
}

base::TimeDelta GetResourceStrongReferenceTimeout(Resource* resource) {
  base::TimeDelta lifetime = resource->FreshnessLifetime();
  if (resource->GetResponse().ResponseTime() + lifetime < base::Time::Now()) {
    return base::TimeDelta();
  }
  return resource->GetResponse().ResponseTime() + lifetime - base::Time::Now();
}

static ResourceFetcher::ResourceFetcherSet& MainThreadFetchersSet() {
  DEFINE_STATIC_LOCAL(
      Persistent<ResourceFetcher::ResourceFetcherSet>, fetchers,
      (MakeGarbageCollected<ResourceFetcher::ResourceFetcherSet>()));
  return *fetchers;
}

static bool& PriorityObserverMapCreated() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(bool, priority_observer_map_created, (false));
  return priority_observer_map_created;
}

// Calls to PriorityObservers() that don't need to explicitly interact with the
// map should be guarded with a call to PriorityObserverMapCreated(), to avoid
// unnecessarily creating a PriorityObserverMap.
using PriorityObserverMap = HashMap<String, base::OnceCallback<void(int)>>;
static ThreadSpecific<PriorityObserverMap>& PriorityObservers() {
  PriorityObserverMapCreated() = true;
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<PriorityObserverMap>, map, ());
  return map;
}


std::unique_ptr<TracedValue> CreateTracedValueWithPriority(
    blink::ResourceLoadPriority priority) {
  auto value = std::make_unique<TracedValue>();
  value->SetInteger("priority", static_cast<int>(priority));
  return value;
}

std::unique_ptr<TracedValue> CreateTracedValueForUnusedPreload(
    const KURL& url,
    Resource::MatchStatus status,
    String request_id) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", String(url.ElidedString().Utf8()));
  value->SetInteger("status", static_cast<int>(status));
  value->SetString("requestId", request_id);
  return value;
}

std::unique_ptr<TracedValue> CreateTracedValueForUnusedEarlyHintsPreload(
    const KURL& url) {
  // TODO(https://crbug.com/1317936): Consider adding more trace values.
  auto value = std::make_unique<TracedValue>();
  value->SetString("url", String(url.ElidedString().Utf8()));
  return value;
}

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class BoostImagePriorityReason {
  kFirstN = 0,
  kLcppForSmallImage = 1,
  kLcppForNotSmallImage = 2,
  kBoth = 3,
  kMaxValue = kBoth,
};

void MaybeRecordBoostImagePriorityReason(const bool is_first_n,
                                         const bool is_potentially_lcp_element,
                                         const bool is_small_image) {
  std::optional<BoostImagePriorityReason> reason;

  if (is_first_n && !is_potentially_lcp_element) {
    reason = BoostImagePriorityReason::kFirstN;
  } else if (!is_first_n && is_potentially_lcp_element) {
    reason = is_small_image ? BoostImagePriorityReason::kLcppForSmallImage
                            : BoostImagePriorityReason::kLcppForNotSmallImage;
  } else if (is_first_n && is_potentially_lcp_element) {
    reason = BoostImagePriorityReason::kBoth;
  }

  // We do not record `!is_first_n && !is_potentially_lcp_element` case since
  // the image was not boosted in such cases.
  if (reason) {
    base::UmaHistogramEnumeration("Blink.LCPP.BoostImagePriorityReason",
                                  *reason);
  }
}

constexpr char kLCPPDeferUnusedPreloadHistogramPrefix[] =
    "Blink.LCPP.DeferUnusedPreload.";

std::string LinkPreloadStrForHistogram(bool link_preload) {
  return link_preload ? "LinkPreload" : "NoLinkPreload";
}

void RecordDeferUnusedPreloadHistograms(const Resource* resource) {
  base::UmaHistogramEnumeration(
      base::StrCat(
          {kLCPPDeferUnusedPreloadHistogramPrefix, "DeferredResource"}),
      resource->GetType());
  base::UmaHistogramEnumeration(
      base::StrCat({kLCPPDeferUnusedPreloadHistogramPrefix, "DeferredResource.",
                    LinkPreloadStrForHistogram(resource->IsLinkPreload())}),
      resource->GetType());

  // When `resource` still not need load, that means the resource load is not
  // started yet because there are no subsequent resource requests or vice
  // versa.
  base::UmaHistogramBoolean(
      base::StrCat({kLCPPDeferUnusedPreloadHistogramPrefix,
                    "UnusedAtDeferredLoadTiming"}),
      resource->StillNeedsLoad());
  base::UmaHistogramBoolean(
      base::StrCat({kLCPPDeferUnusedPreloadHistogramPrefix,
                    "UnusedAtDeferredLoadTiming.",
                    LinkPreloadStrForHistogram(resource->IsLinkPreload())}),
      resource->StillNeedsLoad());
  if (!resource->StillNeedsLoad()) {
    // If the resource load is not needed anymore, that's a false positive case
    // of the LCPP based deferring unused preloads.
    base::UmaHistogramEnumeration(
        base::StrCat(
            {kLCPPDeferUnusedPreloadHistogramPrefix, "PredictionFailed"}),
        resource->GetType());
    base::UmaHistogramEnumeration(
        base::StrCat({kLCPPDeferUnusedPreloadHistogramPrefix,
                      "PredictionFailed.",
                      LinkPreloadStrForHistogram(resource->IsLinkPreload())}),
        resource->GetType());
  }
}
}  // namespace

// Used to ensure a ResourceRequest is correctly configured. Specifically
// PrepareRequestForCacheAccess() is called first. If the resource can not be
// served from the cache, UpgradeForLoaderIfNecessary() is called to complete
// the necessary steps before loading.
class ResourceFetcher::ResourcePrepareHelper final
    : public ResourceRequestContext {
  STACK_ALLOCATED();

 public:
  ResourcePrepareHelper(ResourceFetcher& fetcher,
                        FetchParameters& params,
                        const ResourceFactory& factory);

  std::optional<ResourceRequestBlockedReason> PrepareRequestForCacheAccess(
      WebScopedVirtualTimePauser& pauser);
  void UpgradeForLoaderIfNecessary(WebScopedVirtualTimePauser& pauser);
  bool WasUpgradeForLoaderCalled() const {
    return was_upgrade_for_loader_called_;
  }

  // ResourceRequestContext:
  ResourceLoadPriority ComputeLoadPriority(
      const FetchParameters& params) override;
  void RecordTrace() override;

 private:
  ResourceFetcher& fetcher_;
  FetchParameters& params_;
  KURL bundle_url_for_uuid_resources_;
  const ResourceFactory& factory_;
  const bool has_transparent_placeholder_image_;
  bool was_upgrade_for_loader_called_ = true;
#if DCHECK_IS_ON()
  bool determined_initial_blocked_reason_ = false;
#endif
};

ResourceFetcherInit::ResourceFetcherInit(
    DetachableResourceFetcherProperties& properties,
    FetchContext* context,
    scoped_refptr<base::SingleThreadTaskRunner> freezable_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> unfreezable_task_runner,
    ResourceFetcher::LoaderFactory* loader_factory,
    ContextLifecycleNotifier* context_lifecycle_notifier,
    BackForwardCacheLoaderHelper* back_forward_cache_loader_helper)
    : properties(&properties),
      context(context),
      freezable_task_runner(std::move(freezable_task_runner)),
      unfreezable_task_runner(std::move(unfreezable_task_runner)),
      loader_factory(loader_factory),
      context_lifecycle_notifier(context_lifecycle_notifier),
      back_forward_cache_loader_helper(back_forward_cache_loader_helper) {
  DCHECK(context);
  DCHECK(this->freezable_task_runner);
  DCHECK(this->unfreezable_task_runner);
  DCHECK(loader_factory || properties.IsDetached());
  DCHECK(context_lifecycle_notifier || properties.IsDetached());
}

bool ResourceFetcher::IsSimplifyLoadingTransparentPlaceholderImageEnabled() {
  return transparent_image_optimization_enabled_;
}

mojom::blink::RequestContextType ResourceFetcher::DetermineRequestContext(
    ResourceType type,
    IsImageSet is_image_set) {
  DCHECK((is_image_set == kImageNotImageSet) ||
         (type == ResourceType::kImage && is_image_set == kImageIsImageSet));
  switch (type) {
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kCSSStyleSheet:
      return mojom::blink::RequestContextType::STYLE;
    case ResourceType::kScript:
      return mojom::blink::RequestContextType::SCRIPT;
    case ResourceType::kFont:
      return mojom::blink::RequestContextType::FONT;
    case ResourceType::kImage:
      if (is_image_set == kImageIsImageSet)
        return mojom::blink::RequestContextType::IMAGE_SET;
      return mojom::blink::RequestContextType::IMAGE;
    case ResourceType::kRaw:
      return mojom::blink::RequestContextType::SUBRESOURCE;
    case ResourceType::kLinkPrefetch:
      return mojom::blink::RequestContextType::PREFETCH;
    case ResourceType::kTextTrack:
      return mojom::blink::RequestContextType::TRACK;
    case ResourceType::kSVGDocument:
      return mojom::blink::RequestContextType::IMAGE;
    case ResourceType::kAudio:
      return mojom::blink::RequestContextType::AUDIO;
    case ResourceType::kVideo:
      return mojom::blink::RequestContextType::VIDEO;
    case ResourceType::kManifest:
      return mojom::blink::RequestContextType::MANIFEST;
    case ResourceType::kMock:
      return mojom::blink::RequestContextType::SUBRESOURCE;
    case ResourceType::kSpeculationRules:
      return mojom::blink::RequestContextType::SUBRESOURCE;
    case ResourceType::kDictionary:
      return mojom::blink::RequestContextType::SUBRESOURCE;
  }
  NOTREACHED_IN_MIGRATION();
  return mojom::blink::RequestContextType::SUBRESOURCE;
}

network::mojom::RequestDestination ResourceFetcher::DetermineRequestDestination(
    ResourceType type) {
  switch (type) {
    case ResourceType::kXSLStyleSheet:
    case ResourceType::kCSSStyleSheet:
      return network::mojom::RequestDestination::kStyle;
    case ResourceType::kSpeculationRules:
    case ResourceType::kScript:
      return network::mojom::RequestDestination::kScript;
    case ResourceType::kFont:
      return network::mojom::RequestDestination::kFont;
    case ResourceType::kImage:
      return network::mojom::RequestDestination::kImage;
    case ResourceType::kTextTrack:
      return network::mojom::RequestDestination::kTrack;
    case ResourceType::kSVGDocument:
      return network::mojom::RequestDestination::kImage;
    case ResourceType::kAudio:
      return network::mojom::RequestDestination::kAudio;
    case ResourceType::kVideo:
      return network::mojom::RequestDestination::kVideo;
    case ResourceType::kManifest:
      return network::mojom::RequestDestination::kManifest;
    case ResourceType::kRaw:
    case ResourceType::kLinkPrefetch:
    case ResourceType::kMock:
    case ResourceType::kDictionary:
      return network::mojom::RequestDestination::kEmpty;
  }
  NOTREACHED_IN_MIGRATION();
  return network::mojom::RequestDestination::kEmpty;
}

void ResourceFetcher::AddPriorityObserverForTesting(
    const KURL& resource_url,
    base::OnceCallback<void(int)> callback,
    bool new_load_only) {
  KURL normalized_url =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);

  if (!new_load_only) {
    auto it = cached_resources_map_.find(normalized_url.GetString());
    if (it != cached_resources_map_.end()) {
      Resource* resource = it->value;
      std::move(callback).Run(
          static_cast<int>(resource->GetResourceRequest().InitialPriority()));
      return;
    }
  }

  PriorityObservers()->Set(normalized_url.GetString(), std::move(callback));
}

// This method simply takes in information about a ResourceRequest, and returns
// a priority. It will not be called for ResourceRequests that already have a
// pre-set priority (e.g., requests coming from a Service Worker) except for
// images, which may need to be reprioritized.
// platform/loader/fetch/README.md contains more details on prioritization
// as well as links to all of the relevant places in the code where priority
// is determined. If the priority logic is updated here, be sure to update
// the other code as needed.
ResourceLoadPriority ResourceFetcher::ComputeLoadPriority(
    ResourceType type,
    const ResourceRequestHead& resource_request,
    ResourcePriority::VisibilityStatus visibility,
    FetchParameters::DeferOption defer_option,
    FetchParameters::SpeculativePreloadType speculative_preload_type,
    RenderBlockingBehavior render_blocking_behavior,
    mojom::blink::ScriptType script_type,
    bool is_link_preload,
    const std::optional<float> resource_width,
    const std::optional<float> resource_height,
    bool is_potentially_lcp_element,
    bool is_potentially_lcp_influencer) {
  DCHECK(!resource_request.PriorityHasBeenSet() ||
         type == ResourceType::kImage);
  ResourceLoadPriority priority = TypeToPriority(type);

  // Visible resources (images in practice) get a boost to High priority.
  if (visibility == ResourcePriority::kVisible)
    priority = ResourceLoadPriority::kHigh;

  // Resources before the first image are considered "early" in the document and
  // resources after the first image are "late" in the document.  Important to
  // note that this is based on when the preload scanner discovers a resource
  // for the most part so the main parser may not have reached the image element
  // yet.
  if (type == ResourceType::kImage && !is_link_preload)
    image_fetched_ = true;

  // Check for late-in-document resources discovered by the preload scanner.
  // kInDocument means it was found in the document by the preload scanner.
  // image_fetched_ is used as the divider between "early" and "late" where
  // anything after the first image is considered "late" in the document.
  // This is used for lowering the priority of late-body scripts/stylesheets.
  bool late_document_from_preload_scanner = false;
  if (speculative_preload_type ==
          FetchParameters::SpeculativePreloadType::kInDocument &&
      image_fetched_) {
    late_document_from_preload_scanner = true;
  }

  // A preloaded font should not take precedence over critical CSS or
  // parser-blocking scripts.
  if (type == ResourceType::kFont && is_link_preload)
    priority = ResourceLoadPriority::kHigh;

  if (FetchParameters::kIdleLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (type == ResourceType::kScript) {
    // Special handling for classic scripts.
    // Default/Parser-Blocking/Preload early in document: High (set in
    // typeToPriority)
    // Async/Defer: Low Priority (applies to both preload and parser-inserted)
    // Preload late in document: Medium
    if (script_type == mojom::blink::ScriptType::kClassic) {
      if (FetchParameters::kLazyLoad == defer_option) {
        priority = ResourceLoadPriority::kLow;
      } else if (late_document_from_preload_scanner) {
        priority = ResourceLoadPriority::kMedium;
      }
    }
  } else if (type == ResourceType::kCSSStyleSheet &&
             late_document_from_preload_scanner) {
    // Lower the priority of late-body stylesheets discovered by the preload
    // scanner. They do not block render and this gives them the same behavior
    // as late-body scripts. If the main parser reaches the stylesheet before
    // it is loaded, a non-speculative fetch will be made and the priority will
    // be boosted (just like with scripts).
    priority = ResourceLoadPriority::kMedium;
  } else if (FetchParameters::kLazyLoad == defer_option) {
    priority = ResourceLoadPriority::kVeryLow;
  } else if (resource_request.GetRequestContext() ==
                 mojom::blink::RequestContextType::BEACON ||
             resource_request.GetRequestContext() ==
                 mojom::blink::RequestContextType::PING ||
             resource_request.GetRequestContext() ==
                 mojom::blink::RequestContextType::CSP_REPORT) {
    if (base::FeatureList::IsEnabled(features::kSetLowPriorityForBeacon)) {
      priority = ResourceLoadPriority::kLow;
    } else {
      priority = ResourceLoadPriority::kVeryLow;
    }
  }

  priority = AdjustPriorityWithPriorityHintAndRenderBlocking(
      priority, type, resource_request.GetFetchPriorityHint(),
      render_blocking_behavior);

  priority = AdjustImagePriority(priority, type, resource_request,
                                 speculative_preload_type, is_link_preload,
                                 resource_width, resource_height,
                                 is_potentially_lcp_element);

  if (properties_->IsSubframeDeprioritizationEnabled()) {
    if (!properties_->IsOutermostMainFrame()) {
      // When enabled, the priority of all resources in subframe is dropped.
      // Non-delayable resources are assigned a priority of kLow, and the rest
      // of them are assigned a priority of kLowest. This ensures that if the
      // webpage fetches most of its primary content using iframes, then high
      // priority requests within the iframe go on the network first.
      if (priority >= ResourceLoadPriority::kHigh) {
        priority = ResourceLoadPriority::kLow;
      } else {
        priority = ResourceLoadPriority::kLowest;
      }
    }
  }

  // LCP Critical Path Predictor identified resources get a priority boost.
  if (LcppEnabled()) {
    bool should_modify_request_priority = false;
    features::LcppResourceLoadPriority preferred_priority =
        features::LcppResourceLoadPriority::kMedium;

    if (is_potentially_lcp_element) {
      // Adjust priority of LCP image request.
      if (base::FeatureList::IsEnabled(features::kLCPCriticalPathPredictor) &&
          features::kLCPCriticalPathAdjustImageLoadPriority.Get()) {
        should_modify_request_priority = true;
        preferred_priority =
            features::kLCPCriticalPathPredictorImageLoadPriority.Get();
      }

      if (base::FeatureList::IsEnabled(features::kLCPScriptObserver) &&
          features::kLCPScriptObserverAdjustImageLoadPriority.Get()) {
        should_modify_request_priority = true;
        preferred_priority =
            features::kLCPScriptObserverImageLoadPriority.Get();
      }
    }

    if (is_potentially_lcp_influencer &&
        base::FeatureList::IsEnabled(features::kLCPScriptObserver)) {
      // Adjust priority of LCP influencing script request.
      should_modify_request_priority = true;
      preferred_priority = features::kLCPScriptObserverScriptLoadPriority.Get();
    }

    if (should_modify_request_priority) {
      ++potentially_lcp_resource_priority_boosts_;

      switch (preferred_priority) {
        case features::LcppResourceLoadPriority::kMedium:
          priority = std::max(priority, ResourceLoadPriority::kMedium);
          break;
        case features::LcppResourceLoadPriority::kHigh:
          priority = std::max(priority, ResourceLoadPriority::kHigh);
          break;
        case features::LcppResourceLoadPriority::kVeryHigh:
          priority = std::max(priority, ResourceLoadPriority::kVeryHigh);
          break;
      }
    }
  }

  return priority;
}

// Boost the priority for the first N not-small images from the preload scanner
ResourceLoadPriority ResourceFetcher::AdjustImagePriority(
    const ResourceLoadPriority priority_so_far,
    const ResourceType type,
    const ResourceRequestHead& resource_request,
    const FetchParameters::SpeculativePreloadType speculative_preload_type,
    const bool is_link_preload,
    const std::optional<float> resource_width,
    const std::optional<float> resource_height,
    const bool is_potentially_lcp_element) {
  if (type != ResourceType::kImage) {
    return priority_so_far;
  }

  ResourceLoadPriority new_priority = priority_so_far;

  // If the width or height is available, determine if it is a "small" image
  // where "small" is any image that covers less than 10,000px^2.
  // If a size can not be determined then it defaults to "not small"
  // and gets the relevant priority boost.
  bool is_small_image = false;
  if (resource_width && resource_height) {
    float image_area = resource_width.value() * resource_height.value();
    if (image_area <= small_image_max_size_) {
      is_small_image = true;
    }
  } else if (resource_width && resource_width == 0) {
    is_small_image = true;
  } else if (resource_height && resource_height == 0) {
    is_small_image = true;
  }

  if (speculative_preload_type ==
          FetchParameters::SpeculativePreloadType::kInDocument &&
      !is_link_preload && boosted_image_count_ < boosted_image_target_) {
    // Count all candidate images
    if (!is_small_image) {
      ++boosted_image_count_;

      // only boost the priority if one wasn't explicitly set
      if (new_priority < ResourceLoadPriority::kMedium &&
          resource_request.GetFetchPriorityHint() ==
              mojom::blink::FetchPriorityHint::kAuto) {
        new_priority = ResourceLoadPriority::kMedium;
      }
    }
  }

  // The following code disables AdjustImagePriority when there is LCPP
  // LcpElementLocator hint data. The reason why not to early return from this
  // function is that we want to record UMA with following
  // MaybeRecordBoostImagePriorityReason() function even when we disables
  // AdjustImagePriority.
  static const bool kOverrideFirstNBoost =
      base::FeatureList::IsEnabled(features::kLCPCriticalPathPredictor) &&
      features::kLCPCriticalPathAdjustImageLoadPriority.Get() &&
      features::kLCPCriticalPathAdjustImageLoadPriorityOverrideFirstNBoost
          .Get();
  if (kOverrideFirstNBoost &&
      context_->DoesLCPPHaveLcpElementLocatorHintData()) {
    new_priority = priority_so_far;
  }

  // Only records HTTP family URLs (e.g. Exclude data URLs).
  if (resource_request.Url().ProtocolIsInHTTPFamily()) {
    MaybeRecordBoostImagePriorityReason(priority_so_far != new_priority,
                                        is_potentially_lcp_element,
                                        is_small_image);
  }

  return new_priority;
}

ResourceFetcher::ResourceFetcher(const ResourceFetcherInit& init)
    : properties_(*init.properties),
      context_(init.context),
      freezable_task_runner_(init.freezable_task_runner),
      unfreezable_task_runner_(init.unfreezable_task_runner),
      use_counter_(init.use_counter
                       ? init.use_counter
                       : MakeGarbageCollected<DetachableUseCounter>(nullptr)),
      console_logger_(init.console_logger
                          ? init.console_logger
                          : MakeGarbageCollected<DetachableConsoleLogger>()),
      loader_factory_(init.loader_factory),
      scheduler_(MakeGarbageCollected<ResourceLoadScheduler>(
          init.initial_throttling_policy,
          init.throttle_option_override,
          *properties_,
          init.frame_or_worker_scheduler,
          *console_logger_,
          init.loading_behavior_observer)),
      back_forward_cache_loader_helper_(init.back_forward_cache_loader_helper),
      archive_(init.archive),
      resource_timing_report_timer_(
          freezable_task_runner_,
          this,
          &ResourceFetcher::ResourceTimingReportTimerFired),
      frame_or_worker_scheduler_(
          init.frame_or_worker_scheduler
              ? init.frame_or_worker_scheduler->GetWeakPtr()
              : nullptr),
      blob_registry_remote_(init.context_lifecycle_notifier),
      context_lifecycle_notifier_(init.context_lifecycle_notifier),
      auto_load_images_(true),
      allow_stale_resources_(false),
      image_fetched_(false),
      transparent_image_optimization_enabled_(base::FeatureList::IsEnabled(
          features::kSimplifyLoadingTransparentPlaceholderImage)) {
  InstanceCounters::IncrementCounter(InstanceCounters::kResourceFetcherCounter);

  // Determine the number of images that should get a boosted priority and the
  // pixel area threshold for determining "small" images.
  // TODO(http://crbug.com/1431169): Change these to constexpr after the
  // experiments determine appropriate values.
  if (base::FeatureList::IsEnabled(features::kBoostImagePriority)) {
    boosted_image_target_ = features::kBoostImagePriorityImageCount.Get();
    small_image_max_size_ = features::kBoostImagePriorityImageSize.Get();
  }

  if (IsMainThread()) {
    MainThreadFetchersSet().insert(this);
    MemoryPressureListenerRegistry::Instance().RegisterClient(this);
  }
}

ResourceFetcher::~ResourceFetcher() {
  InstanceCounters::DecrementCounter(InstanceCounters::kResourceFetcherCounter);
}

bool ResourceFetcher::IsDetached() const {
  return properties_->IsDetached();
}

Resource* ResourceFetcher::CachedResource(const KURL& resource_url) const {
  if (resource_url.IsEmpty())
    return nullptr;
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);
  const auto it = cached_resources_map_.find(url);
  if (it == cached_resources_map_.end())
    return nullptr;
  return it->value.Get();
}

bool ResourceFetcher::ResourceHasBeenEmulatedLoadStartedForInspector(
    const KURL& resource_url) const {
  if (resource_url.IsEmpty()) {
    return false;
  }
  KURL url = MemoryCache::RemoveFragmentIdentifierIfNeeded(resource_url);
  const auto it = emulated_load_started_for_inspector_resources_map_.find(url);
  if (it == emulated_load_started_for_inspector_resources_map_.end()) {
    return false;
  }
  return true;
}

const HeapHashSet<Member<Resource>>
ResourceFetcher::MoveResourceStrongReferences() {
  document_resource_strong_refs_total_size_ = 0;
  return std::move(document_resource_strong_refs_);
}

mojom::ControllerServiceWorkerMode
ResourceFetcher::IsControlledByServiceWorker() const {
  return properties_->GetControllerServiceWorkerMode();
}

ResourceFetcher::DeferPolicy ResourceFetcher::GetDeferPolicy(
    ResourceType type,
    const FetchParameters& params) const {
  // Defer a font load until it is actually needed unless this is a link
  // preload.
  if (type == ResourceType::kFont && !params.IsLinkPreload()) {
    return DeferPolicy::kDefer;
  }

  // Defer loading images when:
  // - images are disabled.
  // - image loading is disabled and the image is not a data url.
  // - instructed to defer loading images from network.
  if (type == ResourceType::kImage &&
      (ShouldDeferImageLoad(params.Url()) ||
       params.GetImageRequestBehavior() ==
           FetchParameters::ImageRequestBehavior::kDeferImageLoad)) {
    return DeferPolicy::kDefer;
  }

  // Check if the resource is marked as a potentially unused preload request.
  if (IsPotentiallyUnusedPreload(type, params)) {
    return DeferPolicy::kDeferAndSchedule;
  }

  return DeferPolicy::kNoDefer;
}

bool ResourceFetcher::ResourceAlreadyLoadStarted(Resource* resource,
                                                 RevalidationPolicy policy) {
  return policy == RevalidationPolicy::kUse && resource &&
         !resource->StillNeedsLoad();
}

bool ResourceFetcher::ResourceNeedsLoad(Resource* resource,
                                        RevalidationPolicy policy,
                                        DeferPolicy defer_policy) const {
  switch (defer_policy) {
    case DeferPolicy::kNoDefer:
      // MHTML documents should not trigger actual loads (i.e. all resource
      // requests should be fulfilled by the MHTML archive).
      return !archive_ && !ResourceAlreadyLoadStarted(resource, policy);
    case DeferPolicy::kDefer:
    case DeferPolicy::kDeferAndSchedule:
      return false;
  }
}

void ResourceFetcher::DidLoadResourceFromMemoryCache(
    Resource* resource,
    const ResourceRequest& request,
    bool is_static_data,
    RenderBlockingBehavior render_blocking_behavior) {
  if (IsDetached() || !resource_load_observer_)
    return;

  if (!is_static_data) {
    MarkEarlyHintConsumedIfNeeded(request.InspectorId(), resource,
                                  resource->GetResponse());
  }

  // Only call ResourceLoadObserver callbacks for placeholder images when
  // devtools is opened to get maximum performance.
  // TODO(crbug.com/41496436): Explore optimizing this in general for
  // `is_static_data`.
  if (!IsSimplifyLoadingTransparentPlaceholderImageEnabled() ||
      (request.GetKnownTransparentPlaceholderImageIndex() == kNotFound) ||
      (resource_load_observer_->InterestedInAllRequests())) {
    resource_load_observer_->WillSendRequest(
        request, ResourceResponse() /* redirects */, resource->GetType(),
        resource->Options(), render_blocking_behavior, resource);
    resource_load_observer_->DidReceiveResponse(
        request.InspectorId(), request, resource->GetResponse(), resource,
        ResourceLoadObserver::ResponseSource::kFromMemoryCache);
    if (resource->EncodedSize() > 0) {
      resource_load_observer_->DidReceiveData(
          request.InspectorId(),
          base::SpanOrSize<const char>(resource->EncodedSize()));
    }
    resource_load_observer_->DidFinishLoading(
        request.InspectorId(), base::TimeTicks(), 0,
        resource->GetResponse().DecodedBodyLength());
  }

  if (!is_static_data) {
    base::TimeTicks now = base::TimeTicks::Now();
    ResourceResponse final_response = resource->GetResponse();
    final_response.SetResourceLoadTiming(nullptr);
    final_response.SetEncodedDataLength(0);
    // Resources loaded from memory cache should be reported the first time
    // they're used.
    KURL initial_url =
        resource->GetResourceRequest().GetRedirectInfo().has_value()
            ? resource->GetResourceRequest().GetRedirectInfo()->original_url
            : resource->GetResourceRequest().Url();
    mojom::blink::ResourceTimingInfoPtr info =
        CreateResourceTimingInfo(now, initial_url, &final_response);
    info->response_end = now;
    info->render_blocking_status =
        render_blocking_behavior == RenderBlockingBehavior::kBlocking;

    // Create a ResourceLoadTiming object and store LCP breakdown timings for
    // images.
    if (resource->GetType() == ResourceType::kImage) {
      // The resource_load_timing may be null in tests.
      if (ResourceLoadTiming* resource_load_timing =
              resource->GetResponse().GetResourceLoadTiming()) {
        resource_load_timing->SetDiscoveryTime(info->start_time);
        resource_load_timing->SetSendStart(info->start_time);
        resource_load_timing->SetResponseEnd(info->start_time);
      }
    }

    AtomicString initiator_type = resource->IsPreloadedByEarlyHints()
                                      ? AtomicString(kEarlyHintsInitiatorType)
                                      : resource->Options().initiator_info.name;
    // If the fetch originated from user agent CSS we do not emit a resource
    // timing entry.
    if (initiator_type != fetch_initiator_type_names::kUacss) {
      scheduled_resource_timing_reports_.push_back(
          ScheduledResourceTimingInfo{std::move(info), initiator_type});

      if (!resource_timing_report_timer_.IsActive()) {
        resource_timing_report_timer_.StartOneShot(base::TimeDelta(),
                                                   FROM_HERE);
      }
    }
  }
}

Resource* ResourceFetcher::CreateResourceForStaticData(
    const FetchParameters& params,
    const ResourceFactory& factory) {
  const KURL& url = params.GetResourceRequest().Url();
  DCHECK(url.ProtocolIsData() || archive_);

  if (!archive_ && factory.GetType() == ResourceType::kRaw)
    return nullptr;

  const String cache_identifier = GetCacheIdentifier(url);
  // Most off-main-thread resource fetches use Resource::kRaw and don't reach
  // this point, but off-main-thread module fetches might.
  if (IsMainThread()) {
    if (Resource* old_resource =
            MemoryCache::Get()->ResourceForURL(url, cache_identifier)) {
      // There's no reason to re-parse if we saved the data from the previous
      // parse.
      if (params.Options().data_buffering_policy != kDoNotBufferData)
        return old_resource;
      MemoryCache::Get()->Remove(old_resource);
    }
  }

  ResourceResponse response;
  scoped_refptr<SharedBuffer> data;
  if (IsSimplifyLoadingTransparentPlaceholderImageEnabled() &&
      (params.GetResourceRequest().GetKnownTransparentPlaceholderImageIndex() !=
       kNotFound)) {
    // Skip the construction of `data`, since we won't use it.

    // We can defer the construction of `response`, but that would result in
    // `ImageResource` instantiation even in the data url parse failure
    // cases. Probably that's okay.
    // TODO(crbug.com/41496436): Revisit this.
  } else if (url.ProtocolIsData()) {
    int result;
    std::tie(result, response, data) = network_utils::ParseDataURL(
        url, params.GetResourceRequest().HttpMethod(),
        params.GetResourceRequest().GetUkmSourceId(), UkmRecorder());
    if (result != net::OK) {
      return nullptr;
    }
    // TODO(yhirano): Consider removing this.
    if (!IsSupportedMimeType(response.MimeType().Utf8())) {
      return nullptr;
    }
  } else {
    ArchiveResource* archive_resource =
        archive_->SubresourceForURL(params.Url());
    // The archive doesn't contain the resource, the request must be
    // aborted.
    if (!archive_resource) {
      return nullptr;
    }
    data = archive_resource->Data();
    response.SetCurrentRequestUrl(url);
    response.SetMimeType(archive_resource->MimeType());
    response.SetExpectedContentLength(data->size());
    response.SetTextEncodingName(archive_resource->TextEncoding());
    response.SetFromArchive(true);
  }

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  switch (resource->GetStatus()) {
    case ResourceStatus::kNotStarted:
      // We should not reach here on the transparent placeholder image
      // fast-path.
      CHECK(!IsSimplifyLoadingTransparentPlaceholderImageEnabled() ||
            (params.GetResourceRequest()
                 .GetKnownTransparentPlaceholderImageIndex() == kNotFound));

      // The below code, with the exception of `NotifyStartLoad()` and
      // `Finish()`, is the same as in
      // `CreateResourceForTransparentPlaceholderImage()`.
      resource->NotifyStartLoad();
      // FIXME: We should provide a body stream here.
      resource->ResponseReceived(response);
      resource->SetDataBufferingPolicy(kBufferData);
      if (data->size()) {
        resource->SetResourceBuffer(data);
      }
      resource->SetCacheIdentifier(cache_identifier);
      resource->Finish(base::TimeTicks(), freezable_task_runner_.get());
      break;

    case ResourceStatus::kCached:
      // The constructed resource already has a synthetic response set.

      // We should only reach here on the transparent placeholder image
      // fast-path.
      CHECK(IsSimplifyLoadingTransparentPlaceholderImageEnabled());
      CHECK_NE(params.GetResourceRequest()
                   .GetKnownTransparentPlaceholderImageIndex(),
               kNotFound);

      use_counter_->CountUse(
          WebFeature::kSimplifyLoadingTransparentPlaceholderImage);

      // There shouldn't be any `ResourceClient`s that need to be
      // notified of synthetic response received steps.
      CHECK(!resource->HasClientsOrObservers());
      break;

    default:
      CHECK(false) << "Unexpected resource status: "
                   << (int)resource->GetStatus();
  }

  AddToMemoryCacheIfNeeded(params, resource);
  return resource;
}

Resource* ResourceFetcher::ResourceForBlockedRequest(
    const FetchParameters& params,
    const ResourceFactory& factory,
    ResourceRequestBlockedReason blocked_reason,
    ResourceClient* client) {
  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  if (client)
    client->SetResource(resource, freezable_task_runner_.get());
  resource->FinishAsError(ResourceError::CancelledDueToAccessCheckError(
                              params.Url(), blocked_reason),
                          freezable_task_runner_.get());
  return resource;
}

void ResourceFetcher::MakePreloadedResourceBlockOnloadIfNeeded(
    Resource* resource,
    const FetchParameters& params) {
  // TODO(yoav): Test that non-blocking resources (video/audio/track) continue
  // to not-block even after being preloaded and discovered.
  if (resource && resource->Loader() &&
      resource->IsLoadEventBlockingResourceType() &&
      resource->IsLinkPreload() && !params.IsLinkPreload() &&
      non_blocking_loaders_.Contains(resource->Loader())) {
    non_blocking_loaders_.erase(resource->Loader());
    loaders_.insert(resource->Loader());
    if (resource_load_observer_) {
      resource_load_observer_->DidChangeRenderBlockingBehavior(resource,
                                                               params);
    }
  }
}

ResourceFetcher::RevalidationPolicyForMetrics
ResourceFetcher::MapToPolicyForMetrics(RevalidationPolicy policy,
                                       Resource* resource,
                                       DeferPolicy defer_policy) {
  switch (defer_policy) {
    case DeferPolicy::kNoDefer:
      break;
    case DeferPolicy::kDefer:
    case DeferPolicy::kDeferAndSchedule:
      if (!ResourceAlreadyLoadStarted(resource, policy)) {
        return RevalidationPolicyForMetrics::kDefer;
      }
      break;
  }
  // A resource in memory cache but not yet loaded is a deferred resource
  // created in previous loads.
  if (policy == RevalidationPolicy::kUse && resource->StillNeedsLoad()) {
    return RevalidationPolicyForMetrics::kPreviouslyDeferredLoad;
  }
  switch (policy) {
    case RevalidationPolicy::kUse:
      return RevalidationPolicyForMetrics::kUse;
    case RevalidationPolicy::kRevalidate:
      return RevalidationPolicyForMetrics::kRevalidate;
    case RevalidationPolicy::kReload:
      return RevalidationPolicyForMetrics::kReload;
    case RevalidationPolicy::kLoad:
      return RevalidationPolicyForMetrics::kLoad;
  }
}

void ResourceFetcher::UpdateMemoryCacheStats(
    Resource* resource,
    RevalidationPolicyForMetrics policy,
    const FetchParameters& params,
    const ResourceFactory& factory,
    bool is_static_data,
    bool same_top_frame_site_resource_cached) const {
  // Do not count static data or data not associated with the MemoryCache.
  if (is_static_data || !IsMainThread()) {
    return;
  }

  if (params.IsSpeculativePreload() || params.IsLinkPreload()) {
    RecordResourceHistogram("Preload.", factory.GetType(), policy);
  } else {
    RecordResourceHistogram("", factory.GetType(), policy);
  }

  // Aims to count Resource only referenced from MemoryCache (i.e. what would be
  // dead if MemoryCache holds weak references to Resource). Currently we check
  // references to Resource from ResourceClient and `preloads_` only, because
  // they are major sources of references.
  if (resource && !resource->IsAlive() && !ContainsAsPreload(resource)) {
    RecordResourceHistogram("Dead.", factory.GetType(), policy);
  }

  // Async (and defer) scripts may have more cache misses, track them
  // separately. See https://crbug.com/1043679 for context.
  if (params.Defer() != FetchParameters::DeferOption::kNoDefer &&
      factory.GetType() == ResourceType::kScript) {
    UMA_HISTOGRAM_ENUMERATION(RESOURCE_HISTOGRAM_PREFIX "AsyncScript", policy);
  }
}

bool ResourceFetcher::ContainsAsPreload(Resource* resource) const {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  return it != preloads_.end() && it->value == resource;
}

void ResourceFetcher::RemovePreload(Resource* resource) {
  auto it = preloads_.find(PreloadKey(resource->Url(), resource->GetType()));
  if (it == preloads_.end())
    return;
  if (it->value == resource)
    preloads_.erase(it);
}

std::optional<ResourceRequestBlockedReason>
ResourceFetcher::UpdateRequestForTransparentPlaceholderImage(
    FetchParameters& params) {
  ResourceRequest& resource_request = params.MutableResourceRequest();
  // Should only be called if request has transparent-placholder-image.
  DCHECK(IsSimplifyLoadingTransparentPlaceholderImageEnabled() &&
         (resource_request.GetKnownTransparentPlaceholderImageIndex() !=
          kNotFound));
  // Since we are not actually sending the request to the server,
  // we skip construction of the full ResourceRequest for performance,
  // and only set the properties needed for observer callbacks.
  // TODO(crbug.com/41496436): We need additional work to expand to
  // generic data urls.
  resource_request.SetPriority(ResourceLoadPriority::kLow);
  SetReferrer(resource_request, properties_->GetFetchClientSettingsObject());

  // We check the report-only and enforced headers here to ensure we report
  // and block things we ought to block.
  if (Context().CheckAndEnforceCSPForRequest(
          resource_request.GetRequestContext(),
          resource_request.GetRequestDestination(), params.Url(),
          params.Options(), ReportingDisposition::kReport, params.Url(),
          ResourceRequestHead::RedirectStatus::kNoRedirect) ==
      ResourceRequestBlockedReason::kCSP) {
    return ResourceRequestBlockedReason::kCSP;
  }

  return std::nullopt;
}

KURL ResourceFetcher::PrepareRequestForWebBundle(
    ResourceRequest& resource_request) const {
  if (resource_request.GetWebBundleTokenParams()) {
    DCHECK_EQ(resource_request.GetRequestDestination(),
              network::mojom::RequestDestination::kWebBundle);
    return KURL();
  }
  if (SubresourceWebBundle* bundle =
          GetMatchingBundle(resource_request.Url())) {
    resource_request.SetWebBundleTokenParams(
        ResourceRequestHead::WebBundleTokenParams(bundle->GetBundleUrl(),
                                                  bundle->WebBundleToken(),
                                                  mojo::NullRemote()));

    // Skip the service worker for a short term solution.
    // TODO(crbug.com/1240424): Figure out the ideal design of the service
    // worker integration.
    resource_request.SetSkipServiceWorker(true);
  }
  if (resource_request.Url().Protocol() == "uuid-in-package" &&
      resource_request.GetWebBundleTokenParams()) {
    // We use the bundle URL for uuid-in-package: resources for security
    // checks.
    return resource_request.GetWebBundleTokenParams()->bundle_url;
  }
  return KURL();
}

SubresourceWebBundleList*
ResourceFetcher::GetOrCreateSubresourceWebBundleList() {
  if (subresource_web_bundles_)
    return subresource_web_bundles_.Get();
  subresource_web_bundles_ = MakeGarbageCollected<SubresourceWebBundleList>();
  return subresource_web_bundles_.Get();
}

ukm::MojoUkmRecorder* ResourceFetcher::UkmRecorder() {
  if (ukm_recorder_)
    return ukm_recorder_.get();

  mojo::Remote<ukm::mojom::UkmRecorderFactory> factory;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      factory.BindNewPipeAndPassReceiver());
  ukm_recorder_ = ukm::MojoUkmRecorder::Create(*factory);

  return ukm_recorder_.get();
}

Resource* ResourceFetcher::RequestResource(FetchParameters& params,
                                           const ResourceFactory& factory,
                                           ResourceClient* client) {
  base::AutoReset<bool> r(&is_in_request_resource_, true);

  // If detached, we do very early return here to skip all processing below.
  if (properties_->IsDetached()) {
    return ResourceForBlockedRequest(
        params, factory, ResourceRequestBlockedReason::kOther, client);
  }

  if (resource_load_observer_) {
    resource_load_observer_->DidStartRequest(params, factory.GetType());
  }

  // Otherwise, we assume we can send network requests and the fetch client's
  // settings object's origin is non-null.
  DCHECK(properties_->GetFetchClientSettingsObject().GetSecurityOrigin());

  uint64_t identifier = CreateUniqueIdentifier();
  ResourceRequest& resource_request = params.MutableResourceRequest();
  resource_request.SetInspectorId(identifier);
  resource_request.SetFromOriginDirtyStyleSheet(
      params.IsFromOriginDirtyStyleSheet());
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
      TRACE_ID_WITH_SCOPE("BlinkResourceID", TRACE_ID_LOCAL(identifier)), "url",
      resource_request.Url());
  absl::Cleanup record_times = [start = base::TimeTicks::Now(), &params] {
    base::TimeDelta elapsed = base::TimeTicks::Now() - start;
    base::UmaHistogramMicrosecondsTimes("Blink.Fetch.RequestResourceTime2",
                                        elapsed);
    if (params.Url().ProtocolIsData()) {
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Fetch.RequestResourceTime2.Data", elapsed);
      if (params.GetResourceRequest()
              .GetKnownTransparentPlaceholderImageIndex() != kNotFound) {
        base::UmaHistogramMicrosecondsTimes(
            "Blink.Fetch.RequestResourceTime2.TransparentPlaceholderImage",
            elapsed);
      }
    }
    if (params.IsSpeculativePreload() || params.IsLinkPreload()) {
      base::UmaHistogramMicrosecondsTimes(
          "Blink.Fetch.RequestResourceTime2.Preload", elapsed);
    }
  };
  TRACE_EVENT1("blink,blink.resource", "ResourceFetcher::requestResource",
               "url", params.Url().ElidedString().Utf8());

  // |resource_request|'s origin can be null here, corresponding to the "client"
  // value in the spec. In that case client's origin is used.
  if (!resource_request.RequestorOrigin()) {
    resource_request.SetRequestorOrigin(
        properties_->GetFetchClientSettingsObject().GetSecurityOrigin());
  }

  const ResourceType resource_type = factory.GetType();

  WebScopedVirtualTimePauser pauser;

  ResourcePrepareHelper prepare_helper(*this, params, factory);
  std::optional<ResourceRequestBlockedReason> blocked_reason =
      prepare_helper.PrepareRequestForCacheAccess(pauser);
  if (blocked_reason) {
    auto* resource = ResourceForBlockedRequest(params, factory,
                                               blocked_reason.value(), client);
    StorePerformanceTimingInitiatorInformation(
        resource, params.GetRenderBlockingBehavior());
    auto info = resource_timing_info_map_.Take(resource);
    if (!info.is_null()) {
      PopulateAndAddResourceTimingInfo(resource, info,
                                       /*response_end=*/base::TimeTicks::Now());
    }
    return resource;
  }

  Resource* resource = nullptr;
  RevalidationPolicy policy = RevalidationPolicy::kLoad;

  bool is_data_url = resource_request.Url().ProtocolIsData();
  bool is_static_data = is_data_url || archive_;
  bool is_stale_revalidation = params.IsStaleRevalidation();
  DeferPolicy defer_policy = GetDeferPolicy(resource_type, params);
  // MHTML archives do not load from the network and must load immediately. Data
  // urls can also load immediately, except in cases when they should be
  // deferred.
  if (!is_stale_revalidation &&
      (archive_ || (is_data_url && defer_policy != DeferPolicy::kDefer))) {
    prepare_helper.UpgradeForLoaderIfNecessary(pauser);
    resource = CreateResourceForStaticData(params, factory);
    if (resource) {
      policy =
          DetermineRevalidationPolicy(resource_type, params, *resource, true);
    } else if (!is_data_url && archive_) {
      // Abort the request if the archive doesn't contain the resource, except
      // in the case of data URLs which might have resources such as fonts that
      // need to be decoded only on demand. These data URLs are allowed to be
      // processed using the normal ResourceFetcher machinery.
      return ResourceForBlockedRequest(
          params, factory, ResourceRequestBlockedReason::kOther, client);
    }
  }

  bool same_top_frame_site_resource_cached = false;
  bool in_cached_resources_map = cached_resources_map_.Contains(
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url()));

  if (!is_stale_revalidation && !resource) {
    if (!prepare_helper.WasUpgradeForLoaderCalled() &&
        preloads_.find(PreloadKey(params.Url(), resource_type)) !=
            preloads_.end()) {
      prepare_helper.UpgradeForLoaderIfNecessary(pauser);
    }
    resource = MatchPreload(params, resource_type);
    if (resource) {
      policy = RevalidationPolicy::kUse;
      prepare_helper.UpgradeForLoaderIfNecessary(pauser);
      // If |params| is for a blocking resource and a preloaded resource is
      // found, we may need to make it block the onload event.
      MakePreloadedResourceBlockOnloadIfNeeded(resource, params);
    } else if (IsMainThread()) {
      resource = MemoryCache::Get()->ResourceForURL(
          params.Url(), GetCacheIdentifier(params.Url()));
      if (resource) {
        policy = DetermineRevalidationPolicy(resource_type, params, *resource,
                                             is_static_data);
        scoped_refptr<const SecurityOrigin> top_frame_origin =
            resource_request.TopFrameOrigin();
        if (top_frame_origin) {
          same_top_frame_site_resource_cached =
              resource->AppendTopFrameSiteForMetrics(*top_frame_origin);
        }
      }
    }
  }
  if (!prepare_helper.WasUpgradeForLoaderCalled() &&
      policy != RevalidationPolicy::kUse) {
    prepare_helper.UpgradeForLoaderIfNecessary(pauser);
  }

  UpdateMemoryCacheStats(
      resource, MapToPolicyForMetrics(policy, resource, defer_policy), params,
      factory, is_static_data, same_top_frame_site_resource_cached);

  switch (policy) {
    case RevalidationPolicy::kReload:
      MemoryCache::Get()->Remove(resource);
      [[fallthrough]];
    case RevalidationPolicy::kLoad:
      resource = CreateResourceForLoading(params, factory);
      break;
    case RevalidationPolicy::kRevalidate:
      InitializeRevalidation(resource_request, resource);
      break;
    case RevalidationPolicy::kUse:
      if (resource_request.AllowsStaleResponse() &&
          resource->ShouldRevalidateStaleResponse()) {
        ScheduleStaleRevalidate(resource);
      }
      break;
  }
  DCHECK(resource);
  DCHECK_EQ(resource->GetType(), resource_type);

  // in_cached_resources_map is checked to detect Resources shared across
  // Documents, in the same way as features::kScopeMemoryCachePerContext.
  if (!is_static_data && policy == RevalidationPolicy::kUse &&
      !in_cached_resources_map) {
    base::UmaHistogramEnumeration(kCrossDocumentCachedResource,
                                  resource->GetType());
  }

  if (policy != RevalidationPolicy::kUse)
    resource->VirtualTimePauser() = std::move(pauser);

  if (client)
    client->SetResource(resource, freezable_task_runner_.get());

  // Increase the priority of an existing request if the new request is
  // of a higher priority.
  // This can happen in a lot of cases but a common one is if a resource is
  // preloaded at a low priority but then the resource itself requires a
  // high-priority load.
  if (resource_request.Priority() > resource->GetResourceRequest().Priority()) {
    resource->DidChangePriority(resource_request.Priority(), 0);
  }

  // If only the fragment identifiers differ, it is the same resource.
  DCHECK(EqualIgnoringFragmentIdentifier(resource->Url(), params.Url()));
  if (policy == RevalidationPolicy::kUse &&
      resource->GetStatus() == ResourceStatus::kCached &&
      !in_cached_resources_map) {
    // Loaded from MemoryCache.
    DidLoadResourceFromMemoryCache(resource, resource_request, is_static_data,
                                   params.GetRenderBlockingBehavior());
  }
  if (!is_stale_revalidation) {
    String resource_url =
        MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
    cached_resources_map_.Set(resource_url, resource);
    MaybeSaveResourceToStrongReference(resource);
    if (PriorityObserverMapCreated() &&
        PriorityObservers()->Contains(resource_url)) {
      // Resolve the promise.
      std::move(PriorityObservers()->Take(resource_url))
          .Run(static_cast<int>(
              resource->GetResourceRequest().InitialPriority()));
    }
  }

  // Image loaders are by default added to |loaders_|, and are therefore
  // load-blocking. Lazy loaded images that are eventually fetched, however,
  // should always be added to |non_blocking_loaders_|, as they are never
  // load-blocking.
  ImageLoadBlockingPolicy load_blocking_policy =
      ImageLoadBlockingPolicy::kDefault;
  if (resource->GetType() == ResourceType::kImage) {
    not_loaded_image_resources_.insert(resource);
    if (params.GetImageRequestBehavior() ==
        FetchParameters::ImageRequestBehavior::kNonBlockingImage) {
      load_blocking_policy = ImageLoadBlockingPolicy::kForceNonBlockingLoad;
    }
  }

  // Returns with an existing resource if the resource does not need to start
  // loading immediately. If revalidation policy was determined as |Revalidate|,
  // the resource was already initialized for the revalidation here, but won't
  // start loading.
  const bool needs_load = ResourceNeedsLoad(resource, policy, defer_policy);
  if (needs_load) {
    // If a load is necessary, force upgrade so that the resource width is
    // updated. This is a bit heavyweight, and could be optimized by adding
    // a new function specifically to add the width.
    prepare_helper.UpgradeForLoaderIfNecessary(pauser);
  }

  // The resource width can change after the request was initially created.
  if (prepare_helper.WasUpgradeForLoaderCalled()) {
    resource->UpdateResourceWidth(
        resource_request.HttpHeaderField(AtomicString("sec-ch-width")));
  }

  if (needs_load) {
    if (!StartLoad(resource,
                   std::move(params.MutableResourceRequest().MutableBody()),
                   load_blocking_policy, params.GetRenderBlockingBehavior())) {
      resource->FinishAsError(ResourceError::CancelledError(params.Url()),
                              freezable_task_runner_.get());
    }
  }

  if (defer_policy == DeferPolicy::kDeferAndSchedule) {
    // If |resource| is potentially unused preload based on the LCPP hint,
    // schedule the loading instead of calling `StartLoad()`.
    ScheduleLoadingPotentiallyUnusedPreload(resource);
  }

  if (policy != RevalidationPolicy::kUse)
    InsertAsPreloadIfNecessary(resource, params, resource_type);

  if (resource->InspectorId() != identifier ||
      (!resource->StillNeedsLoad() && !resource->IsLoading())) {
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        TRACE_DISABLED_BY_DEFAULT("network"), "ResourceLoad",
        TRACE_ID_WITH_SCOPE("BlinkResourceID", TRACE_ID_LOCAL(identifier)),
        "outcome", "Fail");
  }
  return resource;
}

void ResourceFetcher::RemoveResourceStrongReference(Resource* resource) {
  if (resource && document_resource_strong_refs_.Contains(resource)) {
    const size_t resource_size =
        static_cast<size_t>(resource->GetResponse().DecodedBodyLength());
    document_resource_strong_refs_.erase(resource);
    CHECK_GE(document_resource_strong_refs_total_size_, resource_size);
    document_resource_strong_refs_total_size_ -= resource_size;
  }
}

bool ResourceFetcher::HasStrongReferenceForTesting(Resource* resource) {
  return document_resource_strong_refs_.Contains(resource);
}

void ResourceFetcher::ResourceTimingReportTimerFired(TimerBase* timer) {
  DCHECK_EQ(timer, &resource_timing_report_timer_);
  Vector<ScheduledResourceTimingInfo> timing_reports;
  timing_reports.swap(scheduled_resource_timing_reports_);
  for (auto& scheduled_report : timing_reports) {
    Context().AddResourceTiming(std::move(scheduled_report.info),
                                scheduled_report.initiator_type);
  }
}

void ResourceFetcher::InitializeRevalidation(
    ResourceRequest& revalidating_request,
    Resource* resource) {
  DCHECK(resource);
  DCHECK(MemoryCache::Get()->Contains(resource));
  DCHECK(resource->IsLoaded());
  DCHECK(resource->CanUseCacheValidator());
  DCHECK(!resource->IsCacheValidator());
  DCHECK_EQ(properties_->GetControllerServiceWorkerMode(),
            mojom::ControllerServiceWorkerMode::kNoController);
  // RawResource doesn't support revalidation.
  CHECK(!IsRawResource(*resource));

  revalidating_request.SetIsRevalidating(true);

  const AtomicString& last_modified =
      resource->GetResponse().HttpHeaderField(http_names::kLastModified);
  const AtomicString& e_tag =
      resource->GetResponse().HttpHeaderField(http_names::kETag);
  if (!last_modified.empty() || !e_tag.empty()) {
    DCHECK_NE(mojom::blink::FetchCacheMode::kBypassCache,
              revalidating_request.GetCacheMode());
    if (revalidating_request.GetCacheMode() ==
        mojom::blink::FetchCacheMode::kValidateCache) {
      revalidating_request.SetHttpHeaderField(http_names::kCacheControl,
                                              AtomicString("max-age=0"));
    }
  }
  if (!last_modified.empty()) {
    revalidating_request.SetHttpHeaderField(http_names::kIfModifiedSince,
                                            last_modified);
  }
  if (!e_tag.empty())
    revalidating_request.SetHttpHeaderField(http_names::kIfNoneMatch, e_tag);

  resource->SetRevalidatingRequest(revalidating_request);
}

namespace {

bool UseRenderBlockingTaskPriority(
    const mojom::blink::RequestContextType request_context,
    const RenderBlockingBehavior render_blocking_behavior) {
  switch (request_context) {
    case mojom::blink::RequestContextType::IMAGE:
      // Always boost the priority of images (see: https://crbug.com/1416030).
      return true;
    case mojom::blink::RequestContextType::IMAGE_SET:
      return base::FeatureList::IsEnabled(
          features::kBoostImageSetLoadingTaskPriority);
    case mojom::blink::RequestContextType::FONT:
      return base::FeatureList::IsEnabled(
          features::kBoostFontLoadingTaskPriority);
    case mojom::blink::RequestContextType::VIDEO:
      return base::FeatureList::IsEnabled(
          features::kBoostVideoLoadingTaskPriority);
    case mojom::blink::RequestContextType::STYLE:
      if (render_blocking_behavior == RenderBlockingBehavior::kBlocking) {
        return base::FeatureList::IsEnabled(
            features::kBoostRenderBlockingStyleLoadingTaskPriority);
      }
      return base::FeatureList::IsEnabled(
          features::kBoostNonRenderBlockingStyleLoadingTaskPriority);
    default:
      return false;
  }
}

}  // namespace

std::unique_ptr<URLLoader> ResourceFetcher::CreateURLLoader(
    const network::ResourceRequest& network_request,
    const ResourceLoaderOptions& options,
    const mojom::blink::RequestContextType request_context,
    const RenderBlockingBehavior render_blocking_behavior,
    const std::optional<base::UnguessableToken>&
        service_worker_race_network_request_token,
    bool is_from_origin_dirty_style_sheet) {
  DCHECK(!GetProperties().IsDetached());
  // TODO(http://crbug.com/1252983): Revert this to DCHECK.
  CHECK(loader_factory_);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      unfreezable_task_runner_;
  if (network_request.keepalive &&
      (!base::FeatureList::IsEnabled(
           blink::features::kKeepAliveInBrowserMigration) ||
       (network_request.attribution_reporting_eligibility !=
            network::mojom::AttributionReportingEligibility::kUnset &&
        !base::FeatureList::IsEnabled(
            features::kAttributionReportingInBrowserMigration)))) {
    // Set the `task_runner` to the `AgentGroupScheduler`'s task-runner for
    // keepalive fetches because we want it to keep running even after the
    // frame is detached. It's pretty fragile to do that with the
    // `unfreezable_task_runner_` that's saved in the ResourceFetcher, because
    // that task runner is frame-associated.
    if (auto* frame_or_worker_scheduler = GetFrameOrWorkerScheduler()) {
      if (auto* frame_scheduler =
              frame_or_worker_scheduler->ToFrameScheduler()) {
        task_runner =
            frame_scheduler->GetAgentGroupScheduler()->DefaultTaskRunner();
      }
    }
  } else if (UseRenderBlockingTaskPriority(request_context,
                                           render_blocking_behavior)) {
    if (auto* frame_or_worker_scheduler = GetFrameOrWorkerScheduler()) {
      if (auto* frame_scheduler =
              frame_or_worker_scheduler->ToFrameScheduler()) {
        task_runner = frame_scheduler->GetTaskRunner(
            TaskType::kNetworkingUnfreezableRenderBlockingLoading);
      }
    }
  }
  return loader_factory_->CreateURLLoader(
      network_request, options, freezable_task_runner_, task_runner,
      back_forward_cache_loader_helper_,
      service_worker_race_network_request_token,
      is_from_origin_dirty_style_sheet);
}

CodeCacheHost* ResourceFetcher::GetCodeCacheHost() {
  DCHECK(!GetProperties().IsDetached());
  // TODO(http://crbug.com/1252983): Revert this to DCHECK.
  CHECK(loader_factory_);
  return loader_factory_->GetCodeCacheHost();
}

void ResourceFetcher::AddToMemoryCacheIfNeeded(const FetchParameters& params,
                                               Resource* resource) {
  if (!ShouldResourceBeAddedToMemoryCache(params, resource))
    return;

  MemoryCache::Get()->Add(resource);
}

Resource* ResourceFetcher::CreateResourceForLoading(
    const FetchParameters& params,
    const ResourceFactory& factory) {
  const String cache_identifier =
      GetCacheIdentifier(params.GetResourceRequest().Url());
  DCHECK(!IsMainThread() || params.IsStaleRevalidation() ||
         !MemoryCache::Get()->ResourceForURL(params.GetResourceRequest().Url(),
                                             cache_identifier));

  RESOURCE_LOADING_DVLOG(1) << "Loading Resource for "
                            << params.GetResourceRequest().Url().ElidedString();

  Resource* resource = factory.Create(
      params.GetResourceRequest(), params.Options(), params.DecoderOptions());
  resource->SetLinkPreload(params.IsLinkPreload());
  resource->SetCacheIdentifier(cache_identifier);

  AddToMemoryCacheIfNeeded(params, resource);
  return resource;
}

void ResourceFetcher::StorePerformanceTimingInitiatorInformation(
    Resource* resource,
    RenderBlockingBehavior render_blocking_behavior) {
  const AtomicString& fetch_initiator = resource->Options().initiator_info.name;
  if (fetch_initiator == fetch_initiator_type_names::kInternal)
    return;

  resource_timing_info_map_.insert(
      resource,
      PendingResourceTimingInfo{base::TimeTicks::Now(), fetch_initiator,
                                render_blocking_behavior});
}

void ResourceFetcher::RecordResourceTimingOnRedirect(
    Resource* resource,
    const ResourceResponse& redirect_response,
    const KURL& new_url) {
  PendingResourceTimingInfoMap::iterator it =
      resource_timing_info_map_.find(resource);
  if (it != resource_timing_info_map_.end()) {
    if (ResourceLoadTiming* load_timing =
            redirect_response.GetResourceLoadTiming()) {
      it->value.redirect_end_time = load_timing->ReceiveHeadersEnd();
    }
  }
}

static bool IsDownloadOrStreamRequest(const ResourceRequest& request) {
  // Never use cache entries for DownloadToBlob / UseStreamOnResponse requests.
  // The data will be delivered through other paths.
  return request.DownloadToBlob() || request.UseStreamOnResponse();
}

Resource* ResourceFetcher::MatchPreload(const FetchParameters& params,
                                        ResourceType type) {
  // TODO(crbug.com/1099975): PreloadKey should be modified to also take into
  // account the DOMWrapperWorld corresponding to the resource. This is because
  // we probably don't want to share preloaded resources across different
  // DOMWrapperWorlds to ensure predicatable behavior for preloads.
  auto it = preloads_.find(PreloadKey(params.Url(), type));
  if (it == preloads_.end())
    return nullptr;

  Resource* resource = it->value;

  if (resource->MustRefetchDueToIntegrityMetadata(params)) {
    if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
      PrintPreloadMismatch(resource, Resource::MatchStatus::kIntegrityMismatch);
    return nullptr;
  }

  if (params.IsSpeculativePreload())
    return resource;
  if (params.IsLinkPreload()) {
    resource->SetLinkPreload(true);
    return resource;
  }

  const ResourceRequest& request = params.GetResourceRequest();
  if (request.DownloadToBlob()) {
    PrintPreloadMismatch(resource, Resource::MatchStatus::kBlobRequest);
    return nullptr;
  }

  if (IsImageResourceDisallowedToBeReused(*resource)) {
    PrintPreloadMismatch(resource,
                         Resource::MatchStatus::kImageLoadingDisabled);
    return nullptr;
  }

  const Resource::MatchStatus match_status = resource->CanReuse(params);
  if (match_status != Resource::MatchStatus::kOk) {
    PrintPreloadMismatch(resource, match_status);
    return nullptr;
  }

  resource->MatchPreload(params);
  preloads_.erase(it);
  matched_preloads_.push_back(resource);
  return resource;
}

void ResourceFetcher::PrintPreloadMismatch(Resource* resource,
                                           Resource::MatchStatus status) {
  if (!resource->IsLinkPreload())
    return;

  StringBuilder builder;
  builder.Append("A preload for '");
  builder.Append(resource->Url().GetString());
  builder.Append("' is found, but is not used ");

  switch (status) {
    case Resource::MatchStatus::kOk:
      NOTREACHED_IN_MIGRATION();
      break;
    case Resource::MatchStatus::kUnknownFailure:
      builder.Append("due to an unknown reason.");
      break;
    case Resource::MatchStatus::kIntegrityMismatch:
      builder.Append("due to an integrity mismatch.");
      break;
    case Resource::MatchStatus::kBlobRequest:
      builder.Append("because the new request loads the content as a blob.");
      break;
    case Resource::MatchStatus::kImageLoadingDisabled:
      builder.Append("because image loading is disabled.");
      break;
    case Resource::MatchStatus::kSynchronousFlagDoesNotMatch:
      builder.Append("because the new request is synchronous.");
      break;
    case Resource::MatchStatus::kRequestModeDoesNotMatch:
      builder.Append("because the request mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kRequestCredentialsModeDoesNotMatch:
      builder.Append("because the request credentials mode does not match. ");
      builder.Append("Consider taking a look at crossorigin attribute.");
      break;
    case Resource::MatchStatus::kKeepaliveSet:
      builder.Append("because the keepalive flag is set.");
      break;
    case Resource::MatchStatus::kRequestMethodDoesNotMatch:
      builder.Append("because the request HTTP method does not match.");
      break;
    case Resource::MatchStatus::kScriptTypeDoesNotMatch:
      builder.Append("because the script type does not match.");
      break;
  }
  console_logger_->AddConsoleMessage(mojom::ConsoleMessageSource::kOther,
                                     mojom::ConsoleMessageLevel::kWarning,
                                     builder.ToString());

  TRACE_EVENT1("blink,blink.resource", "ResourceFetcher::PrintPreloadMismatch",
               "data",
               CreateTracedValueForUnusedPreload(
                   resource->Url(), status,
                   resource->GetResourceRequest().GetDevToolsId()));
}

void ResourceFetcher::InsertAsPreloadIfNecessary(Resource* resource,
                                                 const FetchParameters& params,
                                                 ResourceType type) {
  if (!params.IsSpeculativePreload() && !params.IsLinkPreload())
    return;
  DCHECK(!params.IsStaleRevalidation());
  // CSP web tests verify that preloads are subject to access checks by
  // seeing if they are in the `preload started` list. Therefore do not add
  // them to the list if the load is immediately denied.
  if (resource->LoadFailedOrCanceled() &&
      resource->GetResourceError().IsAccessCheck()) {
    return;
  }
  PreloadKey key(params.Url(), type);
  if (base::Contains(preloads_, key)) {
    return;
  }

  preloads_.insert(key, resource);
  resource->MarkAsPreload();
  if (preloaded_urls_for_test_)
    preloaded_urls_for_test_->insert(resource->Url().GetString());
}

bool ResourceFetcher::IsImageResourceDisallowedToBeReused(
    const Resource& existing_resource) const {
  // When images are disabled, don't ever load images, even if the image is
  // cached or it is a data: url. In this case:
  // - remove the image from the memory cache, and
  // - create a new resource but defer loading (this is done by
  //   ResourceNeedsLoad()).
  //
  // This condition must be placed before the condition on |is_static_data| to
  // prevent loading a data: URL.
  //
  // TODO(japhet): Can we get rid of one of these settings?

  if (existing_resource.GetType() != ResourceType::kImage)
    return false;

  return !Context().AllowImage();
}

ResourceFetcher::RevalidationPolicy
ResourceFetcher::DetermineRevalidationPolicy(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  RevalidationPolicy policy;
  const char* reason;
  std::tie(policy, reason) = DetermineRevalidationPolicyInternal(
      type, fetch_params, existing_resource, is_static_data);
  DCHECK(reason);

  RESOURCE_LOADING_DVLOG(1)
      << "ResourceFetcher::DetermineRevalidationPolicy "
      << "url = " << fetch_params.Url() << ", policy = " << GetNameFor(policy)
      << ", reason = \"" << reason << "\"";

  TRACE_EVENT_INSTANT2("blink", "ResourceFetcher::DetermineRevalidationPolicy",
                       TRACE_EVENT_SCOPE_THREAD, "policy", GetNameFor(policy),
                       "reason", reason);
  return policy;
}

const char* ResourceFetcher::GetNameFor(RevalidationPolicy policy) {
  switch (policy) {
    case RevalidationPolicy::kUse:
      return "use";
    case RevalidationPolicy::kRevalidate:
      return "revalidate";
    case RevalidationPolicy::kReload:
      return "reload";
    case RevalidationPolicy::kLoad:
      return "load";
  }
  NOTREACHED_IN_MIGRATION();
}

std::pair<ResourceFetcher::RevalidationPolicy, const char*>
ResourceFetcher::DetermineRevalidationPolicyInternal(
    ResourceType type,
    const FetchParameters& fetch_params,
    const Resource& existing_resource,
    bool is_static_data) const {
  const ResourceRequest& request = fetch_params.GetResourceRequest();

  Resource* cached_resource_in_fetcher = CachedResource(request.Url());

  if (IsDownloadOrStreamRequest(request)) {
    return {RevalidationPolicy::kReload,
            "It is for download or for streaming."};
  }

  if (IsImageResourceDisallowedToBeReused(existing_resource)) {
    return {RevalidationPolicy::kReload,
            "Reload due to 'allow image' settings."};
  }

  // If the existing resource is loading and the associated fetcher is not equal
  // to |this|, we must not use the resource. Otherwise, CSP violation may
  // happen in redirect handling.
  if (existing_resource.Loader() &&
      existing_resource.Loader()->Fetcher() != this) {
    return {RevalidationPolicy::kReload,
            "The existing resource is loading in a foreign fetcher."};
  }

  // It's hard to share a not-yet-referenced preloads via MemoryCache correctly.
  // A not-yet-matched preloads made by a foreign ResourceFetcher and stored in
  // the memory cache could be used without this block.
  if ((fetch_params.IsLinkPreload() || fetch_params.IsSpeculativePreload()) &&
      existing_resource.IsUnusedPreload()) {
    return {RevalidationPolicy::kReload,
            "The existing resource is an unused preload made "
            "from a foreign fetcher."};
  }

  // Checks if the resource has an explicit policy about integrity metadata.
  //
  // This is necessary because ScriptResource and CSSStyleSheetResource objects
  // do not keep the raw data around after the source is accessed once, so if
  // the resource is accessed from the MemoryCache for a second time, there is
  // no way to redo an integrity check.
  //
  // Thus, Blink implements a scheme where it caches the integrity information
  // for those resources after the first time it is checked, and if there is
  // another request for that resource, with the same integrity metadata, Blink
  // skips the integrity calculation. However, if the integrity metadata is a
  // mismatch, the MemoryCache must be skipped here, and a new request for the
  // resource must be made to get the raw data. This is expected to be an
  // uncommon case, however, as it implies two same-origin requests to the same
  // resource, but with different integrity metadata.
  if (existing_resource.MustRefetchDueToIntegrityMetadata(fetch_params)) {
    return {RevalidationPolicy::kReload, "Reload due to resource integrity."};
  }

  // If the same URL has been loaded as a different type, we need to reload.
  if (existing_resource.GetType() != type) {
    // FIXME: If existingResource is a Preload and the new type is LinkPrefetch
    // We really should discard the new prefetch since the preload has more
    // specific type information! crbug.com/379893
    // fast/dom/HTMLLinkElement/link-and-subresource-test hits this case.
    return {RevalidationPolicy::kReload, "Reload due to type mismatch."};
  }

  // If resource was populated from archive or data: url, use it.
  // This doesn't necessarily mean that |resource| was just created by using
  // CreateResourceForStaticData().
  if (is_static_data) {
    return {RevalidationPolicy::kUse, "Use the existing static resource."};
  }

  if (existing_resource.CanReuse(fetch_params) != Resource::MatchStatus::kOk) {
    return {RevalidationPolicy::kReload, "Reload due to Resource::CanReuse."};
  }

  // Don't reload resources while pasting.
  if (allow_stale_resources_) {
    return {RevalidationPolicy::kUse,
            "Use the existing resource due to |allow_stale_resources_|."};
  }

  // FORCE_CACHE uses the cache no matter what.
  if (request.GetCacheMode() == mojom::blink::FetchCacheMode::kForceCache) {
    return {RevalidationPolicy::kUse,
            "Use the existing resource due to cache-mode: 'force-cache'."};
  }

  // Don't reuse resources with Cache-control: no-store.
  if (existing_resource.HasCacheControlNoStoreHeader()) {
    return {RevalidationPolicy::kReload,
            "Reload due to cache-control: no-store."};
  }

  // During the initial load, avoid loading the same resource multiple times for
  // a single document, even if the cache policies would tell us to. We also
  // group loads of the same resource together. Raw resources are exempted, as
  // XHRs fall into this category and may have user-set Cache-Control: headers
  // or other factors that require separate requests.
  if (type != ResourceType::kRaw) {
    if (!properties_->IsLoadComplete() &&
        cached_resources_map_.Contains(
            MemoryCache::RemoveFragmentIdentifierIfNeeded(
                existing_resource.Url()))) {
      return {RevalidationPolicy::kUse,
              "Avoid making multiple requests for the same URL "
              "during the initial load."};
    }
    if (existing_resource.IsLoading()) {
      return {RevalidationPolicy::kUse,
              "Use the existing resource because it's being loaded."};
    }
  }

  // RELOAD always reloads
  if (request.GetCacheMode() == mojom::blink::FetchCacheMode::kBypassCache) {
    return {RevalidationPolicy::kReload, "Reload due to cache-mode: 'reload'."};
  }

  // We'll try to reload the resource if it failed last time.
  if (existing_resource.ErrorOccurred()) {
    return {RevalidationPolicy::kReload,
            "Reload because the existing resource has failed loading."};
  }

  // List of available images logic allows images to be re-used without cache
  // validation. We restrict this only to images from memory cache which are the
  // same as the version in the current document.
  if (type == ResourceType::kImage &&
      &existing_resource == cached_resource_in_fetcher) {
    return {RevalidationPolicy::kUse,
            "Images can be reused without cache validation."};
  }

  if (existing_resource.MustReloadDueToVaryHeader(request)) {
    return {RevalidationPolicy::kReload, "Reload due to vary header."};
  }

  // If any of the redirects in the chain to loading the resource were not
  // cacheable, we cannot reuse our cached resource.
  if (!existing_resource.CanReuseRedirectChain()) {
    return {RevalidationPolicy::kReload,
            "Reload due to an uncacheable redirect."};
  }

  // Check if the cache headers requires us to revalidate (cache expiration for
  // example).
  if (request.GetCacheMode() == mojom::blink::FetchCacheMode::kValidateCache ||
      existing_resource.MustRevalidateDueToCacheHeaders(
          request.AllowsStaleResponse()) ||
      request.CacheControlContainsNoCache()) {
    // Revalidation is harmful for non-matched preloads because it may lead to
    // sharing one preloaded resource among multiple ResourceFetchers.
    if (existing_resource.IsUnusedPreload()) {
      return {RevalidationPolicy::kReload,
              "Revalidation is harmful for non-matched preloads."};
    }

    // See if the resource has usable ETag or Last-modified headers. If the page
    // is controlled by the ServiceWorker, we choose the Reload policy because
    // the revalidation headers should not be exposed to the
    // ServiceWorker.(crbug.com/429570)
    //
    // TODO(falken): If the controller has no fetch event handler, we probably
    // can treat it as not being controlled in the S13nSW case. In the
    // non-S13nSW, we don't know what controller the request will ultimately go
    // to (due to skipWaiting) so be conservative.
    if (existing_resource.CanUseCacheValidator() &&
        properties_->GetControllerServiceWorkerMode() ==
            mojom::ControllerServiceWorkerMode::kNoController) {
      // If the resource is already a cache validator but not started yet, the
      // |Use| policy should be applied to subsequent requests.
      if (existing_resource.IsCacheValidator()) {
        DCHECK(existing_resource.StillNeedsLoad());
        return {RevalidationPolicy::kUse,
                "Merged to the revalidate request which has not yet started."};
      }
      return {RevalidationPolicy::kRevalidate, ""};
    }

    // No, must reload.
    return {RevalidationPolicy::kReload,
            "Reload due to missing cache validators."};
  }

  return {RevalidationPolicy::kUse,
          "Use the existing resource because there is no reason not to do so."};
}

void ResourceFetcher::SetAutoLoadImages(bool enable) {
  if (enable == auto_load_images_)
    return;

  auto_load_images_ = enable;

  if (!auto_load_images_)
    return;

  ReloadImagesIfNotDeferred();
}

bool ResourceFetcher::ShouldDeferImageLoad(const KURL& url) const {
  return !Context().AllowImage() ||
         (!auto_load_images_ && !url.ProtocolIsData());
}

void ResourceFetcher::ReloadImagesIfNotDeferred() {
  for (Resource* resource : not_loaded_image_resources_) {
    DCHECK_EQ(resource->GetType(), ResourceType::kImage);
    if (resource->StillNeedsLoad() && !ShouldDeferImageLoad(resource->Url()))
      StartLoad(resource);
  }
}

FetchContext& ResourceFetcher::Context() const {
  return *context_;
}

void ResourceFetcher::ClearContext() {
  scheduler_->Shutdown();
  ClearPreloads(ResourceFetcher::kClearAllPreloads);

  {
    // This block used to be
    //  context_ = Context().Detach();
    // While we are splitting FetchContext to multiple classes we need to call
    // "detach" for multiple objects in a coordinated manner. See
    // https://crbug.com/914739 for the progress.
    // TODO(yhirano): Remove the cross-class dependency.
    context_ = Context().Detach();
    properties_->Detach();
  }

  resource_load_observer_ = nullptr;
  use_counter_->Detach();
  console_logger_->Detach();
  if (back_forward_cache_loader_helper_)
    back_forward_cache_loader_helper_->Detach();
  loader_factory_ = nullptr;

  unused_preloads_timer_.Cancel();

  // Make sure the only requests still going are keepalive requests.
  // Callers of ClearContext() should be calling StopFetching() prior
  // to this, but it's possible for additional requests to start during
  // StopFetching() (e.g., fallback fonts that only trigger when the
  // first choice font failed to load).
  StopFetching();

  if (!loaders_.empty() || !non_blocking_loaders_.empty()) {
    CHECK(!base::FeatureList::IsEnabled(
              blink::features::kKeepAliveInBrowserMigration) ||
          !base::FeatureList::IsEnabled(
              blink::features::kAttributionReportingInBrowserMigration));
    // There are some keepalive requests.

    // The use of WrapPersistent creates a reference cycle intentionally,
    // to keep the ResourceFetcher and ResourceLoaders alive until the requests
    // complete or the timer fires.
    keepalive_loaders_task_handle_ = PostDelayedCancellableTask(
        *freezable_task_runner_, FROM_HERE,
        WTF::BindOnce(&ResourceFetcher::StopFetchingIncludingKeepaliveLoaders,
                      WrapPersistent(this)),
        kKeepaliveLoadersTimeout);
  }
}

int ResourceFetcher::BlockingRequestCount() const {
  return loaders_.size();
}

int ResourceFetcher::NonblockingRequestCount() const {
  return non_blocking_loaders_.size();
}

int ResourceFetcher::ActiveRequestCount() const {
  return loaders_.size() + non_blocking_loaders_.size();
}

void ResourceFetcher::EnableIsPreloadedForTest() {
  if (preloaded_urls_for_test_)
    return;
  preloaded_urls_for_test_ = std::make_unique<HashSet<String>>();

  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    preloaded_urls_for_test_->insert(resource->Url().GetString());
  }
}

bool ResourceFetcher::IsPreloadedForTest(const KURL& url) const {
  DCHECK(preloaded_urls_for_test_);
  return preloaded_urls_for_test_->Contains(url.GetString());
}

void ResourceFetcher::ClearPreloads(ClearPreloadsPolicy policy) {
  Vector<PreloadKey> keys_to_be_removed;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (policy == kClearAllPreloads || !resource->IsLinkPreload()) {
      MemoryCache::Get()->Remove(resource);
      keys_to_be_removed.push_back(pair.key);
    }
  }
  preloads_.RemoveAll(keys_to_be_removed);

  matched_preloads_.clear();
}

void ResourceFetcher::ScheduleWarnUnusedPreloads(
    base::OnceCallback<void(Vector<KURL> unused_preloads)> callback) {
  // If preloads_ is not empty here, it's full of link
  // preloads, as speculative preloads should have already been cleared when
  // parsing finished.
  if (preloads_.empty() && unused_early_hints_preloaded_resources_.empty()) {
    return;
  }
  unused_preloads_timer_ = PostDelayedCancellableTask(
      *freezable_task_runner_, FROM_HERE,
      WTF::BindOnce(&ResourceFetcher::WarnUnusedPreloads,
                    WrapWeakPersistent(this), std::move(callback)),
      kUnusedPreloadTimeout);
}

void ResourceFetcher::WarnUnusedPreloads(
    base::OnceCallback<void(Vector<KURL> unused_preloads)> callback) {
  int unused_resource_count = 0;
  Vector<KURL> unused_preloads;
  for (const auto& pair : preloads_) {
    Resource* resource = pair.value;
    if (!resource || !resource->IsUnusedPreload()) {
      continue;
    }

    ++unused_resource_count;
    unused_preloads.push_back(resource->Url());
    if (resource->IsLinkPreload()) {
      String message =
          "The resource " + resource->Url().GetString() + " was preloaded " +
          "using link preload but not used within a few seconds from the " +
          "window's load event. Please make sure it has an appropriate `as` " +
          "value and it is preloaded intentionally.";
      console_logger_->AddConsoleMessage(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning, message);
      TRACE_EVENT1("blink,blink.resource",
                   "ResourceFetcher::WarnUnusedPreloads", "data",
                   CreateTracedValueForUnusedPreload(
                       resource->Url(), Resource::MatchStatus::kOk,
                       resource->GetResourceRequest().GetDevToolsId()));

      base::UmaHistogramCounts100("Renderer.Preload.UnusedResource",
                                  static_cast<int>(resource->GetType()));
    }
    base::UmaHistogramEnumeration("Renderer.Preload.UnusedResource2",
                                  resource->GetType());
    base::UmaHistogramEnumeration(
        base::StrCat({"Renderer.Preload.UnusedResource2.",
                      LinkPreloadStrForHistogram(resource->IsLinkPreload())}),
        resource->GetType());
  }
  base::UmaHistogramCounts100("Renderer.Preload.UnusedResourceCount",
                              unused_resource_count);
  // Record the total count of deferred preloads based on the LCPP signal.
  base::UmaHistogramCounts100(
      base::StrCat(
          {kLCPPDeferUnusedPreloadHistogramPrefix, "DeferredResourceCount"}),
      deferred_preloads_.size());

  for (auto& pair : unused_early_hints_preloaded_resources_) {
    if (pair.value.state == EarlyHintsPreloadEntry::State::kWarnedUnused)
      continue;

    // TODO(https://crbug.com/1317936): Consider not showing the following
    // warning message when an Early Hints response requested preloading the
    // resource but the HTTP cache already had the response and no network
    // request was made for the resource. In such a situation not using the
    // resource wouldn't be harmful. We need to plumb information from the
    // browser process to check whether the resource was already in the HTTP
    // cache.
    String message = "The resource " + pair.key.GetString() +
                     " was preloaded using link preload in Early Hints but not "
                     "used within a few seconds from the window's load event.";
    console_logger_->AddConsoleMessage(
        mojom::blink::ConsoleMessageSource::kJavaScript,
        mojom::blink::ConsoleMessageLevel::kWarning, message);
    TRACE_EVENT1("blink,blink.resource",
                 "ResourceFetcher::WarnUnusedEarlyHintsPreloads", "data",
                 CreateTracedValueForUnusedEarlyHintsPreload(pair.key));
    pair.value.state = EarlyHintsPreloadEntry::State::kWarnedUnused;
  }

  // Notify the unused preload list to the LCPP host.
  std::move(callback).Run(std::move(unused_preloads));
}

void ResourceFetcher::HandleLoaderFinish(Resource* resource,
                                         base::TimeTicks response_end,
                                         LoaderFinishType type,
                                         uint32_t inflight_keepalive_bytes) {
  DCHECK(resource);

  // kRaw might not be subresource, and we do not need them.
  if (resource->GetType() != ResourceType::kRaw) {
    ++subresource_load_metrics_.number_of_subresources_loaded;
    if (resource->GetResponse().WasFetchedViaServiceWorker()) {
      ++subresource_load_metrics_
            .number_of_subresource_loads_handled_by_service_worker;
    }
  }

  if (IsControlledByServiceWorker() ==
      mojom::blink::ControllerServiceWorkerMode::kControlled) {
    if (resource->GetResponse().WasFetchedViaServiceWorker()) {
      base::UmaHistogramEnumeration("ServiceWorker.Subresource.Handled.Type2",
                                    resource->GetType());
    } else {
      base::UmaHistogramEnumeration(
          "ServiceWorker.Subresource.Fallbacked.Type2", resource->GetType());
    }
    UpdateServiceWorkerSubresourceMetrics(
        resource->GetType(),
        resource->GetResponse().WasFetchedViaServiceWorker(),
        resource->GetResponse().GetServiceWorkerRouterInfo());
  }

  context_->UpdateSubresourceLoadMetrics(subresource_load_metrics_);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  ResourceLoader* loader = resource->Loader();
  if (type == kDidFinishFirstPartInMultipart) {
    // When loading a multipart resource, make the loader non-block when
    // finishing loading the first part.
    MoveResourceLoaderToNonBlocking(loader);
  } else {
    RemoveResourceLoader(loader);
    DCHECK(!non_blocking_loaders_.Contains(loader));
  }
  DCHECK(!loaders_.Contains(loader));

  const int64_t encoded_data_length =
      resource->GetResponse().EncodedDataLength();

  PendingResourceTimingInfo info = resource_timing_info_map_.Take(resource);
  if (!info.is_null()) {
    if (resource->GetResponse().ShouldPopulateResourceTiming())
      PopulateAndAddResourceTimingInfo(resource, std::move(info), response_end);
  }

  resource->VirtualTimePauser().UnpauseVirtualTime();

  // A response should not serve partial content if it was not requested via a
  // Range header: https://fetch.spec.whatwg.org/#main-fetch so keep it out
  // of the preload cache in case of a non-206 response (which generates an
  // error).
  if (resource->GetResponse().GetType() ==
          network::mojom::FetchResponseType::kOpaque &&
      resource->GetResponse().HasRangeRequested() &&
      !resource->GetResourceRequest().HttpHeaderFields().Contains(
          http_names::kRange)) {
    RemovePreload(resource);
  }

  if (type == kDidFinishLoading) {
    resource->Finish(response_end, freezable_task_runner_.get());

    // Since this resource came from the network stack we only schedule a stale
    // while revalidate request if the network asked us to. If we called
    // ShouldRevalidateStaleResponse here then the resource would be checking
    // the freshness based on current time. It is possible that the resource
    // is fresh at the time of the network stack handling but not at the time
    // handling here and we should not be forcing a revalidation in that case.
    // eg. network stack returning a resource with max-age=0.
    if (resource->GetResourceRequest().AllowsStaleResponse() &&
        resource->StaleRevalidationRequested()) {
      ScheduleStaleRevalidate(resource);
    }
  }
  if (resource_load_observer_) {
    DCHECK(!IsDetached());
    resource_load_observer_->DidFinishLoading(
        resource->InspectorId(), response_end, encoded_data_length,
        resource->GetResponse().DecodedBodyLength());
  }
  MaybeSaveResourceToStrongReference(resource);
}

void ResourceFetcher::HandleLoaderError(Resource* resource,
                                        base::TimeTicks finish_time,
                                        const ResourceError& error,
                                        uint32_t inflight_keepalive_bytes) {
  DCHECK(resource);

  DCHECK_LE(inflight_keepalive_bytes, inflight_keepalive_bytes_);
  inflight_keepalive_bytes_ -= inflight_keepalive_bytes;

  RemoveResourceLoader(resource->Loader());
  PendingResourceTimingInfo info = resource_timing_info_map_.Take(resource);

  if (!info.is_null()) {
    if (resource->GetResourceRequest().Url().ProtocolIsInHTTPFamily() ||
        (resource->GetResourceRequest().GetWebBundleTokenParams() &&
         resource->GetResourceRequest()
             .GetWebBundleTokenParams()
             ->bundle_url.IsValid())) {
      PopulateAndAddResourceTimingInfo(resource, std::move(info), finish_time);
    }
  }

  resource->VirtualTimePauser().UnpauseVirtualTime();
  // If the preload was cancelled due to an HTTP error, we don't want to request
  // the resource a second time.
  if (error.IsCancellation() && !error.IsCancelledFromHttpError())
    RemovePreload(resource);
  if (network_utils::IsCertificateTransparencyRequiredError(
          error.ErrorCode())) {
    use_counter_->CountUse(
        mojom::WebFeature::kCertificateTransparencyRequiredErrorOnResourceLoad);
  }
  resource->FinishAsError(error, freezable_task_runner_.get());
  if (resource_load_observer_) {
    DCHECK(!IsDetached());
    resource_load_observer_->DidFailLoading(
        resource->LastResourceRequest().Url(), resource->InspectorId(), error,
        resource->GetResponse().EncodedDataLength(),
        ResourceLoadObserver::IsInternalRequest(
            resource->Options().initiator_info.name ==
            fetch_initiator_type_names::kInternal));
  }
}

void ResourceFetcher::MoveResourceLoaderToNonBlocking(ResourceLoader* loader) {
  DCHECK(loader);
  DCHECK(loaders_.Contains(loader));
  non_blocking_loaders_.insert(loader);
  loaders_.erase(loader);
}

bool ResourceFetcher::StartLoad(Resource* resource,
                                bool is_potentially_unused_preload) {
  CHECK(resource->GetType() == ResourceType::kFont ||
        resource->GetType() == ResourceType::kImage ||
        is_potentially_unused_preload);
  // Currently the metrics collection codes are duplicated here and in
  // UpdateMemoryCacheStats() because we have two calling paths for triggering a
  // load here and RequestResource().
  // TODO(https://crbug.com/1376866): Consider merging the duplicated code.
  if (resource->GetType() == ResourceType::kFont) {
    base::UmaHistogramEnumeration(
        RESOURCE_HISTOGRAM_PREFIX "Font",
        RevalidationPolicyForMetrics::kPreviouslyDeferredLoad);
  } else if (resource->GetType() == ResourceType::kImage) {
    base::UmaHistogramEnumeration(
        RESOURCE_HISTOGRAM_PREFIX "Image",
        RevalidationPolicyForMetrics::kPreviouslyDeferredLoad);
  }
  return StartLoad(resource, ResourceRequestBody(),
                   ImageLoadBlockingPolicy::kDefault,
                   RenderBlockingBehavior::kNonBlocking);
}

bool ResourceFetcher::StartLoad(
    Resource* resource,
    ResourceRequestBody request_body,
    ImageLoadBlockingPolicy policy,
    RenderBlockingBehavior render_blocking_behavior) {
  DCHECK(resource);
  DCHECK(resource->StillNeedsLoad());

  ResourceLoader* loader = nullptr;

  {
    // Forbids JavaScript/revalidation until start()
    // to prevent unintended state transitions.
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    if (properties_->ShouldBlockLoadingSubResource() && IsMainThread()) {
      MemoryCache::Get()->Remove(resource);
      return false;
    }

    const ResourceRequestHead& request_head = resource->GetResourceRequest();

    if (resource_load_observer_) {
      DCHECK(!IsDetached());
      ResourceRequest request(request_head);
      request.SetHttpBody(request_body.FormBody());
      ResourceResponse response;
      resource_load_observer_->WillSendRequest(
          request, response, resource->GetType(), resource->Options(),
          render_blocking_behavior, resource);
    }

    using QuotaType = decltype(inflight_keepalive_bytes_);
    QuotaType size = 0;
    if (request_head.GetKeepalive() && request_body.FormBody()) {
      auto original_size = request_body.FormBody()->SizeInBytes();
      DCHECK_LE(inflight_keepalive_bytes_, kKeepaliveInflightBytesQuota);
      if (original_size > std::numeric_limits<QuotaType>::max())
        return false;
      size = static_cast<QuotaType>(original_size);
      if (kKeepaliveInflightBytesQuota - inflight_keepalive_bytes_ < size)
        return false;

      inflight_keepalive_bytes_ += size;
    }

    loader = MakeGarbageCollected<ResourceLoader>(
        this, scheduler_, resource, context_lifecycle_notifier_,
        std::move(request_body), size);
    // Preload requests should not block the load event. IsLinkPreload()
    // actually continues to return true for Resources matched from the preload
    // cache that must block the load event, but that is OK because this method
    // is not responsible for promoting matched preloads to load-blocking. This
    // is handled by MakePreloadedResourceBlockOnloadIfNeeded().
    if (!resource->IsLinkPreload() &&
        resource->IsLoadEventBlockingResourceType() &&
        policy != ImageLoadBlockingPolicy::kForceNonBlockingLoad) {
      loaders_.insert(loader);
    } else {
      non_blocking_loaders_.insert(loader);
    }
    resource->VirtualTimePauser().PauseVirtualTime();

    StorePerformanceTimingInitiatorInformation(resource,
                                               render_blocking_behavior);
  }

  loader->Start();

  {
    Resource::RevalidationStartForbiddenScope
        revalidation_start_forbidden_scope(resource);
    ScriptForbiddenScope script_forbidden_scope;

    // NotifyStartLoad() shouldn't cause AddClient/RemoveClient().
    Resource::ProhibitAddRemoveClientInScope
        prohibit_add_remove_client_in_scope(resource);
    if (!resource->IsLoaded())
      resource->NotifyStartLoad();
  }
  return true;
}

void ResourceFetcher::ScheduleLoadingPotentiallyUnusedPreload(
    Resource* resource) {
  // Check the resource is already scheduled to start load or not.
  PreloadKey key(resource->Url(), resource->GetType());
  auto it = deferred_preloads_.find(key);
  if (it != deferred_preloads_.end() && it->value == resource) {
    return;
  }
  deferred_preloads_.insert(key, resource);

  static const features::LcppDeferUnusedPreloadTiming load_timing =
      features::kLcppDeferUnusedPreloadTiming.Get();
  switch (load_timing) {
    case features::LcppDeferUnusedPreloadTiming::kPostTask:
      ScheduleStartLoadAndFinishIfFailed(
          resource, /*is_potentially_unused_preload=*/true);
      break;
    case features::LcppDeferUnusedPreloadTiming::kLcpTimingPredictor:
      context_->AddLcpPredictedCallback(
          WTF::BindOnce(&ResourceFetcher::StartLoadAndFinishIfFailed,
                        WrapWeakPersistent(this), WrapWeakPersistent(resource),
                        /*is_potentially_unused_preload=*/true));
      break;
    case features::LcppDeferUnusedPreloadTiming::
        kLcpTimingPredictorWithPostTask:
      context_->AddLcpPredictedCallback(
          WTF::BindOnce(&ResourceFetcher::ScheduleStartLoadAndFinishIfFailed,
                        WrapWeakPersistent(this), WrapWeakPersistent(resource),
                        /*is_potentially_unused_preload=*/true));
      break;
  }
}

void ResourceFetcher::StartLoadAndFinishIfFailed(
    Resource* resource,
    bool is_potentially_unused_preload) {
  if (!resource) {
    return;
  }

  if (is_potentially_unused_preload) {
    RecordDeferUnusedPreloadHistograms(resource);
  }

  if (!resource->StillNeedsLoad()) {
    // When `resource` does not need load anymore, the resource load was already
    // started by a subsequent resource request.
    return;
  }
  if (!StartLoad(resource, is_potentially_unused_preload)) {
    resource->FinishAsError(ResourceError::CancelledError(resource->Url()),
                            freezable_task_runner_.get());
  }
}

void ResourceFetcher::ScheduleStartLoadAndFinishIfFailed(
    Resource* resource,
    bool is_potentially_unused_preload) {
  freezable_task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ResourceFetcher::StartLoadAndFinishIfFailed,
                    WrapWeakPersistent(this), WrapWeakPersistent(resource),
                    is_potentially_unused_preload));
}

void ResourceFetcher::RemoveResourceLoader(ResourceLoader* loader) {
  DCHECK(loader);

  if (loaders_.Contains(loader))
    loaders_.erase(loader);
  else if (non_blocking_loaders_.Contains(loader))
    non_blocking_loaders_.erase(loader);
  else
    NOTREACHED_IN_MIGRATION();

  if (loaders_.empty() && non_blocking_loaders_.empty())
    keepalive_loaders_task_handle_.Cancel();
}

void ResourceFetcher::StopFetching() {
  StopFetchingInternal(StopFetchingTarget::kExcludingKeepaliveLoaders);
}

void ResourceFetcher::SetDefersLoading(LoaderFreezeMode mode) {
  for (const auto& loader : non_blocking_loaders_)
    loader->SetDefersLoading(mode);
  for (const auto& loader : loaders_)
    loader->SetDefersLoading(mode);
}

void ResourceFetcher::UpdateAllImageResourcePriorities() {
  TRACE_EVENT0(
      "blink",
      "ResourceLoadPriorityOptimizer::updateAllImageResourcePriorities");

  HeapVector<Member<Resource>> to_be_removed;
  for (Resource* resource : not_loaded_image_resources_) {
    DCHECK_EQ(resource->GetType(), ResourceType::kImage);
    if (resource->IsLoaded()) {
      to_be_removed.push_back(resource);
      continue;
    }

    if (!resource->IsLoading())
      continue;

    auto priorities = resource->PriorityFromObservers();
    ResourcePriority resource_priority = priorities.first;
    ResourceLoadPriority computed_load_priority = ComputeLoadPriority(
        ResourceType::kImage, resource->GetResourceRequest(),
        resource_priority.visibility, FetchParameters::DeferOption::kNoDefer,
        FetchParameters::SpeculativePreloadType::kNotSpeculative,
        RenderBlockingBehavior::kNonBlocking,
        mojom::blink::ScriptType::kClassic, false, std::nullopt, std::nullopt,
        resource_priority.is_lcp_resource);

    ResourcePriority resource_priority_excluding_image_loader =
        priorities.second;
    ResourceLoadPriority computed_load_priority_excluding_image_loader =
        ComputeLoadPriority(
            ResourceType::kImage, resource->GetResourceRequest(),
            resource_priority_excluding_image_loader.visibility,
            FetchParameters::DeferOption::kNoDefer,
            FetchParameters::SpeculativePreloadType::kNotSpeculative,
            RenderBlockingBehavior::kNonBlocking,
            mojom::blink::ScriptType::kClassic, false, std::nullopt,
            std::nullopt,
            resource_priority_excluding_image_loader.is_lcp_resource);

    // When enabled, `priority` is used, which considers the resource priority
    // via ImageLoader, i.e. ImageResourceContent
    // -> ImageLoader (as ImageResourceObserver)
    // -> LayoutImageResource
    // -> LayoutObject.
    //
    // The same priority is considered in `priority_excluding_image_loader` via
    // ImageResourceContent
    // -> LayoutObject (as ImageResourceObserver),
    // but the LayoutObject might be not registered yet as an
    // ImageResourceObserver while loading.
    // See https://crbug.com/1369823 for details.
    static const bool fix_enabled =
        base::FeatureList::IsEnabled(features::kImageLoadingPrioritizationFix);

    if (computed_load_priority !=
        computed_load_priority_excluding_image_loader) {
      // Mark pages affected by this fix for performance evaluation.
      use_counter_->CountUse(
          WebFeature::kEligibleForImageLoadingPrioritizationFix);
    }
    if (!fix_enabled) {
      resource_priority = resource_priority_excluding_image_loader;
      computed_load_priority = computed_load_priority_excluding_image_loader;
    }

    // Only boost the priority of an image, never lower it. This ensures that
    // there isn't priority churn if images move in and out of the viewport, or
    // are displayed more than once, both in and out of the viewport.
    if (computed_load_priority <= resource->GetResourceRequest().Priority())
      continue;

    DCHECK_GT(computed_load_priority,
              resource->GetResourceRequest().Priority());
    resource->DidChangePriority(computed_load_priority,
                                resource_priority.intra_priority_value);
    TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
        TRACE_DISABLED_BY_DEFAULT("network"), "ResourcePrioritySet",
        TRACE_ID_WITH_SCOPE("BlinkResourceID",
                            TRACE_ID_LOCAL(resource->InspectorId())),
        "data", CreateTracedValueWithPriority(computed_load_priority));
    DCHECK(!IsDetached());
    resource_load_observer_->DidChangePriority(
        resource->InspectorId(), computed_load_priority,
        resource_priority.intra_priority_value);
  }

  not_loaded_image_resources_.RemoveAll(to_be_removed);
  // Explicitly free the backing store to not regress memory.
  // TODO(bikineev): Revisit when young generation is done.
  to_be_removed.clear();
}

String ResourceFetcher::GetCacheIdentifier(const KURL& url) const {
  if (properties_->GetControllerServiceWorkerMode() !=
      mojom::ControllerServiceWorkerMode::kNoController) {
    return String::Number(properties_->ServiceWorkerId());
  }

  // Requests that can be satisfied via `archive_` (i.e. MHTML) or
  // `subresource_web_bundles_` should not participate in the global caching,
  // but should use a bundle/mhtml-specific cache.
  if (archive_)
    return archive_->GetCacheIdentifier();

  SubresourceWebBundle* bundle = GetMatchingBundle(url);
  if (bundle)
    return bundle->GetCacheIdentifier();

  return MemoryCache::DefaultCacheIdentifier();
}

std::optional<base::UnguessableToken>
ResourceFetcher::GetSubresourceBundleToken(const KURL& url) const {
  SubresourceWebBundle* bundle = GetMatchingBundle(url);
  if (!bundle)
    return std::nullopt;
  return bundle->WebBundleToken();
}

std::optional<KURL> ResourceFetcher::GetSubresourceBundleSourceUrl(
    const KURL& url) const {
  SubresourceWebBundle* bundle = GetMatchingBundle(url);
  if (!bundle)
    return std::nullopt;
  return bundle->GetBundleUrl();
}

void ResourceFetcher::EmulateLoadStartedForInspector(
    Resource* resource,
    mojom::blink::RequestContextType request_context,
    network::mojom::RequestDestination request_destination,
    const AtomicString& initiator_name) {
  base::AutoReset<bool> r(&is_in_request_resource_, true);

  const KURL& url = resource->Url();
  if (CachedResource(url)) {
    return;
  }

  if (ResourceHasBeenEmulatedLoadStartedForInspector(url)) {
    return;
  }

  if (resource->ErrorOccurred()) {
    // We should ideally replay the error steps, but we cannot.
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kEmulateLoadStartedForInspectorOncePerResource)) {
    // Update the emulated load started for inspector resources map with the
    // resource so that future emulations of the same resource won't happen.
    String resource_url = MemoryCache::RemoveFragmentIdentifierIfNeeded(url);
    emulated_load_started_for_inspector_resources_map_.Set(resource_url,
                                                           resource);
  }

  ResourceRequest resource_request(url);
  resource_request.SetRequestContext(request_context);
  resource_request.SetRequestDestination(request_destination);
  if (!resource_request.PriorityHasBeenSet()) {
    resource_request.SetPriority(ComputeLoadPriority(
        resource->GetType(), resource_request, ResourcePriority::kNotVisible));
  }
  resource_request.SetPriorityIncremental(
      ShouldLoadIncremental(resource->GetType()));
  resource_request.SetReferrerString(Referrer::NoReferrer());
  resource_request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kNever);
  resource_request.SetInspectorId(CreateUniqueIdentifier());

  ResourceLoaderOptions options = resource->Options();
  options.initiator_info.name = initiator_name;
  FetchParameters params(std::move(resource_request), options);
  ResourceRequest last_resource_request(resource->LastResourceRequest());
  Context().CanRequest(resource->GetType(), last_resource_request,
                       last_resource_request.Url(), params.Options(),
                       ReportingDisposition::kReport,
                       last_resource_request.GetRedirectInfo());
  if (resource->GetStatus() == ResourceStatus::kNotStarted ||
      resource->GetStatus() == ResourceStatus::kPending) {
    // If the loading has not started, then we return here because loading
    // related events will be reported to the ResourceLoadObserver. If the
    // loading is ongoing, then we return here too because the loading
    // activity is merged.
    return;
  }
  DCHECK_EQ(resource->GetStatus(), ResourceStatus::kCached);
  DidLoadResourceFromMemoryCache(resource, params.GetResourceRequest(),
                                 false /* is_static_data */,
                                 params.GetRenderBlockingBehavior());
}

void ResourceFetcher::PrepareForLeakDetection() {
  // Stop loaders including keepalive ones that may persist after page
  // navigation and thus affect instance counters of leak detection.
  StopFetchingIncludingKeepaliveLoaders();
}

void ResourceFetcher::StopFetchingInternal(StopFetchingTarget target) {
  // TODO(toyoshim): May want to suspend scheduler while canceling loaders so
  // that the cancellations below do not awake unnecessary scheduling.

  HeapVector<Member<ResourceLoader>> loaders_to_cancel;
  for (const auto& loader : non_blocking_loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }
  for (const auto& loader : loaders_) {
    if (target == StopFetchingTarget::kIncludingKeepaliveLoaders ||
        !loader->ShouldBeKeptAliveWhenDetached()) {
      loaders_to_cancel.push_back(loader);
    }
  }

  for (const auto& loader : loaders_to_cancel) {
    if (loaders_.Contains(loader) || non_blocking_loaders_.Contains(loader))
      loader->Cancel();
  }
}

void ResourceFetcher::StopFetchingIncludingKeepaliveLoaders() {
  StopFetchingInternal(StopFetchingTarget::kIncludingKeepaliveLoaders);
}

void ResourceFetcher::ScheduleStaleRevalidate(Resource* stale_resource) {
  if (stale_resource->StaleRevalidationStarted())
    return;
  stale_resource->SetStaleRevalidationStarted();
  freezable_task_runner_->PostTask(
      FROM_HERE,
      WTF::BindOnce(&ResourceFetcher::RevalidateStaleResource,
                    WrapWeakPersistent(this), WrapPersistent(stale_resource)));
}

void ResourceFetcher::RevalidateStaleResource(Resource* stale_resource) {
  // Creating FetchParams from Resource::GetResourceRequest doesn't create
  // the exact same request as the original one, while for revalidation
  // purpose this is probably fine.
  // TODO(dtapuska): revisit this when we have a better way to re-dispatch
  // requests.
  ResourceRequest request;
  request.CopyHeadFrom(stale_resource->GetResourceRequest());
  // TODO(https://crbug.com/1405800): investigate whether it's correct to use a
  // null `world` in the ResourceLoaderOptions below.
  FetchParameters params(std::move(request),
                         ResourceLoaderOptions(/*world=*/nullptr));
  params.SetStaleRevalidation(true);
  params.MutableResourceRequest().SetSkipServiceWorker(true);
  // Stale revalidation resource requests should be very low regardless of
  // the |type|.
  params.MutableResourceRequest().SetPriority(ResourceLoadPriority::kVeryLow);
  RawResource::Fetch(
      params, this,
      MakeGarbageCollected<StaleRevalidationResourceClient>(stale_resource));
}

mojom::blink::BlobRegistry* ResourceFetcher::GetBlobRegistry() {
  if (!blob_registry_remote_.is_bound()) {
    Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
        blob_registry_remote_.BindNewPipeAndPassReceiver(
            freezable_task_runner_));
  }
  return blob_registry_remote_.get();
}

FrameOrWorkerScheduler* ResourceFetcher::GetFrameOrWorkerScheduler() {
  return frame_or_worker_scheduler_.get();
}

void ResourceFetcher::PopulateAndAddResourceTimingInfo(
    Resource* resource,
    const PendingResourceTimingInfo& pending_info,
    base::TimeTicks response_end) {
  if (resource->GetResourceRequest().IsFromOriginDirtyStyleSheet()) {
    return;
  }

  // Resource timing entries that correspond to resources fetched by extensions
  // are precluded.
  if (resource->Options().world_for_csp &&
      resource->Options().world_for_csp->IsIsolatedWorld()) {
    return;
  }

  AtomicString initiator_type = resource->IsPreloadedByEarlyHints()
                                    ? AtomicString(kEarlyHintsInitiatorType)
                                    : pending_info.initiator_type;

  const KURL& initial_url =
      resource->GetResourceRequest().GetRedirectInfo().has_value()
          ? resource->GetResourceRequest().GetRedirectInfo()->original_url
          : resource->GetResourceRequest().Url();

  mojom::blink::ResourceTimingInfoPtr info = CreateResourceTimingInfo(
      pending_info.start_time, initial_url, &resource->GetResponse());
  if (info->allow_timing_details) {
    info->last_redirect_end_time = pending_info.redirect_end_time;
  }
  info->render_blocking_status = pending_info.render_blocking_behavior ==
                                 RenderBlockingBehavior::kBlocking;
  info->response_end = response_end;
  // Store LCP breakdown timings for images.
  if (resource->GetType() == ResourceType::kImage) {
    // The resource_load_timing may be null in tests.
    if (ResourceLoadTiming* resource_load_timing =
            resource->GetResponse().GetResourceLoadTiming()) {
      resource_load_timing->SetDiscoveryTime(info->start_time);
      resource_load_timing->SetResponseEnd(response_end);
    }
  }

  Context().AddResourceTiming(std::move(info), initiator_type);
}

SubresourceWebBundle* ResourceFetcher::GetMatchingBundle(
    const KURL& url) const {
  return subresource_web_bundles_
             ? subresource_web_bundles_->GetMatchingBundle(url)
             : nullptr;
}

void ResourceFetcher::CancelWebBundleSubresourceLoadersFor(
    const base::UnguessableToken& web_bundle_token) {
  // Copy to avoid concurrent iteration and modification.
  auto loaders = loaders_;
  for (const auto& loader : loaders) {
    loader->CancelIfWebBundleTokenMatches(web_bundle_token);
  }
  auto non_blocking_loaders = non_blocking_loaders_;
  for (const auto& loader : non_blocking_loaders) {
    loader->CancelIfWebBundleTokenMatches(web_bundle_token);
  }
}

void ResourceFetcher::MaybeSaveResourceToStrongReference(Resource* resource) {
  if (!base::FeatureList::IsEnabled(features::kMemoryCacheStrongReference)) {
    return;
  }

  const size_t total_size_threshold = static_cast<size_t>(
      features::kMemoryCacheStrongReferenceTotalSizeThresholdParam.Get());
  const size_t resource_size_threshold = static_cast<size_t>(
      features::kMemoryCacheStrongReferenceResourceSizeThresholdParam.Get());
  const size_t resource_size =
      static_cast<size_t>(resource->GetResponse().DecodedBodyLength());
  const bool size_is_small_enough = resource_size <= resource_size_threshold &&
                                    resource_size <= total_size_threshold;

  if (!size_is_small_enough) {
    return;
  }

  const SecurityOrigin* settings_object_origin =
      properties_->GetFetchClientSettingsObject().GetSecurityOrigin();
  if (!ShouldResourceBeKeptStrongReference(resource, settings_object_origin)) {
    return;
  }

  if (base::FeatureList::IsEnabled(
          features::kResourceFetcherStoresStrongReferences)) {
    // If the size would take us over, don't store it.
    if (document_resource_strong_refs_total_size_ + resource_size >
        total_size_threshold) {
      return;
    }
    document_resource_strong_refs_.insert(resource);
    document_resource_strong_refs_total_size_ += resource_size;
    freezable_task_runner_->PostDelayedTask(
        FROM_HERE,
        WTF::BindOnce(&ResourceFetcher::RemoveResourceStrongReference,
                      WrapWeakPersistent(this), WrapWeakPersistent(resource)),
        GetResourceStrongReferenceTimeout(resource));
  } else {
    MemoryCache::Get()->SaveStrongReference(resource);
  }
}

void ResourceFetcher::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  document_resource_strong_refs_.clear();
  document_resource_strong_refs_total_size_ = 0;
}

void ResourceFetcher::MaybeRecordLCPPSubresourceMetrics(
    const KURL& document_url) {
  if (!document_url.IsValid() || !document_url.ProtocolIsInHTTPFamily()) {
    return;
  }

  if (!properties_->IsOutermostMainFrame()) {
    return;
  }

  if (!context_->DoesLCPPHaveAnyHintData()) {
    return;
  }

  base::UmaHistogramCounts100(
      "Blink.LCPP.PotentiallyLCPResourcePriorityBoosts2",
      potentially_lcp_resource_priority_boosts_);
}

void ResourceFetcher::MarkEarlyHintConsumedIfNeeded(
    uint64_t inspector_id,
    Resource* resource,
    const ResourceResponse& response) {
  const KURL& initial_url =
      resource->GetResourceRequest().GetRedirectInfo().has_value()
          ? resource->GetResourceRequest().GetRedirectInfo()->original_url
          : resource->GetResourceRequest().Url();
  auto iter = unused_early_hints_preloaded_resources_.find(initial_url);
  if (iter != unused_early_hints_preloaded_resources_.end()) {
    unused_early_hints_preloaded_resources_.erase(iter);
    // The network service may not reuse the response fetched by the early hints
    // due to cache control policies.
    if (!response.NetworkAccessed() &&
        (!response.WasFetchedViaServiceWorker() ||
         response.IsServiceWorkerPassThrough())) {
      resource->SetIsPreloadedByEarlyHints();
    }
  }
}

bool ResourceFetcher::IsPotentiallyUnusedPreload(
    ResourceType type,
    const FetchParameters& params) const {
  static const bool kDeferUnusedPreload =
      base::FeatureList::IsEnabled(features::kLCPPDeferUnusedPreload);
  if (!kDeferUnusedPreload && !defer_unused_preload_enabled_for_testing_) {
    return false;
  }

  static const LcppDeferUnusedPreloadPreloadedReason kPreloadedReason =
      features::kLcppDeferUnusedPreloadPreloadedReason.Get();
  LcppDeferUnusedPreloadPreloadedReason preloaded_reason;
  if (defer_unused_preload_enabled_for_testing_) {
    preloaded_reason = defer_unused_preload_preloaded_reason_for_testing_;
  } else {
    preloaded_reason = kPreloadedReason;
  }
  bool reason_matched = false;
  switch (preloaded_reason) {
    case LcppDeferUnusedPreloadPreloadedReason::kAll:
      reason_matched = params.IsLinkPreload() || params.IsSpeculativePreload();
      break;
    case LcppDeferUnusedPreloadPreloadedReason::kLinkPreloadOnly:
      reason_matched = params.IsLinkPreload();
      break;
    case LcppDeferUnusedPreloadPreloadedReason::kBrowserSpeculativePreloadOnly:
      // Check |is_link_preload| here because |is_link_preload| and
      // |is_speculative_preload| are not mutually exclusive. When
      // |is_speculative_preload| is true, it's possible that |is_link_preload|
      // is also true. That is the case when the resource was made via preload
      // scanner for <link rel=preload>.
      reason_matched = params.IsSpeculativePreload() && !params.IsLinkPreload();
      break;
  }
  if (!reason_matched) {
    return false;
  }

  static const LcppDeferUnusedPreloadExcludedResourceType
      kExcludedResourceType =
          features::kLcppDeferUnusedPreloadExcludedResourceType.Get();
  LcppDeferUnusedPreloadExcludedResourceType excluded_resource_type;
  if (defer_unused_preload_enabled_for_testing_) {
    excluded_resource_type =
        defer_unused_preload_excluded_resource_type_for_testing_;
  } else {
    excluded_resource_type = kExcludedResourceType;
  }
  switch (excluded_resource_type) {
    case LcppDeferUnusedPreloadExcludedResourceType::kNone:
      break;
    case LcppDeferUnusedPreloadExcludedResourceType::kStyleSheet:
      if (type == ResourceType::kCSSStyleSheet) {
        return false;
      }
      break;
    case LcppDeferUnusedPreloadExcludedResourceType::kScript:
      if (type == ResourceType::kScript) {
        return false;
      }
      break;
    case LcppDeferUnusedPreloadExcludedResourceType::kMock:
      if (type == ResourceType::kMock) {
        return false;
      }
      break;
  }

  return base::Contains(context_->GetPotentiallyUnusedPreloads(), params.Url());
}

void ResourceFetcher::Trace(Visitor* visitor) const {
  visitor->Trace(context_);
  visitor->Trace(properties_);
  visitor->Trace(resource_load_observer_);
  visitor->Trace(use_counter_);
  visitor->Trace(console_logger_);
  visitor->Trace(loader_factory_);
  visitor->Trace(scheduler_);
  visitor->Trace(back_forward_cache_loader_helper_);
  visitor->Trace(archive_);
  visitor->Trace(resource_timing_report_timer_);
  visitor->Trace(loaders_);
  visitor->Trace(non_blocking_loaders_);
  visitor->Trace(cached_resources_map_);
  visitor->Trace(emulated_load_started_for_inspector_resources_map_);
  visitor->Trace(not_loaded_image_resources_);
  visitor->Trace(preloads_);
  visitor->Trace(matched_preloads_);
  visitor->Trace(deferred_preloads_);
  visitor->Trace(resource_timing_info_map_);
  visitor->Trace(blob_registry_remote_);
  visitor->Trace(subresource_web_bundles_);
  visitor->Trace(document_resource_strong_refs_);
  visitor->Trace(context_lifecycle_notifier_);
  MemoryPressureListener::Trace(visitor);
}

// static
const ResourceFetcher::ResourceFetcherSet&
ResourceFetcher::MainThreadFetchers() {
  return MainThreadFetchersSet();
}

// The followings should match with `ResourceType` in
// `third_party/blink/renderer/platform/loader/fetch/resource.h`
void ResourceFetcher::RecordResourceHistogram(
    std::string_view prefix,
    ResourceType type,
    RevalidationPolicyForMetrics policy) const {
  base::UmaHistogramEnumeration(
      base::StrCat({RESOURCE_HISTOGRAM_PREFIX, prefix, ResourceTypeName(type)}),
      policy);
}

void ResourceFetcher::UpdateServiceWorkerSubresourceMetrics(
    ResourceType resource_type,
    bool handled_by_serviceworker,
    const blink::ServiceWorkerRouterInfo* router_info) {
  if (!subresource_load_metrics_.service_worker_subresource_load_metrics) {
    subresource_load_metrics_.service_worker_subresource_load_metrics =
        blink::ServiceWorkerSubresourceLoadMetrics{};
  }
  auto& metrics =
      *subresource_load_metrics_.service_worker_subresource_load_metrics;
  switch (resource_type) {
    case ResourceType::kImage:  // 1
      if (handled_by_serviceworker) {
        metrics.image_handled |= true;
      } else {
        metrics.image_fallback |= true;
      }
      break;
    case ResourceType::kCSSStyleSheet:  // 2
      if (handled_by_serviceworker) {
        metrics.css_handled |= true;
      } else {
        metrics.css_fallback |= true;
      }
      break;
    case ResourceType::kScript:  // 3
      if (handled_by_serviceworker) {
        metrics.script_handled |= true;
      } else {
        metrics.script_fallback |= true;
      }
      break;
    case ResourceType::kFont:  // 4
      if (handled_by_serviceworker) {
        metrics.font_handled |= true;
      } else {
        metrics.font_fallback |= true;
      }
      break;
    case ResourceType::kRaw:  // 5
      if (handled_by_serviceworker) {
        metrics.raw_handled |= true;
      } else {
        metrics.raw_fallback |= true;
      }
      break;
    case ResourceType::kSVGDocument:  // 6
      if (handled_by_serviceworker) {
        metrics.svg_handled |= true;
      } else {
        metrics.svg_fallback |= true;
      }
      break;
    case ResourceType::kXSLStyleSheet:  // 7
      if (handled_by_serviceworker) {
        metrics.xsl_handled |= true;
      } else {
        metrics.xsl_fallback |= true;
      }
      break;
    case ResourceType::kLinkPrefetch:  // 8
      if (handled_by_serviceworker) {
        metrics.link_prefetch_handled |= true;
      } else {
        metrics.link_prefetch_fallback |= true;
      }
      break;
    case ResourceType::kTextTrack:  // 9
      if (handled_by_serviceworker) {
        metrics.text_track_handled |= true;
      } else {
        metrics.text_track_fallback |= true;
      }
      break;
    case ResourceType::kAudio:  // 10
      if (handled_by_serviceworker) {
        metrics.audio_handled |= true;
      } else {
        metrics.audio_fallback |= true;
      }
      break;
    case ResourceType::kVideo:  // 11
      if (handled_by_serviceworker) {
        metrics.video_handled |= true;
      } else {
        metrics.video_fallback |= true;
      }
      break;
    case ResourceType::kManifest:  // 12
      if (handled_by_serviceworker) {
        metrics.manifest_handled |= true;
      } else {
        metrics.manifest_fallback |= true;
      }
      break;
    case ResourceType::kSpeculationRules:  // 13
      if (handled_by_serviceworker) {
        metrics.speculation_rules_handled |= true;
      } else {
        metrics.speculation_rules_fallback |= true;
      }
      break;
    case ResourceType::kMock:  // 14
      if (handled_by_serviceworker) {
        metrics.mock_handled |= true;
      } else {
        metrics.mock_fallback |= true;
      }
      break;
    case ResourceType::kDictionary:  // 15
      if (handled_by_serviceworker) {
        metrics.dictionary_handled |= true;
      } else {
        metrics.dictionary_fallback |= true;
      }
      break;
  }

  // Count the matched route info of static routing API for sub-resources
  // if it exists.
  if (!router_info || !router_info->MatchedSourceType()) {
    return;
  }

  metrics.total_router_evaluation_time_for_subresources +=
      router_info->RouterEvaluationTime();

  switch (*router_info->MatchedSourceType()) {
    case network::mojom::ServiceWorkerRouterSourceType::kCache:
      metrics.total_cache_lookup_time_for_subresources +=
          router_info->CacheLookupTime();
      metrics.matched_cache_router_source_count++;
      break;
    case network::mojom::ServiceWorkerRouterSourceType::kFetchEvent:
      metrics.matched_fetch_event_router_source_count++;
      break;
    case network::mojom::ServiceWorkerRouterSourceType::kNetwork:
      metrics.matched_network_router_source_count++;
      break;
    case network::mojom::ServiceWorkerRouterSourceType::kRace:
      metrics.matched_race_network_and_fetch_router_source_count++;
      break;
  }
}

ResourceFetcher::ResourcePrepareHelper::ResourcePrepareHelper(
    ResourceFetcher& fetcher,
    FetchParameters& params,
    const ResourceFactory& factory)
    : fetcher_(fetcher),
      params_(params),
      factory_(factory),
      has_transparent_placeholder_image_(
          fetcher.IsSimplifyLoadingTransparentPlaceholderImageEnabled() &&
          (params.GetResourceRequest()
               .GetKnownTransparentPlaceholderImageIndex() != kNotFound)) {}

std::optional<ResourceRequestBlockedReason>
ResourceFetcher::ResourcePrepareHelper::PrepareRequestForCacheAccess(
    WebScopedVirtualTimePauser& pauser) {
#if DCHECK_IS_ON()
  DCHECK(!determined_initial_blocked_reason_);
  determined_initial_blocked_reason_ = true;
#endif
  if (has_transparent_placeholder_image_) {
    return fetcher_.UpdateRequestForTransparentPlaceholderImage(params_);
  }
  ResourceRequest& resource_request = params_.MutableResourceRequest();
  bundle_url_for_uuid_resources_ =
      fetcher_.PrepareRequestForWebBundle(resource_request);

  ResourceType resource_type = factory_.GetType();
  const ResourceLoaderOptions& options = params_.Options();

  DCHECK(options.synchronous_policy == kRequestAsynchronously ||
         resource_type == ResourceType::kRaw ||
         resource_type == ResourceType::kXSLStyleSheet);

  if (!RuntimeEnabledFeatures::
          MinimimalResourceRequestPrepBeforeCacheLookupEnabled()) {
    params_.OverrideContentType(factory_.ContentType());
    return PrepareResourceRequest(
        resource_type, fetcher_.properties_->GetFetchClientSettingsObject(),
        params_, fetcher_.Context(), pauser, *this,
        bundle_url_for_uuid_resources_);
  }

  std::optional<ResourceRequestBlockedReason> blocked_reason =
      PrepareResourceRequestForCacheAccess(
          resource_type, fetcher_.properties_->GetFetchClientSettingsObject(),
          bundle_url_for_uuid_resources_, *this, fetcher_.Context(), params_);
  if (blocked_reason) {
    return blocked_reason;
  }
  was_upgrade_for_loader_called_ = false;
  if (params_.GetResourceRequest().RequiresUpgradeForLoader()) {
    UpgradeForLoaderIfNecessary(pauser);
  }
  return std::nullopt;
}

void ResourceFetcher::ResourcePrepareHelper::UpgradeForLoaderIfNecessary(
    WebScopedVirtualTimePauser& pauser) {
#if DCHECK_IS_ON()
  DCHECK(determined_initial_blocked_reason_);
#endif
  if (was_upgrade_for_loader_called_) {
    return;
  }
  was_upgrade_for_loader_called_ = true;
  params_.OverrideContentType(factory_.ContentType());
  UpgradeResourceRequestForLoaderNew(factory_.GetType(), params_,
                                     fetcher_.Context(), *this, pauser);
}

ResourceLoadPriority
ResourceFetcher::ResourcePrepareHelper::ComputeLoadPriority(
    const FetchParameters& params) {
  return fetcher_.ComputeLoadPriority(
      factory_.GetType(), params.GetResourceRequest(),
      ResourcePriority::kNotVisible, params.Defer(),
      params.GetSpeculativePreloadType(), params.GetRenderBlockingBehavior(),
      params.GetScriptType(), params.IsLinkPreload(), params.GetResourceWidth(),
      params.GetResourceHeight(), params.IsPotentiallyLCPElement(),
      params.IsPotentiallyLCPInfluencer());
}

void ResourceFetcher::ResourcePrepareHelper::RecordTrace() {
  const ResourceRequest& resource_request = params_.GetResourceRequest();
  TRACE_EVENT_NESTABLE_ASYNC_INSTANT1(
      TRACE_DISABLED_BY_DEFAULT("network"), "ResourcePrioritySet",
      TRACE_ID_WITH_SCOPE("BlinkResourceID",
                          TRACE_ID_LOCAL(resource_request.InspectorId())),
      "priority", resource_request.Priority());
}

}  // namespace blink
