// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_

#include "services/device/public/mojom/nfc.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/modules/nfc/ndef_message.h"
#include "third_party/blink/renderer/modules/nfc/ndef_record.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class DOMException;

size_t GetNDEFMessageSize(const device::mojom::blink::NDEFMessage& message);

bool SetNDEFMessageURL(const String& origin,
                       device::mojom::blink::NDEFMessage* message);

device::mojom::blink::NDEFPushTarget StringToNDEFPushTarget(
    const WTF::String& target);

DOMException* NDEFErrorTypeToDOMException(
    device::mojom::blink::NDEFErrorType error_type);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_UTILS_H_
