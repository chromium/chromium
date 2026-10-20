// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_chrome_data.h"

#include "net/http/transport_security_state_pins_source.h"

namespace net {

namespace {

#include "net/http/transport_security_state_static_pins.h"  // nogncheck

}  // namespace

const TransportSecurityStatePinsSource* GetCompiledPinsSource() {
  return &kPinsSource;
}

base::Time GetCompiledPinsListTimestamp() {
  return kPinsListTimestamp;
}

}  // namespace net
