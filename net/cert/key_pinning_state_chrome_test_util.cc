// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/key_pinning_state_chrome_test_util.h"

#include "net/cert/key_pinning_state_chrome.h"
#include "net/http/transport_security_state_pins_source.h"

namespace net {

namespace {

namespace test_default {
#include "net/http/transport_security_state_static_pins_unittest_default.h"
}

const TransportSecurityStatePinsSource* GetCompiledSource(
    ScopedKeyPinningStateChromeDefaults::Source source) {
  using Source = ScopedKeyPinningStateChromeDefaults::Source;
  switch (source) {
    case Source::kUnittestDefault:
    case Source::kUnittestDefaultWithoutAlwaysTimely:
      return &test_default::kPinsSource;
    case Source::kForceEnableProduction:
    case Source::kForceDisabled:
      return nullptr;
  }
}

}  // namespace

ScopedKeyPinningStateChromeDefaults::ScopedKeyPinningStateChromeDefaults(
    Source source) {
  Reset(source);
}

ScopedKeyPinningStateChromeDefaults::~ScopedKeyPinningStateChromeDefaults() {
  ClearKeyPinningStateChromeOverridesForTesting();
}

void ScopedKeyPinningStateChromeDefaults::Reset(Source source) {
  const bool enable_value = source != Source::kForceDisabled;

  const bool always_timely_value =
      enable_value && source != Source::kUnittestDefaultWithoutAlwaysTimely;

  SetKeyPinningStateChromeOverridesForTesting(
      GetCompiledSource(source),
      /*enabled=*/enable_value,
      /*pins_list_always_timely_for_testing=*/always_timely_value);
}

}  // namespace net
