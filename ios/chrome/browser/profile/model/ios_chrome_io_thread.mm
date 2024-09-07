// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/profile/model/ios_chrome_io_thread.h"

#import "components/variations/net/variations_http_headers.h"
#import "ios/chrome/browser/net/model/ios_chrome_network_delegate.h"
#import "ios/chrome/common/channel_info.h"
#import "ios/web/public/init/network_context_owner.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

IOSChromeIOThread::IOSChromeIOThread(PrefService* local_state,
                                     net::NetLog* net_log)
    : IOSIOThread(local_state, net_log) {
  IOSChromeNetworkDelegate::InitializePrefsOnUIThread(nullptr, local_state);
}

IOSChromeIOThread::~IOSChromeIOThread() = default;

std::unique_ptr<net::NetworkDelegate>
IOSChromeIOThread::CreateSystemNetworkDelegate() {
  return std::make_unique<IOSChromeNetworkDelegate>();
}

std::string IOSChromeIOThread::GetChannelString() const {
  return ::GetChannelString();
}

network::mojom::NetworkContext* IOSChromeIOThread::GetSystemNetworkContext() {
  if (!network_context_) {
    network::mojom::NetworkContextParamsPtr network_context_params =
        network::mojom::NetworkContextParams::New();
    variations::UpdateCorsExemptHeaderForVariations(
        network_context_params.get());
    network_context_owner_ = std::make_unique<web::NetworkContextOwner>(
        system_url_request_context_getter(),
        network_context_params->cors_exempt_header_list, &network_context_);
  }
  return network_context_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeIOThread::GetSharedURLLoaderFactory() {
  if (!url_loader_factory_) {
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_orb_enabled = false;
    url_loader_factory_params->is_trusted = true;
    GetSystemNetworkContext()->CreateURLLoaderFactory(
        url_loader_factory_.BindNewPipeAndPassReceiver(),
        std::move(url_loader_factory_params));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }
  return shared_url_loader_factory_;
}

void IOSChromeIOThread::NetworkTearDown() {
  if (shared_url_loader_factory_) {
    shared_url_loader_factory_->Detach();
  }

  if (network_context_) {
    web::GetIOThreadTaskRunner({})->DeleteSoon(
        FROM_HERE, network_context_owner_.release());
  }
}
