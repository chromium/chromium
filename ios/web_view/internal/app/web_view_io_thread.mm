// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/app/web_view_io_thread.h"

#include "net/base/network_delegate_impl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewIOThread::WebViewIOThread(PrefService* local_state, net::NetLog* net_log)
    : IOSIOThread(local_state, net_log) {}

WebViewIOThread::~WebViewIOThread() = default;

std::unique_ptr<net::NetworkDelegate>
WebViewIOThread::CreateSystemNetworkDelegate() {
  return std::make_unique<net::NetworkDelegateImpl>();
}

std::string WebViewIOThread::GetChannelString() const {
  return std::string();
}

}  // namespace ios_web_view
