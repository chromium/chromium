// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_

#include <memory>

#include "build/build_config.h"

static_assert(BUILDFLAG(IS_MAC));

namespace content {

class NativeScreenCapturePicker;

std::unique_ptr<NativeScreenCapturePicker> CreateNativeScreenCapturePickerMac();

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_NATIVE_SCREEN_CAPTURE_PICKER_MAC_H_
