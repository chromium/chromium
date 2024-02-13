// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"
#include "third_party/blink/public/mojom/webpreferences/web_preferences.mojom.h"
#include "ui/base/ui_base_switches_util.h"

namespace {

bool IsTouchDragDropEnabled() {
  // Cache the enabled state so it isn't queried on every WebPreferences
  // creation. Note that this means unit tests can't override the state.
  static const bool enabled = switches::IsTouchDragDropEnabled();
  return enabled;
}

}  // namespace

namespace blink {

namespace web_pref {

using blink::mojom::EffectiveConnectionType;

// "Zyyy" is the ISO 15924 script code for undetermined script aka Common.
const char kCommonScript[] = "Zyyy";

WebPreferences::WebPreferences()
    : touch_drag_drop_enabled(IsTouchDragDropEnabled()) {
  standard_font_family_map[web_pref::kCommonScript] = u"Times New Roman";
#if BUILDFLAG(IS_MAC)
  fixed_font_family_map[web_pref::kCommonScript] = u"Menlo";
#else
  fixed_font_family_map[web_pref::kCommonScript] = u"Courier New";
#endif
  serif_font_family_map[web_pref::kCommonScript] = u"Times New Roman";
  sans_serif_font_family_map[web_pref::kCommonScript] = u"Arial";
  cursive_font_family_map[web_pref::kCommonScript] = u"Script";
  fantasy_font_family_map[web_pref::kCommonScript] = u"Impact";
  // Latin Modern Math is an open source font available in LaTeX distributions,
  // and consequently other installable system packages. It provides the default
  // "Computer Modern" style that math people are used to and contains an
  // OpenType MATH table for math layout. It is thus a good default choice which
  // may be refined via resource files for the Chrome profile, in order to take
  // into account platform-specific availability of math fonts.
  math_font_family_map[web_pref::kCommonScript] = u"Latin Modern Math";
}

WebPreferences::WebPreferences(const WebPreferences& other) = default;

WebPreferences::WebPreferences(WebPreferences&& other) = default;

WebPreferences::~WebPreferences() = default;

WebPreferences& WebPreferences::operator=(const WebPreferences& other) =
    default;

WebPreferences& WebPreferences::operator=(WebPreferences&& other) = default;

}  // namespace web_pref

}  // namespace blink
