// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_NETWORK_LAYER_H_
#define NET_HTTP_HTTP_NETWORK_LAYER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/power_monitor/power_observer.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/http/http_transaction_factory.h"

namespace net {

class HttpNetworkSession;

class NET_EXPORT HttpNetworkLayer : public HttpTransactionFactory,
                                    public base::PowerObserver {
 public:
  // Construct a HttpNetworkLayer with an existing HttpNetworkSession which
  // contains a valid ProxyResolutionService. The HttpNetworkLayer must be
  // destroyed before |session|.
  explicit HttpNetworkLayer(HttpNetworkSession* session);
  ~HttpNetworkLayer() override;

  // HttpTransactionFactory methods:
  int CreateTransaction(RequestPriority priority,
                        std::unique_ptr<HttpTransaction>* trans) override;
  HttpCache* GetCache() override;
  HttpNetworkSession* GetSession() override;

  // base::PowerObserver methods:
  void OnSuspend() override;
  void OnResume() override;

 private:
  HttpNetworkSession* const session_;
  bool suspended_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(HttpNetworkLayer);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_NETWORK_LAYER_H_
