/*
 * Copyright (c) 2006, 2007, 2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_FALLBACK_WIN_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_FALLBACK_WIN_H_

#include <unicode/locid.h>
#include <unicode/uscript.h>

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/dwrite_font_proxy/dwrite_font_proxy.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/fonts/font_description.h"
#include "third_party/blink/renderer/platform/fonts/font_fallback_priority.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/skia/include/core/SkFontStyle.h"

class SkFontMgr;

namespace blink {

// Return a font family that can render |character| based on what script
// that characters belong to based on hard-coded tables that have been curated
// over time. When scriptChecked is non-zero, the script used to determine the
// family is returned.
PLATFORM_EXPORT const UChar* GetFallbackFamily(
    UChar32 character,
    FontDescription::GenericFamilyType,
    const LayoutLocale* content_locale,
    UScriptCode* script_checked,
    FontFallbackPriority,
    SkFontMgr* font_manager);

// Return a font family that can render |character| based on what script
// that characters belong to by performing an out of process lookup and using
// system fallback API based on IDWriteTextLayout. This method is only to be
// used on pre Windows 8.1, as otherwise IDWriteFontFallback API is available.
PLATFORM_EXPORT bool GetOutOfProcessFallbackFamily(
    UChar32 character,
    FontDescription::GenericFamilyType,
    String bcp47_language_tag,
    FontFallbackPriority,
    const mojo::Remote<mojom::blink::DWriteFontProxy>& font_proxy,
    String* fallback_family,
    SkFontStyle* fallback_style);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_WIN_FONT_FALLBACK_WIN_H_
