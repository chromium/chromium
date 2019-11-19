// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/browser/bluetooth/web_bluetooth_service_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"

namespace content {

BluetoothDeviceScanningPromptController::
    BluetoothDeviceScanningPromptController(
        WebBluetoothServiceImpl* web_bluetooth_service,
        RenderFrameHost* render_frame_host)
    : web_bluetooth_service_(web_bluetooth_service),
      render_frame_host_(render_frame_host),
      web_contents_(WebContents::FromRenderFrameHost(render_frame_host_)) {}

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
  WebContentsDelegate* delegate =
      WebContents::FromRenderFrameHost(render_frame_host_)->GetDelegate();
  if (delegate) {
    prompt_ = delegate->ShowBluetoothScanningPrompt(
        render_frame_host_, std::move(prompt_event_handler));
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
    const base::string16& device_name) {
  if (prompt_)
    prompt_->AddOrUpdateDevice(device_id, should_update_name, device_name);
}

}  // namespace content
