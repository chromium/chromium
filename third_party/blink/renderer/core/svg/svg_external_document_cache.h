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
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {
class Document;
class ExecutionContext;

class CORE_EXPORT SVGExternalDocumentCache
    : public GarbageCollected<SVGExternalDocumentCache>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static SVGExternalDocumentCache* From(Document&);
  explicit SVGExternalDocumentCache(Document&);
  void Trace(Visitor*) const override;

  class Client : public GarbageCollectedMixin {
   public:
    virtual void NotifyFinished(Document*) = 0;
  };

  class CORE_EXPORT Entry final : public GarbageCollected<Entry>,
                                  public ResourceClient {
   public:
    explicit Entry(ExecutionContext* context) : context_(context) {}
    ~Entry() override = default;
    void Trace(Visitor*) const override;
    Document* GetDocument();
    const KURL& Url() const { return GetResource()->Url(); }

   private:
    friend class SVGExternalDocumentCache;
    void AddClient(Client*);

    // ResourceClient overrides;
    void NotifyFinished(Resource*) override;
    String DebugName() const override { return "SVGExternalDocumentCache"; }

    Member<Document> document_;
    Member<ExecutionContext> context_;
    HeapHashSet<WeakMember<Client>> clients_;
  };

  Entry* Get(Client*,
             const KURL&,
             const AtomicString& initiator_name,
             network::mojom::blink::CSPDisposition =
                 network::mojom::blink::CSPDisposition::CHECK);

 private:
  HeapHashMap<WeakMember<Resource>, WeakMember<Entry>> entries_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SVG_SVG_EXTERNAL_DOCUMENT_CACHE_H_
