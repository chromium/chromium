// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_ios.h"

#import "base/functional/bind.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/permissions/constants.h"
#import "components/permissions/permission_util.h"
#import "components/strings/grit/components_strings.h"
#import "content/public/browser/render_frame_host.h"
#import "content/public/browser/web_contents.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_coordinator.h"
#import "content/shell/browser/bluetooth/ios/shell_bluetooth_chooser_mediator.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/gfx/native_widget_types.h"
#import "url/origin.h"

namespace content {

namespace {

std::u16string CreateChooserTitle(RenderFrameHost* render_frame_host) {
  if (!render_frame_host) {
    return u"";
  }
  // Ensure the permission request is attributed to the main frame.
  render_frame_host = render_frame_host->GetMainFrame();

  const url::Origin& origin = render_frame_host->GetLastCommittedOrigin();
  return l10n_util::GetStringFUTF16(IDS_BLUETOOTH_DEVICE_CHOOSER_PROMPT,
                                    base::UTF8ToUTF16(origin.Serialize()));
}

}  // namespace

ShellBluetoothChooserIOS::ShellBluetoothChooserIOS(
    RenderFrameHost* frame,
    const EventHandler& event_handler)
    : web_contents_(WebContents::FromRenderFrameHost(frame)),
      event_handler_(event_handler) {
  gfx::NativeWindow gfx_window = web_contents_->GetTopLevelNativeWindow();
  shell_bluetooth_chooser_coordinator_holder_ =
      std::make_unique<ShellBluetoothChooserCoordinatorHolder>(
          gfx_window, this, CreateChooserTitle(frame));
}

ShellBluetoothChooserIOS::~ShellBluetoothChooserIOS() = default;

void ShellBluetoothChooserIOS::AddOrUpdateDevice(
    const std::string& device_id,
    bool should_update_name,
    const std::u16string& device_name,
    bool is_gatt_connected,
    bool is_paired,
    int signal_strength_level) {
  [shell_bluetooth_chooser_coordinator_holder_->getMediator()
        addOrUpdateDevice:base::SysUTF8ToNSString(device_id)
         shouldUpdateName:should_update_name
               deviceName:base::SysUTF16ToNSString(device_name)
            gattConnected:is_gatt_connected
      signalStrengthLevel:signal_strength_level];
}

void ShellBluetoothChooserIOS::OnDialogFinished(DialogClosedState state,
                                                std::string& device_id) {
  switch (state) {
    case DialogClosedState::kDialogCanceled:
      event_handler_.Run(BluetoothChooserEvent::CANCELLED, "");
      return;
    case DialogClosedState::kDialogItemSelected:
      event_handler_.Run(BluetoothChooserEvent::SELECTED, device_id);
      return;
  }
  NOTREACHED();
}

}  // namespace content
