// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation_from_network.h"

#include "components/web_package/web_bundle_utils.h"
#include "content/browser/web_package/web_bundle_interceptor_for_history_navigation.h"
#include "content/browser/web_package/web_bundle_reader.h"
#include "content/browser/web_package/web_bundle_redirect_url_loader.h"
#include "content/browser/web_package/web_bundle_source.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/single_request_url_loader_factory.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "url/gurl.h"

namespace content {

WebBundleInterceptorForHistoryNavigationFromNetwork::
    WebBundleInterceptorForHistoryNavigationFromNetwork(
        std::unique_ptr<WebBundleSource> source,
        const GURL& target_inner_url,
        WebBundleDoneCallback done_callback,
        int frame_tree_node_id)
    : WebBundleInterceptorForHistoryNavigation(target_inner_url,
                                               std::move(done_callback),
                                               frame_tree_node_id),
      source_(std::move(source)) {
  DCHECK(source_->IsPathRestrictionSatisfied(target_inner_url_));
}

WebBundleInterceptorForHistoryNavigationFromNetwork::
    ~WebBundleInterceptorForHistoryNavigationFromNetwork() = default;

// NavigationLoaderInterceptor implementation
void WebBundleInterceptorForHistoryNavigationFromNetwork::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case State::kInitial:
      DCHECK_EQ(tentative_resource_request.url, target_inner_url_);
      std::move(callback).Run(
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              base::BindOnce(
                  &WebBundleInterceptorForHistoryNavigationFromNetwork::
                      StartRedirectResponse,
                  weak_factory_.GetWeakPtr())));
      return;
    case State::kRedirectedToWebBundle:
      DCHECK(!reader_);
      if (tentative_resource_request.url != source_->url()) {
        std::move(callback).Run(
            base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
                base::BindOnce(
                    &WebBundleInterceptorForHistoryNavigationFromNetwork::
                        StartErrorResponseForUnexpectedRedirect,
                    weak_factory_.GetWeakPtr())));
      } else {
        std::move(callback).Run({});
      }
      return;
    case State::kWebBundleRecieved:
      NOTREACHED();
      return;
    case State::kMetadataReady:
      std::move(callback).Run(
          base::MakeRefCounted<network::SingleRequestURLLoaderFactory>(
              base::BindOnce(
                  &WebBundleInterceptorForHistoryNavigationFromNetwork::
                      StartResponse,
                  weak_factory_.GetWeakPtr())));
  }
}

bool WebBundleInterceptorForHistoryNavigationFromNetwork::
    MaybeCreateLoaderForResponse(
        const network::ResourceRequest& request,
        network::mojom::URLResponseHeadPtr* response_head,
        mojo::ScopedDataPipeConsumerHandle* response_body,
        mojo::PendingRemote<network::mojom::URLLoader>* loader,
        mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
        blink::ThrottlingURLLoader* url_loader,
        bool* skip_other_interceptors,
        bool* will_return_and_handle_unsafe_redirect) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kRedirectedToWebBundle, state_);
  DCHECK_EQ(source_->url(), request.url);
  state_ = State::kWebBundleRecieved;
  *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
  if ((*response_head)->mime_type !=
      web_bundle_utils::kWebBundleFileMimeTypeWithoutParameters) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        "Unexpected content type.");
    return true;
  }
  if (!web_package::HasNoSniffHeader(**response_head)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kNoSniffErrorMessage);
    return true;
  }
  uint64_t length_hint =
      (*response_head)->content_length > 0
          ? static_cast<uint64_t>((*response_head)->content_length)
          : 0;

  // TODO(crbug.com/1018640): Check the special HTTP response header if we
  // decided to require one for WBN navigation.

  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id_);
  DCHECK(web_contents);
  BrowserContext* browser_context = web_contents->GetBrowserContext();
  DCHECK(browser_context);
  reader_ = base::MakeRefCounted<WebBundleReader>(
      std::move(source_), length_hint, std::move(*response_body),
      url_loader->Unbind(), browser_context->GetBlobStorageContext());
  reader_->ReadMetadata(base::BindOnce(
      &WebBundleInterceptorForHistoryNavigationFromNetwork::OnMetadataReady,
      weak_factory_.GetWeakPtr(), request));
  return true;
}

void WebBundleInterceptorForHistoryNavigationFromNetwork::OnMetadataReady(
    network::ResourceRequest request,
    web_package::mojom::BundleMetadataParseErrorPtr error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kWebBundleRecieved, state_);
  state_ = State::kMetadataReady;
  if (error) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::GetMetadataParseErrorMessage(error));
    return;
  }
  const absl::optional<GURL>& primary_url = reader_->GetPrimaryURL();
  if (!primary_url.has_value()) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kNoPrimaryUrlErrorMessage);
    return;
  }
  if (!web_bundle_utils::IsAllowedExchangeUrl(*primary_url)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kInvalidPrimaryUrlErrorMessage);
    return;
  }
  if (!base::ranges::all_of(reader_->GetEntries(),
                            &web_bundle_utils::IsAllowedExchangeUrl)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        web_bundle_utils::kInvalidExchangeUrlErrorMessage);
    return;
  }
  if (!reader_->HasEntry(target_inner_url_)) {
    web_bundle_utils::CompleteWithInvalidWebBundleError(
        std::move(forwarding_client_), frame_tree_node_id_,
        "The expected URL resource is not found in the web bundle.");
    return;
  }
  CreateWebBundleURLLoaderFactory(reader_);
  auto redirect_loader =
      std::make_unique<WebBundleRedirectURLLoader>(forwarding_client_.Unbind());
  redirect_loader->OnReadyToRedirect(request, target_inner_url_);
}

void WebBundleInterceptorForHistoryNavigationFromNetwork::StartRedirectResponse(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kInitial, state_);
  state_ = State::kRedirectedToWebBundle;
  auto redirect_loader =
      std::make_unique<WebBundleRedirectURLLoader>(std::move(client));
  redirect_loader->OnReadyToRedirect(resource_request, source_->url());
  mojo::MakeSelfOwnedReceiver(
      std::move(redirect_loader),
      mojo::PendingReceiver<network::mojom::URLLoader>(std::move(receiver)));
}

void WebBundleInterceptorForHistoryNavigationFromNetwork::
    StartErrorResponseForUnexpectedRedirect(
        const network::ResourceRequest& resource_request,
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kRedirectedToWebBundle, state_);
  DCHECK_NE(source_->url(), resource_request.url);
  web_bundle_utils::CompleteWithInvalidWebBundleError(
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client)),
      frame_tree_node_id_, "Unexpected redirect.");
}

void WebBundleInterceptorForHistoryNavigationFromNetwork::StartResponse(
    const network::ResourceRequest& resource_request,
    mojo::PendingReceiver<network::mojom::URLLoader> receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(State::kMetadataReady, state_);
  CreateLoaderAndStartAndDone(resource_request, std::move(receiver),
                              std::move(client));
}
}  // namespace content
