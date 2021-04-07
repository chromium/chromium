// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/navigation_early_hints_manager.h"

#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/url_loader_throttles.h"
#include "content/public/browser/web_contents.h"
#include "net/base/load_flags.h"
#include "net/base/schemeful_site.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/empty_url_loader_client.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/origin.h"

namespace content {

namespace {

const net::NetworkTrafficAnnotationTag kEarlyHintsPreloadTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("early_hints_preload",
                                        R"(
    semantics {
      sender: "Early Hints"
      description:
        "This request is issued during a main frame navigation to "
        "speculatively fetch resources that will likely be used in the frame."
      trigger:
        "A 103 Early Hints HTTP informational response is received during "
        "navigation."
      data:
        "Arbitrary site-controlled data can be included in the URL."
        "Requests may include cookies and site-specific credentials."
      destination: WEBSITE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "user"
      setting:
        "This feature cannot be disabled by Settings. This feature is not "
        "enabled by default yet. TODO(crbug.com/671310): Update this "
        "description once the feature is ready."
      chrome_policy {
        URLBlocklist {
          URLBlocklist: { entries: '*' }
        }
      }
      chrome_policy {
        URLAllowlist {
          URLAllowlist { }
        }
      }
    }
    comments:
      "Chrome uses this type of request during navigation and it cannot be "
      "disabled. Using either URLBlocklist or URLAllowlist (or a combination "
      "of both) limits the scope of these requests."
)");

network::mojom::RequestDestination LinkAsAttributeToRequestDestination(
    network::mojom::LinkAsAttribute attr) {
  switch (attr) {
    case network::mojom::LinkAsAttribute::kUnspecified:
      return network::mojom::RequestDestination::kEmpty;
    case network::mojom::LinkAsAttribute::kImage:
      return network::mojom::RequestDestination::kImage;
    case network::mojom::LinkAsAttribute::kFont:
      return network::mojom::RequestDestination::kFont;
    case network::mojom::LinkAsAttribute::kScript:
      return network::mojom::RequestDestination::kScript;
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return network::mojom::RequestDestination::kStyle;
  }
  NOTREACHED();
  return network::mojom::RequestDestination::kEmpty;
}

// TODO(crbug.com/671310): This is almost the same as GetRequestPriority() in
// loading_predictor_tab_helper.cc. Merge them.
net::RequestPriority CalculateRequestPriority(
    const network::mojom::LinkHeaderPtr& link) {
  switch (link->as) {
    case network::mojom::LinkAsAttribute::kFont:
    case network::mojom::LinkAsAttribute::kStyleSheet:
      return net::HIGHEST;
    case network::mojom::LinkAsAttribute::kScript:
      return net::MEDIUM;
    case network::mojom::LinkAsAttribute::kImage:
      return net::LOWEST;
    case network::mojom::LinkAsAttribute::kUnspecified:
      return net::IDLE;
  }
  NOTREACHED();
  return net::IDLE;
}

network::mojom::RequestMode CalculateRequestMode(
    network::mojom::CrossOriginAttribute attr) {
  switch (attr) {
    case network::mojom::CrossOriginAttribute::kUnspecified:
      return network::mojom::RequestMode::kNoCors;
    case network::mojom::CrossOriginAttribute::kAnonymous:
    case network::mojom::CrossOriginAttribute::kUseCredentials:
      return network::mojom::RequestMode::kCors;
  }
  NOTREACHED();
  return network::mojom::RequestMode::kSameOrigin;
}

network::mojom::CredentialsMode CalculateCredentialMode(
    network::mojom::CrossOriginAttribute attr) {
  switch (attr) {
    case network::mojom::CrossOriginAttribute::kUnspecified:
    case network::mojom::CrossOriginAttribute::kUseCredentials:
      return network::mojom::CredentialsMode::kInclude;
    case network::mojom::CrossOriginAttribute::kAnonymous:
      return network::mojom::CredentialsMode::kSameOrigin;
  }
  NOTREACHED();
  return network::mojom::CredentialsMode::kOmit;
}

}  // namespace

NavigationEarlyHintsManager::InflightPreload::InflightPreload(
    std::unique_ptr<blink::ThrottlingURLLoader> loader,
    std::unique_ptr<network::EmptyURLLoaderClient> client)
    : loader(std::move(loader)), client(std::move(client)) {}

NavigationEarlyHintsManager::InflightPreload::~InflightPreload() = default;

NavigationEarlyHintsManager::NavigationEarlyHintsManager(
    BrowserContext& browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    int frame_tree_node_id)
    : browser_context_(browser_context),
      loader_factory_(std::move(loader_factory)),
      frame_tree_node_id_(frame_tree_node_id) {
  DCHECK(loader_factory_);
}

NavigationEarlyHintsManager::~NavigationEarlyHintsManager() = default;

void NavigationEarlyHintsManager::HandleEarlyHints(
    network::mojom::EarlyHintsPtr early_hints,
    const network::ResourceRequest& navigation_request) {
  for (const auto& link : early_hints->headers->link_headers) {
    // TODO(crbug.com/671310): Support other `rel` attributes.
    if (link->rel == network::mojom::LinkRelAttribute::kPreload)
      MaybePreloadHintedResource(link, navigation_request);
  }
}

void NavigationEarlyHintsManager::MaybePreloadHintedResource(
    const network::mojom::LinkHeaderPtr& link,
    const network::ResourceRequest& navigation_request) {
  if (base::Contains(inflight_preloads_, link->href) ||
      base::Contains(preloaded_resources_, link->href)) {
    return;
  }

  // Subframes aren't supported. To support subframes, this needs to know the
  // origin of the top frame to create an appropriate IsolationInfo.
  if (!navigation_request.is_main_frame)
    return;

  DCHECK(navigation_request.url.SchemeIsHTTPOrHTTPS());
  auto top_frame_origin = url::Origin::Create(navigation_request.url);
  auto preload_origin = url::Origin::Create(link->href);

  net::SiteForCookies site_for_cookies =
      net::SiteForCookies(net::SchemefulSite(top_frame_origin));
  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kGetMethod;
  request.priority = CalculateRequestPriority(link);
  request.destination = LinkAsAttributeToRequestDestination(link->as);
  request.url = link->href;
  request.site_for_cookies = site_for_cookies;
  request.request_initiator = url::Origin::Create(navigation_request.url);
  request.referrer = navigation_request.url;
  request.referrer_policy = navigation_request.referrer_policy;
  request.load_flags = net::LOAD_NORMAL;
  request.resource_type =
      static_cast<int>(blink::mojom::ResourceType::kSubResource);
  request.mode = CalculateRequestMode(link->cross_origin);
  request.credentials_mode = CalculateCredentialMode(link->cross_origin);

  request.trusted_params = network::ResourceRequest::TrustedParams();
  // Ideally, IsolationInfo for preloading subresources should be created by
  // RenderFrameHostImpl::ComputeIsolationInfoForSubresourcesForPendingCommit()
  // but RenderFrameHostImpl isn't available at this point because the final
  // response is needed to determine the host. Using `top_frame_origin`
  // should create the same IsolationInfo as RenderFrameHostImpl creates for
  // top-level frames with HTTP/HTTPS URLs.
  request.trusted_params->isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kOther, top_frame_origin,
      /*frame_origin=*/top_frame_origin, site_for_cookies);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles =
      CreateContentBrowserURLLoaderThrottles(
          request, &browser_context_,
          base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                              frame_tree_node_id_),
          /*navigation_ui_data=*/nullptr, frame_tree_node_id_);

  auto loader_client = std::make_unique<network::EmptyURLLoaderClient>();
  auto* raw_client = loader_client.get();

  auto loader = blink::ThrottlingURLLoader::CreateLoaderAndStart(
      loader_factory_, std::move(throttles),
      content::GlobalRequestID::MakeBrowserInitiated().request_id,
      network::mojom::kURLLoadOptionNone, &request, raw_client,
      kEarlyHintsPreloadTrafficAnnotation, base::ThreadTaskRunnerHandle::Get());

  inflight_preloads_[request.url] = std::make_unique<InflightPreload>(
      std::move(loader), std::move(loader_client));
  raw_client->Drain(
      base::BindOnce(&NavigationEarlyHintsManager::OnPreloadComplete,
                     base::Unretained(this), request.url));
}

void NavigationEarlyHintsManager::OnPreloadComplete(
    const GURL& url,
    const network::URLLoaderCompletionStatus& status) {
  DCHECK(base::Contains(inflight_preloads_, url));
  DCHECK(!base::Contains(preloaded_resources_, url));
  preloaded_resources_[url] = status.error_code;
  inflight_preloads_.erase(url);
  // TODO(crbug.com/671310): Consider to delete `this` when there is no inflight
  // preloads.
}

}  // namespace content
