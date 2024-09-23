// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/quota/quota_utils.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"

namespace blink {

void ConnectToQuotaManagerHost(
    ExecutionContext* execution_context,
    mojo::PendingReceiver<mojom::blink::QuotaManagerHost> receiver) {
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      std::move(receiver));
}

}  // namespace blink
