// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/push_messaging/push_messaging_type_converters.h"

#include <string>
#include <utility>

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/push_messaging/push_subscription_options.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace mojo {

blink::mojom::blink::PushSubscriptionOptionsPtr TypeConverter<
    blink::mojom::blink::PushSubscriptionOptionsPtr,
    blink::PushSubscriptionOptions*>::Convert(blink::PushSubscriptionOptions*
                                                  input) {
  Vector<uint8_t> application_server_key;
  application_server_key.Append(
      reinterpret_cast<uint8_t*>(input->applicationServerKey()->Data()),
      input->applicationServerKey()->DeprecatedByteLengthAsUnsigned());

  return blink::mojom::blink::PushSubscriptionOptions::New(
      input->userVisibleOnly(), application_server_key);
}

}  // namespace mojo
