// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_DELEGATE_H_
#define EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_DELEGATE_H_

#include <memory>
#include <vector>

#include "base/functional/callback.h"
#include "extensions/common/api/cec_private.h"

namespace extensions::api {

// The cec_private API ultimately forwards requests to another api which will
// differ depending on platform.
// CecPrivateDelegate provides an abstraction interface to absorb the
// underlying api differences depending on platform.
// CecPrivateDelegate implementations can have a different lifetime from their
// underlying api provider and are responsible for handling its appearance and
// disappearance.
// When the underlying api provider absent, CecPrivateDelegates will shield
// callers from related errors with NOOPs.
class CecPrivateDelegate {
 public:
  using QueryDisplayPowerStateCallback = base::OnceCallback<void(
      const std::vector<cec_private::DisplayCecPowerState>& power_states)>;

  CecPrivateDelegate(const CecPrivateDelegate&) = delete;
  CecPrivateDelegate& operator=(const CecPrivateDelegate&) = delete;
  virtual ~CecPrivateDelegate() = default;

  // Creates a platform-specific CecPrivateDelegate instance.
  static std::unique_ptr<CecPrivateDelegate> CreateInstance();

  // Sends a HDMI-CEC power control message to all attached displays requesting
  // that they go into standby mode (a.k.a. sleep).
  // The effect of calling this method is on a best-effort basis. No guarantees
  // are made about whether the montitors will actually go into standby mode.
  // Does nothing if the underlying provider isn't available. In this case the
  // callback is still invoked.
  virtual void SendStandBy(base::OnceClosure callback) = 0;

  // Announces this device as the active input source to all displays and sends
  // a HDMI-CEC power control message to all attached displays requesting that
  // they wake up.
  // The effect of calling this method is on a best-effort basis. No guarantees
  // are made about whether the montitors will actually wake up.
  // Does nothing if the underlying provider isn't available. In this case the
  // callback is still invoked.
  virtual void SendWakeUp(base::OnceClosure callback) = 0;

  // Gets the current power state of all attached displays. Displays that do not
  // support HDMI-CEC may not appear in the results.
  // Passes an empty list to the callback if the underlying provider isn't
  // available.
  virtual void QueryDisplayCecPowerState(
      QueryDisplayPowerStateCallback callback) = 0;

 protected:
  CecPrivateDelegate() = default;
};

}  // namespace extensions::api

#endif  // EXTENSIONS_BROWSER_API_CEC_PRIVATE_CEC_PRIVATE_DELEGATE_H_
