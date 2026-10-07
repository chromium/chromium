// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/skeleton/skeleton.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/keywords.h"
#include "third_party/blink/renderer/core/sanitizer/sanitizer_builtins.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_initiator_type_names.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/loader/link_header.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

Skeleton::Skeleton(Observer& observer, Document& owner_document)
    : observer_(&observer), owner_document_(&owner_document) {}

// Class for doing an HTTP HEAD request to retrieve the link header for
// potential skeleton urls.
class Skeleton::LinkFetcher final
    : public GarbageCollected<Skeleton::LinkFetcher>,
      public RawResourceClient {
 public:
  explicit LinkFetcher(Skeleton& skeleton) : skeleton_(&skeleton) {}

  void Fetch(const KURL& url, ResourceFetcher* fetcher) {
    ResourceRequest request(url);
    // TODO(crbug.com/513276602): The request parameters need to be tweaked
    request.SetHttpMethod(http_names::kHEAD);
    request.SetRequestContext(mojom::blink::RequestContextType::INTERNAL);
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);

    FetchParameters params(std::move(request),
                           ResourceLoaderOptions(skeleton_->GetOwnerDocument()
                                                     .GetExecutionContext()
                                                     ->GetCurrentWorld()));
    params.MutableOptions().initiator_info.name =
        fetch_initiator_type_names::kInternal;

    RawResource::Fetch(params, fetcher, this);
  }

  void ResponseReceived(Resource* resource,
                        const ResourceResponse& response) override {
    DCHECK_EQ(resource, GetResource());
    LinkHeaderSet header_set(response.HttpHeaderField(http_names::kLink));
    for (const LinkHeader& header : header_set) {
      if (EqualIgnoringAsciiCase(header.Rel(), "skeleton")) {
        KURL skeleton_url = KURL(resource->Url(), header.Url());
        if (skeleton_url.IsValid()) {
          skeleton_->StartHTMLFetch(skeleton_url);
        }
        // TODO(crbug.com/513276602): Should we handle multiple links if the
        // first we encounter is invalid or fails to load?
        return;
      }
    }
  }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(resource, GetResource());
    ClearResource();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(skeleton_);
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "Skeleton:::LinkFetcher"; }

 private:
  Member<Skeleton> skeleton_;
};

// Class for fetching the skeleton document
class Skeleton::HTMLFetcher final
    : public GarbageCollected<Skeleton::HTMLFetcher>,
      public RawResourceClient {
 public:
  explicit HTMLFetcher(Skeleton& skeleton) : skeleton_(&skeleton) {}

  void Fetch(const KURL& url, ResourceFetcher* fetcher) {
    ResourceRequest request(url);
    // TODO(crbug.com/513276602): The request parameters need to be tweaked
    request.SetRequestContext(mojom::blink::RequestContextType::INTERNAL);
    request.SetReferrerPolicy(network::mojom::ReferrerPolicy::kDefault);

    FetchParameters params(std::move(request),
                           ResourceLoaderOptions(skeleton_->GetOwnerDocument()
                                                     .GetExecutionContext()
                                                     ->GetCurrentWorld()));
    params.MutableOptions().initiator_info.name =
        fetch_initiator_type_names::kInternal;

    RawResource::Fetch(params, fetcher, this);
  }

  void NotifyFinished(Resource* resource) override {
    DCHECK_EQ(resource, GetResource());
    int status_code = resource->GetResponse().HttpStatusCode();
    if (resource->ErrorOccurred() || status_code >= 400) {
      skeleton_->HTMLFetchFinished("", false);
    } else {
      auto decoder = std::make_unique<TextResourceDecoder>(
          TextResourceDecoderOptions(TextResourceDecoderOptions::kHTMLContent));
      if (resource->Encoding().IsValid()) {
        decoder->SetEncoding(resource->Encoding(),
                             TextResourceDecoder::kEncodingFromHTTPHeader);
      }

      scoped_refptr<const SharedBuffer> buffer = resource->ResourceBuffer();
      StringBuilder html_builder;
      if (buffer) {
        for (const auto& span : *buffer) {
          html_builder.Append(decoder->Decode(span));
        }
      }
      html_builder.Append(decoder->Flush());
      skeleton_->HTMLFetchFinished(html_builder.ToString(), true);
    }
    ClearResource();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(skeleton_);
    RawResourceClient::Trace(visitor);
  }

  String DebugName() const override { return "Skeleton::HTMLFetcher"; }

 private:
  Member<Skeleton> skeleton_;
};

void Skeleton::Render() {
  ExecutionContext* context = GetOwnerDocument().GetExecutionContext();
  skeleton_document_ = DocumentInit::Create()
                           .WithTypeFrom(keywords::kTextHtml)
                           .WithExecutionContext(context)
                           .WithAgent(*context->GetAgent())
                           .CreateDocument();
  skeleton_document_->setAllowDeclarativeShadowRoots(true);
  skeleton_document_->SetMimeType(keywords::kTextHtml);

  render_requested_ = true;

  if (html_fetch_completed_ && fetched_html_) {
    ParseSkeletonHTML(fetched_html_);
    observer_->DocumentReady(*this);
  }
}

void Skeleton::FetchSkeletonURL(KURL url) {
  if (!link_fetcher_) {
    link_fetcher_ = MakeGarbageCollected<LinkFetcher>(*this);
  }
  link_fetcher_->Fetch(url, GetOwnerDocument().Fetcher());
}

void Skeleton::StartHTMLFetch(const KURL& skeleton_url) {
  if (!html_fetcher_) {
    html_fetcher_ = MakeGarbageCollected<HTMLFetcher>(*this);
  }
  html_fetcher_->Fetch(skeleton_url, GetOwnerDocument().Fetcher());
}

void Skeleton::HTMLFetchFinished(const String& html, bool success) {
  html_fetch_completed_ = true;
  if (!success) {
    return;
  }

  fetched_html_ = html;

  if (render_requested_) {
    ParseSkeletonHTML(fetched_html_);
    observer_->DocumentReady(*this);
  }
}

void Skeleton::ParseSkeletonHTML(const String& html) {
  CHECK(skeleton_document_);
  skeleton_document_->SetContent(
      html, MakeGarbageCollected<StreamingSanitizer>(
                SanitizerBuiltins::GetBaseline(), Sanitizer::Mode::kSafe));
}

void Skeleton::Trace(Visitor* visitor) const {
  visitor->Trace(observer_);
  visitor->Trace(owner_document_);
  visitor->Trace(skeleton_document_);
  visitor->Trace(html_fetcher_);
  visitor->Trace(link_fetcher_);
}

}  // namespace blink
