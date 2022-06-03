// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_discovery_base.h"

namespace device {

FidoDiscoveryBase::FidoDiscoveryBase(FidoTransportProtocol transport)
    : transport_(transport) {}
FidoDiscoveryBase::~FidoDiscoveryBase() = default;

bool FidoDiscoveryBase::MaybeStop() {
  return false;
}

}  // namespace device
