// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/web_bundle/web_bundle_loader.h"

#include "base/unguessable_token.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/loader/threadable_loader.h"
#include "third_party/blink/renderer/core/loader/threadable_loader_client.h"
#include "third_party/blink/renderer/platform/loader/cors/cors.h"
#include "third_party/blink/renderer/platform/loader/fetch/bytes_consumer.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/subresource_web_bundle.h"

namespace blink {

WebBundleLoader::WebBundleLoader(
    SubresourceWebBundle& subresource_web_bundle,
    Document& document,
    const KURL& url,
    network::mojom::CredentialsMode credentials_mode)
    : subresource_web_bundle_(&subresource_web_bundle),
      url_(url),
      security_origin_(SecurityOrigin::Create(url)),
      web_bundle_token_(base::UnguessableToken::Create()),
      task_runner_(
          document.GetFrame()->GetTaskRunner(TaskType::kInternalLoading)),
      receivers_(this, document.GetExecutionContext()) {
  ResourceRequest request(url);
  request.SetUseStreamOnResponse(true);
  request.SetRequestContext(
      mojom::blink::RequestContextType::SUBRESOURCE_WEBBUNDLE);

  // Spec:
  // https://github.com/WICG/webpackage/blob/main/explainers/subresource-loading.md#requests-mode-and-credentials-mode
  request.SetMode(network::mojom::blink::RequestMode::kCors);
  request.SetTargetAddressSpace(network::mojom::IPAddressSpace::kUnknown);
  request.SetCredentialsMode(credentials_mode);

  request.SetRequestDestination(network::mojom::RequestDestination::kWebBundle);
  request.SetPriority(ResourceLoadPriority::kHigh);
  // Skip the service worker for a short term solution.
  // TODO(crbug.com/1240424): Figure out the ideal design of the service
  // worker integration.
  request.SetSkipServiceWorker(true);

  mojo::PendingRemote<network::mojom::blink::WebBundleHandle> web_bundle_handle;
  receivers_.Add(web_bundle_handle.InitWithNewPipeAndPassReceiver(),
                 task_runner_);
  request.SetWebBundleTokenParams(ResourceRequestHead::WebBundleTokenParams(
      url_, web_bundle_token_, std::move(web_bundle_handle)));

  ExecutionContext* execution_context = document.GetExecutionContext();
  ResourceLoaderOptions resource_loader_options(
      execution_context->GetCurrentWorld());
  resource_loader_options.data_buffering_policy = kDoNotBufferData;

  loader_ = MakeGarbageCollected<ThreadableLoader>(*execution_context, this,
                                                   resource_loader_options);
  loader_->Start(std::move(request));
}

void WebBundleLoader::Trace(Visitor* visitor) const {
  visitor->Trace(subresource_web_bundle_);
  visitor->Trace(loader_);
  visitor->Trace(receivers_);
}

void WebBundleLoader::DidStartLoadingResponseBody(BytesConsumer& consumer) {
  // Drain |consumer| so that DidFinishLoading is surely called later.
  consumer.DrainAsDataPipe();
}

void WebBundleLoader::DidFail(uint64_t, const ResourceError&) {
  DidFailInternal();
}

void WebBundleLoader::DidFailRedirectCheck(uint64_t) {
  DidFailInternal();
}

void WebBundleLoader::Clone(
    mojo::PendingReceiver<network::mojom::blink::WebBundleHandle> receiver) {
  receivers_.Add(std::move(receiver), task_runner_);
}

void WebBundleLoader::OnWebBundleError(
    network::mojom::blink::WebBundleErrorType type,
    const String& message) {
  subresource_web_bundle_->OnWebBundleError(url_.ElidedString() + ": " +
                                            message);
}

void WebBundleLoader::OnWebBundleLoadFinished(bool success) {
  if (load_state_ != LoadState::kInProgress)
    return;
  if (success) {
    load_state_ = LoadState::kSuccess;
  } else {
    load_state_ = LoadState::kFailed;
  }

  subresource_web_bundle_->NotifyLoadingFinished();
}

void WebBundleLoader::ClearReceivers() {
  // Clear receivers_ explicitly so that resources in the netwok process are
  // released.
  receivers_.Clear();
}

void WebBundleLoader::DidFailInternal() {
  if (load_state_ != LoadState::kInProgress)
    return;
  load_state_ = LoadState::kFailed;
  subresource_web_bundle_->NotifyLoadingFinished();
}

}  // namespace blink
