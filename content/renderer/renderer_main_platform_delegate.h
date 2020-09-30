// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
#define CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/macros.h"
#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"

#if defined(OS_FUCHSIA)
#include "content/renderer/media/audio/fuchsia_audio_device_factory.h"
#endif  // defined(OS_FUCHSIA)

namespace content {

class CONTENT_EXPORT RendererMainPlatformDelegate {
 public:
  explicit RendererMainPlatformDelegate(
      const MainFunctionParams& parameters);
  ~RendererMainPlatformDelegate();

  // Called first thing and last thing in the process' lifecycle, i.e. before
  // the sandbox is enabled.
  void PlatformInitialize();
  void PlatformUninitialize();

  // Initiate Lockdown, returns true on success.
  bool EnableSandbox();

 private:
#if defined(OS_WIN)
  const MainFunctionParams& parameters_;
#endif

#if defined(OS_FUCHSIA)
  std::unique_ptr<FuchsiaAudioDeviceFactory> fuchsia_audio_device_factory_;
#endif  // defined(OS_FUCHSIA)

  DISALLOW_COPY_AND_ASSIGN(RendererMainPlatformDelegate);
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
