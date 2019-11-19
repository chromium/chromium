// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include "base/logging.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_job.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/third_party/quiche/src/spdy/core/spdy_framer.h"

namespace net {

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : session_(session),
      suspended_(false) {
  DCHECK(session_);
#if defined(OS_WIN)
  base::PowerMonitor::AddObserver(this);
#endif
}

HttpNetworkLayer::~HttpNetworkLayer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if defined(OS_WIN)
  base::PowerMonitor::RemoveObserver(this);
#endif
}

int HttpNetworkLayer::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* trans) {
  if (suspended_)
    return ERR_NETWORK_IO_SUSPENDED;

  trans->reset(new HttpNetworkTransaction(priority, GetSession()));
  return OK;
}

HttpCache* HttpNetworkLayer::GetCache() {
  return nullptr;
}

HttpNetworkSession* HttpNetworkLayer::GetSession() {
  return session_;
}

void HttpNetworkLayer::OnSuspend() {
  suspended_ = true;
  session_->CloseIdleConnections();
}

void HttpNetworkLayer::OnResume() {
  suspended_ = false;
}

}  // namespace net
