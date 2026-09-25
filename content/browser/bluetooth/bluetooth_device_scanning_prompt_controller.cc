// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/bluetooth/bluetooth_device_scanning_prompt_controller.h"

#include <utility>

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
        base::WeakPtr<WebBluetoothServiceImpl> web_bluetooth_service,
        RenderFrameHost& render_frame_host)
    : web_bluetooth_service_(std::move(web_bluetooth_service)),
      render_frame_host_token_(render_frame_host.GetGlobalFrameToken()) {}

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

  // Non-active RFHs can't show UI elements like prompts to the user.
  RenderFrameHost* render_frame_host =
      RenderFrameHost::FromFrameToken(render_frame_host_token_);
  if (!render_frame_host || !render_frame_host->IsActive()) {
    return;
  }

  auto* delegate = GetContentClient()->browser()->GetBluetoothDelegate();
  if (!delegate) {
    return;
  }

  // The delegate's prompt implementation may spin a nested message loop (e.g.
  // to drop fullscreen), during which the frame may be detached and the
  // controller destroyed. In addition, the prompt event handler could be
  // invoked synchronously. Check that the controller is still alive and that
  // the event hasn't already been handled before assigning `prompt_`.
  auto weak_this = weak_ptr_factory_.GetWeakPtr();
  auto prompt = delegate->ShowBluetoothScanningPrompt(
      render_frame_host, std::move(prompt_event_handler));
  if (!weak_this || prompt_event_received_) {
    return;
  }
  // If the delegate fails to create a prompt or prompt UI is unsupported,
  // cancel immediately so the request does not hang.
  if (!prompt) {
    OnBluetoothScanningPromptEvent(BluetoothScanningPrompt::Event::kCanceled);
    return;
  }
  prompt_ = std::move(prompt);
}

void BluetoothDeviceScanningPromptController::OnBluetoothScanningPromptEvent(
    BluetoothScanningPrompt::Event event) {
  CHECK(web_bluetooth_service_, base::NotFatalUntil::M160);

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
