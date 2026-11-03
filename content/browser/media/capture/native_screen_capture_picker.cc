// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker.h"

#if BUILDFLAG(IS_MAC)
#include "base/command_line.h"
#include "content/browser/media/capture/fake_screen_capture_picker_mac.h"
#include "content/browser/media/capture/native_screen_capture_picker_mac.h"
#include "media/base/media_switches.h"
#endif

namespace content {

std::unique_ptr<NativeScreenCapturePicker>
MaybeCreateNativeScreenCapturePicker() {
#if BUILDFLAG(IS_MAC)
  if (!base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker)) {
    return nullptr;
  }
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUseFakeNativeScreenCapturePicker)) {
    // The optional switch value filters the browser window by title.
    std::optional<std::string> window_title;
    std::string switch_value = command_line->GetSwitchValueUTF8(
        switches::kUseFakeNativeScreenCapturePicker);
    if (!switch_value.empty()) {
      window_title = std::move(switch_value);
    }
    return CreateFakeScreenCapturePickerMac(std::move(window_title));
  }
  return CreateNativeScreenCapturePickerMac();
#else
  return nullptr;
#endif
}

}  // namespace content
