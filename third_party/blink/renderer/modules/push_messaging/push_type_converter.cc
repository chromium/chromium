// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_type_converter.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

// static
blink::mojom::blink::PushSubscriptionOptionsPtr
TypeConverter<blink::mojom::blink::PushSubscriptionOptionsPtr,
              blink::PushSubscriptionOptions*>::
    Convert(const blink::PushSubscriptionOptions* input) {
  Vector<uint8_t> application_server_key;
  application_server_key.AppendSpan(input->applicationServerKey()->ByteSpan());
  return blink::mojom::blink::PushSubscriptionOptions::New(
      input->userVisibleOnly(), std::move(application_server_key));
}

}  // namespace mojo
