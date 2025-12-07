/*
    Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
    Copyright (C) 2011 Cosmin Truta <ctruta@gmail.com>
    Copyright (C) 2012 University of Szeged
    Copyright (C) 2012 Renata Hodovan <reni@webkit.org>

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
#include "third_party/blink/renderer/core/svg/svg_document_resource_tracker.h"

#include <algorithm>

#include "third_party/blink/renderer/core/loader/resource/svg_document_resource.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

SVGDocumentResourceTracker::SVGDocumentResourceTracker(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    const String& cache_identifier)
    : dispose_task_runner_(std::move(task_runner)),
      cache_identifier_(cache_identifier) {}

SVGDocumentResourceTracker::CacheKey SVGDocumentResourceTracker::MakeCacheKey(
    const FetchParameters& params) {
  const KURL url_without_fragment =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  return {url_without_fragment.GetString(),
          params.GetResourceRequest().GetMode()};
}

String SVGDocumentResourceTracker::MakeCacheIdentifier(
    StringView browser_context_group_token) {
  // Setting a unique cache identifier allows us to use global `MemoryCache`
  // to store SVG resources specific to each page.
  constexpr char kSVGDocumentResourcePrefix[] = "svg-resources:";
  return StrCat({kSVGDocumentResourcePrefix, browser_context_group_token});
}

SVGResourceDocumentContent* SVGDocumentResourceTracker::Get(
    const CacheKey& key) {
  auto it = entries_.find(key);
  return it != entries_.end() ? it->value : nullptr;
}

void SVGDocumentResourceTracker::Put(const CacheKey& key,
                                     SVGResourceDocumentContent* content) {
  auto result = entries_.insert(key, content);
  // No existing entry, we're done.
  if (result.is_new_entry) {
    return;
  }
  // Existing entry. Replace with the new content and then dispose of the old.
  SVGResourceDocumentContent* old_content =
      std::exchange(result.stored_value->value, content);
  if (old_content) {
    old_content->Dispose();
  }
}

void SVGDocumentResourceTracker::WillBeDestroyed() {
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    for (const auto& resource : tracked_resources_) {
      resource->GetContent()->Dispose();
    }
    MemoryCache::Get()->EvictResourcesForCacheIdentifier(cache_identifier_);
    tracked_resources_.clear();
  } else {
    for (SVGResourceDocumentContent* content : entries_.Values()) {
      content->Dispose();
    }
  }
}

void SVGDocumentResourceTracker::DisposeUnobserved() {
  dispose_task_pending_ = false;

  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    HeapVector<Member<SVGDocumentResource>> to_remove;
    for (const auto& resource : tracked_resources_) {
      if (!resource->GetContent()->HasObservers()) {
        resource->GetContent()->Dispose();
        to_remove.push_back(resource);
        MemoryCache::Get()->Remove(resource);
      }
    }
    tracked_resources_.RemoveAll(to_remove);
  } else {
    Vector<CacheKey> to_remove;
    for (auto& entry : entries_) {
      SVGResourceDocumentContent* content = entry.value;
      if (content->HasObservers()) {
        continue;
      }
      content->Dispose();
      to_remove.push_back(entry.key);
    }
    entries_.RemoveAll(to_remove);
  }
}

void SVGDocumentResourceTracker::ProcessCustomWeakness(
    const LivenessBroker& info) {
  // Don't need to do anything if there's a pending dispose task or not entries
  // to process.
  if (RuntimeEnabledFeatures::
          SvgPartitionSVGDocumentResourcesInMemoryCacheEnabled()) {
    if (dispose_task_pending_ || tracked_resources_.empty()) {
      return;
    }

    // Avoid scheduling spurious dispose tasks.
    const bool all_resources_are_observed = std::ranges::all_of(
        tracked_resources_, [](SVGDocumentResource* resource) {
          return resource->GetContent()->HasObservers();
        });
    if (all_resources_are_observed) {
      return;
    }

  } else {
    if (dispose_task_pending_ || entries_.empty()) {
      return;
    }
    // Avoid scheduling spurious dispose tasks.
    const bool all_entries_are_observed = std::ranges::all_of(
        entries_.Values(), [](SVGResourceDocumentContent* content) {
          return content->HasObservers();
        });
    if (all_entries_are_observed) {
      return;
    }
  }
  dispose_task_pending_ = dispose_task_runner_->PostTask(
      FROM_HERE, BindOnce(&SVGDocumentResourceTracker::DisposeUnobserved,
                          WrapWeakPersistent(this)));
}

void SVGDocumentResourceTracker::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      SVGDocumentResourceTracker,
      &SVGDocumentResourceTracker::ProcessCustomWeakness>(this);
  visitor->Trace(entries_);
  visitor->Trace(tracked_resources_);
}

void SVGDocumentResourceTracker::AddResource(SVGDocumentResource* resource) {
  tracked_resources_.insert(resource);
}

bool SVGDocumentResourceTracker::HasContentForTesting(
    SVGResourceDocumentContent* content) const {
  return std::ranges::any_of(tracked_resources_,
                             [content](const SVGDocumentResource* resource) {
                               return resource->GetContent() == content;
                             });
}

}  // namespace blink
