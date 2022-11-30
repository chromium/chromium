// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_

#include <memory>

#include "content/common/content_export.h"

namespace net {
class HttpNetworkSession;
class HttpTransactionFactory;
}  // namespace net

namespace content {

CONTENT_EXPORT std::unique_ptr<net::HttpTransactionFactory>
CreateDevToolsNetworkTransactionFactory(net::HttpNetworkSession* session);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DEVTOOLS_NETWORK_TRANSACTION_FACTORY_H_
