// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {}

void RendererMainPlatformDelegate::PlatformInitialize() {
  fuchsia_audio_device_factory_ = std::make_unique<FuchsiaAudioDeviceFactory>();
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
  fuchsia_audio_device_factory_.reset();
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  return true;
}

}  // namespace content
