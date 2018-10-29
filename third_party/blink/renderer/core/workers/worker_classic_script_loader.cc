/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009, 2011 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "third_party/blink/renderer/core/workers/worker_classic_script_loader.h"

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/public/mojom/net/ip_address_space.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/loader/allowed_by_nosniff.h"
#include "third_party/blink/renderer/core/loader/resource/script_resource.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/text_resource_decoder_options.h"
#include "third_party/blink/renderer/platform/network/content_security_policy_response_headers.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

WorkerClassicScriptLoader::WorkerClassicScriptLoader()
    : response_address_space_(mojom::IPAddressSpace::kPublic) {}

WorkerClassicScriptLoader::~WorkerClassicScriptLoader() {
  // If |threadable_loader_| is still working, we have to cancel it here.
  // Otherwise DidFail() of the deleted |this| will be called from
  // ThreadableLoader::NotifyFinished() when the frame will be
  // destroyed.
  if (need_to_cancel_)
    Cancel();
}

void WorkerClassicScriptLoader::LoadSynchronously(
    ExecutionContext& execution_context,
    const KURL& url,
    mojom::RequestContextType request_context,
    mojom::IPAddressSpace creation_address_space) {
  url_ = url;
  execution_context_ = &execution_context;

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::GET);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      creation_address_space);
  request.SetRequestContext(request_context);

  SECURITY_DCHECK(execution_context.IsWorkerGlobalScope());

  ResourceLoaderOptions resource_loader_options;
  resource_loader_options.parser_disposition =
      ParserDisposition::kNotParserInserted;
  resource_loader_options.synchronous_policy = kRequestSynchronously;

  threadable_loader_ = new ThreadableLoader(
      execution_context, this, resource_loader_options);
  threadable_loader_->Start(request);
}

void WorkerClassicScriptLoader::LoadTopLevelScriptAsynchronously(
    ExecutionContext& execution_context,
    const KURL& url,
    mojom::RequestContextType request_context,
    network::mojom::FetchRequestMode fetch_request_mode,
    network::mojom::FetchCredentialsMode fetch_credentials_mode,
    mojom::IPAddressSpace creation_address_space,
    base::OnceClosure response_callback,
    base::OnceClosure finished_callback) {
  DCHECK(response_callback || finished_callback);
  response_callback_ = std::move(response_callback);
  finished_callback_ = std::move(finished_callback);
  url_ = url;
  execution_context_ = &execution_context;
  forbid_cross_origin_redirects_ = true;

  ResourceRequest request(url);
  request.SetHTTPMethod(HTTPNames::GET);
  request.SetExternalRequestStateFromRequestorAddressSpace(
      creation_address_space);
  request.SetRequestContext(request_context);
  request.SetFetchRequestMode(fetch_request_mode);
  request.SetFetchCredentialsMode(fetch_credentials_mode);

  ResourceLoaderOptions resource_loader_options;

  // During create, callbacks may happen which could remove the last reference
  // to this object, while some of the callchain assumes that the client and
  // loader wouldn't be deleted within callbacks.
  // (E.g. see crbug.com/524694 for why we can't easily remove this protect)
  scoped_refptr<WorkerClassicScriptLoader> protect(this);
  need_to_cancel_ = true;
  threadable_loader_ = new ThreadableLoader(
      execution_context, this, resource_loader_options);
  threadable_loader_->Start(request);
  if (failed_)
    NotifyFinished();
}

const KURL& WorkerClassicScriptLoader::ResponseURL() const {
  DCHECK(!Failed());
  return response_url_;
}

void WorkerClassicScriptLoader::DidReceiveResponse(
    unsigned long identifier,
    const ResourceResponse& response,
    std::unique_ptr<WebDataConsumerHandle> handle) {
  DCHECK(!handle);
  if (response.HttpStatusCode() / 100 != 2 && response.HttpStatusCode()) {
    NotifyError();
    return;
  }
  if (!AllowedByNosniff::MimeTypeAsScript(execution_context_, response)) {
    NotifyError();
    return;
  }

  if (forbid_cross_origin_redirects_ && url_ != response.Url() &&
      !SecurityOrigin::AreSameSchemeHostPort(url_, response.Url())) {
    // Forbid cross-origin redirects to ensure the request and response URLs
    // have the same SecurityOrigin.
    execution_context_->AddConsoleMessage(ConsoleMessage::Create(
        kSecurityMessageSource, kErrorMessageLevel,
        "Refused to cross-origin redirects of the top-level worker script."));
    NotifyError();
    return;
  }

  identifier_ = identifier;
  if (response.WasFetchedViaServiceWorker() &&
      !response.OriginalURLViaServiceWorker().IsEmpty()) {
    response_url_ = response.OriginalURLViaServiceWorker();
  } else {
    response_url_ = response.Url();
  }

  response_encoding_ = response.TextEncodingName();
  app_cache_id_ = response.AppCacheID();

  referrer_policy_ = response.HttpHeaderField(HTTPNames::Referrer_Policy);
  ProcessContentSecurityPolicy(response);
  origin_trial_tokens_ = OriginTrialContext::ParseHeaderValue(
      response.HttpHeaderField(HTTPNames::Origin_Trial));

  if (network_utils::IsReservedIPAddress(response.RemoteIPAddress())) {
    response_address_space_ =
        SecurityOrigin::Create(response_url_)->IsLocalhost()
            ? mojom::IPAddressSpace::kLocal
            : mojom::IPAddressSpace::kPrivate;
  }

  if (response_callback_)
    std::move(response_callback_).Run();
}

void WorkerClassicScriptLoader::DidReceiveData(const char* data, unsigned len) {
  if (failed_)
    return;

  if (!decoder_) {
    decoder_ = TextResourceDecoder::Create(TextResourceDecoderOptions(
        TextResourceDecoderOptions::kPlainTextContent,
        response_encoding_.IsEmpty() ? UTF8Encoding()
                                     : WTF::TextEncoding(response_encoding_)));
  }

  if (!len)
    return;

  source_text_.Append(decoder_->Decode(data, len));
}

void WorkerClassicScriptLoader::DidReceiveCachedMetadata(const char* data,
                                                         int size) {
  cached_metadata_ = std::make_unique<Vector<char>>(size);
  memcpy(cached_metadata_->data(), data, size);
}

void WorkerClassicScriptLoader::DidFinishLoading(unsigned long identifier) {
  need_to_cancel_ = false;
  if (!failed_ && decoder_)
    source_text_.Append(decoder_->Flush());

  NotifyFinished();
}

void WorkerClassicScriptLoader::DidFail(const ResourceError& error) {
  need_to_cancel_ = false;
  canceled_ = error.IsCancellation();
  NotifyError();
}

void WorkerClassicScriptLoader::DidFailRedirectCheck() {
  // When didFailRedirectCheck() is called, the ResourceLoader for the script
  // is not canceled yet. So we don't reset |m_needToCancel| here.
  NotifyError();
}

void WorkerClassicScriptLoader::Cancel() {
  need_to_cancel_ = false;
  if (threadable_loader_)
    threadable_loader_->Cancel();
}

String WorkerClassicScriptLoader::SourceText() {
  return source_text_.ToString();
}

void WorkerClassicScriptLoader::NotifyError() {
  failed_ = true;
  // NotifyError() could be called before ThreadableLoader::Create() returns
  // e.g. from DidFail(), and in that case threadable_loader_ is not yet set
  // (i.e. still null).
  // Since the callback invocation in NotifyFinished() potentially delete
  // |this| object, the callback invocation should be postponed until the
  // create() call returns. See LoadAsynchronously() for the postponed call.
  if (threadable_loader_)
    NotifyFinished();
}

void WorkerClassicScriptLoader::NotifyFinished() {
  if (!finished_callback_)
    return;

  std::move(finished_callback_).Run();
}

void WorkerClassicScriptLoader::ProcessContentSecurityPolicy(
    const ResourceResponse& response) {
  // Per http://www.w3.org/TR/CSP2/#processing-model-workers, if the Worker's
  // URL is not a GUID, then it grabs its CSP from the response headers
  // directly.  Otherwise, the Worker inherits the policy from the parent
  // document (which is implemented in WorkerMessagingProxy, and
  // m_contentSecurityPolicy should be left as nullptr to inherit the policy).
  if (!response.Url().ProtocolIs("blob") &&
      !response.Url().ProtocolIs("file") &&
      !response.Url().ProtocolIs("filesystem")) {
    content_security_policy_ = ContentSecurityPolicy::Create();
    content_security_policy_->SetOverrideURLForSelf(response.Url());
    content_security_policy_->DidReceiveHeaders(
        ContentSecurityPolicyResponseHeaders(response));
  }
}

}  // namespace blink
