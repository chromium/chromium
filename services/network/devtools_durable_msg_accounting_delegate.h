// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_ACCOUNTING_DELEGATE_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_ACCOUNTING_DELEGATE_H_

#include <stdint.h>

namespace network {
class DevtoolsDurableMessage;

class DevtoolsDurableMessageAccountingDelegate {
 public:
  virtual ~DevtoolsDurableMessageAccountingDelegate() = default;
  virtual void WillAddBytes(DevtoolsDurableMessage& message,
                            int64_t chunk_size) = 0;
  virtual void WillRemoveBytes(DevtoolsDurableMessage& message) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_ACCOUNTING_DELEGATE_H_
