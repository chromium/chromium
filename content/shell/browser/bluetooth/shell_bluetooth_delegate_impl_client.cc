// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/bluetooth/shell_bluetooth_delegate_impl_client.h"

#include "build/build_config.h"
#include "content/public/browser/render_frame_host.h"

#if BUILDFLAG(IS_IOS)
#include "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_ios.h"
#endif

namespace content {

ShellBluetoothDelegateImplClient::ShellBluetoothDelegateImplClient() = default;

ShellBluetoothDelegateImplClient::~ShellBluetoothDelegateImplClient() = default;

permissions::BluetoothChooserContext*
ShellBluetoothDelegateImplClient::GetBluetoothChooserContext(
    RenderFrameHost* frame) {
  // TODO(crbug.com/40263537): Implement ShellBluetoothChooserContextFactory.
  return nullptr;
}

std::unique_ptr<content::BluetoothChooser>
ShellBluetoothDelegateImplClient::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
#if BUILDFLAG(IS_IOS)
  return std::make_unique<ShellBluetoothChooserIOS>(frame, event_handler);
#else
  return nullptr;
#endif
}

std::unique_ptr<BluetoothScanningPrompt>
ShellBluetoothDelegateImplClient::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  // TODO(crbug.com/40263537): Implement BluetoothScanningPrompt for iOS.
  return nullptr;
}

void ShellBluetoothDelegateImplClient::ShowBluetoothDevicePairDialog(
    RenderFrameHost* frame,
    const std::u16string& device_identifier,
    BluetoothDelegate::PairPromptCallback callback,
    BluetoothDelegate::PairingKind,
    const std::optional<std::u16string>& pin) {
  std::move(callback).Run(BluetoothDelegate::PairPromptResult(
      BluetoothDelegate::PairPromptStatus::kCancelled));
}
}  // namespace content
