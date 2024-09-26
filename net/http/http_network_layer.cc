// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_layer.h"

#include <memory>

#include "base/check_op.h"
#include "base/power_monitor/power_monitor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_factory_job.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/third_party/quiche/src/quiche/http2/core/spdy_framer.h"

namespace net {

HttpNetworkLayer::HttpNetworkLayer(HttpNetworkSession* session)
    : session_(session) {
  DCHECK(session_);
#if BUILDFLAG(IS_WIN)
  base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
#endif
}

HttpNetworkLayer::~HttpNetworkLayer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_WIN)
  base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
#endif
}

int HttpNetworkLayer::CreateTransaction(
    RequestPriority priority,
    std::unique_ptr<HttpTransaction>* trans) {
  if (suspended_)
    return ERR_NETWORK_IO_SUSPENDED;

  *trans = std::make_unique<HttpNetworkTransaction>(priority, GetSession());
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
  session_->CloseIdleConnections("Entering suspend mode");
}

void HttpNetworkLayer::OnResume() {
  suspended_ = false;
}

}  // namespace net
