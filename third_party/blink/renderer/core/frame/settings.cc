/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/frame/settings.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/graphics/dark_mode_settings.h"

namespace blink {

// NOTEs
//  1) EditingMacBehavior comprises builds on Mac;
//  2) EditingWindowsBehavior comprises builds on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but
//     Darwin/MacOS/Android (and then abusing the terminology);
//  4) EditingAndroidBehavior comprises Android builds.
// 99) MacEditingBehavior is used a fallback.
static EditingBehaviorType EditingBehaviorTypeForPlatform() {
  return
#if defined(OS_MACOSX)
      kEditingMacBehavior
#elif defined(OS_WIN)
      kEditingWindowsBehavior
#elif defined(OS_ANDROID)
      kEditingAndroidBehavior
#else  // Rest of the UNIX-like systems
      kEditingUnixBehavior
#endif
      ;
}

#if defined(OS_WIN)
static const bool kDefaultSelectTrailingWhitespaceEnabled = true;
#else
static const bool kDefaultSelectTrailingWhitespaceEnabled = false;
#endif

Settings::Settings()
    : text_autosizing_enabled_(false) SETTINGS_INITIALIZER_LIST {}

SETTINGS_SETTER_BODIES

void Settings::SetDelegate(SettingsDelegate* delegate) {
  delegate_ = delegate;
}

void Settings::Invalidate(SettingsDelegate::ChangeType change_type) {
  if (delegate_)
    delegate_->SettingsChanged(change_type);
}

void Settings::SetTextAutosizingEnabled(bool text_autosizing_enabled) {
  if (text_autosizing_enabled_ == text_autosizing_enabled)
    return;

  text_autosizing_enabled_ = text_autosizing_enabled;
  Invalidate(SettingsDelegate::kTextAutosizingChange);
}

// TODO: Move to Settings.json5 once make_settings can understand IntSize.
void Settings::SetTextAutosizingWindowSizeOverride(
    const IntSize& text_autosizing_window_size_override) {
  if (text_autosizing_window_size_override_ ==
      text_autosizing_window_size_override)
    return;

  text_autosizing_window_size_override_ = text_autosizing_window_size_override;
  Invalidate(SettingsDelegate::kTextAutosizingChange);
}

void Settings::SetForceDarkModeEnabled(bool enabled) {
  if (force_dark_mode_ == enabled)
    return;
  force_dark_mode_ = enabled;
  SetDarkModeEnabled(force_dark_mode_);
  Invalidate(SettingsDelegate::kColorSchemeChange);
}

}  // namespace blink
