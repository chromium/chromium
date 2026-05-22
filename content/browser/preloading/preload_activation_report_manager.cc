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
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

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

  auto* storage_partition =
      web_contents->GetPrimaryMainFrame()->GetStoragePartition();

  auto* loader_ptr = loader.get();
  loader_ptr->DownloadHeadersOnly(
      storage_partition->GetURLLoaderFactoryForBrowserProcess().get(),
      base::BindOnce(
          [](std::unique_ptr<network::SimpleURLLoader> loader,
             scoped_refptr<net::HttpResponseHeaders> headers) {
            // The loader is destroyed here when it goes out of scope.
          },
          std::move(loader)));
}

}  // namespace content
