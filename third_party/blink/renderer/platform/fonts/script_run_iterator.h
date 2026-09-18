// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SCRIPT_RUN_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SCRIPT_RUN_ITERATOR_H_

#include <unicode/uchar.h>
#include <unicode/uscript.h>

#include <bitset>

#include "base/containers/span.h"
#include "base/synchronization/lock.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ScriptData;

class PLATFORM_EXPORT ScriptRunIterator {
  STACK_ALLOCATED();

 public:
  explicit ScriptRunIterator(base::span<const UChar> text);

  // This maintains a reference to data. It must exist for the lifetime of
  // this object. Typically data is a singleton that exists for the life of
  // the process.
  ScriptRunIterator(base::span<const UChar> text, const ScriptData*);

  ScriptRunIterator(const ScriptRunIterator&) = delete;
  ScriptRunIterator& operator=(const ScriptRunIterator&) = delete;

  bool Consume(unsigned* limit, UScriptCode*);

  static constexpr int kMaxUnicodeScriptExtensions = 23;
  static constexpr int kMaxScriptCount = kMaxUnicodeScriptExtensions + 1;
  using UScriptCodeList = Vector<UScriptCode, kMaxScriptCount>;

 private:
  struct BracketRec {
    DISALLOW_NEW();
    UChar32 ch;
    UScriptCode script;
  };
  void OpenBracket(UChar32);
  void CloseBracket(UChar32);
  bool MergeSets();
  void FixupStack(UScriptCode resolved_script, bool exclude_last);
  bool Fetch(wtf_size_t* pos, UChar32*);
  bool FetchNextCharacter();

  UScriptCode ResolveCurrentScript() const;

  const UChar* text_;
  const wtf_size_t length_;

  Deque<BracketRec> brackets_;
  wtf_size_t brackets_fixup_depth_ = 0;
  // Limit max brackets so that the bracket tracking buffer does not grow
  // excessively large when processing long runs of text.
  static const int kMaxBrackets = 32;

  UScriptCodeList current_set_;
  // Because next_set_ and ahead_set_ are swapped as we consume characters, and
  // swapping inlined vector is not cheap, next_set_ and ahead_set_ are
  // pointers.
  std::unique_ptr<UScriptCodeList> next_set_ =
      std::make_unique<UScriptCodeList>();
  std::unique_ptr<UScriptCodeList> ahead_set_ =
      std::make_unique<UScriptCodeList>();

  // The initial value of ahead_character_ is not used.
  UChar32 ahead_character_ = 0;
  wtf_size_t ahead_pos_ = 0;

  UScriptCode common_preferred_ = USCRIPT_COMMON;

  const ScriptData* script_data_;
};

// ScriptData is a wrapper which returns a set of scripts for a particular
// character retrieved from the character's primary script and script
// extensions, as per ICU / Unicode data. ScriptData maintains a certain
// priority order of the returned values, which are essential for mergeSets
// method to work correctly.
class PLATFORM_EXPORT ScriptData {
  USING_FAST_MALLOC(ScriptData);

 protected:
  ScriptData() = default;

 public:
  ScriptData(const ScriptData&) = delete;
  ScriptData& operator=(const ScriptData&) = delete;
  virtual ~ScriptData();

  enum PairedBracketType {
    kBracketTypeNone,
    kBracketTypeOpen,
    kBracketTypeClose,
    kBracketTypeCount
  };

  static constexpr int kMaxUnicodeScriptExtensions =
      ScriptRunIterator::kMaxUnicodeScriptExtensions;
  static constexpr int kMaxScriptCount = ScriptRunIterator::kMaxScriptCount;
  using UScriptCodeList = ScriptRunIterator::UScriptCodeList;

  virtual void GetScripts(UChar32, UScriptCodeList& dst) const = 0;

  virtual UChar32 GetPairedBracket(UChar32) const = 0;

  virtual PairedBracketType GetPairedBracketType(UChar32) const = 0;

  static constexpr unsigned kFirstSurrogate = 0xD800;
  using UnicodeBitSet = std::bitset<kFirstSurrogate>;

  // Get the set of Unicode code points that would be allowed to skip
  // once we know which script we're in (i.e., current_set_ is a singleton
  // that's not common or inherited). This set is, generally, the set
  // of code points that either explicitly supports the given script,
  // or is classified as common/inherited. However, we exclude brackets,
  // which have special handling.
  //
  // This takes a bit of time to compute, so we cache it per-script
  // across the entire rendering process (ICUScriptData is already
  // a process-wide singleton). We also only care about the code points
  // up to U+D800 (which covers most of the BMP), in order to never
  // have to worry about surrogates; as long as the UChars we read
  // fit in the bitset, we can assume one codepoint == one UChar.
  //
  // The second return value is the set of inherited and not-common
  // characters (see inherited_not_common_chars_ for description).
  struct RunExtensionLookups {
    const ScriptData::UnicodeBitSet* can_remain_in_script;
    const ScriptData::UnicodeBitSet* inherited_not_common_chars;
  };
  virtual RunExtensionLookups GetSafeToExtendExistingRun(
      UScriptCode script) const = 0;
};

class PLATFORM_EXPORT ICUScriptData final : public ScriptData {
 public:
  ~ICUScriptData() override = default;

  static const ICUScriptData* Instance();

  void GetScripts(UChar32, UScriptCodeList& dst) const override;

  UChar32 GetPairedBracket(UChar32) const override;

  PairedBracketType GetPairedBracketType(UChar32) const override;

 private:
  RunExtensionLookups GetSafeToExtendExistingRun(
      UScriptCode script) const override;

  // For each script we've seen so far, a bitmap specifying which characters
  // are acceptable in that script and can be simply skipped (and that are not
  // brackets). Allowed to have false negatives.
  mutable HashMap<UScriptCode, std::unique_ptr<UnicodeBitSet>> bits_cache_
      GUARDED_BY(bits_cache_lock_);

  // There are some characters that have primary script USCRIPT_INHERITED
  // but a nonempty script extension list (scx); this means that is inherits
  // the previous character's script, but only if it is in the scx.
  // Specifically, FetchNextCharacter() handles this by overwriting
  // USCRIPT_COMMON script list by the following character's scx
  // (if it is inherited); so if there's e.g. a run of Latin characters
  // followed by a digit (normally common) and then a combining character
  // that's only valid for e.g. Indic scripts, then the digit should
  // _not_ be allowed to be part of the Latin run even though it's common.
  //
  // If this happens in the fast path where we skip characters, we would
  // have accepted the digit, and then need to unaccept it (go back one
  // character) when we see the inherited character one step later.
  // This bitmap notes which characters have that property.
  //
  // Note that for any given script, a character may be in both its bits_cache_
  // bitmap and this (because its scx contains the given script); if so,
  // the former takes precedence.
  mutable UnicodeBitSet inherited_not_common_chars_;

  mutable base::Lock bits_cache_lock_;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SCRIPT_RUN_ITERATOR_H_
