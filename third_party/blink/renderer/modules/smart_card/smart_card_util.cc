// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/smart_card/smart_card_util.h"
#include "services/device/public/mojom/smart_card.mojom-blink.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_access_mode.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_smart_card_protocol.h"

namespace blink {

device::mojom::blink::SmartCardShareMode ToMojoSmartCardShareMode(
    V8SmartCardAccessMode access_mode) {
  switch (access_mode.AsEnum()) {
    case blink::V8SmartCardAccessMode::Enum::kShared:
      return device::mojom::blink::SmartCardShareMode::kShared;
    case blink::V8SmartCardAccessMode::Enum::kExclusive:
      return device::mojom::blink::SmartCardShareMode::kExclusive;
    case blink::V8SmartCardAccessMode::Enum::kDirect:
      return device::mojom::blink::SmartCardShareMode::kDirect;
  }
}

device::mojom::blink::SmartCardProtocolsPtr ToMojoSmartCardProtocols(
    const Vector<V8SmartCardProtocol>& preferred_protocols) {
  auto result = device::mojom::blink::SmartCardProtocols::New();

  for (const auto& protocol : preferred_protocols) {
    switch (protocol.AsEnum()) {
      case blink::V8SmartCardProtocol::Enum::kRaw:
        result->raw = true;
        break;
      case blink::V8SmartCardProtocol::Enum::kT0:
        result->t0 = true;
        break;
      case blink::V8SmartCardProtocol::Enum::kT1:
        result->t1 = true;
        break;
    }
  }

  return result;
}

}  // namespace blink
