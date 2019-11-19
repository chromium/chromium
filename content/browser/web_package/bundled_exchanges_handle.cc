// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_package/bundled_exchanges_handle.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/sequence_checker.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/browser/loader/navigation_loader_interceptor.h"
#include "content/browser/loader/single_request_url_loader_factory.h"
#include "content/browser/web_package/bundled_exchanges_handle_tracker.h"
#include "content/browser/web_package/bundled_exchanges_navigation_info.h"
#include "content/browser/web_package/bundled_exchanges_reader.h"
#include "content/browser/web_package/bundled_exchanges_source.h"
#include "content/browser/web_package/bundled_exchanges_url_loader_factory.h"
#include "content/browser/web_package/bundled_exchanges_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/mojom/bundled_exchanges_parser.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"

namespace content {

namespace {

using DoneCallback = base::OnceCallback<void(
    const GURL& target_inner_url,
    std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory)>;

const net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("bundled_exchanges_start_url_loader",
                                        R"(
  semantics {
    sender: "BundledExchanges primary_url Loader"
    description:
      "Navigation request for the primary_url provided by a BundledExchanges "
      "that is requested by the user. This does not trigger any network "
      "transaction directly, but access to an entry in a local file, or in a "
      "previously fetched resource over network."
    trigger: "The user navigates to a BundledExchanges."
    data: "Nothing."
    destination: LOCAL
  }
  policy {
    cookies_allowed: NO
    setting: "These requests cannot be disabled in settings."
    policy_exception_justification:
      "Not implemented. This request does not make any network transaction."
  }
  comments:
    "Usually the request accesses an entry in a local file that contains "
    "multiple archived entries. But once the feature is exposed to the public "
    "web API, the archive file can be streamed over network. In such case, the "
    "streaming should be provided by another URLLoader request that is issued "
    "by Blink, but based on a user initiated navigation."
  )");

void AddErrorMessageToConsole(int frame_tree_node_id,
                              const std::string& error_message) {
  WebContents* web_contents =
      WebContents::FromFrameTreeNodeId(frame_tree_node_id);
  if (!web_contents)
    return;
  web_contents->GetMainFrame()->AddMessageToConsole(
      blink::mojom::ConsoleMessageLevel::kError, error_message);
}

void AddMetadataParseErrorMessageToConsole(
    int frame_tree_node_id,
    const data_decoder::mojom::BundleMetadataParseErrorPtr& metadata_error) {
  AddErrorMessageToConsole(
      frame_tree_node_id,
      std::string("Failed to read metadata of Web Bundle file: ") +
          metadata_error->message);
}

// A class to provide a network::mojom::URLLoader interface to redirect a
// request to the BundledExchanges to the main resource url.
class PrimaryURLRedirectLoader final : public network::mojom::URLLoader {
 public:
  PrimaryURLRedirectLoader(
      mojo::PendingRemote<network::mojom::URLLoaderClient> client)
      : client_(std::move(client)) {}

  void OnReadyToRedirect(const network::ResourceRequest& resource_request,
                         const GURL& url) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(client_.is_connected());
    network::ResourceResponseHead response_head;
    response_head.encoded_data_length = 0;
    response_head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(
            base::StringPrintf("HTTP/1.1 %d %s\r\n", 303, "See Other")));

    net::RedirectInfo redirect_info = net::RedirectInfo::ComputeRedirectInfo(
        "GET", resource_request.url, resource_request.request_initiator,
        resource_request.site_for_cookies,
        resource_request.update_first_party_url_on_redirect
            ? net::URLRequest::FirstPartyURLPolicy::
                  UPDATE_FIRST_PARTY_URL_ON_REDIRECT
            : net::URLRequest::FirstPartyURLPolicy::
                  NEVER_CHANGE_FIRST_PARTY_URL,
        resource_request.referrer_policy, resource_request.referrer.spec(), 303,
        url, /*referrer_policy_header=*/base::nullopt,
        /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/true,
        /*is_signed_exchange_fallback_redirect=*/false);
    client_->OnReceiveRedirect(redirect_info, response_head);
  }

 private:
  // mojom::URLLoader overrides:
  void FollowRedirect(const std::vector<std::string>& removed_headers,
                      const net::HttpRequestHeaders& modified_headers,
                      const base::Optional<GURL>& new_url) override {}
  void SetPriority(net::RequestPriority priority,
                   int intra_priority_value) override {}
  void PauseReadingBodyFromNet() override {}
  void ResumeReadingBodyFromNet() override {}

  SEQUENCE_CHECKER(sequence_checker_);

  mojo::Remote<network::mojom::URLLoaderClient> client_;

  DISALLOW_COPY_AND_ASSIGN(PrimaryURLRedirectLoader);
};

// A class to inherit NavigationLoaderInterceptor for a navigation to a
// untrustable BundledExchanges file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for all navigation requests. It calls the
//     |callback| with a null RequestHandler.
// [2] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     If the response mime type is not "application/webbundle", returns false.
//     Otherwise starts reading the metadata and returns true. Once the metadata
//     is read, OnMetadataReady() is called, and a redirect loader is
//     created to redirect the navigation request to the Bundle's synthesized
//     primary URL ("file:///tmp/a.wbn?https://example.com/a.html").
// [3] MaybeCreateLoader() is called again for the redirect. It continues on
//     StartResponse() to create the loader for the main resource.
class InterceptorForFile final : public NavigationLoaderInterceptor {
 public:
  explicit InterceptorForFile(DoneCallback done_callback,
                              int frame_tree_node_id)
      : done_callback_(std::move(done_callback)),
        frame_tree_node_id_(frame_tree_node_id) {}
  ~InterceptorForFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // InterceptorForFile::MaybeCreateLoader() creates a loader only after
    // recognising that the response is a bundled exchange file at
    // MaybeCreateLoaderForResponse() and successfully created
    // |url_loader_factory_|.
    if (!url_loader_factory_) {
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(
        base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
            &InterceptorForFile::StartResponse, weak_factory_.GetWeakPtr())));
  }

  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      network::mojom::URLLoaderPtr* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_unsafe_redirect) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(bundled_exchanges_utils::IsSupprtedFileScheme(request.url));
    if (response_head.mime_type !=
        bundled_exchanges_utils::
            kBundledExchangesFileMimeTypeWithoutParameters) {
      return false;
    }
    std::unique_ptr<BundledExchangesSource> source =
        BundledExchangesSource::MaybeCreateFromFileUrl(request.url);
    if (!source)
      return false;
    reader_ = base::MakeRefCounted<BundledExchangesReader>(std::move(source));
    reader_->ReadMetadata(base::BindOnce(&InterceptorForFile::OnMetadataReady,
                                         weak_factory_.GetWeakPtr(), request));
    *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
    *will_return_unsafe_redirect = true;
    return true;
  }

  void OnMetadataReady(
      network::ResourceRequest request,
      data_decoder::mojom::BundleMetadataParseErrorPtr metadata_error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (metadata_error) {
      AddMetadataParseErrorMessageToConsole(frame_tree_node_id_,
                                            metadata_error);
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }
    DCHECK(reader_);
    primary_url_ = reader_->GetPrimaryURL();
    url_loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
        std::move(reader_), frame_tree_node_id_);

    const GURL new_url =
        bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
            request.url, primary_url_);
    auto redirect_loader =
        std::make_unique<PrimaryURLRedirectLoader>(forwarding_client_.Unbind());
    redirect_loader->OnReadyToRedirect(request, new_url);
  }

  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    network::ResourceRequest new_resource_request = resource_request;
    new_resource_request.url = primary_url_;
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        new_resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_).Run(primary_url_, std::move(url_loader_factory_));
  }

  DoneCallback done_callback_;
  const int frame_tree_node_id_;
  scoped_refptr<BundledExchangesReader> reader_;
  GURL primary_url_;
  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForFile> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForFile);
};

// A class to inherit NavigationLoaderInterceptor for a navigation to a
// trustable BundledExchanges file (eg: "file:///tmp/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for the navigation request to the trustable
//     BundledExchanges file. It continues on CreateURLLoader() to create the
//     loader for the main resource.
//     - If OnMetadataReady() has not been called yet:
//         Wait for OnMetadataReady() to be called.
//     - If OnMetadataReady() was called with an error:
//         Completes the request with ERR_INVALID_BUNDLED_EXCHANGES.
//     - If OnMetadataReady() was called whthout errors:
//         A redirect loader is created to redirect the navigation request to
//         the Bundle's primary URL ("https://example.com/a.html").
// [2] MaybeCreateLoader() is called again for the redirect. It continues on
//     CreateURLLoader() to create the loader for the main resource.
class InterceptorForTrustableFile final : public NavigationLoaderInterceptor {
 public:
  InterceptorForTrustableFile(std::unique_ptr<BundledExchangesSource> source,
                              DoneCallback done_callback,
                              int frame_tree_node_id)
      : source_(std::move(source)),
        reader_(base::MakeRefCounted<BundledExchangesReader>(source_->Clone())),
        done_callback_(std::move(done_callback)),
        frame_tree_node_id_(frame_tree_node_id) {
    reader_->ReadMetadata(
        base::BindOnce(&InterceptorForTrustableFile::OnMetadataReady,
                       weak_factory_.GetWeakPtr()));
  }
  ~InterceptorForTrustableFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(
        base::BindOnce(&InterceptorForTrustableFile::CreateURLLoader,
                       weak_factory_.GetWeakPtr())));
  }

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (metadata_error_) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }

    if (!url_loader_factory_) {
      // This must be the first request to the bundled exchange file.
      DCHECK_EQ(source_->url(), resource_request.url);
      pending_resource_request_ = resource_request;
      pending_receiver_ = std::move(receiver);
      pending_client_ = std::move(client);
      return;
    }

    // Currently |source_| must be a local file. And the bundle's primary URL
    // can't be a local file URL. So while handling redirected request to the
    // primary URL, |resource_request.url| must not be same as the |source_|'s
    // URL.
    if (source_->url() != resource_request.url) {
      url_loader_factory_->CreateLoaderAndStart(
          std::move(receiver), /*routing_id=*/0, /*request_id=*/0,
          /*options=*/0, resource_request, std::move(client),
          net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
      std::move(done_callback_)
          .Run(resource_request.url, std::move(url_loader_factory_));
      return;
    }

    auto redirect_loader =
        std::make_unique<PrimaryURLRedirectLoader>(std::move(client));
    redirect_loader->OnReadyToRedirect(resource_request, primary_url_);
    mojo::MakeSelfOwnedReceiver(
        std::move(redirect_loader),
        mojo::PendingReceiver<network::mojom::URLLoader>(std::move(receiver)));
  }

  void OnMetadataReady(data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!url_loader_factory_);

    if (error) {
      AddMetadataParseErrorMessageToConsole(frame_tree_node_id_, error);
      metadata_error_ = std::move(error);
    } else {
      primary_url_ = reader_->GetPrimaryURL();
      url_loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
          std::move(reader_), frame_tree_node_id_);
    }

    if (pending_receiver_) {
      DCHECK(pending_client_);
      CreateURLLoader(pending_resource_request_, std::move(pending_receiver_),
                      std::move(pending_client_));
    }
  }

  std::unique_ptr<BundledExchangesSource> source_;
  scoped_refptr<BundledExchangesReader> reader_;
  DoneCallback done_callback_;
  const int frame_tree_node_id_;

  network::ResourceRequest pending_resource_request_;
  mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client_;

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  GURL primary_url_;
  data_decoder::mojom::BundleMetadataParseErrorPtr metadata_error_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForTrustableFile> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForTrustableFile);
};

// A class to inherit NavigationLoaderInterceptor for a navigation to a
// BundledExchanges file on HTTPS server (eg: "https://example.com/a.wbn").
// The overridden methods of NavigationLoaderInterceptor are called in the
// following sequence:
// [1] MaybeCreateLoader() is called for all navigation requests. It calls the
//     |callback| with a null RequestHandler.
// [2] MaybeCreateLoaderForResponse() is called for all navigation responses.
//     - If the response mime type is not "application/webbundle", or attachment
//       Content-Disposition header is set, returns false.
//     - If the URL isn't HTTPS nor localhost HTTP, or the Content-Length header
//       is not a positive value, completes the requests with
//       ERR_INVALID_BUNDLED_EXCHANGES and returns true.
//     - Otherwise starts reading the metadata and returns true. Once the
//       metadata is read, OnMetadataReady() is called, and a redirect loader is
//       created to redirect the navigation request to the Bundle's primary URL
//       ("https://example.com/a.html").
// [3] MaybeCreateLoader() is called again for the redirect. It continues on
//     StartResponse() to create the loader for the main resource.
class InterceptorForNetwork final : public NavigationLoaderInterceptor {
 public:
  InterceptorForNetwork(DoneCallback done_callback,
                        BrowserContext* browser_context,
                        int frame_tree_node_id)
      : done_callback_(std::move(done_callback)),
        browser_context_(browser_context),
        frame_tree_node_id_(frame_tree_node_id) {
    DCHECK(browser_context_);
  }
  ~InterceptorForNetwork() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(
      const network::ResourceRequest& tentative_resource_request,
      BrowserContext* browser_context,
      LoaderCallback callback,
      FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!reader_) {
      std::move(callback).Run({});
      return;
    }
    std::move(callback).Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(
        base::BindOnce(&InterceptorForNetwork::StartResponse,
                       weak_factory_.GetWeakPtr())));
  }

  bool MaybeCreateLoaderForResponse(
      const network::ResourceRequest& request,
      const network::ResourceResponseHead& response_head,
      mojo::ScopedDataPipeConsumerHandle* response_body,
      network::mojom::URLLoaderPtr* loader,
      mojo::PendingReceiver<network::mojom::URLLoaderClient>* client_receiver,
      blink::ThrottlingURLLoader* url_loader,
      bool* skip_other_interceptors,
      bool* will_return_and_handle_unsafe_redirect) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (response_head.mime_type !=
        bundled_exchanges_utils::
            kBundledExchangesFileMimeTypeWithoutParameters) {
      return false;
    }
    if (download_utils::MustDownload(request.url, response_head.headers.get(),
                                     response_head.mime_type)) {
      return false;
    }

    auto source =
        BundledExchangesSource::MaybeCreateFromNetworkUrl(request.url);
    if (!source) {
      AddErrorMessageToConsole(
          frame_tree_node_id_,
          "Web Bundle response must be served from HTTPS or localhost HTTP.");
      *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return true;
    }

    if (response_head.content_length <= 0) {
      AddErrorMessageToConsole(
          frame_tree_node_id_,
          "Web Bundle response must have valid Content-Length header.");
      *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return true;
    }

    // TODO(crbug.com/1018640): Check the special HTTP response header if we
    // decided to require one for WBN navigation.

    reader_ = base::MakeRefCounted<BundledExchangesReader>(
        std::move(source), response_head.content_length,
        std::move(*response_body), url_loader->Unbind(),
        BrowserContext::GetBlobStorageContext(browser_context_));
    reader_->ReadMetadata(
        base::BindOnce(&InterceptorForNetwork::OnMetadataReady,
                       weak_factory_.GetWeakPtr(), request));
    *client_receiver = forwarding_client_.BindNewPipeAndPassReceiver();
    return true;
  }

  void OnMetadataReady(network::ResourceRequest request,
                       data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    if (error) {
      AddMetadataParseErrorMessageToConsole(frame_tree_node_id_, error);
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }
    primary_url_ = reader_->GetPrimaryURL();
    if (!reader_->HasEntry(primary_url_)) {
      AddErrorMessageToConsole(
          frame_tree_node_id_,
          "The primary URL resource is not found in the web bundle.");
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }

    // TODO(crbug.com/1018640): Check the scope to see if it's allowed to load
    // the URL.
    if (primary_url_.GetOrigin() != reader_->source().url().GetOrigin()) {
      AddErrorMessageToConsole(frame_tree_node_id_,
                               "The origin of primary URL doesn't match with "
                               "the origin of the web bundle.");
      std::move(forwarding_client_)
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }
    url_loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
        reader_, frame_tree_node_id_);
    auto redirect_loader =
        std::make_unique<PrimaryURLRedirectLoader>(forwarding_client_.Unbind());
    redirect_loader->OnReadyToRedirect(request, primary_url_);
  }

  void StartResponse(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    network::ResourceRequest new_resource_request = resource_request;
    new_resource_request.url = primary_url_;
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), 0, 0, 0, new_resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_).Run(primary_url_, std::move(url_loader_factory_));
  }

  DoneCallback done_callback_;
  BrowserContext* browser_context_;
  const int frame_tree_node_id_;
  scoped_refptr<BundledExchangesReader> reader_;
  GURL primary_url_;
  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;

  mojo::Remote<network::mojom::URLLoaderClient> forwarding_client_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForNetwork> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForNetwork);
};

// A class to inherit NavigationLoaderInterceptor for a navigation within a
// trustable BundledExchanges file.
// For example:
//   A user opened "file:///tmp/a.wbn", and InterceptorForTrustableFile
//   redirected to "https://example.com/a.html" and "a.html" in "a.wbn" was
//   loaded. And the user clicked a link to "https://example.com/b.html" from
//   "a.html".
// In this case, this interceptor intecepts the navigation request to "b.html",
// and creates a URLLoader using the BundledExchangesURLLoaderFactory to load
// the response of "b.html" in "a.wbn".
class InterceptorForTrackedNavigationFromTrustableFile final
    : public NavigationLoaderInterceptor {
 public:
  InterceptorForTrackedNavigationFromTrustableFile(
      scoped_refptr<BundledExchangesReader> reader,
      DoneCallback done_callback,
      int frame_tree_node_id)
      : url_loader_factory_(std::make_unique<BundledExchangesURLLoaderFactory>(
            std::move(reader),
            frame_tree_node_id)),
        done_callback_(std::move(done_callback)) {}
  ~InterceptorForTrackedNavigationFromTrustableFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(url_loader_factory_->reader()->HasEntry(resource_request.url));
    std::move(callback).Run(
        base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
            &InterceptorForTrackedNavigationFromTrustableFile::CreateURLLoader,
            weak_factory_.GetWeakPtr())));
  }

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_)
        .Run(resource_request.url, std::move(url_loader_factory_));
  }

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;
  DoneCallback done_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForTrackedNavigationFromTrustableFile>
      weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForTrackedNavigationFromTrustableFile);
};

// A class to inherit NavigationLoaderInterceptor for a navigation within a
// BundledExchanges file.
// For example:
//   A user opened "file:///tmp/a.wbn", and InterceptorForFile redirected to
//   "file:///tmp/a.wbn?https://example.com/a.html" and "a.html" in "a.wbn" was
//   loaded. And the user clicked a link to "https://example.com/b.html" from
//   "a.html".
// In this case, this interceptor intecepts the navigation request to "b.html",
// and redirect the navigation request to
// "file:///tmp/a.wbn?https://example.com/b.html" and creates a URLLoader using
// the BundledExchangesURLLoaderFactory to load the response of "b.html" in
// "a.wbn".
class InterceptorForTrackedNavigationFromFile final
    : public NavigationLoaderInterceptor {
 public:
  InterceptorForTrackedNavigationFromFile(
      scoped_refptr<BundledExchangesReader> reader,
      DoneCallback done_callback,
      int frame_tree_node_id)
      : url_loader_factory_(std::make_unique<BundledExchangesURLLoaderFactory>(
            std::move(reader),
            frame_tree_node_id)),
        done_callback_(std::move(done_callback)) {}
  ~InterceptorForTrackedNavigationFromFile() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(
        base::MakeRefCounted<SingleRequestURLLoaderFactory>(base::BindOnce(
            &InterceptorForTrackedNavigationFromFile::CreateURLLoader,
            weak_factory_.GetWeakPtr())));
  }

  bool ShouldBypassRedirectChecks() override { return true; }

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!is_redirected_) {
      DCHECK(url_loader_factory_->reader()->HasEntry(resource_request.url));
      is_redirected_ = true;
      original_request_url_ = resource_request.url;

      GURL bundled_exchanges_url =
          url_loader_factory_->reader()->source().url();
      const GURL new_url =
          bundled_exchanges_utils::GetSynthesizedUrlForBundledExchanges(
              bundled_exchanges_url, original_request_url_);
      auto redirect_loader =
          std::make_unique<PrimaryURLRedirectLoader>(std::move(client));
      redirect_loader->OnReadyToRedirect(resource_request, new_url);
      mojo::MakeSelfOwnedReceiver(
          std::move(redirect_loader),
          mojo::PendingReceiver<network::mojom::URLLoader>(
              std::move(receiver)));
      return;
    }
    network::ResourceRequest new_resource_request = resource_request;
    new_resource_request.url = original_request_url_;
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        new_resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_)
        .Run(original_request_url_, std::move(url_loader_factory_));
  }

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;
  DoneCallback done_callback_;

  bool is_redirected_ = false;
  GURL original_request_url_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForTrackedNavigationFromFile> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForTrackedNavigationFromFile);
};

// A class to inherit NavigationLoaderInterceptor for the history navigation to
// a BundledExchanges file.
// - MaybeCreateLoader() is called for the history navigation request. It
//   continues on CreateURLLoader() to create the loader for the main resource.
//   - If OnMetadataReady() has not been called yet:
//       Wait for OnMetadataReady() to be called.
//   - If OnMetadataReady() was called with an error:
//       Completes the request with ERR_INVALID_BUNDLED_EXCHANGES.
//   - If OnMetadataReady() was called whthout errors:
//       Creates the loader for the main resource.
class InterceptorForNavigationInfo final : public NavigationLoaderInterceptor {
 public:
  InterceptorForNavigationInfo(
      std::unique_ptr<BundledExchangesNavigationInfo> navigation_info,
      DoneCallback done_callback,
      int frame_tree_node_id)
      : reader_(base::MakeRefCounted<BundledExchangesReader>(
            navigation_info->source().Clone())),
        target_inner_url_(navigation_info->target_inner_url()),
        done_callback_(std::move(done_callback)),
        frame_tree_node_id_(frame_tree_node_id) {
    reader_->ReadMetadata(
        base::BindOnce(&InterceptorForNavigationInfo::OnMetadataReady,
                       weak_factory_.GetWeakPtr()));
  }
  ~InterceptorForNavigationInfo() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

 private:
  // NavigationLoaderInterceptor implementation
  void MaybeCreateLoader(const network::ResourceRequest& resource_request,
                         BrowserContext* browser_context,
                         LoaderCallback callback,
                         FallbackCallback fallback_callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::move(callback).Run(base::MakeRefCounted<SingleRequestURLLoaderFactory>(
        base::BindOnce(&InterceptorForNavigationInfo::CreateURLLoader,
                       weak_factory_.GetWeakPtr())));
  }

  void CreateURLLoader(
      const network::ResourceRequest& resource_request,
      mojo::PendingReceiver<network::mojom::URLLoader> receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (metadata_error_) {
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(client))
          ->OnComplete(network::URLLoaderCompletionStatus(
              net::ERR_INVALID_BUNDLED_EXCHANGES));
      return;
    }

    if (!url_loader_factory_) {
      pending_resource_request_ = resource_request;
      pending_receiver_ = std::move(receiver);
      pending_client_ = std::move(client);
      return;
    }

    network::ResourceRequest new_resource_request = resource_request;
    new_resource_request.url = target_inner_url_;
    url_loader_factory_->CreateLoaderAndStart(
        std::move(receiver), /*routing_id=*/0, /*request_id=*/0, /*options=*/0,
        new_resource_request, std::move(client),
        net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation));
    std::move(done_callback_)
        .Run(target_inner_url_, std::move(url_loader_factory_));
  }

  void OnMetadataReady(data_decoder::mojom::BundleMetadataParseErrorPtr error) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!url_loader_factory_);

    if (error) {
      AddMetadataParseErrorMessageToConsole(frame_tree_node_id_, error);
      metadata_error_ = std::move(error);
    } else {
      url_loader_factory_ = std::make_unique<BundledExchangesURLLoaderFactory>(
          std::move(reader_), frame_tree_node_id_);
    }

    if (pending_receiver_) {
      DCHECK(pending_client_);
      CreateURLLoader(pending_resource_request_, std::move(pending_receiver_),
                      std::move(pending_client_));
    }
  }

  scoped_refptr<BundledExchangesReader> reader_;
  const GURL target_inner_url_;
  DoneCallback done_callback_;
  const int frame_tree_node_id_;

  network::ResourceRequest pending_resource_request_;
  mojo::PendingReceiver<network::mojom::URLLoader> pending_receiver_;
  mojo::PendingRemote<network::mojom::URLLoaderClient> pending_client_;

  std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory_;
  data_decoder::mojom::BundleMetadataParseErrorPtr metadata_error_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterceptorForNavigationInfo> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(InterceptorForNavigationInfo);
};

}  // namespace

// static
std::unique_ptr<BundledExchangesHandle> BundledExchangesHandle::CreateForFile(
    int frame_tree_node_id) {
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForFile>(
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::CreateForTrustableFile(
    std::unique_ptr<BundledExchangesSource> source,
    int frame_tree_node_id) {
  DCHECK(source->is_trusted_file());
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForTrustableFile>(
      std::move(source),
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::CreateForNetwork(BrowserContext* browser_context,
                                         int frame_tree_node_id) {
  DCHECK(base::FeatureList::IsEnabled(features::kWebBundlesFromNetwork));
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForNetwork>(
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      browser_context, frame_tree_node_id));
  return handle;
}

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::CreateForTrackedNavigation(
    scoped_refptr<BundledExchangesReader> reader,
    int frame_tree_node_id) {
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  switch (reader->source().type()) {
    case BundledExchangesSource::Type::kTrustedFile:
      handle->SetInterceptor(
          std::make_unique<InterceptorForTrackedNavigationFromTrustableFile>(
              std::move(reader),
              base::BindOnce(
                  &BundledExchangesHandle::OnBundledExchangesFileLoaded,
                  handle->weak_factory_.GetWeakPtr()),
              frame_tree_node_id));
      break;
    case BundledExchangesSource::Type::kFile:
      handle->SetInterceptor(
          std::make_unique<InterceptorForTrackedNavigationFromFile>(
              std::move(reader),
              base::BindOnce(
                  &BundledExchangesHandle::OnBundledExchangesFileLoaded,
                  handle->weak_factory_.GetWeakPtr()),
              frame_tree_node_id));
      break;
    case BundledExchangesSource::Type::kNetwork:
      // Currently navigation within web bundles from network is not supported.
      // TODO(crbug.com/1018640): Implement this.
      NOTREACHED();
      break;
  }
  return handle;
}

// static
std::unique_ptr<BundledExchangesHandle>
BundledExchangesHandle::MaybeCreateForNavigationInfo(
    std::unique_ptr<BundledExchangesNavigationInfo> navigation_info,
    int frame_tree_node_id) {
  // Currently history navigation is not supported for web bundles from network.
  // TODO(crbug.com/1018640): Implement this.
  if (navigation_info->source().is_network())
    return nullptr;
  auto handle = base::WrapUnique(new BundledExchangesHandle());
  handle->SetInterceptor(std::make_unique<InterceptorForNavigationInfo>(
      std::move(navigation_info),
      base::BindOnce(&BundledExchangesHandle::OnBundledExchangesFileLoaded,
                     handle->weak_factory_.GetWeakPtr()),
      frame_tree_node_id));
  return handle;
}

BundledExchangesHandle::BundledExchangesHandle() = default;

BundledExchangesHandle::~BundledExchangesHandle() = default;

std::unique_ptr<NavigationLoaderInterceptor>
BundledExchangesHandle::TakeInterceptor() {
  DCHECK(interceptor_);
  return std::move(interceptor_);
}

void BundledExchangesHandle::CreateURLLoaderFactory(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver,
    mojo::Remote<network::mojom::URLLoaderFactory> fallback_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url_loader_factory_);

  url_loader_factory_->SetFallbackFactory(std::move(fallback_factory));
  url_loader_factory_->Clone(std::move(receiver));
}

std::unique_ptr<BundledExchangesHandleTracker>
BundledExchangesHandle::MaybeCreateTracker() {
  if (!url_loader_factory_)
    return nullptr;
  return std::make_unique<BundledExchangesHandleTracker>(
      url_loader_factory_->reader(), navigation_info_->target_inner_url());
}

bool BundledExchangesHandle::IsReadyForLoading() {
  return !!url_loader_factory_;
}

void BundledExchangesHandle::SetInterceptor(
    std::unique_ptr<NavigationLoaderInterceptor> interceptor) {
  interceptor_ = std::move(interceptor);
}

void BundledExchangesHandle::OnBundledExchangesFileLoaded(
    const GURL& target_inner_url,
    std::unique_ptr<BundledExchangesURLLoaderFactory> url_loader_factory) {
  auto source = url_loader_factory->reader()->source().Clone();
  if (source->is_file())
    base_url_override_ = target_inner_url;
  navigation_info_ = std::make_unique<BundledExchangesNavigationInfo>(
      std::move(source), target_inner_url);
  url_loader_factory_ = std::move(url_loader_factory);
}

}  // namespace content
