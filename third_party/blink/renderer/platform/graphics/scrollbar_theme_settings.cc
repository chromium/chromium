// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/scrollbar_theme_settings.h"

namespace blink {

static bool g_mock_scrollbars_enabled = false;
static bool g_overlay_scrollbars_enabled = false;
static bool g_fluent_scrollbars_enabled = false;

void ScrollbarThemeSettings::SetMockScrollbarsEnabled(bool flag) {
  g_mock_scrollbars_enabled = flag;
}

bool ScrollbarThemeSettings::MockScrollbarsEnabled() {
  return g_mock_scrollbars_enabled;
}

void ScrollbarThemeSettings::SetOverlayScrollbarsEnabled(bool flag) {
  g_overlay_scrollbars_enabled = flag;
}

bool ScrollbarThemeSettings::OverlayScrollbarsEnabled() {
  return g_overlay_scrollbars_enabled;
}

void ScrollbarThemeSettings::SetFluentScrollbarsEnabled(bool flag) {
  g_fluent_scrollbars_enabled = flag;
}

bool ScrollbarThemeSettings::FluentScrollbarsEnabled() {
  return g_fluent_scrollbars_enabled;
}

}  // namespace blink
