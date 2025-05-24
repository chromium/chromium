// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_FACTORY_H_
#define NET_HTTP_HTTP_TRANSACTION_FACTORY_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/base/request_priority.h"

namespace net {

class HttpCache;
class HttpNetworkSession;
class HttpTransaction;

// An interface to a class that can create HttpTransaction objects.
class NET_EXPORT HttpTransactionFactory {
 public:
  virtual ~HttpTransactionFactory() = default;

  // Creates a HttpTransaction object.
  virtual std::unique_ptr<HttpTransaction> CreateTransaction(
      RequestPriority priority) = 0;

  // Returns the associated cache if any (may be NULL).
  virtual HttpCache* GetCache() = 0;

  // Returns the associated HttpNetworkSession used by new transactions.
  virtual HttpNetworkSession* GetSession() = 0;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_TRANSACTION_FACTORY_H_
