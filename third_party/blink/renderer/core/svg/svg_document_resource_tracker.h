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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_DOCUMENT_RESOURCE_TRACKER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_DOCUMENT_RESOURCE_TRACKER_H_

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class FetchParameters;
class SVGDocumentResource;
class SVGResourceDocumentContent;

class CORE_EXPORT SVGDocumentResourceTracker final
    : public GarbageCollected<SVGDocumentResourceTracker> {
 public:
  explicit SVGDocumentResourceTracker(
      scoped_refptr<base::SingleThreadTaskRunner>,
      const String& cache_identifier);

  // The key is "URL (without fragment)" and the request mode (kSameOrigin or
  // kCors - other modes should be filtered by AllowedRequestMode).
  using CacheKey = std::pair<String, network::mojom::blink::RequestMode>;

  static CacheKey MakeCacheKey(const FetchParameters& params);
  static String MakeCacheIdentifier(StringView browser_context_group_token);

  SVGResourceDocumentContent* Get(const CacheKey& key);
  void Put(const CacheKey& key, SVGResourceDocumentContent* content);

  void WillBeDestroyed();

  void Trace(Visitor*) const;

  void AddResource(SVGDocumentResource* resource);

  const String& GetCacheIdentifier() const { return cache_identifier_; }

  bool HasContentForTesting(SVGResourceDocumentContent* content) const;

 private:
  void DisposeUnobserved();
  void ProcessCustomWeakness(const LivenessBroker&);

  // TODO(dmangal) Remove below hashmap during feature flag removal.
  HeapHashMap<CacheKey, Member<SVGResourceDocumentContent>> entries_;
  HeapHashSet<Member<SVGDocumentResource>> tracked_resources_;
  scoped_refptr<base::SingleThreadTaskRunner> dispose_task_runner_;
  bool dispose_task_pending_ = false;
  String cache_identifier_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_DOCUMENT_RESOURCE_TRACKER_H_
