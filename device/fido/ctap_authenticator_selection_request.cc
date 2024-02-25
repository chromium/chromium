// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ctap_authenticator_selection_request.h"

namespace device {

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapAuthenticatorSelectionRequest&) {
  return std::make_pair(CtapRequestCommand::kAuthenticatorSelection,
                        std::nullopt);
}

}  // namespace device
