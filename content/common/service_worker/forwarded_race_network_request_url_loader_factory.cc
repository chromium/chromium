// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_worker/forwarded_race_network_request_url_loader_factory.h"

#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {
// Kill switch for multiple CreateLoaderAndStart calls.
BASE_FEATURE(kKillSwitchForRaceNetworkRequestMultipleCreateLoaderAndStartCalls,
             base::FEATURE_ENABLED_BY_DEFAULT);

// Feature flag to enable/disable proxying the URLLoader control pipe in the
// browser. Enabled by default. Used as a killswitch.
BASE_FEATURE(kServiceWorkerRaceNetworkRequestURLLoaderProxy,
             base::FEATURE_ENABLED_BY_DEFAULT);

// A proxy class that implements URLLoader to intercept and secure control
// messages from the renderer. It forwards safe messages (like SetPriority) to
// the actual URLLoader in the network process, and blocks/terminates on
// unauthorized messages (like FollowRedirect).
// This prevents renderers from driving redirects on browser-privileged loaders.
class URLLoaderProxy : public network::mojom::URLLoader {
 public:
  explicit URLLoaderProxy(mojo::PendingRemote<network::mojom::URLLoader> remote)
      : remote_(std::move(remote)) {}

  URLLoaderProxy(const URLLoaderProxy&) = delete;
  URLLoaderProxy& operator=(const URLLoaderProxy&) = delete;
  ~URLLoaderProxy() override = default;

  // network::mojom::URLLoader:
  void FollowRedirect(
      const std::vector<std::string>& removed_headers,
      const net::HttpRequestHeaders& modified_headers,
      const net::HttpRequestHeaders& modified_cors_exempt_headers,
      const std::optional<GURL>& new_url) override {
    // Renderers should never drive redirects for main resource navigations.
    // Report bad message and terminate the connection.
    mojo::ReportBadMessage(
        "URLLoaderProxy: FollowRedirect is forbidden from renderer.");
  }

  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {
    remote_->SetPriority(priority, intra_priority_value);
  }

 private:
  mojo::Remote<network::mojom::URLLoader> remote_;
};
}  // namespace

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory(
        mojo::PendingReceiver<network::mojom::URLLoaderClient> client_receiver,
        scoped_refptr<network::SharedURLLoaderFactory> fallback_factory,
        bool is_main_resource)
    : client_receiver_(std::move(client_receiver)),
      fallback_factory_(fallback_factory),
      is_main_resource_(is_main_resource) {}

ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    ~ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory() = default;

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& resource_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  base::UmaHistogramBoolean(
      base::StrCat({"ServiceWorker.FetchEvent.",
                    is_main_resource_ ? "MainResource" : "Subresource",
                    ".RaceNetworkRequest.ForwardedFactory.CreateLoaderAndStart."
                    "MultipleCalls"}),
      is_data_pipe_fused_);
  if (!is_data_pipe_fused_) {
    // We fuse the URLLoaderClient pipes directly. The URLLoaderClient is a data
    // delivery channel (outgoing from the network process to the renderer). It
    // only carries response data and metadata (like the Mojo Data Pipe handle
    // for the body), and does not expose any control methods that the renderer
    // can use to drive the network process.
    bool result =
        mojo::FusePipes(std::move(client_receiver_), std::move(client));
    CHECK(result) << resource_request.url;
    if (is_main_resource_ &&
        base::FeatureList::IsEnabled(
            kServiceWorkerRaceNetworkRequestURLLoaderProxy)) {
      // The URLLoader is a control channel. A renderer could call
      // FollowRedirect on it to drive redirects on a privileged loader. To
      // prevent this, we proxy the URLLoader in the browser to intercept and
      // block FollowRedirect.
      mojo::MakeSelfOwnedReceiver(
          std::make_unique<URLLoaderProxy>(std::move(loader_)),
          std::move(receiver));
    } else {
      // For subresources, fuse the control pipe directly as the loader does not
      // have browser-level privileges.
      result = mojo::FusePipes(std::move(receiver), std::move(loader_));
      CHECK(result) << resource_request.url;
    }
    is_data_pipe_fused_ = true;
  } else {
    SCOPED_CRASH_KEY_STRING1024("ServiceWorker", "race_req_url",
                                resource_request.url.spec());
    SCOPED_CRASH_KEY_BOOL("ServiceWorker", "race_req_is_main",
                          is_main_resource_);
    base::debug::DumpWithoutCrashing();
    // A legitimate renderer will never hit this branch.
    // If we are here, the renderer is compromised or severely buggy.
    if (base::FeatureList::IsEnabled(
            kKillSwitchForRaceNetworkRequestMultipleCreateLoaderAndStartCalls)) {
      receiver_.ReportBadMessage(
          "ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory: "
          "CreateLoaderAndStart called multiple times.");
    } else {
      // If already fused, create a new URLLoader and start the new request.
      // TODO(crbug.com/497437113): Remove this once the kill switch is removed.
      fallback_factory_->CreateLoaderAndStart(
          std::move(receiver), request_id, options, resource_request,
          std::move(client), traffic_annotation);
    }
  }
}

void ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  receiver_.Bind(std::move(receiver));
}

mojo::PendingReceiver<network::mojom::URLLoader>
ServiceWorkerForwardedRaceNetworkRequestURLLoaderFactory::
    InitURLLoaderNewPipeAndPassReceiver() {
  return loader_.InitWithNewPipeAndPassReceiver();
}
}  // namespace content
