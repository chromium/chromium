// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PARSER_IOS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PARSER_IOS_H_

#include "services/device/public/mojom/nfc.mojom-blink.h"

namespace blink {

device::mojom::blink::NDEFMessagePtr ParseRawNDEFMessage(
    device::mojom::blink::NDEFRawMessagePtr message);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_NFC_NFC_PARSER_IOS_H_
