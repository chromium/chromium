// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/character.h"

#include <unicode/uvernum.h>
#include "third_party/blink/renderer/platform/text/icu_error.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"

#if defined(USING_SYSTEM_ICU) || (U_ICU_VERSION_MAJOR_NUM <= 61)
#include <unicode/uniset.h>

namespace {
Mutex& GetFreezePatternMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}
}  // namespace

#else
#include <unicode/uchar.h>
#endif

namespace blink {

// ICU 61 or earlier does not have up to date v11 Emoji properties, so we're
// temporarily uing our own functions again. Compare crbug.com/628333 Other than
// that: versions before 56 do not have an API for Emoji properties, but
// Chrome's copy of ICU 56 does.
#if defined(USING_SYSTEM_ICU) || (U_ICU_VERSION_MAJOR_NUM <= 61)
// The following UnicodeSet patterns were compiled from
// https://unicode.org/Public/emoji//11.0/emoji-data.txt

// The following patterns can be generated from the respective sections of the
// emoji_data.txt file by using the following Elisp function in Emacs.
// Known issues:
// 1) Does not insert the double [[ and ]] at the beginning and end of the
// pattern.
// 2) Does not insert \U0000 at the very last codepoint of a pattern.
//
// (defun convertemojidata ()
//   "Convert a section of the emoji_data.txt file to an ICU trie definition."
//   (interactive)
//   (goto-char 0)
//   (while (re-search-forward " *;.*$" nil t)
//     (replace-match "" nil nil))
//   (goto-char 0)
//   (while (re-search-forward "\\.\\." nil t)
//     (replace-match "-" nil nil))
//   (goto-char 0)
//   ; Pad 4 digit characters, step 1
//   (while (re-search-forward
//           "\\([^0-9A-F]*\\)\\([0-9A-F]\\{4\\}\\)\\([^0-9A-F]\\)"
//           nil t)
//     (replace-match "\\1\\\\U0000\\2\\3" nil nil))
//   (goto-char 0)
//   ; Fix up 5 digit characters padding, step 2
//   (while (re-search-forward "1\\\\U0000" nil t)
//     (replace-match "\\\\U0001" nil nil))
//   (goto-char 0)
//   (while (re-search-forward "^\\(.*\\)$" nil t)
//     (replace-match "[\\1]" nil nil))
//   (goto-char 0)
//   (replace-string "\n" " ")
//   (set-fill-column 72)
//   (goto-char 0)
//   (fill-paragraph)
//   (replace-string " " "")
//   (goto-char 0)
//   (while (re-search-forward "^\\(.*\\)$" nil t)
//     (replace-match "    R\"(\\1)\"" nil nil))
//   (goto-char 8)
//   (insert "[")
//   (goto-char (- (point-max) 3))
//   (insert "]")
//   )

static const char kEmojiTextPattern[] =
    R"([[\U00000023][\U0000002A][\U00000030-\U00000039][\U000000A9])"
    R"([\U000000AE][\U0000203C][\U00002049][\U00002122][\U00002139])"
    R"([\U00002194-\U00002199][\U000021A9-\U000021AA][\U0000231A-\U0000231B])"
    R"([\U00002328][\U000023CF][\U000023E9-\U000023F3])"
    R"([\U000023F8-\U000023FA][\U000024C2][\U000025AA-\U000025AB])"
    R"([\U000025B6][\U000025C0][\U000025FB-\U000025FE])"
    R"([\U00002600-\U00002604][\U0000260E][\U00002611])"
    R"([\U00002614-\U00002615][\U00002618][\U0000261D][\U00002620])"
    R"([\U00002622-\U00002623][\U00002626][\U0000262A])"
    R"([\U0000262E-\U0000262F][\U00002638-\U0000263A][\U00002640])"
    R"([\U00002642][\U00002648-\U00002653][\U0000265F-\U00002660])"
    R"([\U00002663][\U00002665-\U00002666][\U00002668][\U0000267B])"
    R"([\U0000267E-\U0000267F][\U00002692-\U00002697][\U00002699])"
    R"([\U0000269B-\U0000269C][\U000026A0-\U000026A1][\U000026AA-\U000026AB])"
    R"([\U000026B0-\U000026B1][\U000026BD-\U000026BE][\U000026C4-\U000026C5])"
    R"([\U000026C8][\U000026CE][\U000026CF][\U000026D1])"
    R"([\U000026D3-\U000026D4][\U000026E9-\U000026EA][\U000026F0-\U000026F5])"
    R"([\U000026F7-\U000026FA][\U000026FD][\U00002702][\U00002705])"
    R"([\U00002708-\U00002709][\U0000270A-\U0000270B][\U0000270C-\U0000270D])"
    R"([\U0000270F][\U00002712][\U00002714][\U00002716][\U0000271D])"
    R"([\U00002721][\U00002728][\U00002733-\U00002734][\U00002744])"
    R"([\U00002747][\U0000274C][\U0000274E][\U00002753-\U00002755])"
    R"([\U00002757][\U00002763-\U00002764][\U00002795-\U00002797])"
    R"([\U000027A1][\U000027B0][\U000027BF][\U00002934-\U00002935])"
    R"([\U00002B05-\U00002B07][\U00002B1B-\U00002B1C][\U00002B50])"
    R"([\U00002B55][\U00003030][\U0000303D][\U00003297][\U00003299])"
    R"([\U0001F004][\U0001F0CF][\U0001F170-\U0001F171][\U0001F17E])"
    R"([\U0001F17F][\U0001F18E][\U0001F191-\U0001F19A])"
    R"([\U0001F1E6-\U0001F1FF][\U0001F201-\U0001F202][\U0001F21A])"
    R"([\U0001F22F][\U0001F232-\U0001F23A][\U0001F250-\U0001F251])"
    R"([\U0001F300-\U0001F320][\U0001F321][\U0001F324-\U0001F32C])"
    R"([\U0001F32D-\U0001F32F][\U0001F330-\U0001F335][\U0001F336])"
    R"([\U0001F337-\U0001F37C][\U0001F37D][\U0001F37E-\U0001F37F])"
    R"([\U0001F380-\U0001F393][\U0001F396-\U0001F397][\U0001F399-\U0001F39B])"
    R"([\U0001F39E-\U0001F39F][\U0001F3A0-\U0001F3C4][\U0001F3C5])"
    R"([\U0001F3C6-\U0001F3CA][\U0001F3CB-\U0001F3CE][\U0001F3CF-\U0001F3D3])"
    R"([\U0001F3D4-\U0001F3DF][\U0001F3E0-\U0001F3F0][\U0001F3F3-\U0001F3F5])"
    R"([\U0001F3F7][\U0001F3F8-\U0001F3FF][\U0001F400-\U0001F43E])"
    R"([\U0001F43F][\U0001F440][\U0001F441][\U0001F442-\U0001F4F7])"
    R"([\U0001F4F8][\U0001F4F9-\U0001F4FC][\U0001F4FD][\U0001F4FF])"
    R"([\U0001F500-\U0001F53D][\U0001F549-\U0001F54A][\U0001F54B-\U0001F54E])"
    R"([\U0001F550-\U0001F567][\U0001F56F-\U0001F570][\U0001F573-\U0001F579])"
    R"([\U0001F57A][\U0001F587][\U0001F58A-\U0001F58D][\U0001F590])"
    R"([\U0001F595-\U0001F596][\U0001F5A4][\U0001F5A5][\U0001F5A8])"
    R"([\U0001F5B1-\U0001F5B2][\U0001F5BC][\U0001F5C2-\U0001F5C4])"
    R"([\U0001F5D1-\U0001F5D3][\U0001F5DC-\U0001F5DE][\U0001F5E1])"
    R"([\U0001F5E3][\U0001F5E8][\U0001F5EF][\U0001F5F3][\U0001F5FA])"
    R"([\U0001F5FB-\U0001F5FF][\U0001F600][\U0001F601-\U0001F610])"
    R"([\U0001F611][\U0001F612-\U0001F614][\U0001F615][\U0001F616])"
    R"([\U0001F617][\U0001F618][\U0001F619][\U0001F61A][\U0001F61B])"
    R"([\U0001F61C-\U0001F61E][\U0001F61F][\U0001F620-\U0001F625])"
    R"([\U0001F626-\U0001F627][\U0001F628-\U0001F62B][\U0001F62C])"
    R"([\U0001F62D][\U0001F62E-\U0001F62F][\U0001F630-\U0001F633])"
    R"([\U0001F634][\U0001F635-\U0001F640][\U0001F641-\U0001F642])"
    R"([\U0001F643-\U0001F644][\U0001F645-\U0001F64F][\U0001F680-\U0001F6C5])"
    R"([\U0001F6CB-\U0001F6CF][\U0001F6D0][\U0001F6D1-\U0001F6D2])"
    R"([\U0001F6E0-\U0001F6E5][\U0001F6E9][\U0001F6EB-\U0001F6EC])"
    R"([\U0001F6F0][\U0001F6F3][\U0001F6F4-\U0001F6F6])"
    R"([\U0001F6F7-\U0001F6F8][\U0001F6F9][\U0001F910-\U0001F918])"
    R"([\U0001F919-\U0001F91E][\U0001F91F][\U0001F920-\U0001F927])"
    R"([\U0001F928-\U0001F92F][\U0001F930][\U0001F931-\U0001F932])"
    R"([\U0001F933-\U0001F93A][\U0001F93C-\U0001F93E][\U0001F940-\U0001F945])"
    R"([\U0001F947-\U0001F94B][\U0001F94C][\U0001F94D-\U0001F94F])"
    R"([\U0001F950-\U0001F95E][\U0001F95F-\U0001F96B][\U0001F96C-\U0001F970])"
    R"([\U0001F973-\U0001F976][\U0001F97A][\U0001F97C-\U0001F97F])"
    R"([\U0001F980-\U0001F984][\U0001F985-\U0001F991][\U0001F992-\U0001F997])"
    R"([\U0001F998-\U0001F9A2][\U0001F9B0-\U0001F9B9][\U0001F9C0])"
    R"([\U0001F9C1-\U0001F9C2][\U0001F9D0-\U0001F9E6][\U0001F9E7-\U0001F9FF]])";

static const char kEmojiEmojiPattern[] =
    R"([[\U0000231A-\U0000231B][\U000023E9-\U000023EC][\U000023F0])"
    R"([\U000023F3][\U000025FD-\U000025FE][\U00002614-\U00002615])"
    R"([\U00002648-\U00002653][\U0000267F][\U00002693][\U000026A1])"
    R"([\U000026AA-\U000026AB][\U000026BD-\U000026BE][\U000026C4-\U000026C5])"
    R"([\U000026CE][\U000026D4][\U000026EA][\U000026F2-\U000026F3])"
    R"([\U000026F5][\U000026FA][\U000026FD][\U00002705])"
    R"([\U0000270A-\U0000270B][\U00002728][\U0000274C][\U0000274E])"
    R"([\U00002753-\U00002755][\U00002757][\U00002795-\U00002797])"
    R"([\U000027B0][\U000027BF][\U00002B1B-\U00002B1C][\U00002B50])"
    R"([\U00002B55][\U0001F004][\U0001F0CF][\U0001F18E])"
    R"([\U0001F191-\U0001F19A][\U0001F1E6-\U0001F1FF][\U0001F201])"
    R"([\U0001F21A][\U0001F22F][\U0001F232-\U0001F236])"
    R"([\U0001F238-\U0001F23A][\U0001F250-\U0001F251][\U0001F300-\U0001F320])"
    R"([\U0001F32D-\U0001F32F][\U0001F330-\U0001F335][\U0001F337-\U0001F37C])"
    R"([\U0001F37E-\U0001F37F][\U0001F380-\U0001F393][\U0001F3A0-\U0001F3C4])"
    R"([\U0001F3C5][\U0001F3C6-\U0001F3CA][\U0001F3CF-\U0001F3D3])"
    R"([\U0001F3E0-\U0001F3F0][\U0001F3F4][\U0001F3F8-\U0001F3FF])"
    R"([\U0001F400-\U0001F43E][\U0001F440][\U0001F442-\U0001F4F7])"
    R"([\U0001F4F8][\U0001F4F9-\U0001F4FC][\U0001F4FF])"
    R"([\U0001F500-\U0001F53D][\U0001F54B-\U0001F54E][\U0001F550-\U0001F567])"
    R"([\U0001F57A][\U0001F595-\U0001F596][\U0001F5A4])"
    R"([\U0001F5FB-\U0001F5FF][\U0001F600][\U0001F601-\U0001F610])"
    R"([\U0001F611][\U0001F612-\U0001F614][\U0001F615][\U0001F616])"
    R"([\U0001F617][\U0001F618][\U0001F619][\U0001F61A][\U0001F61B])"
    R"([\U0001F61C-\U0001F61E][\U0001F61F][\U0001F620-\U0001F625])"
    R"([\U0001F626-\U0001F627][\U0001F628-\U0001F62B][\U0001F62C])"
    R"([\U0001F62D][\U0001F62E-\U0001F62F][\U0001F630-\U0001F633])"
    R"([\U0001F634][\U0001F635-\U0001F640][\U0001F641-\U0001F642])"
    R"([\U0001F643-\U0001F644][\U0001F645-\U0001F64F][\U0001F680-\U0001F6C5])"
    R"([\U0001F6CC][\U0001F6D0][\U0001F6D1-\U0001F6D2])"
    R"([\U0001F6EB-\U0001F6EC][\U0001F6F4-\U0001F6F6][\U0001F6F7-\U0001F6F8])"
    R"([\U0001F6F9][\U0001F910-\U0001F918][\U0001F919-\U0001F91E])"
    R"([\U0001F91F][\U0001F920-\U0001F927][\U0001F928-\U0001F92F])"
    R"([\U0001F930][\U0001F931-\U0001F932][\U0001F933-\U0001F93A])"
    R"([\U0001F93C-\U0001F93E][\U0001F940-\U0001F945][\U0001F947-\U0001F94B])"
    R"([\U0001F94C][\U0001F94D-\U0001F94F][\U0001F950-\U0001F95E])"
    R"([\U0001F95F-\U0001F96B][\U0001F96C-\U0001F970][\U0001F973-\U0001F976])"
    R"([\U0001F97A][\U0001F97C-\U0001F97F][\U0001F980-\U0001F984])"
    R"([\U0001F985-\U0001F991][\U0001F992-\U0001F997][\U0001F998-\U0001F9A2])"
    R"([\U0001F9B0-\U0001F9B9][\U0001F9C0][\U0001F9C1-\U0001F9C2])"
    R"([\U0001F9D0-\U0001F9E6][\U0001F9E7-\U0001F9FF]])";

static const char kEmojiModifierBasePattern[] =
    R"([[\U0000261D][\U000026F9][\U0000270A-\U0000270B])"
    R"([\U0000270C-\U0000270D][\U0001F385][\U0001F3C2-\U0001F3C4])"
    R"([\U0001F3C7][\U0001F3CA][\U0001F3CB-\U0001F3CC])"
    R"([\U0001F442-\U0001F443][\U0001F446-\U0001F450][\U0001F466-\U0001F469])"
    R"([\U0001F46E][\U0001F470-\U0001F478][\U0001F47C])"
    R"([\U0001F481-\U0001F483][\U0001F485-\U0001F487][\U0001F4AA])"
    R"([\U0001F574-\U0001F575][\U0001F57A][\U0001F590])"
    R"([\U0001F595-\U0001F596][\U0001F645-\U0001F647][\U0001F64B-\U0001F64F])"
    R"([\U0001F6A3][\U0001F6B4-\U0001F6B6][\U0001F6C0][\U0001F6CC])"
    R"([\U0001F918][\U0001F919-\U0001F91C][\U0001F91E][\U0001F91F])"
    R"([\U0001F926][\U0001F930][\U0001F931-\U0001F932])"
    R"([\U0001F933-\U0001F939][\U0001F93D-\U0001F93E][\U0001F9B5-\U0001F9B6])"
    R"([\U0001F9B8-\U0001F9B9][\U0001F9D1-\U0001F9DD]])";

static void applyPatternAndFreezeIfEmpty(icu::UnicodeSet* unicodeSet,
                                         const char* pattern) {
  MutexLocker mutexLocker(GetFreezePatternMutex());
  if (!unicodeSet->isEmpty())
    return;
  ICUError err;
  // Use ICU's invariant-character initialization method.
  unicodeSet->applyPattern(icu::UnicodeString(pattern, -1, US_INV), err);
  unicodeSet->freeze();
  DCHECK_EQ(err, U_ZERO_ERROR);
}

bool Character::IsEmoji(UChar32 ch) {
  return Character::IsEmojiTextDefault(ch) ||
         Character::IsEmojiEmojiDefault(ch);
}

bool Character::IsEmojiTextDefault(UChar32 ch) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(icu::UnicodeSet, emojiTextSet, ());
  applyPatternAndFreezeIfEmpty(&emojiTextSet, kEmojiTextPattern);
  return emojiTextSet.contains(ch) && !IsEmojiEmojiDefault(ch);
}

bool Character::IsEmojiEmojiDefault(UChar32 ch) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(icu::UnicodeSet, emojiEmojiSet, ());
  applyPatternAndFreezeIfEmpty(&emojiEmojiSet, kEmojiEmojiPattern);
  return emojiEmojiSet.contains(ch);
}

bool Character::IsEmojiModifierBase(UChar32 ch) {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(icu::UnicodeSet, emojieModifierBaseSet, ());
  applyPatternAndFreezeIfEmpty(&emojieModifierBaseSet,
                               kEmojiModifierBasePattern);
  return emojieModifierBaseSet.contains(ch);
}
#else
bool Character::IsEmoji(UChar32 ch) {
  return u_hasBinaryProperty(ch, UCHAR_EMOJI);
}
bool Character::IsEmojiTextDefault(UChar32 ch) {
  return u_hasBinaryProperty(ch, UCHAR_EMOJI) &&
         !u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
}

bool Character::IsEmojiEmojiDefault(UChar32 ch) {
  return u_hasBinaryProperty(ch, UCHAR_EMOJI_PRESENTATION);
}

bool Character::IsEmojiModifierBase(UChar32 ch) {
  return u_hasBinaryProperty(ch, UCHAR_EMOJI_MODIFIER_BASE);
}
#endif  // defined(USING_SYSTEM_ICU) && (U_ICU_VERSION_MAJOR_NUM <= 57)

bool Character::IsRegionalIndicator(UChar32 ch) {
  return (ch >= 0x1F1E6 && ch <= 0x1F1FF);
}

}  // namespace blink
