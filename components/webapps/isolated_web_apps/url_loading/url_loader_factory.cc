// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/url_loading/url_loader_factory.h"

#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "components/web_package/mojom/web_bundle_parser.mojom.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/web_package/web_bundle_utils.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/reading/response_reader.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry.h"
#include "components/webapps/isolated_web_apps/reading/response_reader_registry_factory.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "components/webapps/isolated_web_apps/types/storage_location.h"
#include "components/webapps/isolated_web_apps/types/url_loading_types.h"
#include "components/webapps/isolated_web_apps/url_loading/url_loader.h"
#include "components/webapps/isolated_web_apps/url_loading/utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/self_deleting_url_loader_factory.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

constexpr char kIsolatedAppCspTemplate[] =
    "base-uri 'none';"
    "default-src 'self';"
    "object-src 'none';"
    "frame-src 'self' https: blob: data:;"
    "connect-src 'self' https: wss: blob: data:%s;"
    "script-src 'self' 'wasm-unsafe-eval';"
    "img-src 'self' https: blob: data:;"
    "media-src 'self' https: blob: data:;"
    "font-src 'self' blob: data:;"
    "style-src 'self' 'unsafe-inline';"
    "require-trusted-types-for 'script';"
    "frame-ancestors 'self';";

bool IsSupportedHttpMethod(const std::string& method) {
  return method == net::HttpRequestHeaders::kGetMethod ||
         method == net::HttpRequestHeaders::kHeadMethod;
}

const std::string& GetDefaultCsp() {
  static const base::NoDestructor<std::string> default_csp(
      [] { return base::StringPrintf(kIsolatedAppCspTemplate, ""); }());
  return *default_csp;
}

std::optional<std::string> ComputeCspOverride(const IwaSourceWithMode& source) {
  auto* proxy_source = std::get_if<IwaSourceProxy>(&source.variant());
  if (proxy_source && proxy_source->proxy_url().scheme() == "http") {
    url::Origin origin = proxy_source->proxy_url();
    std::string proxy_ws_url =
        base::StringPrintf(" ws://%s:%i", origin.host().c_str(), origin.port());
    return base::StringPrintf(kIsolatedAppCspTemplate, proxy_ws_url.c_str());
  }
  return std::nullopt;
}

class ForwardingURLLoaderClient : public network::mojom::URLLoaderClient {
 public:
  explicit ForwardingURLLoaderClient(
      mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client)
      : url_loader_client_(std::move(url_loader_client)) {}

  network::mojom::URLLoaderClient* url_loader_client() {
    CHECK(url_loader_client_.is_bound());
    return url_loader_client_.get();
  }

 private:
  // `network::mojom::URLLoaderClient`:
  void OnReceiveEarlyHints(network::mojom::EarlyHintsPtr early_hints) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveEarlyHints(std::move(early_hints));
  }

  void OnReceiveRedirect(
      const net::RedirectInfo& redirect_info,
      network::mojom::URLResponseHeadPtr response_head) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnReceiveRedirect(redirect_info,
                                          std::move(response_head));
  }

  void OnUploadProgress(int64_t current_position,
                        int64_t total_size,
                        OnUploadProgressCallback ack_callback) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnUploadProgress(current_position, total_size,
                                         std::move(ack_callback));
  }

  void OnTransferSizeUpdated(int32_t transfer_size_diff) override {
    DCHECK(url_loader_client_.is_bound());
    url_loader_client_->OnTransferSizeUpdated(transfer_size_diff);
  }

  mojo::Remote<network::mojom::URLLoaderClient> url_loader_client_;
};

// This URL loader client parses the headers propagated from the underlying
// layer. This is necessary for service worker loading (which relies on
// `response_head->parsed_headers` for deducing cross-origin isolation
// correctly) and implies an additional hop via the network service.
class HeaderParsingURLLoaderClient : public ForwardingURLLoaderClient {
 public:
  HeaderParsingURLLoaderClient(
      const GURL& url,
      mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client)
      : ForwardingURLLoaderClient(std::move(url_loader_client)), url_(url) {}

 private:
  // network::mojom::URLLoaderClient:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    defer_complete_ = true;

    auto headers = response_head->headers;
    content::GetNetworkService()->ParseHeaders(
        url_, std::move(headers),
        base::BindOnce(&HeaderParsingURLLoaderClient::RespondWithParsedHeaders,
                       weak_factory_.GetWeakPtr(), std::move(response_head),
                       std::move(body), std::move(cached_metadata)));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    if (defer_complete_) {
      deferred_completion_status_ = status;
      return;
    }
    url_loader_client()->OnComplete(status);
  }

  void RespondWithParsedHeaders(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata,
      network::mojom::ParsedHeadersPtr parsed_headers) {
    response_head->parsed_headers = std::move(parsed_headers);
    url_loader_client()->OnReceiveResponse(
        std::move(response_head), std::move(body), std::move(cached_metadata));

    defer_complete_ = false;
    if (deferred_completion_status_) {
      url_loader_client()->OnComplete(std::move(*deferred_completion_status_));
    }
  }

  const GURL url_;

  // Set to true if the response has been received but not yet propagated to the
  // next layer due to in-progress header parsing.
  bool defer_complete_ = false;
  std::optional<network::URLLoaderCompletionStatus> deferred_completion_status_;

  base::WeakPtrFactory<HeaderParsingURLLoaderClient> weak_factory_{this};
};

class HeaderInjectionURLLoaderClient : public ForwardingURLLoaderClient {
 public:
  explicit HeaderInjectionURLLoaderClient(
      mojo::PendingRemote<network::mojom::URLLoaderClient> url_loader_client)
      : ForwardingURLLoaderClient(std::move(url_loader_client)) {}

  void set_csp_override(std::string_view csp_override) {
    csp_override_ = csp_override;
  }

  base::WeakPtr<HeaderInjectionURLLoaderClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void OnReceiveResponse(
      network::mojom::URLResponseHeadPtr response_head,
      mojo::ScopedDataPipeConsumerHandle body,
      std::optional<mojo_base::BigBuffer> cached_metadata) override {
    scoped_refptr<net::HttpResponseHeaders> headers = response_head->headers;
    size_t original_size = headers->raw_headers().size();

    std::string csp_header =
        csp_override_.has_value() ? csp_override_.value() : GetDefaultCsp();

    // Apps could specify a more restrictive CSP than what we enforce, which
    // we don't want to overwrite. We add our CSP here so that existing CSPs
    // will still be enforced. Existing CO*P headers are replaced.
    headers->AddHeader("Content-Security-Policy", csp_header);
    headers->SetHeader("Cross-Origin-Opener-Policy", "same-origin");
    headers->SetHeader("Cross-Origin-Embedder-Policy", "require-corp");
    headers->SetHeader("Cross-Origin-Resource-Policy", "same-origin");

    header_size_delta_ = headers->raw_headers().size() - original_size;

    // The Network Service will have already parsed the headers for
    // proxy-based IWAs, and navigation code will try to reuse the already
    // parsed headers if they're available. However, we're modifying the
    // headers so we want them to be re-parsed. This re-parsing requires an
    // additional round-trip to the Network Service.
    response_head->parsed_headers = nullptr;

    url_loader_client()->OnReceiveResponse(
        std::move(response_head), std::move(body), std::move(cached_metadata));
  }

  void OnComplete(const network::URLLoaderCompletionStatus& status) override {
    network::URLLoaderCompletionStatus adjusted_status = status;
    adjusted_status.encoded_data_length += header_size_delta_;
    url_loader_client()->OnComplete(adjusted_status);
  }

  std::optional<std::string> csp_override_ = std::nullopt;
  int header_size_delta_ = 0;

  base::WeakPtrFactory<HeaderInjectionURLLoaderClient> weak_factory_{this};
};

class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "IsolatedWebAppURLLoaderShutdownNotifierFactory") {
    DependsOn(IsolatedWebAppReaderRegistryFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override = default;

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return content::AreIsolatedWebAppsEnabled(context) ? context : nullptr;
  }
};

class IsolatedWebAppURLLoaderFactoryImpl
    : public network::SelfDeletingURLLoaderFactory {
 public:
  IsolatedWebAppURLLoaderFactoryImpl(
      content::BrowserContext* browser_context,
      std::optional<url::Origin> app_origin,
      std::optional<content::FrameTreeNodeId> frame_tree_node_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory> factory_receiver)
      : network::SelfDeletingURLLoaderFactory(std::move(factory_receiver)),
        browser_context_(content::AreIsolatedWebAppsEnabled(browser_context)
                             ? browser_context
                             : nullptr),
        app_origin_(std::move(app_origin)),
        frame_tree_node_id_(frame_tree_node_id) {
    CHECK(!app_origin_.has_value() ||
          app_origin_->scheme() == webapps::kIsolatedAppScheme);
    // TODO(crbug.com/432676258): Do not create the factory for inelibigle
    // contexts at all.
    if (browser_context_) {
      browser_context_shutdown_subscription_ =
          ShutdownNotifierFactory::GetInstance()
              ->Get(browser_context_)
              ->Subscribe(base::BindOnce(&IsolatedWebAppURLLoaderFactoryImpl::
                                             DisconnectReceiversAndDestroy,
                                         base::Unretained(this)));
    }
  }

  IsolatedWebAppURLLoaderFactoryImpl(
      const IsolatedWebAppURLLoaderFactoryImpl&) = delete;
  IsolatedWebAppURLLoaderFactoryImpl& operator=(
      const IsolatedWebAppURLLoaderFactoryImpl&) = delete;

 private:
  void LogErrorAndFail(
      const std::string& error_message,
      mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
    ::web_app::LogErrorAndFail(error_message, frame_tree_node_id_,
                               std::move(client));
  }

  // network::mojom::URLLoaderFactory:
  ~IsolatedWebAppURLLoaderFactoryImpl() override = default;

  void CreateLoaderAndStart(
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      int32_t request_id,
      uint32_t options,
      const network::ResourceRequest& resource_request,
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          original_loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
      override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    CHECK(resource_request.url.SchemeIs(webapps::kIsolatedAppScheme));
    DCHECK(resource_request.url.IsStandard());

    if (resource_request.headers.GetHeader("Service-Worker") == "script") {
      // Service worker script loading expects parsed headers in the response,
      // whereas `HeaderInjectionURLLoaderClient` explicitly sets them to
      // nullptr. This is fine for navigation requests which re-parse them at a
      // higher layer, but for SW having `response_head->parsed_headers` be
      // present is a must to deduce cross-origin isolation correctly.
      mojo::PendingRemote<network::mojom::URLLoaderClient>
          forwarding_loader_client;

      auto receiving_end =
          forwarding_loader_client.InitWithNewPipeAndPassReceiver();
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<HeaderParsingURLLoaderClient>(
              resource_request.url,
              std::exchange(original_loader_client,
                            std::move(forwarding_loader_client))),
          std::move(receiving_end));

      // Now calls to `original_loader_client` will be routed through the new
      // `HeaderParsingURLLoaderClient`.
    }

    auto header_injection_client =
        std::make_unique<HeaderInjectionURLLoaderClient>(
            std::move(original_loader_client));
    base::WeakPtr<HeaderInjectionURLLoaderClient> weak_header_injection_client =
        header_injection_client->GetWeakPtr();
    mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client;
    mojo::MakeSelfOwnedReceiver(std::move(header_injection_client),
                                loader_client.InitWithNewPipeAndPassReceiver());

    if (!CanRequestUrl(resource_request.url)) {
      network::URLLoaderCompletionStatus status(net::ERR_BLOCKED_BY_CLIENT);
      mojo::Remote<network::mojom::URLLoaderClient>(std::move(loader_client))
          ->OnComplete(status);
      return;
    }

    if (!browser_context_) {
      // This field is unset if IWAs are not enabled.
      LogErrorAndFail("Isolated Web Apps are not available for this profile.",
                      std::move(loader_client));
      return;
    }

    ASSIGN_OR_RETURN(web_package::SignedWebBundleId web_bundle_id,
                     IwaOrigin::Create(resource_request.url)
                         .transform([](const auto& iwa_origin) {
                           return iwa_origin.web_bundle_id();
                         }),
                     [&](const std::string& error) {
                       LogErrorAndFail(std::move(error),
                                       std::move(loader_client));
                     });

    IwaClient::GetInstance()->GetIwaSourceForRequest(
        browser_context_, web_bundle_id, resource_request, frame_tree_node_id_,
        base::BindOnce(&IsolatedWebAppURLLoaderFactoryImpl::HandleRequest,
                       weak_factory_.GetWeakPtr(), resource_request,
                       web_bundle_id, std::move(loader_receiver),
                       std::move(loader_client), traffic_annotation,
                       weak_header_injection_client));
  }

  void HandleRequest(
      const network::ResourceRequest& resource_request,
      const web_package::SignedWebBundleId& web_bundle_id,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
      base::WeakPtr<HeaderInjectionURLLoaderClient>
          weak_header_injection_client,
      base::expected<IwaSourceWithModeOrGeneratedResponse, std::string>
          result) {
    ASSIGN_OR_RETURN(IwaSourceWithModeOrGeneratedResponse source_or_response,
                     std::move(result), [&](const std::string& error) {
                       LogErrorAndFail(error, std::move(loader_client));
                     });

    if (!IsSupportedHttpMethod(resource_request.method)) {
      CompleteWithGeneratedResponse(
          mojo::Remote<network::mojom::URLLoaderClient>(
              std::move(loader_client)),
          net::HTTP_METHOD_NOT_ALLOWED);
      return;
    }

    std::visit(
        absl::Overload{
            [&](const GeneratedResponse& generated_response) {
              CompleteWithGeneratedResponse(
                  mojo::Remote<network::mojom::URLLoaderClient>(
                      std::move(loader_client)),
                  net::HTTP_OK, generated_response.response_body);
            },
            [&](const IwaSourceWithMode& source) {
              if (weak_header_injection_client) {
                if (auto csp_override = ComputeCspOverride(source)) {
                  weak_header_injection_client->set_csp_override(*csp_override);
                }
              }
              HandleRequestFromSource(resource_request, web_bundle_id, source,
                                      std::move(loader_receiver),
                                      std::move(loader_client),
                                      traffic_annotation);
            }},
        source_or_response);
  }

  void HandleRequestFromSource(
      const network::ResourceRequest& resource_request,
      const web_package::SignedWebBundleId& web_bundle_id,
      const IwaSourceWithMode& source,
      mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
      mojo::PendingRemote<network::mojom::URLLoaderClient> loader_client,
      const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
    std::visit(
        absl::Overload{[&](const IwaSourceBundleWithMode& bundle) {
                         CHECK(!web_bundle_id.is_for_proxy_mode());
                         IsolatedWebAppURLLoader::CreateAndStart(
                             browser_context_, bundle.path(), bundle.dev_mode(),
                             web_bundle_id, std::move(loader_receiver),
                             std::move(loader_client), resource_request,
                             frame_tree_node_id_);
                       },
                       [&](const IwaSourceProxy& proxy) {
                         CHECK(web_bundle_id.is_for_proxy_mode());
                         HandleProxy(browser_context_, web_bundle_id, proxy,
                                     std::move(loader_receiver),
                                     std::move(loader_client), resource_request,
                                     traffic_annotation, frame_tree_node_id_);
                       }},
        source.variant());
  }

  bool CanRequestUrl(const GURL& url) const {
    // If no origin was specified we should allow the request. This will be the
    // case for navigations and worker script/update loads.
    if (!app_origin_) {
      return true;
    }
    return app_origin_->IsSameOriginWith(url);
  }

  // These two fields will be null if `content::AreIsolatedWebAppsEnabled()` is
  // false.
  const raw_ptr<content::BrowserContext> browser_context_;
  base::CallbackListSubscription browser_context_shutdown_subscription_;

  const std::optional<url::Origin> app_origin_;
  const std::optional<content::FrameTreeNodeId> frame_tree_node_id_;
  base::WeakPtrFactory<IsolatedWebAppURLLoaderFactoryImpl> weak_factory_{this};
};

mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateInternal(
    content::BrowserContext* browser_context,
    std::optional<url::Origin> app_origin,
    std::optional<content::FrameTreeNodeId> frame_tree_node_id) {
  DCHECK(browser_context);
  DCHECK(!browser_context->ShutdownStarted());

  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_remote;

  // The IsolatedWebAppURLLoaderFactoryImpl will delete itself when there are no
  // more receivers - see the
  // network::SelfDeletingURLLoaderFactory::OnDisconnect method.
  new IsolatedWebAppURLLoaderFactoryImpl(
      browser_context, std::move(app_origin), frame_tree_node_id,
      pending_remote.InitWithNewPipeAndPassReceiver());

  return pending_remote;
}

}  // namespace

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::CreateForFrame(
    content::BrowserContext* browser_context,
    std::optional<url::Origin> app_origin,
    content::FrameTreeNodeId frame_tree_node_id) {
  return CreateInternal(browser_context, std::move(app_origin),
                        frame_tree_node_id);
}

// static
mojo::PendingRemote<network::mojom::URLLoaderFactory>
IsolatedWebAppURLLoaderFactory::Create(content::BrowserContext* browser_context,
                                       std::optional<url::Origin> app_origin) {
  return CreateInternal(browser_context, std::move(app_origin),
                        /*frame_tree_node_id=*/std::nullopt);
}

// static
void IsolatedWebAppURLLoaderFactory::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

}  // namespace web_app
