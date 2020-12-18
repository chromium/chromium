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

#include "third_party/blink/renderer/core/svg/svg_external_document_cache.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/loader/resource/text_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_client.h"

namespace blink {

namespace {

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
                                 .WithExecutionContext(execution_context));
  document->SetContent(resource->DecodedText());
  return document;
}

}  // namespace

Document* SVGExternalDocumentCache::Entry::GetDocument() {
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

const KURL& SVGExternalDocumentCache::Entry::Url() const {
  return resource_->Url();
}

void SVGExternalDocumentCache::Entry::Trace(Visitor* visitor) const {
  visitor->Trace(resource_);
  visitor->Trace(document_);
  visitor->Trace(context_);
}

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

SVGExternalDocumentCache::Entry* SVGExternalDocumentCache::Get(
    ResourceClient* client,
    const KURL& url,
    const AtomicString& initiator_name,
    network::mojom::blink::CSPDisposition csp_disposition) {
  Document* context_document = GetSupplementable();
  ExecutionContext* execution_context = context_document->GetExecutionContext();
  ResourceLoaderOptions options(execution_context->GetCurrentWorld());
  options.initiator_info.name = initiator_name;
  FetchParameters params(ResourceRequest(url), options);
  params.SetContentSecurityCheck(csp_disposition);
  params.MutableResourceRequest().SetMode(
      network::mojom::blink::RequestMode::kSameOrigin);
  params.SetRequestContext(mojom::blink::RequestContextType::IMAGE);
  params.SetRequestDestination(network::mojom::RequestDestination::kImage);

  TextResource* resource = TextResource::FetchSVGDocument(
      params, context_document->Fetcher(), client);
  if (!resource)
    return nullptr;
  auto& entry = entries_.insert(resource, nullptr).stored_value->value;
  if (!entry)
    entry = MakeGarbageCollected<Entry>(resource, execution_context);
  if (resource->IsCacheValidator())
    entry->SetWasRevalidating();
  return entry;
}

void SVGExternalDocumentCache::Trace(Visitor* visitor) const {
  Supplement<Document>::Trace(visitor);
  visitor->Trace(entries_);
}

}  // namespace blink
