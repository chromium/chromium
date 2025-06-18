// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/wtf/text/math_transform.h"

#include "base/check.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {
namespace unicode {

static UChar32 mathVariantGreek(UChar32 code_point, UChar32 base_char) {
  // As the ranges are contiguous, to find the desired math_variant range it
  // is sufficient to multiply the position within the sequence order
  // (multiplier) with the period of the sequence (which is constant for all
  // number sequences) and to add the character point of the first character
  // within the number math_variant range. To this the base_char calculated
  // earlier is added to obtain the final code point.
  auto ret = base_char + uchar::kMathBoldUpperAlpha +
             (uchar::kMathItalicUpperAlpha - uchar::kMathBoldUpperAlpha);
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
      base_char + uchar::kMathBoldUpperA +
      (uchar::kMathItalicUpperA - uchar::kMathBoldUpperA);
  // https://w3c.github.io/mathml-core/#italic-mappings
  if (transformed_char == 0x1D455)
    return 0x210E;
  return transformed_char;
}

UChar32 ItalicMathVariant(UChar32 code_point) {
  // Exceptional characters with at most one possible transformation.
  if (code_point == uchar::kHoleGreekUpperTheta) {
    return code_point;  // Nothing at this code point is transformed
  }
  if (code_point == uchar::kGreekLetterDigamma) {
    return code_point;
  }
  if (code_point == uchar::kGreekSmallLetterDigamma) {
    return code_point;
  }
  if (code_point == uchar::kLatinSmallLetterDotlessI) {
    return uchar::kMathItalicSmallDotlessI;
  }
  if (code_point == uchar::kLatinSmallLetterDotlessJ) {
    return uchar::kMathItalicSmallDotlessJ;
  }

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
    base_char = uchar::kMathBoldSmallA - uchar::kMathBoldUpperA + code_point -
                kASCIILowerStart;
    var_type = kLatin;
  } else if (uchar::kGreekUpperAlpha <= code_point &&
             code_point <= uchar::kGreekUpperOmega) {
    base_char = code_point - uchar::kGreekUpperAlpha;
    var_type = kGreekish;
  } else if (uchar::kGreekLowerAlpha <= code_point &&
             code_point <= uchar::kGreekLowerOmega) {
    // Lowercase Greek comes after uppercase Greek.
    // Note in this instance the presence of an additional character (Nabla)
    // between the end of the uppercase Greek characters and the lowercase ones.
    base_char = uchar::kMathBoldSmallAlpha - uchar::kMathBoldUpperAlpha +
                code_point - uchar::kGreekLowerAlpha;
    var_type = kGreekish;
  } else {
    switch (code_point) {
      case uchar::kGreekUpperTheta:
        base_char = uchar::kMathBoldUpperTheta - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kNabla:
        base_char = uchar::kMathBoldNabla - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kPartialDifferential:
        base_char =
            uchar::kMathBoldPartialDifferential - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekLunateEpsilonSymbol:
        base_char = uchar::kMathBoldEpsilonSymbol - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekThetaSymbol:
        base_char = uchar::kMathBoldThetaSymbol - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekKappaSymbol:
        base_char = uchar::kMathBoldKappaSymbol - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekPhiSymbol:
        base_char = uchar::kMathBoldPhiSymbol - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekRhoSymbol:
        base_char = uchar::kMathBoldRhoSymbol - uchar::kMathBoldUpperAlpha;
        break;
      case uchar::kGreekPiSymbol:
        base_char = uchar::kMathBoldPiSymbol - uchar::kMathBoldUpperAlpha;
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
}  // namespace blink
