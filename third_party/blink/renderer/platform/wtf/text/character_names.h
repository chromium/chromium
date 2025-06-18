/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CHARACTER_NAMES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CHARACTER_NAMES_H_

#include "third_party/blink/renderer/platform/wtf/text/wtf_uchar.h"

namespace blink::uchar {

// Names here are taken from the Unicode standard.
//
// Most of these are UChar constants, not UChar32, which makes them
// more convenient for Blink code that mostly uses UTF-16.
//
// Please keep these in code point order.

// U+00**
const UChar kTabulationCharacter = 0x0009;
const UChar kNewlineCharacter = 0x000A;
const UChar kLineTabulationCharacter = 0x000B;
const UChar kFormFeed = 0x000C;
const UChar kCarriageReturn = 0x000D;
const UChar kSpaceCharacter = 0x0020;
const UChar kComma = 0x002C;
const UChar kHyphenMinus = 0x002D;
const UChar kFullStop = 0x002E;
const UChar kSolidusCharacter = 0x002F;
const UChar kDigitZero = 0x0030;
const UChar kColon = 0x003A;
const UChar kSemiColon = 0x003B;
const UChar kReverseSolidusCharacter = 0x005C;
const UChar kLowLine = 0x005F;
const UChar kVerticalLineCharacter = 0x7C;
const UChar kDeleteCharacter = 0x007F;
const UChar kNoBreakSpaceCharacter = 0x00A0;
const UChar kYenSignCharacter = 0x00A5;
const UChar kSoftHyphenCharacter = 0x00AD;
const UChar kMiddleDotCharacter = 0x00B7;
const UChar kSmallLetterSharpSCharacter = 0x00DF;

// U+0***
const UChar kLatinCapitalLetterIWithDotAbove = 0x0130;
const UChar kLatinSmallLetterDotlessI = 0x0131;
const UChar kLatinSmallLetterDotlessJ = 0x0237;
const UChar kCombiningAcuteAccent = 0x0301;
const UChar kCombiningMinusSignBelow = 0x0320;
const UChar kCombiningLongSolidusOverlay = 0x0338;
const UChar kGreekUpperAlpha = 0x0391;
const UChar kHoleGreekUpperTheta = 0x03A2;
const UChar kGreekUpperOmega = 0x03A9;
const UChar kGreekLowerAlpha = 0x03B1;
const UChar kGreekLowerOmega = 0x03C9;
const UChar kGreekThetaSymbol = 0x03D1;
const UChar kGreekPhiSymbol = 0x03D5;
const UChar kGreekPiSymbol = 0x03D6;
const UChar kGreekLetterDigamma = 0x03DC;
const UChar kGreekSmallLetterDigamma = 0x03DD;
const UChar kGreekKappaSymbol = 0x03F0;
const UChar kGreekRhoSymbol = 0x03F1;
const UChar kGreekUpperTheta = 0x03F4;
const UChar kGreekLunateEpsilonSymbol = 0x03F5;
const UChar kGreekCapitalReversedDottedLunateSigmaSymbol = 0x03FF;
const UChar kHebrewPunctuationGeresh = 0x05F3;
const UChar kHebrewPunctuationGershayim = 0x05F4;
const UChar kArabicLetterMark = 0x061C;
const UChar kTibetanMarkIntersyllabicTshegCharacter = 0x0F0B;
const UChar kTibetanMarkDelimiterTshegBstarCharacter = 0x0F0C;

// U+1***
const UChar kEthiopicWordspace = 0x1361;
const UChar kEthiopicPrefaceColon = 0x1366;
const UChar kEthiopicNumberHundred = 0x137B;
const UChar kEthiopicNumberTenThousand = 0x137C;
const UChar kMongolianFreeVariationSelectorTwo = 0x180C;
const UChar kMongolianLetterA = 0x1820;

// U+2***
const UChar kEnQuad = 0x2000;
const UChar kZeroWidthSpaceCharacter = 0x200B;
const UChar kZeroWidthNonJoinerCharacter = 0x200C;
const UChar kZeroWidthJoinerCharacter = 0x200D;
const UChar kLeftToRightMark = 0x200E;
const UChar kRightToLeftMarkCharacter = 0x200F;
const UChar kHyphen = 0x2010;
const UChar kNonBreakingHyphen = 0x2011;
const UChar kLeftSingleQuotationMark = 0x2018;
const UChar kRightSingleQuotationMarkCharacter = 0x2019;
const UChar kLeftDoubleQuotationMark = 0x201C;
const UChar kRightDoubleQuotationMarkCharacter = 0x201D;
const UChar kBullet = 0x2022;
const UChar kHorizontalEllipsis = 0x2026;
const UChar kHyphenationPoint = 0x2027;
const UChar kLineSeparator = 0x2028;
const UChar kParagraphSeparator = 0x2029;
const UChar kLeftToRightEmbedding = 0x202A;
const UChar kRightToLeftEmbedCharacter = 0x202B;
const UChar kPopDirectionalFormattingCharacter = 0x202C;
const UChar kLeftToRightOverride = 0x202D;
const UChar kRightToLeftOverrideCharacter = 0x202E;
const UChar kOverlineCharacter = 0x203E;
const UChar kLeftToRightIsolate = 0x2066;
const UChar kRightToLeftIsolateCharacter = 0x2067;
const UChar kFirstStrongIsolate = 0x2068;
const UChar kPopDirectionalIsolateCharacter = 0x2069;
const UChar kInhibitSymmetricSwapping = 0x206A;
const UChar kActivateSymmetricSwapping = 0x206B;
const UChar kInhibitArabicFormShaping = 0x206C;
const UChar kActivateArabicFormShaping = 0x206D;
const UChar kNationalDigitShapesCharacter = 0x206E;
const UChar kNominalDigitShapesCharacter = 0x206F;
const UChar kCombiningLongVerticalLineOverlay = 0x20D2;
const UChar kCombiningEnclosingCircleBackslash = 0x20E0;
const UChar kCombiningEnclosingKeycap = 0x20E3;
const UChar kDoubleStruckItalicCapitalD = 0x2145;
const UChar kDoubleStruckItalicSmallD = 0x2146;
const UChar32 kPartialDifferential = 0x2202;
const UChar32 kNabla = 0x2207;
const UChar kMinusSignCharacter = 0x2212;
const UChar32 kSquareRootCharacter = 0x221A;
const UChar kFourthRoot = 0x221C;
const UChar kTildeOperatorCharacter = 0x223C;
const UChar kBlackSquare = 0x25A0;
const UChar kBlackUpPointingTriangle = 0x25B2;
const UChar kWhiteUpPointingTriangleCharacter = 0x25B3;
const UChar kBlackRightPointingSmallTriangle = 0x25B8;
const UChar kBlackDownPointingSmallTriangle = 0x25BE;
const UChar kFisheye = 0x25C9;
const UChar kWhiteCircleCharacter = 0x25CB;
const UChar kBullseye = 0x25CE;
const UChar kBlackCircle = 0x25CF;
const UChar kWhiteBulletCharacter = 0x25E6;
const UChar kFemaleSign = 0x2640;
const UChar kMaleSignCharacter = 0x2642;
const UChar kStaffOfAesculapiusCharacter = 0x2695;
const UChar kHeavyBlackHeartCharacter = 0x2764;
const UChar kHellschreiberPauseSymbol = 0x2BFF;

// U+3***
const UChar kIdeographicSpace = 0x3000;
const UChar kIdeographicComma = 0x3001;
const UChar kIdeographicFullStop = 0x3002;
const UChar kLeftCornerBracket = 0x300C;
const UChar kHiraganaLetterSmallA = 0x3041;
const UChar kHiraganaLetterACharacter = 0x3042;
const UChar kKatakanaMiddleDot = 0x30FB;
const UChar kKatakanaHiraganaProlongedSoundMark = 0x30FC;

// U+6***
const UChar kCjkWater = 0x6C34;

// U+E***
const UChar kPrivateUseFirstCharacter = 0xE000;

// U+F***
const UChar kPrivateUseLastCharacter = 0xF8FF;
const UChar kVariationSelector2Character = 0xFE01;
const UChar kVariationSelector15Character = 0xFE0E;
const UChar kVariationSelector16Character = 0xFE0F;
const UChar kSesameDotCharacter = 0xFE45;
const UChar kWhiteSesameDotCharacter = 0xFE46;
const UChar kZeroWidthNoBreakSpaceCharacter = 0xFEFF;
const UChar kFullwidthExclamationMark = 0xFF01;
const UChar kFullwidthComma = 0xFF0C;
const UChar kFullwidthHyphenMinus = 0xFF0D;
const UChar kFullwidthFullStop = 0xFF0E;
const UChar kFullwidthDigitZero = 0xFF10;
const UChar kFullwidthDigitNine = 0xFF19;
const UChar kFullwidthColon = 0xFF1A;
const UChar kFullwidthSemicolon = 0xFF1B;
const UChar kObjectReplacementCharacter = 0xFFFC;
const UChar kReplacementCharacter = 0xFFFD;
const UChar32 kNonCharacter = 0xFFFF;

// Non-BMP characters.
const UChar32 kAegeanWordSeparatorLine = 0x10100;
const UChar32 kAegeanWordSeparatorDot = 0x10101;
const UChar32 kUgariticWordDividerCharacter = 0x1039F;
const UChar32 kMathBoldUpperA = 0x1D400;
const UChar32 kMathBoldSmallA = 0x1D41A;
const UChar32 kMathItalicUpperA = 0x1D434;
const UChar32 kMathItalicSmallDotlessI = 0x1D6A4;
const UChar32 kMathItalicSmallDotlessJ = 0x1D6A5;
const UChar32 kMathBoldUpperAlpha = 0x1D6A8;
const UChar32 kMathBoldUpperTheta = 0x1D6B9;
const UChar32 kMathBoldNabla = 0x1D6C1;
const UChar32 kMathBoldSmallAlpha = 0x1D6C2;
const UChar32 kMathBoldPartialDifferential = 0x1D6DB;
const UChar32 kMathBoldEpsilonSymbol = 0x1D6DC;
const UChar32 kMathBoldThetaSymbol = 0x1D6DD;
const UChar32 kMathBoldKappaSymbol = 0x1D6DE;
const UChar32 kMathBoldPhiSymbol = 0x1D6DF;
const UChar32 kMathBoldRhoSymbol = 0x1D6E0;
const UChar32 kMathBoldPiSymbol = 0x1D6E1;
const UChar32 kMathItalicUpperAlpha = 0x1D6E2;
const UChar32 kMathBoldSmallDigamma = 0x1D7CB;
const UChar32 kArabicMathematicalOperatorMeemWithHahWithTatweel = 0x1EEF0;
const UChar32 kArabicMathematicalOperatorHahWithDal = 0x1EEF1;
const UChar32 kRainbowCharacter = 0x1F308;
const UChar32 kWavingWhiteFlagCharacter = 0x1F3F3;
const UChar32 kEye = 0x1F441;
const UChar32 kBoyCharacter = 0x1F466;
const UChar32 kGirlCharacter = 0x1F467;
const UChar32 kManCharacter = 0x1F468;
const UChar32 kWomanCharacter = 0x1F469;
const UChar32 kFamily = 0x1F46A;
const UChar32 kKissMarkCharacter = 0x1F48B;
const UChar32 kLeftSpeechBubble = 0x1F5E8;
const UChar32 kShakingFaceEmoji = 0x1FAE8;
const UChar32 kTagDigitZero = 0xE0030;
const UChar32 kTagDigitNine = 0xE0039;
const UChar32 kTagLatinSmallLetterA = 0xE0061;
const UChar32 kTagLatinSmallLetterZ = 0xE007A;
const UChar32 kCancelTag = 0xE007F;

const UChar32 kMaxCodepoint = 0x10ffff;

}  // namespace blink::uchar

// TODO(crbug.com/422768753): Remove the following `using` directives.
using blink::uchar::kMaleSignCharacter;
using blink::uchar::kMiddleDotCharacter;
using blink::uchar::kMinusSignCharacter;
using blink::uchar::kNationalDigitShapesCharacter;
using blink::uchar::kNewlineCharacter;
using blink::uchar::kNoBreakSpaceCharacter;
using blink::uchar::kNominalDigitShapesCharacter;
using blink::uchar::kNonCharacter;
using blink::uchar::kOverlineCharacter;
using blink::uchar::kPopDirectionalFormattingCharacter;
using blink::uchar::kPopDirectionalIsolateCharacter;
using blink::uchar::kPrivateUseFirstCharacter;
using blink::uchar::kPrivateUseLastCharacter;
using blink::uchar::kRainbowCharacter;
using blink::uchar::kReverseSolidusCharacter;
using blink::uchar::kRightDoubleQuotationMarkCharacter;
using blink::uchar::kRightSingleQuotationMarkCharacter;
using blink::uchar::kRightToLeftEmbedCharacter;
using blink::uchar::kRightToLeftIsolateCharacter;
using blink::uchar::kRightToLeftMarkCharacter;
using blink::uchar::kRightToLeftOverrideCharacter;
using blink::uchar::kSesameDotCharacter;
using blink::uchar::kSmallLetterSharpSCharacter;
using blink::uchar::kSoftHyphenCharacter;
using blink::uchar::kSolidusCharacter;
using blink::uchar::kSpaceCharacter;
using blink::uchar::kSquareRootCharacter;
using blink::uchar::kStaffOfAesculapiusCharacter;
using blink::uchar::kTabulationCharacter;
using blink::uchar::kTibetanMarkDelimiterTshegBstarCharacter;
using blink::uchar::kTibetanMarkIntersyllabicTshegCharacter;
using blink::uchar::kTildeOperatorCharacter;
using blink::uchar::kUgariticWordDividerCharacter;
using blink::uchar::kVariationSelector15Character;
using blink::uchar::kVariationSelector16Character;
using blink::uchar::kVariationSelector2Character;
using blink::uchar::kVerticalLineCharacter;
using blink::uchar::kWavingWhiteFlagCharacter;
using blink::uchar::kWhiteBulletCharacter;
using blink::uchar::kWhiteCircleCharacter;
using blink::uchar::kWhiteSesameDotCharacter;
using blink::uchar::kWhiteUpPointingTriangleCharacter;
using blink::uchar::kYenSignCharacter;
using blink::uchar::kZeroWidthJoinerCharacter;
using blink::uchar::kZeroWidthNoBreakSpaceCharacter;
using blink::uchar::kZeroWidthNonJoinerCharacter;
using blink::uchar::kZeroWidthSpaceCharacter;

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_CHARACTER_NAMES_H_
