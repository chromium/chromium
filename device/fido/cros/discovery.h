// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CROS_DISCOVERY_H_
#define DEVICE_FIDO_CROS_DISCOVERY_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/tpm_manager/tpm_manager.pb.h"
#include "device/fido/cros/authenticator.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/fido_discovery_base.h"

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) FidoChromeOSDiscovery
    : public FidoDiscoveryBase {
 public:
  FidoChromeOSDiscovery(
      base::RepeatingCallback<std::string()> generate_request_id_callback,
      std::optional<CtapGetAssertionRequest> get_assertion_request_);
  ~FidoChromeOSDiscovery() override;

  void set_require_power_button_mode(bool require);

  // FidoDiscoveryBase:
  void Start() override;

 private:
  void OnU2FServiceAvailable(bool u2f_service_available);
  void MaybeAddAuthenticator();
  void OnPowerButtonEnabled(bool enabled);
  void OnUvAvailable(bool available);
  void OnLacrosSupported(bool supported);
  void OnRequestComplete();

  base::RepeatingCallback<std::string()> generate_request_id_callback_;
  bool require_power_button_mode_ = false;
  bool power_button_enabled_ = false;
  bool uv_available_ = false;
  bool lacros_supported_ = false;
  uint32_t pending_requests_ = 0;
  std::optional<CtapGetAssertionRequest> get_assertion_request_;
  std::unique_ptr<ChromeOSAuthenticator> authenticator_;
  base::WeakPtrFactory<FidoChromeOSDiscovery> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_FIDO_CROS_DISCOVERY_H_
