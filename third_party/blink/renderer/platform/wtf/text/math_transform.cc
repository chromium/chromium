// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace WTF {
namespace unicode {

static UChar32 mathVariantGreek(UChar32 code_point, UChar32 base_char) {
  // As the ranges are contiguous, to find the desired math_variant range it
  // is sufficient to multiply the position within the sequence order
  // (multiplier) with the period of the sequence (which is constant for all
  // number sequences) and to add the character point of the first character
  // within the number math_variant range. To this the base_char calculated
  // earlier is added to obtain the final code point.
  auto ret = base_char + kMathBoldUpperAlpha +
             (kMathItalicUpperAlpha - kMathBoldUpperAlpha);
  return ret;
}

static UChar32 mathVariantLatin(UChar32 code_point, UChar32 base_char) {
  // As the ranges are contiguous, to find the desired math_variant range it
  // is sufficient to multiply the position within the sequence order
  // (multiplier) with the period of the sequence (which is constant for all
  // number sequences) and to add the character point of the first character
  // within the number math_variant range. To this the base_char calculated
  // earlier is added to obtain the final code point.
  UChar32 transformed_char =
      base_char + kMathBoldUpperA + (kMathItalicUpperA - kMathBoldUpperA);
  // https://w3c.github.io/mathml-core/#italic-mappings
  if (transformed_char == 0x1D455)
    return 0x210E;
  return transformed_char;
}

UChar32 ItalicMathVariant(UChar32 code_point) {
  // Exceptional characters with at most one possible transformation.
  if (code_point == kHoleGreekUpperTheta)
    return code_point;  // Nothing at this code point is transformed
  if (code_point == kGreekLetterDigamma)
    return code_point;
  if (code_point == kGreekSmallLetterDigamma)
    return code_point;
  if (code_point == kLatinSmallLetterDotlessI)
    return kMathItalicSmallDotlessI;
  if (code_point == kLatinSmallLetterDotlessJ)
    return kMathItalicSmallDotlessJ;

  // The Unicode mathematical blocks are divided into four segments: Latin,
  // Greek, numbers and Arabic. In the case of the first three base_char
  // represents the relative order in which the characters are encoded in the
  // Unicode mathematical block, normalised to the first character of that
  // sequence.
  UChar32 base_char = 0;
  enum CharacterType { kLatin, kGreekish };
  CharacterType var_type;
  const UChar32 kASCIIUpperStart = 'A';
  const UChar32 kASCIILowerStart = 'a';
  if (IsASCIIUpper(code_point)) {
    base_char = code_point - kASCIIUpperStart;
    var_type = kLatin;
  } else if (IsASCIILower(code_point)) {
    // Lowercase characters are placed immediately after the uppercase
    // characters in the Unicode mathematical block. The constant subtraction
    // represents the number of characters between the start of the sequence
    // (capital A) and the first lowercase letter.
    base_char =
        kMathBoldSmallA - kMathBoldUpperA + code_point - kASCIILowerStart;
    var_type = kLatin;
  } else if (kGreekUpperAlpha <= code_point && code_point <= kGreekUpperOmega) {
    base_char = code_point - kGreekUpperAlpha;
    var_type = kGreekish;
  } else if (kGreekLowerAlpha <= code_point && code_point <= kGreekLowerOmega) {
    // Lowercase Greek comes after uppercase Greek.
    // Note in this instance the presence of an additional character (Nabla)
    // between the end of the uppercase Greek characters and the lowercase ones.
    base_char = kMathBoldSmallAlpha - kMathBoldUpperAlpha + code_point -
                kGreekLowerAlpha;
    var_type = kGreekish;
  } else {
    switch (code_point) {
      case kGreekUpperTheta:
        base_char = kMathBoldUpperTheta - kMathBoldUpperAlpha;
        break;
      case kNabla:
        base_char = kMathBoldNabla - kMathBoldUpperAlpha;
        break;
      case kPartialDifferential:
        base_char = kMathBoldPartialDifferential - kMathBoldUpperAlpha;
        break;
      case kGreekLunateEpsilonSymbol:
        base_char = kMathBoldEpsilonSymbol - kMathBoldUpperAlpha;
        break;
      case kGreekThetaSymbol:
        base_char = kMathBoldThetaSymbol - kMathBoldUpperAlpha;
        break;
      case kGreekKappaSymbol:
        base_char = kMathBoldKappaSymbol - kMathBoldUpperAlpha;
        break;
      case kGreekPhiSymbol:
        base_char = kMathBoldPhiSymbol - kMathBoldUpperAlpha;
        break;
      case kGreekRhoSymbol:
        base_char = kMathBoldRhoSymbol - kMathBoldUpperAlpha;
        break;
      case kGreekPiSymbol:
        base_char = kMathBoldPiSymbol - kMathBoldUpperAlpha;
        break;
      default:
        return code_point;
    }
    var_type = kGreekish;
  }

  if (var_type == kGreekish)
    return mathVariantGreek(code_point, base_char);
  DCHECK(var_type == kLatin);
  return mathVariantLatin(code_point, base_char);
}

}  // namespace unicode
}  // namespace WTF
