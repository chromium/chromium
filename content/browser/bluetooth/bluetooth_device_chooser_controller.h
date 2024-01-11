// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_CHOOSER_CONTROLLER_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_CHOOSER_CONTROLLER_H_

#include <optional>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "content/browser/devtools/devtools_device_request_prompt_info.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_chooser.h"
#include "third_party/blink/public/mojom/bluetooth/web_bluetooth.mojom.h"

namespace device {
class BluetoothAdapter;
class BluetoothDevice;
class BluetoothDiscoverySession;
class BluetoothDiscoveryFilter;
}  // namespace device

namespace content {

class RenderFrameHost;
class WebBluetoothServiceImpl;

// Class that interacts with a chooser and starts a bluetooth discovery session.
// This class needs to be re-instantiated for each call to GetDevice(). Calling
// GetDevice() twice for the same instance will DCHECK.
class CONTENT_EXPORT BluetoothDeviceChooserController final {
 public:
  using Callback =
      base::OnceCallback<void(blink::mojom::WebBluetoothResult result,
                              blink::mojom::WebBluetoothRequestDeviceOptionsPtr,
                              const std::string& device_address)>;

  enum class TestScanDurationSetting { IMMEDIATE_TIMEOUT, NEVER_TIMEOUT };

  // |web_bluetooth_service_| service that owns this class.
  // |render_frame_host| should be the RenderFrameHost that owns the
  // |web_bluetooth_service_|.
  // |adapter| should be the adapter used to scan for Bluetooth devices.
  BluetoothDeviceChooserController(
      WebBluetoothServiceImpl* web_bluetooth_service_,
      RenderFrameHost& render_frame_host,
      scoped_refptr<device::BluetoothAdapter> adapter);
  ~BluetoothDeviceChooserController();

  // This function performs the following checks before starting a discovery
  // session:
  //   - Validates filters in |request_device_options|.
  //   - Removes any blocklisted UUIDs from
  //     |request_device_options.optinal_services|.
  //   - Checks if the request came from a cross-origin iframe.
  //   - Checks if the request came from a unique origin.
  //   - Checks if the adapter is present.
  //   - Checks if the Web Bluetooth API has been disabled.
  //   - Checks if we are allowed to ask for scanning permission.
  // If any of the previous checks failed then this function runs |callback|
  // with the corresponding error. Otherwise this function populates the
  // embedder provided BluetoothChooser with existing devices and starts a new
  // discovery session.
  //
  // This function should only be called once per
  // BluetoothDeviceChooserController instance. Calling this function more than
  // once will DCHECK.
  void GetDevice(
      blink::mojom::WebBluetoothRequestDeviceOptionsPtr request_device_options,
      Callback callback);

  // Adds a device to the chooser. Should only be called after GetDevice and
  // before either of the callbacks are run.
  void AddFilteredDevice(const device::BluetoothDevice& device);

  // Stops the current discovery session and notifies the chooser
  // that the adapter changed states.
  void AdapterPoweredChanged(bool powered);

  // Received Signal Strength Indicator (RSSI) is a measurement of the power
  // present in a received radio signal.
  static int CalculateSignalStrengthLevel(int8_t rssi);

  // After this method is called, any new instance of
  // BluetoothDeviceChooserController will have a scan duration determined by
  // the |setting| enum. The possible enumerations are described below:
  //   IMMEDIATE_TIMEOUT: Sets the scan duration to 0 seconds.
  //   NEVER_TIMEOUT:     Sets the scan duration to INT_MAX seconds.
  static void SetTestScanDurationForTesting(
      TestScanDurationSetting setting =
          TestScanDurationSetting::IMMEDIATE_TIMEOUT);

  static std::unique_ptr<device::BluetoothDiscoveryFilter> ComputeScanFilter(
      const std::optional<
          std::vector<blink::mojom::WebBluetoothLeScanFilterPtr>>& filters);

 private:
  class BluetoothDeviceRequestPromptInfo final
      : public DevtoolsDeviceRequestPromptInfo {
   public:
    explicit BluetoothDeviceRequestPromptInfo(
        BluetoothDeviceChooserController& controller);
    ~BluetoothDeviceRequestPromptInfo() override;

    std::vector<DevtoolsDeviceRequestPromptDevice> GetDevices() override;
    bool SelectDevice(const std::string& device_id) override;
    void Cancel() override;

   private:
    // The controller that owns this instance.
    raw_ref<BluetoothDeviceChooserController> controller_;
  };

  // Populates the chooser with the GATT connected devices.
  void PopulateConnectedDevices();

  // Notifies the chooser that discovery is starting and starts a discovery
  // session.
  void StartDeviceDiscovery();

  // Stops the discovery session and notifies the chooser.
  void StopDeviceDiscovery();

  // StartDiscoverySessionWithFilter callbacks:
  void OnStartDiscoverySessionSuccess(
      std::unique_ptr<device::BluetoothDiscoverySession> discovery_session);
  void OnStartDiscoverySessionFailed();

  // BluetoothChooser::EventHandler:
  // Runs |error_callback_| if the chooser was cancelled or if we weren't able
  // to show the chooser. Otherwise runs |success_callback_| with
  // |device_address|.
  void OnBluetoothChooserEvent(BluetoothChooserEvent event,
                               const std::string& device_address);

  // Helper function to asynchronously run success_callback_.
  void PostSuccessCallback(const std::string& device_address);
  // Helper function to asynchronously run error_callback_.
  void PostErrorCallback(blink::mojom::WebBluetoothResult result);

  // Stores the scan duration to use for the discovery session timer.
  // The default value is 60 seconds.
  static int64_t scan_duration_;

  // The adapter used to get existing devices and start a discovery session.
  scoped_refptr<device::BluetoothAdapter> adapter_;
  // The WebBluetoothServiceImpl that owns this instance.
  raw_ptr<WebBluetoothServiceImpl> web_bluetooth_service_;
  // The RenderFrameHost that owns web_bluetooth_service_.
  raw_ref<RenderFrameHost> render_frame_host_;

  BluetoothDeviceRequestPromptInfo prompt_info_;

  // Contains the filters and optional services used when scanning.
  blink::mojom::WebBluetoothRequestDeviceOptionsPtr options_;

  // Callback to be called with the result of the chooser.
  Callback callback_;

  // The currently opened BluetoothChooser.
  std::unique_ptr<BluetoothChooser> chooser_;

  // Automatically stops Bluetooth discovery a set amount of time after it was
  // started.
  base::RetainingOneShotTimer discovery_session_timer_;

  // The last discovery session to be started.
  // TODO(ortuno): This should be null unless there is an active discovery
  // session. We need to null it when the platform stops discovery.
  // http://crbug.com/611852
  std::unique_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // The device ids that are currently shown in the chooser.
  std::unordered_set<std::string> device_ids_;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceChooserController> weak_ptr_factory_{
      this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_CHOOSER_CONTROLLER_H_
