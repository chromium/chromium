// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_KEY_PINNING_STATE_CHROME_TEST_UTIL_H_
#define NET_CERT_KEY_PINNING_STATE_CHROME_TEST_UTIL_H_

#include "net/cert/key_pinning_state_chrome.h"

namespace net {

// Set the static key pinning defaults. This only affects the defaults for
// KeyPinningStateChrome instances that are created while the scoper is alive,
// and if these instances outlive the scoper they will continue to use the
// defaults from their creation time.
// KeyPinningStateChrome instances that were created before the scoper will not
// be affected.
class ScopedKeyPinningStateChromeDefaults {
 public:
  enum Source {
    // Force enable with the unittest static data.
    kUnittestDefault,

    // Force enable with the unittest static data, but the timeliness is not
    // overridden, so the data may or may not actually take effect.
    kUnittestDefaultWithoutAlwaysTimely,

    // Force enable with the real production source.
    kForceEnableProduction,

    // Force disable even on builds where it would normally be enabled by
    // default.
    kForceDisabled,
  };

  explicit ScopedKeyPinningStateChromeDefaults(
      Source source = kUnittestDefault);

  void Reset(Source source);

  ScopedKeyPinningStateChromeDefaults(
      const ScopedKeyPinningStateChromeDefaults&) = delete;
  ScopedKeyPinningStateChromeDefaults& operator=(
      const ScopedKeyPinningStateChromeDefaults&) = delete;

  ~ScopedKeyPinningStateChromeDefaults();
};

}  // namespace net

#endif  // NET_CERT_KEY_PINNING_STATE_CHROME_TEST_UTIL_H_
