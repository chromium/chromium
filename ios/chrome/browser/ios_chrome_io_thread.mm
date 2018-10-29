// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ios_chrome_io_thread.h"

#include "ios/chrome/browser/net/ios_chrome_network_delegate.h"
#include "ios/chrome/common/channel_info.h"
#include "ios/web/public/network_context_owner.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
    network_context_owner_ = std::make_unique<web::NetworkContextOwner>(
        system_url_request_context_getter(), &network_context_);
  }
  return network_context_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeIOThread::GetSharedURLLoaderFactory() {
  if (!url_loader_factory_) {
    auto url_loader_factory_params =
        network::mojom::URLLoaderFactoryParams::New();
    url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
    url_loader_factory_params->is_corb_enabled = false;
    GetSystemNetworkContext()->CreateURLLoaderFactory(
        mojo::MakeRequest(&url_loader_factory_),
        std::move(url_loader_factory_params));
    shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_.get());
  }
  return shared_url_loader_factory_;
}

void IOSChromeIOThread::NetworkTearDown() {
  if (shared_url_loader_factory_)
    shared_url_loader_factory_->Detach();

  if (network_context_) {
    web::WebThread::DeleteSoon(web::WebThread::IO, FROM_HERE,
                               network_context_owner_.release());
  }
}
