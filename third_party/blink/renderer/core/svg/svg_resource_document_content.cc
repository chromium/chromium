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

#include "third_party/blink/renderer/core/svg/svg_resource_document_content.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

namespace {

class SVGExternalDocumentCache final
    : public GarbageCollected<SVGExternalDocumentCache>,
      public Supplement<Document> {
 public:
  static const char kSupplementName[];
  static SVGExternalDocumentCache* From(Document&);
  explicit SVGExternalDocumentCache(Document&);

  SVGResourceDocumentContent* Get(TextResource*);

  void Trace(Visitor*) const override;

 private:
  HeapHashMap<WeakMember<Resource>, Member<SVGResourceDocumentContent>>
      entries_;
};

const char SVGExternalDocumentCache::kSupplementName[] =
    "SVGExternalDocumentCache";

SVGExternalDocumentCache* SVGExternalDocumentCache::From(Document& document) {
  SVGExternalDocumentCache* cache =
      Supplement<Document>::From<SVGExternalDocumentCache>(document);
  if (!cache) {
    cache = MakeGarbageCollected<SVGExternalDocumentCache>(document);
    Supplement<Document>::ProvideTo(document, cache);
  }
  return cache;
}

SVGExternalDocumentCache::SVGExternalDocumentCache(Document& document)
    : Supplement<Document>(document) {}

SVGResourceDocumentContent* SVGExternalDocumentCache::Get(
    TextResource* resource) {
  auto& entry = entries_.insert(resource, nullptr).stored_value->value;
  if (!entry) {
    entry = MakeGarbageCollected<SVGResourceDocumentContent>(
        resource, GetSupplementable()->GetExecutionContext());
  }
  return entry;
}

void SVGExternalDocumentCache::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(entries_);
}

bool MimeTypeAllowed(const ResourceResponse& response) {
  AtomicString mime_type = response.MimeType();
  if (response.IsHTTP())
    mime_type = response.HttpContentType();
  return mime_type == "image/svg+xml" || mime_type == "text/xml" ||
         mime_type == "application/xml" || mime_type == "application/xhtml+xml";
}

Document* CreateDocument(const TextResource* resource,
                         ExecutionContext* execution_context) {
  const ResourceResponse& response = resource->GetResponse();
  if (!MimeTypeAllowed(response))
    return nullptr;
  auto* document =
      XMLDocument::CreateSVG(DocumentInit::Create()
                                 .WithURL(response.CurrentRequestUrl())
                                 .WithExecutionContext(execution_context)
                                 .WithAgent(*execution_context->GetAgent()));
  document->SetContent(resource->DecodedText());
  return document;
}

}  // namespace

Document* SVGResourceDocumentContent::GetDocument() {
  if (resource_->IsLoaded()) {
    // If this entry saw a revalidation, re-parse the document.
    // TODO(fs): This will be inefficient for successful revalidations, so we
    // want to detect those and not re-parse the document in those cases.
    if (was_revalidating_) {
      document_.Clear();
      was_revalidating_ = false;
    }
    if (!document_ && resource_->HasData())
      document_ = CreateDocument(resource_, context_);
  }
  return document_;
}

const KURL& SVGResourceDocumentContent::Url() const {
  return resource_->Url();
}

bool SVGResourceDocumentContent::IsLoading() const {
  return resource_->IsLoading();
}

void SVGResourceDocumentContent::Trace(Visitor* visitor) const {
  visitor->Trace(resource_);
  visitor->Trace(document_);
  visitor->Trace(context_);
}

SVGResourceDocumentContent* SVGResourceDocumentContent::Fetch(
    FetchParameters& params,
    Document& document,
    ResourceClient* client) {
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  DCHECK_EQ(params.GetResourceRequest().GetRequestContext(),
            mojom::blink::RequestContextType::UNSPECIFIED);
  params.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
  params.SetRequestDestination(network::mojom::RequestDestination::kImage);

  TextResource* resource =
      TextResource::FetchSVGDocument(params, document.Fetcher(), client);
  if (!resource)
    return nullptr;
  auto* document_content =
      SVGExternalDocumentCache::From(document)->Get(resource);
  if (resource->IsCacheValidator())
    document_content->SetWasRevalidating();
  return document_content;
}

}  // namespace blink
