// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/input/keyboard_shortcut_recorder.h"
#include "base/metrics/histogram_macros.h"

namespace blink {

#if BUILDFLAG(IS_ANDROID)
void RecordKeyboardShortcutForAndroid(
    const KeyboardShortcut keyboard_shortcut) {
  // This call must be identical with the call in
  // content/public/android/java/src/org/chromium/content_public/browser/KeyboardShortcutRecorder.java
  UMA_HISTOGRAM_ENUMERATION("InputMethod.PhysicalKeyboard.KeyboardShortcut",
                            keyboard_shortcut);
}
#endif  // BUILDFLAG(IS_ANDROID)
}  // namespace blink
