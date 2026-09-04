// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/unguessable_token.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/common/features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {
const void* const kPreloadActivationReportManagerKey =
    &kPreloadActivationReportManagerKey;
}  // namespace

// static
PreloadActivationReportManager*
PreloadActivationReportManager::GetOrCreateForBrowserContext(
    BrowserContext* browser_context) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(browser_context);

  auto* manager = static_cast<PreloadActivationReportManager*>(
      browser_context->GetUserData(kPreloadActivationReportManagerKey));
  if (manager) {
    return manager;
  }

  auto new_manager = base::WrapUnique(new PreloadActivationReportManager());
  manager = new_manager.get();
  browser_context->SetUserData(kPreloadActivationReportManagerKey,
                               std::move(new_manager));
  return manager;
}

PreloadActivationReportManager::LoaderInfo::LoaderInfo() = default;
PreloadActivationReportManager::LoaderInfo::~LoaderInfo() = default;
PreloadActivationReportManager::LoaderInfo::LoaderInfo(LoaderInfo&&) = default;
PreloadActivationReportManager::LoaderInfo&
PreloadActivationReportManager::LoaderInfo::operator=(LoaderInfo&&) = default;

PreloadActivationReportManager::PreloadActivationReportManager() = default;

PreloadActivationReportManager::~PreloadActivationReportManager() = default;

void PreloadActivationReportManager::ReportActivation(const GURL& endpoint,
                                                      RenderFrameHost* rfh) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  CHECK(rfh);

  url::Origin original_origin = url::Origin::Create(endpoint);

  std::string devtools_request_id = base::UnguessableToken::Create().ToString();

  // TODO(crbug.com/499814382): Audit if the other parameters of the request.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = endpoint;
  request->method = net::HttpRequestHeaders::kGetMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  request->devtools_request_id = devtools_request_id;
  request->resource_type = static_cast<int>(blink::mojom::ResourceType::kPing);
  if (base::FeatureList::IsEnabled(
          features::kPreloadActivationReportWithExtensionInterception)) {
    request->request_initiator = rfh->GetLastCommittedOrigin();
  }

  devtools_instrumentation::OnPrefetchActivationBeaconWillBeSent(
      rfh->GetFrameTreeNodeId(), devtools_request_id, *request);

  constexpr net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("preload_activation_beacon", R"(
          semantics {
            sender: "Preload Activation Beacon Manager"
            description:
              "Sends a beacon to notify the server that a prefetched or "
              "prerendered page was activated."
            trigger:
              "A prefetched or prerendered page is consumed during navigation."
            destination: WEBSITE
            data: "None."
            user_data {
              type: NONE
            }
            last_reviewed: "2026-05-13"
            internal {
              contacts {
                owners: "//content/browser/preloading/OWNERS"
              }
            }
          }
          policy {
            cookies_allowed: NO
            setting: "The traffic can be disabled by the preloading policy."
            chrome_policy {
              NetworkPredictionOptions {
                NetworkPredictionOptions: 2
              }
            }
          }
        )");

  auto loader =
      network::SimpleURLLoader::Create(std::move(request), traffic_annotation);

  auto* loader_ptr = loader.get();

  LoaderInfo loader_info;
  loader_info.loader = std::move(loader);
  loader_info.devtools_request_id = devtools_request_id;
  loader_info.frame_tree_node_id = rfh->GetFrameTreeNodeId();

  auto it = loaders_.insert(loaders_.end(), std::move(loader_info));

  loader_ptr->SetOnRedirectCallback(base::BindRepeating(
      [](base::WeakPtr<PreloadActivationReportManager> manager,
         UrlLoaderList::iterator it, const url::Origin& original_origin,
         const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* removed_headers) {
        if (manager) {
          manager->OnRedirect(it, original_origin, url_before_redirect,
                              redirect_info, response_head);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), it, original_origin));

  mojo::Remote<network::mojom::URLLoaderFactory> frame_factory;
  network::mojom::URLLoaderFactory* factory = nullptr;

  if (url_loader_factory_for_testing_) {
    factory = url_loader_factory_for_testing_.get();
  } else if (base::FeatureList::IsEnabled(
                 features::kPreloadActivationReportWithExtensionInterception)) {
    rfh->CreateNetworkServiceDefaultFactory(
        frame_factory.BindNewPipeAndPassReceiver());
    factory = frame_factory.get();
  } else {
    factory = rfh->GetStoragePartition()
                  ->GetURLLoaderFactoryForBrowserProcess()
                  .get();
  }

  loader_ptr->DownloadHeadersOnly(
      factory,
      base::BindOnce(
          [](base::WeakPtr<PreloadActivationReportManager> manager,
             UrlLoaderList::iterator it,
             mojo::Remote<network::mojom::URLLoaderFactory> keeper,
             scoped_refptr<net::HttpResponseHeaders> headers) {
            if (manager) {
              manager->OnComplete(it, headers);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), it, std::move(frame_factory)));
}

void PreloadActivationReportManager::OnRedirect(
    UrlLoaderList::iterator it,
    const url::Origin& original_origin,
    const GURL& url_before_redirect,
    const net::RedirectInfo& redirect_info,
    const network::mojom::URLResponseHead& response_head) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  // Enforce that the HTTP method must remain GET.
  if (redirect_info.new_method != net::HttpRequestHeaders::kGetMethod) {
    devtools_instrumentation::OnPrefetchActivationBeaconRequestComplete(
        it->frame_tree_node_id, it->devtools_request_id,
        network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    RemoveLoader(it);
    return;
  }

  if (!url::Origin::Create(redirect_info.new_url)
           .IsSameOriginWith(original_origin)) {
    devtools_instrumentation::OnPrefetchActivationBeaconRequestComplete(
        it->frame_tree_node_id, it->devtools_request_id,
        network::URLLoaderCompletionStatus(net::ERR_ABORTED));
    RemoveLoader(it);
    return;
  }

  network::ResourceRequest redirected_request;
  redirected_request.url = redirect_info.new_url;
  redirected_request.method = redirect_info.new_method;
  redirected_request.referrer = GURL(redirect_info.new_referrer);
  redirected_request.devtools_request_id = it->devtools_request_id;
  redirected_request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kPing);

  auto redirect_head_info = network::ExtractDevToolsInfo(response_head);
  std::pair<const GURL&, const network::mojom::URLResponseHeadDevToolsInfo&>
      redirect_info_for_devtools{url_before_redirect, *redirect_head_info};

  devtools_instrumentation::OnPrefetchActivationBeaconWillBeSent(
      it->frame_tree_node_id, it->devtools_request_id, redirected_request,
      redirect_info_for_devtools);
}

void PreloadActivationReportManager::OnComplete(
    UrlLoaderList::iterator it,
    scoped_refptr<net::HttpResponseHeaders> headers) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);

  if (it->loader->ResponseInfo()) {
    devtools_instrumentation::OnPrefetchActivationBeaconResponseReceived(
        it->frame_tree_node_id, it->devtools_request_id,
        it->loader->GetFinalURL(), *it->loader->ResponseInfo());
  }

  network::URLLoaderCompletionStatus status(it->loader->NetError());
  devtools_instrumentation::OnPrefetchActivationBeaconRequestComplete(
      it->frame_tree_node_id, it->devtools_request_id, status);

  RemoveLoader(it);
}

void PreloadActivationReportManager::RemoveLoader(UrlLoaderList::iterator it) {
  CHECK_CURRENTLY_ON(BrowserThread::UI, base::NotFatalUntil::M159);
  loaders_.erase(it);
}

}  // namespace content
