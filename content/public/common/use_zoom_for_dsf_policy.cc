// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/use_zoom_for_dsf_policy.h"

#include "base/command_line.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
#include "base/feature_list.h"
#endif

namespace {

#if BUILDFLAG(IS_WIN)
const base::Feature kUseZoomForDsfEnabledByDefault{
    "use-zoom-for-dsf enabled by default", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

#if BUILDFLAG(IS_ANDROID)
const base::Feature kUseZoomForDsfEnabledByDefault{
    "use-zoom-for-dsf enabled by default", base::FEATURE_ENABLED_BY_DEFAULT};
#endif

bool IsUseZoomForDSFEnabledByDefault() {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_FUCHSIA) || \
    BUILDFLAG(IS_MAC)
  return true;
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kUseZoomForDsfEnabledByDefault);
#else
  return false;
#endif
}

}  // namespace

namespace content {

bool IsUseZoomForDSFEnabled() {
  static bool use_zoom_for_dsf_enabled_by_default =
      IsUseZoomForDSFEnabledByDefault();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  bool enabled = (command_line->HasSwitch(switches::kEnableUseZoomForDSF) ||
                  use_zoom_for_dsf_enabled_by_default) &&
                 command_line->GetSwitchValueASCII(
                     switches::kEnableUseZoomForDSF) != "false";

  return enabled;
}

}  // namespace content
