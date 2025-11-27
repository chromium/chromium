// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/dark_mode_settings_builder.h"

#include <string>
#include <unordered_map>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

namespace {

// Default values for dark mode settings.
const constexpr int kDefaultForegroundBrightnessThreshold = 150;
const constexpr int kDefaultBackgroundBrightnessThreshold = 205;

template <typename T>
T Clamp(T value, T min_value, T max_value) {
  return std::max(min_value, std::min(value, max_value));
}

DarkModeSettings BuildDarkModeSettings() {
  DarkModeSettings settings;
  settings.foreground_brightness_threshold =
      Clamp<int>(kDefaultForegroundBrightnessThreshold, 0, 255);
  settings.background_brightness_threshold =
      Clamp<int>(kDefaultBackgroundBrightnessThreshold, 0, 255);
  return settings;
}

}  // namespace

// Always get the dark mode settings using this function.
const DarkModeSettings& GetCurrentDarkModeSettings() {
  static DarkModeSettings settings = BuildDarkModeSettings();
  return settings;
}

}  // namespace blink
