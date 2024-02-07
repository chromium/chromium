// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_AUTHENTICATOR_SELECTION_REQUEST_H_
#define DEVICE_FIDO_CTAP_AUTHENTICATOR_SELECTION_REQUEST_H_

#include <optional>
#include <utility>

#include "components/cbor/values.h"
#include "device/fido/fido_constants.h"

namespace device {

struct CtapAuthenticatorSelectionRequest {};

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const CtapAuthenticatorSelectionRequest&);

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_AUTHENTICATOR_SELECTION_REQUEST_H_
