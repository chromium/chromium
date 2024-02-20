// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
#define CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_

#include "base/memory/raw_ref.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "content/common/content_export.h"
#include "content/public/common/main_function_params.h"

namespace content {

class CONTENT_EXPORT RendererMainPlatformDelegate {
 public:
  explicit RendererMainPlatformDelegate(
      const MainFunctionParams& parameters);

  RendererMainPlatformDelegate(const RendererMainPlatformDelegate&) = delete;
  RendererMainPlatformDelegate& operator=(const RendererMainPlatformDelegate&) =
      delete;

  ~RendererMainPlatformDelegate();

  // Called first thing and last thing in the process' lifecycle, i.e. before
  // the sandbox is enabled.
  void PlatformInitialize();
  void PlatformUninitialize();

  // Initiate Lockdown, returns true on success.
  bool EnableSandbox();

 private:
#if BUILDFLAG(IS_WIN)
  const raw_ref<const MainFunctionParams> parameters_;
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_RENDERER_MAIN_PLATFORM_DELEGATE_H_
