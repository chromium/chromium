// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/native_screen_capture_picker.h"

#include "content/browser/media/capture/native_screen_capture_picker_mac.h"
#include "media/base/media_switches.h"

namespace content {

std::unique_ptr<NativeScreenCapturePicker>
MaybeCreateNativeScreenCapturePicker() {
#if BUILDFLAG(IS_MAC)
  if (base::FeatureList::IsEnabled(media::kUseSCContentSharingPicker)) {
    return CreateNativeScreenCapturePickerMac();
  }
#endif
  return nullptr;
}

}  // namespace content
