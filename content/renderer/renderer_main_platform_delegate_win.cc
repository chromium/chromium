// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_main_platform_delegate.h"

#include <dwrite.h>

#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_impl_win.h"
#include "content/child/font_warmup_win.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "content/renderer/render_thread_impl.h"
#include "sandbox/policy/switches.h"
#include "sandbox/policy/win/sandbox_warmup.h"
#include "sandbox/win/src/sandbox.h"
#include "third_party/blink/public/platform/web_runtime_features.h"
#include "third_party/blink/public/web/win/web_font_rendering.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/display/win/dpi.h"
#include "ui/gfx/win/direct_write.h"

namespace content {

RendererMainPlatformDelegate::RendererMainPlatformDelegate(
    const MainFunctionParams& parameters)
    : parameters_(parameters) {}

RendererMainPlatformDelegate::~RendererMainPlatformDelegate() {
}

void RendererMainPlatformDelegate::PlatformInitialize() {
  const base::CommandLine& command_line = *parameters_->command_line;

  // Be mindful of what resources you acquire here. They can be used by
  // malicious code if the renderer gets compromised.
  bool no_sandbox =
      command_line.HasSwitch(sandbox::policy::switches::kNoSandbox);

  if (!no_sandbox) {
    // ICU DateFormat class (used in base/time_format.cc) needs to get the
    // Olson timezone ID by accessing the registry keys under
    // HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Time Zones.
    // After TimeZone::createDefault is called once here, the timezone ID is
    // cached and there's no more need to access the registry. If the sandbox
    // is disabled, we don't have to make this dummy call.
    std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  }

  // Do not initialize DWriteFactory if the SkiaFontService feature is enabled
  // since this will conflict with the experimental font manager.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseSkiaFontManager)) {
    InitializeDWriteFontProxy();
  }
}

void RendererMainPlatformDelegate::PlatformUninitialize() {
  UninitializeDWriteFontProxy();
}

bool RendererMainPlatformDelegate::EnableSandbox() {
  sandbox::TargetServices* target_services =
      parameters_->sandbox_info->target_services;

  if (target_services) {
    sandbox::policy::WarmupRandomnessInfrastructure();

    target_services->LowerToken();
    return true;
  }
  return false;
}

}  // namespace content
