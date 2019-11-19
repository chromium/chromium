/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_GENERIC_FONT_FAMILY_SETTINGS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_GENERIC_FONT_FAMILY_SETTINGS_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <unicode/uscript.h>

namespace blink {

class PLATFORM_EXPORT GenericFontFamilySettings {
  DISALLOW_NEW();

 public:
  GenericFontFamilySettings() = default;

  explicit GenericFontFamilySettings(const GenericFontFamilySettings&);

  bool UpdateStandard(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Standard(UScriptCode = USCRIPT_COMMON) const;

  bool UpdateFixed(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Fixed(UScriptCode = USCRIPT_COMMON) const;

  bool UpdateSerif(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Serif(UScriptCode = USCRIPT_COMMON) const;

  bool UpdateSansSerif(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& SansSerif(UScriptCode = USCRIPT_COMMON) const;

  bool UpdateCursive(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Cursive(UScriptCode = USCRIPT_COMMON) const;

  bool UpdateFantasy(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Fantasy(UScriptCode = USCRIPT_COMMON) const;

  bool UpdatePictograph(const AtomicString&, UScriptCode = USCRIPT_COMMON);
  const AtomicString& Pictograph(UScriptCode = USCRIPT_COMMON) const;

  // Only called by InternalSettings to clear font family maps.
  void Reset();

  GenericFontFamilySettings& operator=(const GenericFontFamilySettings&);

  // Returns a new instance with String instead of AtomicString objects.
  // This allows GenericFontFamilySettings to be sent from one thread to
  // another, since AtomicStrings can't be shared cross-threads.
  // Before using it, call it MakeAtomic() on the final thread, to bring back
  // the AtomicStrings.
  void IsolatedCopyTo(GenericFontFamilySettings& dest) const;

  bool IsIsolated() const { return isolated_copy_.get(); }

  // Transform an IsolatedCopy GenericFontFamilySettings into a regular
  // GenericFontFamilySettings.
  void MakeAtomic();

 private:
  // UScriptCode uses -1 and 0 for UScriptInvalidCode and UScriptCommon.
  // We need to use -2 and -3 for empty value and deleted value.
  struct UScriptCodeHashTraits : WTF::GenericHashTraits<int> {
    STATIC_ONLY(UScriptCodeHashTraits);
    static const bool kEmptyValueIsZero = false;
    static int EmptyValue() { return -2; }
    static void ConstructDeletedValue(int& slot, bool) { slot = -3; }
    static bool IsDeletedValue(int value) { return value == -3; }
  };

  typedef HashMap<int,
                  AtomicString,
                  DefaultHash<int>::Hash,
                  UScriptCodeHashTraits>
      ScriptFontFamilyMap;

  void SetGenericFontFamilyMap(ScriptFontFamilyMap&,
                               const AtomicString&,
                               UScriptCode);
  const AtomicString& GenericFontFamilyForScript(const ScriptFontFamilyMap&,
                                                 UScriptCode) const;

  ScriptFontFamilyMap standard_font_family_map_;
  ScriptFontFamilyMap serif_font_family_map_;
  ScriptFontFamilyMap fixed_font_family_map_;
  ScriptFontFamilyMap sans_serif_font_family_map_;
  ScriptFontFamilyMap cursive_font_family_map_;
  ScriptFontFamilyMap fantasy_font_family_map_;
  ScriptFontFamilyMap pictograph_font_family_map_;

  typedef Vector<std::pair<int, String>> IsolatedCopyVector;
  std::unique_ptr<IsolatedCopyVector[]> isolated_copy_;
};

}  // namespace blink

#endif
