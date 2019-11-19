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

#include "third_party/blink/renderer/core/loader/resource/document_resource.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/xml_document.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

DocumentResource* DocumentResource::FetchSVGDocument(FetchParameters& params,
                                                     ResourceFetcher* fetcher,
                                                     ResourceClient* client) {
  DCHECK_EQ(params.GetResourceRequest().GetMode(),
            network::mojom::RequestMode::kSameOrigin);
  params.SetRequestContext(mojom::RequestContextType::IMAGE);
  return ToDocumentResource(
      fetcher->RequestResource(params, SVGDocumentResourceFactory(), client));
}

DocumentResource::DocumentResource(
    const ResourceRequest& request,
    ResourceType type,
    const ResourceLoaderOptions& options,
    const TextResourceDecoderOptions& decoder_options)
    : TextResource(request, type, options, decoder_options) {
  // FIXME: We'll support more types to support HTMLImports.
  DCHECK_EQ(type, ResourceType::kSVGDocument);
}

DocumentResource::~DocumentResource() = default;

void DocumentResource::Trace(blink::Visitor* visitor) {
  visitor->Trace(document_);
  Resource::Trace(visitor);
}

void DocumentResource::NotifyFinished() {
  if (Data() && MimeTypeAllowed()) {
    // We don't need to create a new frame because the new document belongs to
    // the parent UseElement.
    document_ = CreateDocument(GetResponse().CurrentRequestUrl());
    document_->SetContent(DecodedText());
  }
  Resource::NotifyFinished();
}

bool DocumentResource::MimeTypeAllowed() const {
  DCHECK_EQ(GetType(), ResourceType::kSVGDocument);
  AtomicString mime_type = GetResponse().MimeType();
  if (GetResponse().IsHTTP())
    mime_type = HttpContentType();
  return mime_type == "image/svg+xml" || mime_type == "text/xml" ||
         mime_type == "application/xml" || mime_type == "application/xhtml+xml";
}

Document* DocumentResource::CreateDocument(const KURL& url) {
  switch (GetType()) {
    case ResourceType::kSVGDocument:
      return XMLDocument::CreateSVG(DocumentInit::Create().WithURL(url));
    default:
      // FIXME: We'll add more types to support HTMLImports.
      NOTREACHED();
      return nullptr;
  }
}

}  // namespace blink
