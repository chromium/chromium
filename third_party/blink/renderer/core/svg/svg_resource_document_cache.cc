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

#include "third_party/blink/renderer/core/svg/svg_resource_document_cache.h"

#include "base/ranges/algorithm.h"
#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/memory_cache.h"

namespace blink {

SVGResourceDocumentCache::SVGResourceDocumentCache(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : dispose_task_runner_(std::move(task_runner)) {}

SVGResourceDocumentCache::CacheKey SVGResourceDocumentCache::MakeCacheKey(
    const FetchParameters& params) {
  const KURL url_without_fragment =
      MemoryCache::RemoveFragmentIdentifierIfNeeded(params.Url());
  return {url_without_fragment.GetString(),
          params.GetResourceRequest().GetMode()};
}

SVGResourceDocumentContent* SVGResourceDocumentCache::Get(const CacheKey& key) {
  auto it = entries_.find(key);
  return it != entries_.end() ? it->value : nullptr;
}

void SVGResourceDocumentCache::Put(const CacheKey& key,
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

void SVGResourceDocumentCache::WillBeDestroyed() {
  for (SVGResourceDocumentContent* content : entries_.Values()) {
    content->Dispose();
  }
}

void SVGResourceDocumentCache::DisposeUnobserved() {
  dispose_task_pending_ = false;

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

void SVGResourceDocumentCache::ProcessCustomWeakness(
    const LivenessBroker& info) {
  // Don't need to do anything if there's a pending dispose task or not entries
  // to process.
  if (dispose_task_pending_ || entries_.empty()) {
    return;
  }
  // Avoid scheduling spurious dispose tasks.
  const bool all_entries_are_observed = base::ranges::all_of(
      entries_.Values(), [](SVGResourceDocumentContent* content) {
        return content->HasObservers();
      });
  if (all_entries_are_observed) {
    return;
  }
  dispose_task_pending_ = dispose_task_runner_->PostTask(
      FROM_HERE, WTF::BindOnce(&SVGResourceDocumentCache::DisposeUnobserved,
                               WrapWeakPersistent(this)));
}

void SVGResourceDocumentCache::Trace(Visitor* visitor) const {
  visitor->template RegisterWeakCallbackMethod<
      SVGResourceDocumentCache,
      &SVGResourceDocumentCache::ProcessCustomWeakness>(this);
  visitor->Trace(entries_);
}

}  // namespace blink
