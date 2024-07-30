// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FEATURE_SWITCH_H_
#define EXTENSIONS_COMMON_FEATURE_SWITCH_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"

namespace base {
class CommandLine;
}

namespace extensions {

// A switch that can turn a feature on or off. Typically controlled via
// command-line switches but can be overridden, e.g., for testing.
// A note about priority:
// 1. If an override is present, the override state will be used.
// 2. If there is no switch name, the default value will be used. This is
//    because certain features are specifically designed *not* to be able to
//    be turned off via command-line, so we can't consult it.
// 3. If there is a switch name, and the switch is present in the command line,
//    the command line value will be used.
// 4. Otherwise, the default value is used.
class FeatureSwitch {
 public:
  static FeatureSwitch* prompt_for_external_extensions();
  static FeatureSwitch* embedded_extension_options();
  static FeatureSwitch* trace_app_source();

  enum DefaultValue {
    DEFAULT_ENABLED,
    DEFAULT_DISABLED
  };

  enum OverrideValue {
    OVERRIDE_NONE,
    OVERRIDE_ENABLED,
    OVERRIDE_DISABLED
  };

  // A temporary override for the switch value.
  class ScopedOverride {
   public:
    ScopedOverride(FeatureSwitch* feature, bool override_value);

    ScopedOverride(const ScopedOverride&) = delete;
    ScopedOverride& operator=(const ScopedOverride&) = delete;

    ~ScopedOverride();
   private:
    raw_ptr<FeatureSwitch> feature_;
    FeatureSwitch::OverrideValue previous_value_;
  };

  // |switch_name| can be null, in which case the feature is controlled solely
  // by the default and override values.
  FeatureSwitch(const char* switch_name,
                DefaultValue default_value);
  FeatureSwitch(const base::CommandLine* command_line,
                const char* switch_name,
                DefaultValue default_value);

  FeatureSwitch(const FeatureSwitch&) = delete;
  FeatureSwitch& operator=(const FeatureSwitch&) = delete;

  ~FeatureSwitch();

  // Consider using ScopedOverride instead.
  void SetOverrideValue(OverrideValue value);
  OverrideValue GetOverrideValue() const;

  bool HasValue() const;
  bool IsEnabled() const;

 private:
  std::string GetLegacyEnableFlag() const;
  std::string GetLegacyDisableFlag() const;
  bool ComputeValue() const;

  // TODO(crbug.com/40269737): detect under BRP.
  raw_ptr<const base::CommandLine, DanglingUntriaged> command_line_;
  const char* switch_name_;
  bool default_value_;
  OverrideValue override_value_;
  mutable std::optional<bool> cached_value_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FEATURE_SWITCH_H_
