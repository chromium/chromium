// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_SCANNING_PROMPT_CONTROLLER_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_SCANNING_PROMPT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

namespace content {

class RenderFrameHost;
class WebBluetoothServiceImpl;

// Class that interacts with a prompt.
class BluetoothDeviceScanningPromptController final {
 public:
  // |web_bluetooth_service_| service that owns this class.
  // |render_frame_host| should be the RenderFrameHost that owns the
  // |web_bluetooth_service_|.
  BluetoothDeviceScanningPromptController(
      WebBluetoothServiceImpl* web_bluetooth_service,
      RenderFrameHost& render_frame_host);
  ~BluetoothDeviceScanningPromptController();

  void ShowPermissionPrompt();

  void OnBluetoothScanningPromptEvent(BluetoothScanningPrompt::Event event);

  // Adds a device to the prompt. Should only be called after
  // ShowPermissionPrompt() is called.
  void AddFilteredDevice(const std::string& device_id,
                         bool should_update_name,
                         const std::u16string& device_name);

 private:
  // The WebBluetoothServiceImpl that owns this instance.
  const raw_ptr<WebBluetoothServiceImpl> web_bluetooth_service_;
  // The RenderFrameHost that owns |web_bluetooth_service_|.
  const raw_ref<RenderFrameHost> render_frame_host_;

  // The currently opened BluetoothScanningPrompt.
  std::unique_ptr<BluetoothScanningPrompt> prompt_;

  bool prompt_event_received_ = false;

  // Weak pointer factory for generating 'this' pointers that might live longer
  // than we do.
  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothDeviceScanningPromptController>
      weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_SCANNING_PROMPT_CONTROLLER_H_
