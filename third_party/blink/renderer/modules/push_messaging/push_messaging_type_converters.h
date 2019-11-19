// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_TYPE_CONVERTERS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_TYPE_CONVERTERS_H_

#include "third_party/blink/public/mojom/push_messaging/push_messaging.mojom-blink.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {
class PushSubscriptionOptions;
}  // namespace blink

namespace mojo {

template <>
struct TypeConverter<blink::mojom::blink::PushSubscriptionOptionsPtr,
                     blink::PushSubscriptionOptions*> {
  static blink::mojom::blink::PushSubscriptionOptionsPtr Convert(
      blink::PushSubscriptionOptions* input);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PUSH_MESSAGING_PUSH_MESSAGING_TYPE_CONVERTERS_H_
