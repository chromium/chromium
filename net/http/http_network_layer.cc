// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include <memory>

#include "net/http/http_network_transaction.h"

namespace net {

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : session_(session) {
  DCHECK(session_);
}

HttpNetworkLayer::~HttpNetworkLayer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

std::unique_ptr<HttpTransaction> HttpNetworkLayer::CreateTransaction(
    RequestPriority priority) {
  return std::make_unique<HttpNetworkTransaction>(priority, GetSession());
}

HttpCache* HttpNetworkLayer::GetCache() {
  return nullptr;
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  return session_;
}

}  // namespace net
