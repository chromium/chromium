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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_EXTERNAL_DOCUMENT_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_EXTERNAL_DOCUMENT_CACHE_H_

#include "services/network/public/mojom/content_security_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Document;
class ExecutionContext;
class ResourceClient;
class TextResource;

class CORE_EXPORT SVGExternalDocumentCache final
    : public GarbageCollected<SVGExternalDocumentCache>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static SVGExternalDocumentCache* From(Document&);
  explicit SVGExternalDocumentCache(Document&);
  void Trace(Visitor*) const override;

  class CORE_EXPORT Entry final : public GarbageCollected<Entry> {
   public:
    Entry(TextResource* resource, ExecutionContext* context)
        : resource_(resource), context_(context) {
      DCHECK(resource_);
      DCHECK(context_);
    }
    void SetWasRevalidating() { was_revalidating_ = true; }

    Document* GetDocument();
    const KURL& Url() const;

    void Trace(Visitor*) const;

   private:
    Member<TextResource> resource_;
    Member<Document> document_;
    Member<ExecutionContext> context_;
    bool was_revalidating_ = false;
  };

  Entry* Get(ResourceClient*,
             const KURL&,
             const AtomicString& initiator_name,
             network::mojom::blink::CSPDisposition =
                 network::mojom::blink::CSPDisposition::CHECK);

 private:
  HeapHashMap<WeakMember<Resource>, Member<Entry>> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_EXTERNAL_DOCUMENT_CACHE_H_
