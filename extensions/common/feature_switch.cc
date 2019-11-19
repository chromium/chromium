// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/feature_switch.h"

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/strings/string_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "extensions/common/switches.h"

namespace extensions {

namespace {

// The switch load-media-router-component-extension is defined in
// chrome/common/chrome_switches.cc, but we can't depend on chrome here.
const char kLoadMediaRouterComponentExtensionFlag[] =
    "load-media-router-component-extension";

class CommonSwitches {
 public:
  CommonSwitches()
      : force_dev_mode_highlighting(switches::kForceDevModeHighlighting,
                                    FeatureSwitch::DEFAULT_DISABLED),
        // Intentionally no flag since turning this off outside of tests
        // is a security risk.
        prompt_for_external_extensions(nullptr,
#if defined(OS_WIN) || defined(OS_MACOSX)
                                       FeatureSwitch::DEFAULT_ENABLED),
#else
                                       FeatureSwitch::DEFAULT_DISABLED),
#endif
        error_console(switches::kErrorConsole, FeatureSwitch::DEFAULT_ENABLED),
        enable_override_bookmarks_ui(switches::kEnableOverrideBookmarksUI,
                                     FeatureSwitch::DEFAULT_DISABLED),
        embedded_extension_options(switches::kEmbeddedExtensionOptions,
                                   FeatureSwitch::DEFAULT_DISABLED),
        trace_app_source(switches::kTraceAppSource,
                         FeatureSwitch::DEFAULT_ENABLED),
        load_media_router_component_extension(
            kLoadMediaRouterComponentExtensionFlag,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
            FeatureSwitch::DEFAULT_ENABLED)
#else
            FeatureSwitch::DEFAULT_DISABLED)
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
  {
  }

  FeatureSwitch force_dev_mode_highlighting;

  // Should we prompt the user before allowing external extensions to install?
  // Default is yes.
  FeatureSwitch prompt_for_external_extensions;

  FeatureSwitch error_console;
  FeatureSwitch enable_override_bookmarks_ui;
  FeatureSwitch embedded_extension_options;
  FeatureSwitch trace_app_source;
  FeatureSwitch load_media_router_component_extension;
};

base::LazyInstance<CommonSwitches>::DestructorAtExit g_common_switches =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

FeatureSwitch* FeatureSwitch::force_dev_mode_highlighting() {
  return &g_common_switches.Get().force_dev_mode_highlighting;
}
FeatureSwitch* FeatureSwitch::prompt_for_external_extensions() {
  return &g_common_switches.Get().prompt_for_external_extensions;
}
FeatureSwitch* FeatureSwitch::error_console() {
  return &g_common_switches.Get().error_console;
}
FeatureSwitch* FeatureSwitch::enable_override_bookmarks_ui() {
  return &g_common_switches.Get().enable_override_bookmarks_ui;
}
FeatureSwitch* FeatureSwitch::embedded_extension_options() {
  return &g_common_switches.Get().embedded_extension_options;
}
FeatureSwitch* FeatureSwitch::trace_app_source() {
  return &g_common_switches.Get().trace_app_source;
}
FeatureSwitch* FeatureSwitch::load_media_router_component_extension() {
  return &g_common_switches.Get().load_media_router_component_extension;
}

FeatureSwitch::ScopedOverride::ScopedOverride(FeatureSwitch* feature,
                                              bool override_value)
    : feature_(feature),
      previous_value_(feature->GetOverrideValue()) {
  feature_->SetOverrideValue(
      override_value ? OVERRIDE_ENABLED : OVERRIDE_DISABLED);
}

FeatureSwitch::ScopedOverride::~ScopedOverride() {
  feature_->SetOverrideValue(previous_value_);
}

FeatureSwitch::FeatureSwitch(const char* switch_name,
                             DefaultValue default_value)
    : FeatureSwitch(base::CommandLine::ForCurrentProcess(),
                    switch_name,
                    default_value) {}

FeatureSwitch::FeatureSwitch(const base::CommandLine* command_line,
                             const char* switch_name,
                             DefaultValue default_value)
    : command_line_(command_line),
      switch_name_(switch_name),
      default_value_(default_value == DEFAULT_ENABLED),
      override_value_(OVERRIDE_NONE) {}

bool FeatureSwitch::IsEnabled() const {
  if (override_value_ != OVERRIDE_NONE)
    return override_value_ == OVERRIDE_ENABLED;
  if (!cached_value_.has_value())
    cached_value_ = ComputeValue();
  return cached_value_.value();
}

bool FeatureSwitch::ComputeValue() const {
  if (!switch_name_)
    return default_value_;

  std::string temp = command_line_->GetSwitchValueASCII(switch_name_);
  std::string switch_value;
  base::TrimWhitespaceASCII(temp, base::TRIM_ALL, &switch_value);

  if (switch_value == "1")
    return true;

  if (switch_value == "0")
    return false;

  if (command_line_->HasSwitch(GetLegacyEnableFlag()))
    return true;

  if (command_line_->HasSwitch(GetLegacyDisableFlag()))
    return false;

  return default_value_;
}

bool FeatureSwitch::HasValue() const {
  return override_value_ != OVERRIDE_NONE ||
         command_line_->HasSwitch(switch_name_) ||
         command_line_->HasSwitch(GetLegacyEnableFlag()) ||
         command_line_->HasSwitch(GetLegacyDisableFlag());
}

std::string FeatureSwitch::GetLegacyEnableFlag() const {
  DCHECK(switch_name_);
  return std::string("enable-") + switch_name_;
}

std::string FeatureSwitch::GetLegacyDisableFlag() const {
  DCHECK(switch_name_);
  return std::string("disable-") + switch_name_;
}

void FeatureSwitch::SetOverrideValue(OverrideValue override_value) {
  override_value_ = override_value;
}

FeatureSwitch::OverrideValue FeatureSwitch::GetOverrideValue() const {
  return override_value_;
}

}  // namespace extensions
