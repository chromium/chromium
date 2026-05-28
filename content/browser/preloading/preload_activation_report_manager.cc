// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/preload_activation_report_manager.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

PreloadActivationReportManager::PreloadActivationReportManager() = default;

PreloadActivationReportManager::~PreloadActivationReportManager() = default;

void PreloadActivationReportManager::ReportActivation(
    const GURL& endpoint,
    WebContents* web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK(web_contents);

  url::Origin original_origin = url::Origin::Create(endpoint);

  // TODO(crbug.com/499814382): Audit if the other parameters of the request.
  auto request = std::make_unique<network::ResourceRequest>();
  request->url = endpoint;
  request->method = net::HttpRequestHeaders::kHeadMethod;
  request->credentials_mode = network::mojom::CredentialsMode::kOmit;

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

  auto it = loaders_.insert(loaders_.end(), std::move(loader));

  loader_ptr->SetOnRedirectCallback(base::BindRepeating(
      [](base::WeakPtr<PreloadActivationReportManager> manager,
         UrlLoaderList::iterator it, const url::Origin& original_origin,
         const GURL& url_before_redirect,
         const net::RedirectInfo& redirect_info,
         const network::mojom::URLResponseHead& response_head,
         std::vector<std::string>* removed_headers) {
        if (manager) {
          manager->OnRedirect(it, original_origin, redirect_info);
        }
      },
      weak_ptr_factory_.GetWeakPtr(), it, original_origin));

  network::mojom::URLLoaderFactory* factory =
      url_loader_factory_for_testing_
          ? url_loader_factory_for_testing_.get()
          : web_contents->GetPrimaryMainFrame()
                ->GetStoragePartition()
                ->GetURLLoaderFactoryForBrowserProcess()
                .get();

  loader_ptr->DownloadHeadersOnly(
      factory, base::BindOnce(
                   [](base::WeakPtr<PreloadActivationReportManager> manager,
                      UrlLoaderList::iterator it,
                      scoped_refptr<net::HttpResponseHeaders> headers) {
                     if (manager) {
                       manager->OnComplete(it);
                     }
                   },
                   weak_ptr_factory_.GetWeakPtr(), it));
}

void PreloadActivationReportManager::OnRedirect(
    UrlLoaderList::iterator it,
    const url::Origin& original_origin,
    const net::RedirectInfo& redirect_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Enforce that the HTTP method must remain HEAD.
  if (redirect_info.new_method != net::HttpRequestHeaders::kHeadMethod) {
    RemoveLoader(it);
    return;
  }

  if (!url::Origin::Create(redirect_info.new_url)
           .IsSameOriginWith(original_origin)) {
    RemoveLoader(it);
  }
}

void PreloadActivationReportManager::OnComplete(UrlLoaderList::iterator it) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RemoveLoader(it);
}

void PreloadActivationReportManager::RemoveLoader(UrlLoaderList::iterator it) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  loaders_.erase(it);
}

}  // namespace content
