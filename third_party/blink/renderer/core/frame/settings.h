/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/public/common/css/navigation_controls.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom-shared.h"
#include "third_party/blink/public/platform/pointer_properties.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_viewport_style.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_cache_options.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/events/add_event_listener_options_defaults.h"
#include "third_party/blink/renderer/core/editing/editing_behavior_types.h"
#include "third_party/blink/renderer/core/editing/selection_strategy.h"
#include "third_party/blink/renderer/core/frame/settings_delegate.h"
#include "third_party/blink/renderer/core/html/media/autoplay_policy.h"
#include "third_party/blink/renderer/core/html/track/text_track_kind_user_preference.h"
#include "third_party/blink/renderer/core/loader/frame_loader_types.h"
#include "third_party/blink/renderer/core/settings_macros.h"
#include "third_party/blink/renderer/platform/fonts/generic_font_family_settings.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"
#include "third_party/blink/renderer/platform/graphics/image_animation_policy.h"
#include "third_party/blink/renderer/platform/timer.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

class CORE_EXPORT Settings {
  USING_FAST_MALLOC(Settings);

 public:
  Settings();

  GenericFontFamilySettings& GetGenericFontFamilySettings() {
    return generic_font_family_settings_;
  }
  void NotifyGenericFontFamilyChange() {
    Invalidate(SettingsDelegate::kFontFamilyChange);
  }

  void SetTextAutosizingEnabled(bool);
  bool TextAutosizingEnabled() const { return text_autosizing_enabled_; }

  void SetBypassCSP(bool enabled) { bypass_csp_ = enabled; }
  bool BypassCSP() const { return bypass_csp_; }

  // Only set by web tests, and only used if TextAutosizingEnabled() returns
  // true.
  void SetTextAutosizingWindowSizeOverride(const IntSize&);
  const IntSize& TextAutosizingWindowSizeOverride() const {
    return text_autosizing_window_size_override_;
  }

  void SetForceDarkModeEnabled(bool enabled);
  bool ForceDarkModeEnabled() const { return force_dark_mode_; }

  SETTINGS_GETTERS_AND_SETTERS

  void SetDelegate(SettingsDelegate*);

 private:
  void Invalidate(SettingsDelegate::ChangeType);

  SettingsDelegate* delegate_;

  GenericFontFamilySettings generic_font_family_settings_;
  IntSize text_autosizing_window_size_override_;
  bool text_autosizing_enabled_ : 1;
  bool bypass_csp_ = false;
  bool force_dark_mode_ = false;

  SETTINGS_MEMBER_VARIABLES

  DISALLOW_COPY_AND_ASSIGN(Settings);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_SETTINGS_H_
