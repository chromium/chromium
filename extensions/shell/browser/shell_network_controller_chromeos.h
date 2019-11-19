// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace extensions {

// Handles network-related tasks for app_shell on Chrome OS.
class ShellNetworkController : public chromeos::NetworkStateHandlerObserver {
 public:
  // This class must be instantiated after chromeos::DBusThreadManager and
  // destroyed before it.
  explicit ShellNetworkController(const std::string& preferred_network_name);
  ~ShellNetworkController() override;

  // chromeos::NetworkStateHandlerObserver overrides:
  void NetworkListChanged() override;
  void NetworkConnectionStateChanged(
      const chromeos::NetworkState* state) override;

  // Control whether the cellular network connection allows roaming.
  void SetCellularAllowRoaming(bool allow_roaming);

 private:
  // State of communication with the connection manager.
  enum State {
    // No in-progress requests.
    STATE_IDLE = 0,
    // Waiting for the result of an attempt to connect to the preferred network.
    STATE_WAITING_FOR_PREFERRED_RESULT,
    // Waiting for the result of an attempt to connect to a non-preferred
    // network.
    STATE_WAITING_FOR_NON_PREFERRED_RESULT,
  };

  // Returns the connected or connecting WiFi network or NULL if no network
  // matches that description.
  const chromeos::NetworkState* GetActiveWiFiNetwork();

  // Controls whether scanning is performed periodically.
  void SetScanningEnabled(bool enabled);

  // Asks the connection manager to scan for networks.
  void RequestScan();

  // If not currently connected or connecting, chooses a wireless network and
  // asks the connection manager to connect to it. Also switches to
  // |preferred_network_name_| if possible.
  void ConnectIfUnconnected();

  // Handles a successful or failed connection attempt.
  void HandleConnectionSuccess();
  void HandleConnectionError(const std::string& error_name,
                             std::unique_ptr<base::DictionaryValue> error_data);

  // Current status of communication with the chromeos::NetworkStateHandler.
  // This is tracked to avoid sending duplicate requests before the handler has
  // acknowledged the initial connection attempt.
  State state_;

  // Invokes RequestScan() periodically.
  base::RepeatingTimer scan_timer_;

  // Optionally-supplied name of the preferred network.
  std::string preferred_network_name_;

  // True if the preferred network is connected or connecting.
  bool preferred_network_is_active_;

  base::WeakPtrFactory<ShellNetworkController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ShellNetworkController);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_NETWORK_CONTROLLER_CHROMEOS_H_
