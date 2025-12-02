// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/child/font_integration_init.h"

#include "base/command_line.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_WIN)
#include "content/public/child/dwrite_font_proxy_init_win.h"
#endif
#include "content/child/font_data/font_data_manager.h"
#include "content/common/features.h"
#include "content/public/common/content_switches.h"

namespace content {

void InitializeFontIntegration() {
  bool is_single_process = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
#if BUILDFLAG(IS_WIN)
  // Do not initialize DWriteFactory if the feature flag is enabled
  // since this will conflict with the experimental font manager.
  // Fallback to only DWrite if running in single process mode, since there can
  // only be a single font manager and the browser process always has a DWrite
  // one.
  if (is_single_process || !features::IsFontDataServiceEnabled()) {
    content::InitializeDWriteFontProxy();
    return;
  }
#endif

  // On all platforms that support font data service, don't create it in single
  // process mode because there's already a process-local font manager.
  if (!is_single_process && features::IsFontDataServiceEnabled()) {
    font_data_service::FontDataManager::CreateAndInitialize();
  }
}

void UninitializeFontIntegration() {
#if BUILDFLAG(IS_WIN)
  // This is safe to call even if dwrite font proxy is not initialized, so no
  // need to check experiments.
  content::UninitializeDWriteFontProxy();
#endif
}

}  // namespace content
