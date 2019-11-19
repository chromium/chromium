/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/exported/web_document_loader_impl.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/loader/mhtml_load_result.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_document_subresource_filter.h"
#include "third_party/blink/public/platform/web_loading_hints_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/subresource_filter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"

namespace blink {

// static
bool WebDocumentLoader::WillLoadUrlAsEmpty(const WebURL& url) {
  return DocumentLoader::WillLoadUrlAsEmpty(url);
}

WebURL WebDocumentLoaderImpl::OriginalUrl() const {
  return DocumentLoader::OriginalUrl();
}

WebString WebDocumentLoaderImpl::OriginalReferrer() const {
  return DocumentLoader::OriginalReferrer().referrer;
}

WebURL WebDocumentLoaderImpl::GetUrl() const {
  return DocumentLoader::Url();
}

WebString WebDocumentLoaderImpl::HttpMethod() const {
  return DocumentLoader::HttpMethod();
}

WebString WebDocumentLoaderImpl::Referrer() const {
  return DocumentLoader::GetReferrer().referrer;
}

network::mojom::ReferrerPolicy WebDocumentLoaderImpl::GetReferrerPolicy()
    const {
  return DocumentLoader::GetReferrer().referrer_policy;
}

const WebURLResponse& WebDocumentLoaderImpl::GetResponse() const {
  return response_wrapper_;
}

bool WebDocumentLoaderImpl::HasUnreachableURL() const {
  return !DocumentLoader::UnreachableURL().IsEmpty();
}

WebURL WebDocumentLoaderImpl::UnreachableURL() const {
  return DocumentLoader::UnreachableURL();
}

void WebDocumentLoaderImpl::RedirectChain(WebVector<WebURL>& result) const {
  result.Assign(redirect_chain_);
}

bool WebDocumentLoaderImpl::IsClientRedirect() const {
  return DocumentLoader::IsClientRedirect();
}

bool WebDocumentLoaderImpl::ReplacesCurrentHistoryItem() const {
  return DocumentLoader::ReplacesCurrentHistoryItem();
}

WebNavigationType WebDocumentLoaderImpl::GetNavigationType() const {
  return DocumentLoader::GetNavigationType();
}

WebDocumentLoader::ExtraData* WebDocumentLoaderImpl::GetExtraData() const {
  return extra_data_.get();
}

void WebDocumentLoaderImpl::SetExtraData(
    std::unique_ptr<ExtraData> extra_data) {
  extra_data_ = std::move(extra_data);
}

WebDocumentLoaderImpl::WebDocumentLoaderImpl(
    LocalFrame* frame,
    WebNavigationType navigation_type,
    std::unique_ptr<WebNavigationParams> navigation_params)
    : DocumentLoader(frame, navigation_type, std::move(navigation_params)),
      response_wrapper_(DocumentLoader::GetResponse()) {}

WebDocumentLoaderImpl::~WebDocumentLoaderImpl() {
  // Verify that detachFromFrame() has been called.
  DCHECK(!extra_data_);
}

void WebDocumentLoaderImpl::DetachFromFrame(bool flush_microtask_queue) {
  DocumentLoader::DetachFromFrame(flush_microtask_queue);
  extra_data_.reset();
}

void WebDocumentLoaderImpl::SetSubresourceFilter(
    WebDocumentSubresourceFilter* subresource_filter) {
  DocumentLoader::SetSubresourceFilter(SubresourceFilter::Create(
      *GetFrame()->GetDocument(), base::WrapUnique(subresource_filter)));
}

void WebDocumentLoaderImpl::SetLoadingHintsProvider(
    std::unique_ptr<blink::WebLoadingHintsProvider> loading_hints_provider) {
  DocumentLoader::SetPreviewsResourceLoadingHints(
      PreviewsResourceLoadingHints::CreateFromLoadingHintsProvider(
          *GetFrame()->GetDocument(), std::move(loading_hints_provider)));
}

void WebDocumentLoaderImpl::SetServiceWorkerNetworkProvider(
    std::unique_ptr<WebServiceWorkerNetworkProvider> provider) {
  DocumentLoader::SetServiceWorkerNetworkProvider(std::move(provider));
}

WebServiceWorkerNetworkProvider*
WebDocumentLoaderImpl::GetServiceWorkerNetworkProvider() {
  return DocumentLoader::GetServiceWorkerNetworkProvider();
}

void WebDocumentLoaderImpl::BlockParser() {
  DocumentLoader::BlockParser();
}

void WebDocumentLoaderImpl::ResumeParser() {
  DocumentLoader::ResumeParser();
}

bool WebDocumentLoaderImpl::HasBeenLoadedAsWebArchive() const {
  return archive_ || (archive_load_result_ != mojom::MHTMLLoadResult::kSuccess);
}

WebURLRequest::PreviewsState WebDocumentLoaderImpl::GetPreviewsState() const {
  return DocumentLoader::GetPreviewsState();
}

WebArchiveInfo WebDocumentLoaderImpl::GetArchiveInfo() const {
  if (archive_) {
    DCHECK(archive_->MainResource());
    return {archive_load_result_, archive_->MainResource()->Url(),
            archive_->Date()};
  }
  return {archive_load_result_, WebURL(), base::Time()};
}

bool WebDocumentLoaderImpl::HadUserGesture() const {
  return DocumentLoader::HadTransientActivation();
}

bool WebDocumentLoaderImpl::IsListingFtpDirectory() const {
  return DocumentLoader::IsListingFtpDirectory();
}

void WebDocumentLoaderImpl::Trace(blink::Visitor* visitor) {
  DocumentLoader::Trace(visitor);
}

}  // namespace blink
