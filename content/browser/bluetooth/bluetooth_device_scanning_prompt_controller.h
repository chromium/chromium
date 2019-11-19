// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_SCANNING_PROMPT_CONTROLLER_H_
#define CONTENT_BROWSER_BLUETOOTH_BLUETOOTH_DEVICE_SCANNING_PROMPT_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/public/browser/bluetooth_scanning_prompt.h"

namespace content {

class RenderFrameHost;
class WebContents;
class WebBluetoothServiceImpl;

// Class that interacts with a prompt.
class CONTENT_EXPORT BluetoothDeviceScanningPromptController final {
 public:
  // |web_bluetooth_service_| service that owns this class.
  // |render_frame_host| should be the RenderFrameHost that owns the
  // |web_bluetooth_service_|.
  BluetoothDeviceScanningPromptController(
      WebBluetoothServiceImpl* web_bluetooth_service,
      RenderFrameHost* render_frame_host);
  ~BluetoothDeviceScanningPromptController();

  void ShowPermissionPrompt();

  void OnBluetoothScanningPromptEvent(BluetoothScanningPrompt::Event event);

  // Adds a device to the prompt. Should only be called after
  // ShowPermissionPrompt() is called.
  void AddFilteredDevice(const std::string& device_id,
                         bool should_update_name,
                         const base::string16& device_name);

 private:
  // The WebBluetoothServiceImpl that owns this instance.
  WebBluetoothServiceImpl* const web_bluetooth_service_;
  // The RenderFrameHost that owns |web_bluetooth_service_|.
  RenderFrameHost* const render_frame_host_;
  // The WebContents that owns |render_frame_host_|.
  WebContents* const web_contents_;

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
