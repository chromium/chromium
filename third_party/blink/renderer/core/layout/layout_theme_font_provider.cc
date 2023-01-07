/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
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

#include "third_party/blink/renderer/core/layout/layout_theme_font_provider.h"

#include "third_party/blink/renderer/core/css/font_size_functions.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

constexpr float kDefaultFontSizeFallback = 16.0;

// We aim to match IE here.
// -IE uses a font based on the encoding as the default font for form controls.
// -Gecko uses MS Shell Dlg (actually calls GetStockObject(DEFAULT_GUI_FONT),
// which returns MS Shell Dlg)
// -Safari uses Lucida Grande.
//
// FIXME: The only case where we know we don't match IE is for ANSI encodings.
// IE uses MS Shell Dlg there, which we render incorrectly at certain pixel
// sizes (e.g. 15px). So, for now we just use Arial.
const AtomicString& LayoutThemeFontProvider::DefaultGUIFont() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(const AtomicString, font_face, ("Arial"));
  return font_face;
}

float LayoutThemeFontProvider::DefaultFontSize(const Document* document) {
  const Settings* settings = document ? document->GetSettings() : nullptr;

  // The default font size setting may be uninitialized in some cases, like
  // in the calendar picker of an <input type=date> widget.
  if (!settings || !settings->GetDefaultFontSize())
    return kDefaultFontSizeFallback;

  static const unsigned keyword = FontSizeFunctions::InitialKeywordSize();
  static const bool is_monospace = false;
  return FontSizeFunctions::FontSizeForKeyword(document, keyword, is_monospace);
}

}  // namespace blink
