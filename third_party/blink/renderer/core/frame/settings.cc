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

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

namespace {

// For generated Settings::SetFromStrings().
template <typename T>
struct FromString {
  T operator()(const String& s) { return static_cast<T>(s.ToInt()); }
};

template <>
struct FromString<String> {
  const String& operator()(const String& s) { return s; }
};

template <>
struct FromString<bool> {
  bool operator()(const String& s) { return s.empty() || s == "true"; }
};

template <>
struct FromString<float> {
  float operator()(const String& s) { return s.ToFloat(); }
};

template <>
struct FromString<double> {
  double operator()(const String& s) { return s.ToDouble(); }
};

template <>
struct FromString<gfx::Size> {
  gfx::Size operator()(const String& s) {
    Vector<String> fields;
    s.Split(',', fields);
    return gfx::Size(fields.size() > 0 ? fields[0].ToInt() : 0,
                     fields.size() > 1 ? fields[1].ToInt() : 0);
  }
};

}  // namespace

// NOTEs
//  1) EditingMacBehavior comprises builds on Mac;
//  2) EditingWindowsBehavior comprises builds on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but
//     Darwin/MacOS/Android (and then abusing the terminology);
//  4) EditingAndroidBehavior comprises Android builds.
// 99) MacEditingBehavior is used a fallback.
static mojom::blink::EditingBehavior EditingBehaviorTypeForPlatform() {
  return
#if BUILDFLAG(IS_MAC)
      mojom::blink::EditingBehavior::kEditingMacBehavior
#elif BUILDFLAG(IS_WIN)
      mojom::blink::EditingBehavior::kEditingWindowsBehavior
#elif BUILDFLAG(IS_ANDROID)
      mojom::blink::EditingBehavior::kEditingAndroidBehavior
#elif BUILDFLAG(IS_CHROMEOS)
      mojom::blink::EditingBehavior::kEditingChromeOSBehavior
#else  // Rest of the UNIX-like systems
      mojom::blink::EditingBehavior::kEditingUnixBehavior
#endif
      ;
}

#if BUILDFLAG(IS_WIN)
static const bool kDefaultSelectTrailingWhitespaceEnabled = true;
#else
static const bool kDefaultSelectTrailingWhitespaceEnabled = false;
#endif

Settings::Settings() : delegate_(nullptr) SETTINGS_INITIALIZER_LIST {}

SETTINGS_SETTER_BODIES

void Settings::SetDelegate(SettingsDelegate* delegate) {
  delegate_ = delegate;
}

void Settings::Invalidate(SettingsDelegate::ChangeType change_type) {
  if (delegate_)
    delegate_->SettingsChanged(change_type);
}

}  // namespace blink
