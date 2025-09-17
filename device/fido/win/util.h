// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_WIN_UTIL_H_
#define DEVICE_FIDO_WIN_UTIL_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"

namespace device::fido::win {

// Determines whether Windows Hello can use a biometric device to perform user
// verification.
void COMPONENT_EXPORT(DEVICE_FIDO)
    DeviceHasBiometricsAvailable(base::OnceCallback<void(bool)> callback);

// Returns true if Chrome is currently running under Remote Desktop (RDP). Do
// not cache this result, as it can change over the lifetime of the program.
bool COMPONENT_EXPORT(DEVICE_FIDO) IsRemoteDesktopSession();

// Allows overriding `DeviceHasBiometricsAvailable` in testing.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedBiometricsOverride {
 public:
  explicit ScopedBiometricsOverride(bool has_biometrics);
  ScopedBiometricsOverride(const ScopedBiometricsOverride&) = delete;
  ScopedBiometricsOverride& operator=(const ScopedBiometricsOverride&) = delete;
  ~ScopedBiometricsOverride();
};

// Allows overriding `IsRemoteDesktopSession` in testing.
class COMPONENT_EXPORT(DEVICE_FIDO) ScopedIsRdpSessionOverride {
 public:
  explicit ScopedIsRdpSessionOverride(bool is_rdp);
  ScopedIsRdpSessionOverride(const ScopedIsRdpSessionOverride&) = delete;
  ScopedIsRdpSessionOverride& operator=(const ScopedIsRdpSessionOverride&) =
      delete;
  ~ScopedIsRdpSessionOverride();
};

}  // namespace device::fido::win

#endif  // DEVICE_FIDO_WIN_UTIL_H_
