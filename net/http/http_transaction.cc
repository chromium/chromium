// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_transaction.h"

namespace net {

void HttpTransaction::DeferNetworkErrorLoggingForBodyDecoding() {}

void HttpTransaction::CompleteNetworkErrorLoggingForBodyDecoding(int result) {}

}  // namespace net
