// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/public/browser/bluetooth_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

BluetoothDeviceScanningPromptController::
    BluetoothDeviceScanningPromptController(
        WebBluetoothServiceImpl* web_bluetooth_service,
        RenderFrameHost& render_frame_host)
    : web_bluetooth_service_(web_bluetooth_service),
      render_frame_host_(render_frame_host) {}

BluetoothDeviceScanningPromptController::
    ~BluetoothDeviceScanningPromptController() {
  if (!prompt_event_received_)
    OnBluetoothScanningPromptEvent(BluetoothScanningPrompt::Event::kCanceled);
}

void BluetoothDeviceScanningPromptController::ShowPermissionPrompt() {
  BluetoothScanningPrompt::EventHandler prompt_event_handler =
      base::BindRepeating(&BluetoothDeviceScanningPromptController::
                              OnBluetoothScanningPromptEvent,
                          weak_ptr_factory_.GetWeakPtr());

  if (auto* delegate = GetContentClient()->browser()->GetBluetoothDelegate()) {
    // non-active RFHs can't show UI elements like prompts to the user.
    if (!render_frame_host_->IsActive())
      return;
    prompt_ = delegate->ShowBluetoothScanningPrompt(
        &*render_frame_host_, std::move(prompt_event_handler));
  }
}

void BluetoothDeviceScanningPromptController::OnBluetoothScanningPromptEvent(
    BluetoothScanningPrompt::Event event) {
  DCHECK(web_bluetooth_service_);

  web_bluetooth_service_->OnBluetoothScanningPromptEvent(event, this);
  prompt_event_received_ = true;

  // Close prompt.
  prompt_.reset();
}

void BluetoothDeviceScanningPromptController::AddFilteredDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name) {
  if (prompt_)
    prompt_->AddOrUpdateDevice(device_id, should_update_name, device_name);
}

}  // namespace content
