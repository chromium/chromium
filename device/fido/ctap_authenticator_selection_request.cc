// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_authenticator_selection_request.h"

namespace device {

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapAuthenticatorSelectionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorSelection,
                        base::nullopt);
}

}  // namespace device
