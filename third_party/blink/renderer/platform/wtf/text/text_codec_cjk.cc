/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/wtf/text/text_codec_cjk.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/function_ref.h"
#include "base/memory/ptr_util.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/encoding_tables.h"
#include "third_party/blink/renderer/platform/wtf/text/string_concatenate.h"
#include "third_party/blink/renderer/platform/wtf/text/text_encoding.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace WTF {

class TextCodecCJK::Decoder {
 public:
  virtual ~Decoder() = default;
  virtual String Decode(base::span<const uint8_t> bytes,
                        bool flush,
                        bool stop_on_error,
                        bool& saw_error);

 protected:
  enum class SawError { kNo, kYes };
  virtual SawError ParseByte(uint8_t byte, StringBuilder& result) = 0;
  virtual void Finalize(bool flush, StringBuilder& result) {}

  uint8_t lead_ = 0x00;
  std::optional<uint8_t> prepended_byte_;
};

namespace {

constexpr char kCanonicalNameEucJp[] = "EUC-JP";
constexpr char kCanonicalNameShiftJis[] = "Shift_JIS";
constexpr char kCanonicalNameEucKr[] = "EUC-KR";
constexpr char kCanonicalNameIso2022Jp[] = "ISO-2022-JP";
constexpr char kCanonicalNameGbk[] = "GBK";
constexpr char kCanonicalNameGb18030[] = "gb18030";

constexpr std::array<const char*, 6> kSupportedCanonicalNames{
    kCanonicalNameEucJp,     kCanonicalNameShiftJis, kCanonicalNameEucKr,
    kCanonicalNameIso2022Jp, kCanonicalNameGbk,      kCanonicalNameGb18030,
};

void AppendUnencodableReplacement(UChar32 code_point,
                                  UnencodableHandling handling,
                                  Vector<uint8_t>& result) {
  std::string replacement =
      TextCodec::GetUnencodableReplacement(code_point, handling);
  result.reserve(result.size() + replacement.size());
  for (uint8_t r : replacement) {
    result.UncheckedAppend(r);
  }
}

std::optional<UChar> FindCodePointInJis0208(uint16_t pointer) {
  return FindFirstInSortedPairs(EnsureJis0208EncodeIndexForDecode(), pointer);
}

std::optional<UChar> FindCodePointJis0212(uint16_t pointer) {
  return FindFirstInSortedPairs(EnsureJis0212EncodeIndexForDecode(), pointer);
}

// https://encoding.spec.whatwg.org/#euc-jp-encoder
Vector<uint8_t> EncodeEucJp(StringView string, UnencodableHandling handling) {
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(string.length());

  for (UChar32 code_point : string) {
    if (IsASCII(code_point)) {
      result.push_back(code_point);
      continue;
    }
    if (code_point == kYenSignCharacter) {
      result.push_back(0x5C);
      continue;
    }
    if (code_point == kOverlineCharacter) {
      result.push_back(0x7E);
      continue;
    }
    if (code_point >= 0xFF61 && code_point <= 0xFF9F) {
      result.push_back(0x8E);
      result.push_back(code_point - 0xFF61 + 0xA1);
      continue;
    }
    if (code_point == kMinusSignCharacter)
      code_point = 0xFF0D;

    auto pointer =
        FindFirstInSortedPairs(EnsureJis0208EncodeIndexForEncode(), code_point);
    if (!pointer) {
      AppendUnencodableReplacement(code_point, handling, result);
      continue;
    }
    result.push_back(*pointer / 94 + 0xA1);
    result.push_back(*pointer % 94 + 0xA1);
  }
  return result;
}

class Iso2022JpEncoder {
 public:
  static Vector<uint8_t> Encode(StringView string,
                                UnencodableHandling handling) {
    Iso2022JpEncoder encoder(handling, string.length());
    for (UChar32 code_point : string) {
      encoder.ParseCodePoint(code_point);
    }
    return encoder.Finalize();
  }

 private:
  enum class State : uint8_t { kAscii, kRoman, kJis0208 };

  // From https://encoding.spec.whatwg.org/index-iso-2022-jp-katakana.txt
  static constexpr std::array<UChar32, 63> kIso2022JpKatakana{
      0x3002, 0x300C, 0x300D, 0x3001, 0x30FB, 0x30F2, 0x30A1, 0x30A3, 0x30A5,
      0x30A7, 0x30A9, 0x30E3, 0x30E5, 0x30E7, 0x30C3, 0x30FC, 0x30A2, 0x30A4,
      0x30A6, 0x30A8, 0x30AA, 0x30AB, 0x30AD, 0x30AF, 0x30B1, 0x30B3, 0x30B5,
      0x30B7, 0x30B9, 0x30BB, 0x30BD, 0x30BF, 0x30C1, 0x30C4, 0x30C6, 0x30C8,
      0x30CA, 0x30CB, 0x30CC, 0x30CD, 0x30CE, 0x30CF, 0x30D2, 0x30D5, 0x30D8,
      0x30DB, 0x30DE, 0x30DF, 0x30E0, 0x30E1, 0x30E2, 0x30E4, 0x30E6, 0x30E8,
      0x30E9, 0x30EA, 0x30EB, 0x30EC, 0x30ED, 0x30EF, 0x30F3, 0x309B, 0x309C};
  static_assert(std::size(kIso2022JpKatakana) == 0xFF9F - 0xFF61 + 1);

  Iso2022JpEncoder(UnencodableHandling handling, wtf_size_t length)
      : handling_(handling) {
    result_.ReserveInitialCapacity(length);
  }

  void ChangeStateToAscii() {
    result_.push_back(0x1B);
    result_.push_back(0x28);
    result_.push_back(0x42);
    state_ = State::kAscii;
  }

  void ChangeStateToRoman() {
    result_.push_back(0x1B);
    result_.push_back(0x28);
    result_.push_back(0x4A);
    state_ = State::kRoman;
  }

  void ChangeStateToJis0208() {
    result_.push_back(0x1B);
    result_.push_back(0x24);
    result_.push_back(0x42);
    state_ = State::kJis0208;
  }

  void ParseCodePoint(UChar32 code_point) {
    if ((state_ == State::kAscii || state_ == State::kRoman) &&
        (code_point == 0x000E || code_point == 0x000F ||
         code_point == 0x001B)) {
      StatefulUnencodableHandler(kReplacementCharacter);
      return;
    }
    if (state_ == State::kAscii && IsASCII(code_point)) {
      result_.push_back(code_point);
      return;
    }
    if (state_ == State::kRoman) {
      if (IsASCII(code_point) && code_point != 0x005C && code_point != 0x007E) {
        result_.push_back(code_point);
        return;
      }
      if (code_point == kYenSignCharacter) {
        result_.push_back(0x5C);
        return;
      }
      if (code_point == kOverlineCharacter) {
        result_.push_back(0x7E);
        return;
      }
    }
    if (IsASCII(code_point) && state_ != State::kAscii) {
      ChangeStateToAscii();
      ParseCodePoint(code_point);
      return;
    }
    if ((code_point == kYenSignCharacter || code_point == kOverlineCharacter) &&
        state_ != State::kRoman) {
      ChangeStateToRoman();
      ParseCodePoint(code_point);
      return;
    }
    if (code_point == kMinusSignCharacter)
      code_point = 0xFF0D;
    if (code_point >= 0xFF61 && code_point <= 0xFF9F) {
      code_point = kIso2022JpKatakana[code_point - 0xFF61];
    }

    auto pointer =
        FindFirstInSortedPairs(EnsureJis0208EncodeIndexForEncode(), code_point);
    if (!pointer) {
      StatefulUnencodableHandler(code_point);
      return;
    }
    if (state_ != State::kJis0208) {
      ChangeStateToJis0208();
      ParseCodePoint(code_point);
      return;
    }
    result_.push_back(*pointer / 94 + 0x21);
    result_.push_back(*pointer % 94 + 0x21);
  }

  Vector<uint8_t> Finalize() {
    if (state_ != State::kAscii) {
      ChangeStateToAscii();
    }
    return std::move(result_);
  }

  void StatefulUnencodableHandler(UChar32 code_point) {
    if (state_ == State::kJis0208)
      ChangeStateToAscii();
    AppendUnencodableReplacement(code_point, handling_, result_);
  }

  UnencodableHandling handling_;
  State state_ = State::kAscii;
  Vector<uint8_t> result_;
};

// https://encoding.spec.whatwg.org/#iso-2022-jp-encoder
Vector<uint8_t> EncodeIso2022Jp(StringView string,
                                UnencodableHandling handling) {
  return Iso2022JpEncoder::Encode(string, handling);
}

// https://encoding.spec.whatwg.org/#shift_jis-encoder
Vector<uint8_t> EncodeShiftJis(StringView string,
                               UnencodableHandling handling) {
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(string.length());

  for (UChar32 code_point : string) {
    if (IsASCII(code_point) || code_point == 0x0080) {
      result.push_back(code_point);
      continue;
    }
    if (code_point == kYenSignCharacter) {
      result.push_back(0x5C);
      continue;
    }
    if (code_point == kOverlineCharacter) {
      result.push_back(0x7E);
      continue;
    }
    if (code_point >= 0xFF61 && code_point <= 0xFF9F) {
      result.push_back(code_point - 0xFF61 + 0xA1);
      continue;
    }
    if (code_point == kMinusSignCharacter)
      code_point = 0xFF0D;

    auto range =
        FindInSortedPairs(EnsureJis0208EncodeIndexForEncode(), code_point);
    if (range.first == range.second) {
      AppendUnencodableReplacement(code_point, handling, result);
      continue;
    }

    DCHECK(range.first + 3 >= range.second);
    for (auto pair = range.first; pair < range.second; pair++) {
      uint16_t pointer = pair->second;
      if (pointer >= 8272 && pointer <= 8835)
        continue;
      uint8_t lead = pointer / 188;
      uint8_t lead_offset = lead < 0x1F ? 0x81 : 0xC1;
      uint8_t trail = pointer % 188;
      uint8_t offset = trail < 0x3F ? 0x40 : 0x41;
      result.push_back(lead + lead_offset);
      result.push_back(trail + offset);
      break;
    }
  }
  return result;
}

// https://encoding.spec.whatwg.org/#euc-kr-encoder
Vector<uint8_t> EncodeEucKr(StringView string, UnencodableHandling handling) {
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(string.length());

  for (UChar32 code_point : string) {
    if (IsASCII(code_point)) {
      result.push_back(code_point);
      continue;
    }

    auto pointer =
        FindFirstInSortedPairs(EnsureEucKrEncodeIndexForEncode(), code_point);
    if (!pointer) {
      AppendUnencodableReplacement(code_point, handling, result);
      continue;
    }
    result.push_back(*pointer / 190 + 0x81);
    result.push_back(*pointer % 190 + 0x41);
  }
  return result;
}

// https://encoding.spec.whatwg.org/index-gb18030-ranges.txt
const std::array<std::pair<uint32_t, UChar32>, 207>& Gb18030Ranges() {
  static std::array<std::pair<uint32_t, UChar32>, 207> ranges{
      {{0, 0x0080},     {36, 0x00A5},    {38, 0x00A9},     {45, 0x00B2},
       {50, 0x00B8},    {81, 0x00D8},    {89, 0x00E2},     {95, 0x00EB},
       {96, 0x00EE},    {100, 0x00F4},   {103, 0x00F8},    {104, 0x00FB},
       {105, 0x00FD},   {109, 0x0102},   {126, 0x0114},    {133, 0x011C},
       {148, 0x012C},   {172, 0x0145},   {175, 0x0149},    {179, 0x014E},
       {208, 0x016C},   {306, 0x01CF},   {307, 0x01D1},    {308, 0x01D3},
       {309, 0x01D5},   {310, 0x01D7},   {311, 0x01D9},    {312, 0x01DB},
       {313, 0x01DD},   {341, 0x01FA},   {428, 0x0252},    {443, 0x0262},
       {544, 0x02C8},   {545, 0x02CC},   {558, 0x02DA},    {741, 0x03A2},
       {742, 0x03AA},   {749, 0x03C2},   {750, 0x03CA},    {805, 0x0402},
       {819, 0x0450},   {820, 0x0452},   {7922, 0x2011},   {7924, 0x2017},
       {7925, 0x201A},  {7927, 0x201E},  {7934, 0x2027},   {7943, 0x2031},
       {7944, 0x2034},  {7945, 0x2036},  {7950, 0x203C},   {8062, 0x20AD},
       {8148, 0x2104},  {8149, 0x2106},  {8152, 0x210A},   {8164, 0x2117},
       {8174, 0x2122},  {8236, 0x216C},  {8240, 0x217A},   {8262, 0x2194},
       {8264, 0x219A},  {8374, 0x2209},  {8380, 0x2210},   {8381, 0x2212},
       {8384, 0x2216},  {8388, 0x221B},  {8390, 0x2221},   {8392, 0x2224},
       {8393, 0x2226},  {8394, 0x222C},  {8396, 0x222F},   {8401, 0x2238},
       {8406, 0x223E},  {8416, 0x2249},  {8419, 0x224D},   {8424, 0x2253},
       {8437, 0x2262},  {8439, 0x2268},  {8445, 0x2270},   {8482, 0x2296},
       {8485, 0x229A},  {8496, 0x22A6},  {8521, 0x22C0},   {8603, 0x2313},
       {8936, 0x246A},  {8946, 0x249C},  {9046, 0x254C},   {9050, 0x2574},
       {9063, 0x2590},  {9066, 0x2596},  {9076, 0x25A2},   {9092, 0x25B4},
       {9100, 0x25BE},  {9108, 0x25C8},  {9111, 0x25CC},   {9113, 0x25D0},
       {9131, 0x25E6},  {9162, 0x2607},  {9164, 0x260A},   {9218, 0x2641},
       {9219, 0x2643},  {11329, 0x2E82}, {11331, 0x2E85},  {11334, 0x2E89},
       {11336, 0x2E8D}, {11346, 0x2E98}, {11361, 0x2EA8},  {11363, 0x2EAB},
       {11366, 0x2EAF}, {11370, 0x2EB4}, {11372, 0x2EB8},  {11375, 0x2EBC},
       {11389, 0x2ECB}, {11682, 0x2FFC}, {11686, 0x3004},  {11687, 0x3018},
       {11692, 0x301F}, {11694, 0x302A}, {11714, 0x303F},  {11716, 0x3094},
       {11723, 0x309F}, {11725, 0x30F7}, {11730, 0x30FF},  {11736, 0x312A},
       {11982, 0x322A}, {11989, 0x3232}, {12102, 0x32A4},  {12336, 0x3390},
       {12348, 0x339F}, {12350, 0x33A2}, {12384, 0x33C5},  {12393, 0x33CF},
       {12395, 0x33D3}, {12397, 0x33D6}, {12510, 0x3448},  {12553, 0x3474},
       {12851, 0x359F}, {12962, 0x360F}, {12973, 0x361B},  {13738, 0x3919},
       {13823, 0x396F}, {13919, 0x39D1}, {13933, 0x39E0},  {14080, 0x3A74},
       {14298, 0x3B4F}, {14585, 0x3C6F}, {14698, 0x3CE1},  {15583, 0x4057},
       {15847, 0x4160}, {16318, 0x4338}, {16434, 0x43AD},  {16438, 0x43B2},
       {16481, 0x43DE}, {16729, 0x44D7}, {17102, 0x464D},  {17122, 0x4662},
       {17315, 0x4724}, {17320, 0x472A}, {17402, 0x477D},  {17418, 0x478E},
       {17859, 0x4948}, {17909, 0x497B}, {17911, 0x497E},  {17915, 0x4984},
       {17916, 0x4987}, {17936, 0x499C}, {17939, 0x49A0},  {17961, 0x49B8},
       {18664, 0x4C78}, {18703, 0x4CA4}, {18814, 0x4D1A},  {18962, 0x4DAF},
       {19043, 0x9FA6}, {33469, 0xE76C}, {33470, 0xE7C8},  {33471, 0xE7E7},
       {33484, 0xE815}, {33485, 0xE819}, {33490, 0xE81F},  {33497, 0xE827},
       {33501, 0xE82D}, {33505, 0xE833}, {33513, 0xE83C},  {33520, 0xE844},
       {33536, 0xE856}, {33550, 0xE865}, {37845, 0xF92D},  {37921, 0xF97A},
       {37948, 0xF996}, {38029, 0xF9E8}, {38038, 0xF9F2},  {38064, 0xFA10},
       {38065, 0xFA12}, {38066, 0xFA15}, {38069, 0xFA19},  {38075, 0xFA22},
       {38076, 0xFA25}, {38078, 0xFA2A}, {39108, 0xFE32},  {39109, 0xFE45},
       {39113, 0xFE53}, {39114, 0xFE58}, {39115, 0xFE67},  {39116, 0xFE6C},
       {39265, 0xFF5F}, {39394, 0xFFE6}, {189000, 0x10000}}};
  return ranges;
}

// https://encoding.spec.whatwg.org/#index-gb18030-ranges-code-point
std::optional<UChar32> IndexGb18030RangesCodePoint(uint32_t pointer) {
  if ((pointer > 39419 && pointer < 189000) || pointer > 1237575)
    return std::nullopt;
  if (pointer == 7457)
    return 0xE7C7;

  const auto& gb18030_ranges = Gb18030Ranges();
  auto upper_bound =
      std::upper_bound(gb18030_ranges.begin(), gb18030_ranges.end(),
                       MakeFirstAdapter(pointer), CompareFirst{});
  DCHECK(upper_bound != gb18030_ranges.begin());
  uint32_t offset = (upper_bound - 1)->first;
  UChar32 code_point_offset = (upper_bound - 1)->second;
  return code_point_offset + pointer - offset;
}

// https://encoding.spec.whatwg.org/#index-gb18030-ranges-pointer
uint32_t Gb18030RangesPointer(UChar32 code_point) {
  if (code_point == 0xE7C7)
    return 7457;
  auto upper_bound =
      std::upper_bound(Gb18030Ranges().begin(), Gb18030Ranges().end(),
                       MakeSecondAdapter(code_point), CompareSecond{});
  DCHECK(upper_bound != Gb18030Ranges().begin());
  uint32_t pointer_offset = (upper_bound - 1)->first;
  UChar32 offset = (upper_bound - 1)->second;
  return pointer_offset + code_point - offset;
}

// https://unicode-org.atlassian.net/browse/ICU-22357
// The 2-byte values are handled correctly by values from
// EnsureGb18030EncodeTable() but these need to be exceptions from
// Gb18030Ranges().
static std::optional<uint16_t> Gb18030AsymmetricEncode(UChar32 codePoint) {
  switch (codePoint) {
    case 0xE81E:
      return 0xFE59;
    case 0xE826:
      return 0xFE61;
    case 0xE82B:
      return 0xFE66;
    case 0xE82C:
      return 0xFE67;
    case 0xE832:
      return 0xFE6D;
    case 0xE843:
      return 0xFE7E;
    case 0xE854:
      return 0xFE90;
    case 0xE864:
      return 0xFEA0;
    case 0xE78D:
      return 0xA6D9;
    case 0xE78F:
      return 0xA6DB;
    case 0xE78E:
      return 0xA6DA;
    case 0xE790:
      return 0xA6DC;
    case 0xE791:
      return 0xA6DD;
    case 0xE792:
      return 0xA6DE;
    case 0xE793:
      return 0xA6DF;
    case 0xE794:
      return 0xA6EC;
    case 0xE795:
      return 0xA6ED;
    case 0xE796:
      return 0xA6F3;
  }
  return std::nullopt;
}

// https://encoding.spec.whatwg.org/#gb18030-encoder
enum class IsGbk : bool { kNo, kYes };
Vector<uint8_t> EncodeGbShared(StringView string,
                               UnencodableHandling handling,
                               IsGbk is_gbk) {
  Vector<uint8_t> result;
  result.ReserveInitialCapacity(string.length());

  for (UChar32 code_point : string) {
    if (IsASCII(code_point)) {
      result.push_back(code_point);
      continue;
    }
    if (code_point == 0xE5E5) {
      AppendUnencodableReplacement(code_point, handling, result);
      continue;
    }
    if (is_gbk == IsGbk::kYes && code_point == 0x20AC) {
      result.push_back(0x80);
      continue;
    }
    if (auto encoded = Gb18030AsymmetricEncode(code_point)) {
      result.push_back(*encoded >> 8);
      result.push_back(*encoded);
      continue;
    }
    auto pointer_range =
        FindInSortedPairs(EnsureGb18030EncodeIndexForEncode(), code_point);
    if (pointer_range.first != pointer_range.second) {
      uint16_t pointer = pointer_range.first->second;
      uint8_t lead = pointer / 190 + 0x81;
      uint8_t trail = pointer % 190;
      uint8_t offset = trail < 0x3F ? 0x40 : 0x41;
      result.push_back(lead);
      result.push_back(trail + offset);
      continue;
    }
    if (is_gbk == IsGbk::kYes) {
      AppendUnencodableReplacement(code_point, handling, result);
      continue;
    }
    uint32_t pointer = Gb18030RangesPointer(code_point);
    uint8_t byte1 = pointer / (10 * 126 * 10);
    pointer = pointer % (10 * 126 * 10);
    uint8_t byte2 = pointer / (10 * 126);
    pointer = pointer % (10 * 126);
    uint8_t byte3 = pointer / 10;
    uint8_t byte4 = pointer % 10;
    result.push_back(byte1 + 0x81);
    result.push_back(byte2 + 0x30);
    result.push_back(byte3 + 0x81);
    result.push_back(byte4 + 0x30);
  }
  return result;
}

Vector<uint8_t> EncodeGb18030(StringView string, UnencodableHandling handling) {
  return EncodeGbShared(string, handling, IsGbk::kNo);
}

Vector<uint8_t> EncodeGbk(StringView string, UnencodableHandling handling) {
  return EncodeGbShared(string, handling, IsGbk::kYes);
}

// https://encoding.spec.whatwg.org/#euc-jp-decoder
class EucJpDecoder : public TextCodecCJK::Decoder {
 public:
  EucJpDecoder() = default;

 protected:
  SawError ParseByte(uint8_t byte, StringBuilder& result) override {
    if (uint8_t lead = std::exchange(lead_, 0x00)) {
      if (lead == 0x8E && byte >= 0xA1 && byte <= 0xDF) {
        result.Append(0xFF61 - 0xA1 + byte);
        return SawError::kNo;
      }
      if (lead == 0x8F && byte >= 0xA1 && byte <= 0xFE) {
        jis0212_ = true;
        lead_ = byte;
        return SawError::kNo;
      }
      if (lead >= 0xA1 && lead <= 0xFE && byte >= 0xA1 && byte <= 0xFE) {
        uint16_t pointer = (lead - 0xA1) * 94 + byte - 0xA1;
        if (auto code_point = std::exchange(jis0212_, false)
                                  ? FindCodePointJis0212(pointer)
                                  : FindCodePointInJis0208(pointer)) {
          result.Append(*code_point);
          return SawError::kNo;
        }
      }
      if (IsASCII(byte))
        prepended_byte_ = byte;
      return SawError::kYes;
    }
    if (IsASCII(byte)) {
      result.Append(static_cast<char>(byte));
      return SawError::kNo;
    }
    if (byte == 0x8E || byte == 0x8F || (byte >= 0xA1 && byte <= 0xFE)) {
      lead_ = byte;
      return SawError::kNo;
    }
    return SawError::kYes;
  }

 private:
  bool jis0212_ = false;
};

// https://encoding.spec.whatwg.org/#iso-2022-jp-decoder
class Iso2022JpDecoder : public TextCodecCJK::Decoder {
 public:
  Iso2022JpDecoder() = default;

  String Decode(base::span<const uint8_t> bytes,
                bool flush,
                bool stop_on_error,
                bool& saw_error) override {
    StringBuilder result;
    result.ReserveCapacity(bytes.size());

    if (prepended_byte_ &&
        ParseByte(*std::exchange(prepended_byte_, std::nullopt), result) ==
            SawError::kYes) {
      saw_error = true;
      result.Append(kReplacementCharacter);
      if (stop_on_error) {
        lead_ = 0x00;
        return result.ToString();
      }
    }
    if (second_prepended_byte_ &&
        ParseByte(*std::exchange(second_prepended_byte_, std::nullopt),
                  result) == SawError::kYes &&
        stop_on_error) {
      saw_error = true;
      result.Append(kReplacementCharacter);
      if (stop_on_error) {
        lead_ = 0x00;
        return result.ToString();
      }
    }
    for (size_t i = 0; i < bytes.size(); ++i) {
      if (ParseByte(bytes[i], result) == SawError::kYes) {
        saw_error = true;
        result.Append(kReplacementCharacter);
        if (stop_on_error) {
          lead_ = 0x00;
          return result.ToString();
        }
      }
      if (prepended_byte_ &&
          ParseByte(*std::exchange(prepended_byte_, std::nullopt), result) ==
              SawError::kYes) {
        saw_error = true;
        result.Append(kReplacementCharacter);
        if (stop_on_error) {
          lead_ = 0x00;
          return result.ToString();
        }
      }
      if (second_prepended_byte_ &&
          ParseByte(*std::exchange(second_prepended_byte_, std::nullopt),
                    result) == SawError::kYes &&
          stop_on_error) {
        saw_error = true;
        result.Append(kReplacementCharacter);
        if (stop_on_error) {
          lead_ = 0x00;
          return result.ToString();
        }
      }
    }

    if (flush) {
      switch (decoder_state_) {
        case State::kAscii:
        case State::kRoman:
        case State::kKatakana:
        case State::kLeadByte:
          break;
        case State::kTrailByte:
          decoder_state_ = State::kLeadByte;
          [[fallthrough]];
        case State::kEscapeStart:
          saw_error = true;
          result.Append(kReplacementCharacter);
          break;
        case State::kEscape:
          saw_error = true;
          result.Append(kReplacementCharacter);
          if (lead_) {
            DCHECK(IsASCII(lead_));
            result.Append(std::exchange(lead_, 0x00));
          }
          break;
      }
    }

    return result.ToString();
  }

 protected:
  SawError ParseByte(uint8_t byte, StringBuilder& result) override {
    switch (decoder_state_) {
      case State::kAscii:
        if (byte == 0x1B) {
          decoder_state_ = State::kEscapeStart;
          break;
        }
        if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B) {
          output_ = false;
          result.Append(byte);
          break;
        }
        output_ = false;
        return SawError::kYes;
      case State::kRoman:
        if (byte == 0x1B) {
          decoder_state_ = State::kEscapeStart;
          break;
        }
        if (byte == 0x5C) {
          output_ = false;
          result.Append(static_cast<UChar>(kYenSignCharacter));
          break;
        }
        if (byte == 0x7E) {
          output_ = false;
          result.Append(static_cast<UChar>(kOverlineCharacter));
          break;
        }
        if (byte <= 0x7F && byte != 0x0E && byte != 0x0F && byte != 0x1B &&
            byte != 0x5C && byte != 0x7E) {
          output_ = false;
          result.Append(byte);
          break;
        }
        output_ = false;
        return SawError::kYes;
      case State::kKatakana:
        if (byte == 0x1B) {
          decoder_state_ = State::kEscapeStart;
          break;
        }
        if (byte >= 0x21 && byte <= 0x5F) {
          output_ = false;
          result.Append(static_cast<UChar>(0xFF61 - 0x21 + byte));
          break;
        }
        output_ = false;
        return SawError::kYes;
      case State::kLeadByte:
        if (byte == 0x1B) {
          decoder_state_ = State::kEscapeStart;
          break;
        }
        if (byte >= 0x21 && byte <= 0x7E) {
          output_ = false;
          lead_ = byte;
          decoder_state_ = State::kTrailByte;
          break;
        }
        output_ = false;
        return SawError::kYes;
      case State::kTrailByte:
        if (byte == 0x1B) {
          decoder_state_ = State::kEscapeStart;
          return SawError::kYes;
        }
        decoder_state_ = State::kLeadByte;
        if (byte >= 0x21 && byte <= 0x7E) {
          uint16_t pointer = (lead_ - 0x21) * 94 + byte - 0x21;
          if (auto code_point = FindCodePointInJis0208(pointer)) {
            result.Append(*code_point);
            break;
          }
          return SawError::kYes;
        }
        return SawError::kYes;
      case State::kEscapeStart:
        if (byte == 0x24 || byte == 0x28) {
          lead_ = byte;
          decoder_state_ = State::kEscape;
          break;
        }
        prepended_byte_ = byte;
        output_ = false;
        decoder_state_ = decoder_output_state_;
        return SawError::kYes;
      case State::kEscape: {
        uint8_t lead = std::exchange(lead_, 0x00);
        std::optional<State> state;
        if (lead == 0x28) {
          if (byte == 0x42)
            state = State::kAscii;
          else if (byte == 0x4A)
            state = State::kRoman;
          else if (byte == 0x49)
            state = State::kKatakana;
        } else if (lead == 0x24 && (byte == 0x40 || byte == 0x42)) {
          state = State::kLeadByte;
        }
        if (state) {
          decoder_state_ = *state;
          decoder_output_state_ = *state;
          if (std::exchange(output_, true))
            return SawError::kYes;
          break;
        }
        prepended_byte_ = lead;
        second_prepended_byte_ = byte;
        output_ = false;
        decoder_state_ = decoder_output_state_;
        return SawError::kYes;
      }
    }
    return SawError::kNo;
  }

 private:
  enum class State {
    kAscii,
    kRoman,
    kKatakana,
    kLeadByte,
    kTrailByte,
    kEscapeStart,
    kEscape
  };
  State decoder_state_ = State::kAscii;
  State decoder_output_state_ = State::kAscii;
  bool output_ = false;
  std::optional<uint8_t> second_prepended_byte_;
};

// https://encoding.spec.whatwg.org/#shift_jis-decoder
class ShiftJisDecoder : public TextCodecCJK::Decoder {
 public:
  ShiftJisDecoder() = default;

 protected:
  SawError ParseByte(uint8_t byte, StringBuilder& result) override {
    if (uint8_t lead = std::exchange(lead_, 0x00)) {
      uint8_t offset = byte < 0x7F ? 0x40 : 0x41;
      uint8_t lead_offset = lead < 0xA0 ? 0x81 : 0xC1;
      if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFC)) {
        uint16_t pointer = (lead - lead_offset) * 188 + byte - offset;
        if (pointer >= 8836 && pointer <= 10715) {
          result.Append(static_cast<UChar>(0xE000 - 8836 + pointer));
          return SawError::kNo;
        }
        if (auto code_point = FindCodePointInJis0208(pointer)) {
          result.Append(*code_point);
          return SawError::kNo;
        }
      }
      if (IsASCII(byte))
        prepended_byte_ = byte;
      return SawError::kYes;
    }
    if (IsASCII(byte) || byte == 0x80) {
      result.Append(byte);
      return SawError::kNo;
    }
    if (byte >= 0xA1 && byte <= 0xDF) {
      result.Append(static_cast<UChar>(0xFF61 - 0xA1 + byte));
      return SawError::kNo;
    }
    if ((byte >= 0x81 && byte <= 0x9F) || (byte >= 0xE0 && byte <= 0xFC)) {
      lead_ = byte;
      return SawError::kNo;
    }
    return SawError::kYes;
  }
};

// https://encoding.spec.whatwg.org/#euc-kr-decoder
class EucKrDecoder : public TextCodecCJK::Decoder {
 public:
  EucKrDecoder() = default;

 protected:
  SawError ParseByte(uint8_t byte, StringBuilder& result) override {
    if (uint8_t lead = std::exchange(lead_, 0x00)) {
      if (byte >= 0x41 && byte <= 0xFE) {
        if (auto code_point =
                FindFirstInSortedPairs(EnsureEucKrEncodeIndexForDecode(),
                                       (lead - 0x81) * 190 + byte - 0x41)) {
          result.Append(*code_point);
          return SawError::kNo;
        }
      }
      if (IsASCII(byte))
        prepended_byte_ = byte;
      return SawError::kYes;
    }
    if (IsASCII(byte)) {
      result.Append(byte);
      return SawError::kNo;
    }
    if (byte >= 0x81 && byte <= 0xFE) {
      lead_ = byte;
      return SawError::kNo;
    }
    return SawError::kYes;
  }
};

// https://encoding.spec.whatwg.org/#gb18030-decoder
// https://encoding.spec.whatwg.org/#gbk-decoder
// Note that the same decoder is used for GB18030 and GBK.
class Gb18030Decoder : public TextCodecCJK::Decoder {
 public:
  Gb18030Decoder() = default;

  String Decode(base::span<const uint8_t> bytes,
                bool flush,
                bool stop_on_error,
                bool& saw_error) override {
    saw_error_ = &saw_error;
    String result =
        TextCodecCJK::Decoder::Decode(bytes, flush, stop_on_error, saw_error);
    // Ensures that `saw_error_` won't be used for the next run.
    saw_error_ = nullptr;
    return result;
  }

  SawError ParseByte(uint8_t byte, StringBuilder& result) override {
    DCHECK(saw_error_);
    if (third_) {
      if (byte < 0x30 || byte > 0x39) {
        *saw_error_ = true;
        result.Append(kReplacementCharacter);
        first_ = 0x00;
        uint8_t second = std::exchange(second_, 0x00);
        uint8_t third = std::exchange(third_, 0x00);
        if (ParseByte(second, result) == SawError::kYes) {
          *saw_error_ = true;
          result.Append(kReplacementCharacter);
        }
        if (ParseByte(third, result) == SawError::kYes) {
          *saw_error_ = true;
          result.Append(kReplacementCharacter);
        }
        return ParseByte(byte, result);
      }
      uint8_t first = std::exchange(first_, 0x00);
      uint8_t second = std::exchange(second_, 0x00);
      uint8_t third = std::exchange(third_, 0x00);
      if (auto code_point = IndexGb18030RangesCodePoint(
              ((first - 0x81) * 10 * 126 * 10) + ((second - 0x30) * 10 * 126) +
              ((third - 0x81) * 10) + byte - 0x30)) {
        result.Append(*code_point);
        return SawError::kNo;
      }
      return SawError::kYes;
    }
    if (second_) {
      if (byte >= 0x81 && byte <= 0xFE) {
        third_ = byte;
        return SawError::kNo;
      }
      *saw_error_ = true;
      result.Append(kReplacementCharacter);
      first_ = 0x00;
      if (ParseByte(std::exchange(second_, 0x00), result) == SawError::kYes) {
        *saw_error_ = true;
        result.Append(kReplacementCharacter);
      }
      return ParseByte(byte, result);
    }
    if (first_) {
      if (byte >= 0x30 && byte <= 0x39) {
        second_ = byte;
        return SawError::kNo;
      }
      uint8_t lead = std::exchange(first_, 0x00);
      uint8_t offset = byte < 0x7F ? 0x40 : 0x41;
      if ((byte >= 0x40 && byte <= 0x7E) || (byte >= 0x80 && byte <= 0xFE)) {
        size_t pointer = (lead - 0x81) * 190 + byte - offset;
        if (pointer < EnsureGb18030EncodeTable().size()) {
          result.Append(EnsureGb18030EncodeTable()[pointer]);
          return SawError::kNo;
        }
      }
      if (IsASCII(byte))
        prepended_byte_ = byte;
      return SawError::kYes;
    }
    if (IsASCII(byte)) {
      result.Append(byte);
      return SawError::kNo;
    }
    if (byte == 0x80) {
      result.Append(0x20AC);
      return SawError::kNo;
    }
    if (byte >= 0x81 && byte <= 0xFE) {
      first_ = byte;
      return SawError::kNo;
    }
    return SawError::kYes;
  }

  void Finalize(bool flush, StringBuilder& result) override {
    DCHECK(saw_error_);
    if (flush && (first_ || second_ || third_)) {
      first_ = 0x00;
      second_ = 0x00;
      third_ = 0x00;
      *saw_error_ = true;
      result.Append(kReplacementCharacter);
    }
  }

 private:
  uint8_t first_ = 0x00;
  uint8_t second_ = 0x00;
  uint8_t third_ = 0x00;

  // To share a reference to `saw_error` with `TextCodecCJK::Decoder::Decode`
  // we should keep a pointer to `saw_error`, and use it in `ParseByte` and
  // `Finalize`. Since `saw_error` is given as `TextCodecCJK::Decode` argument,
  // I do not think it is safe to keep the reference after
  // `TextCodecCJK::Decode` finishes.
  bool* saw_error_;
};

}  // namespace

enum class TextCodecCJK::Encoding : uint8_t {
  kEucJp,
  kIso2022Jp,
  kShiftJis,
  kEucKr,
  kGbk,
  kGb18030,
};

TextCodecCJK::TextCodecCJK(Encoding encoding) : encoding_(encoding) {}

void TextCodecCJK::RegisterEncodingNames(EncodingNameRegistrar registrar) {
  // https://encoding.spec.whatwg.org/#names-and-labels
  auto registerAliases = [&](std::initializer_list<const char*> list) {
    for (auto* alias : list)
      registrar(alias, *list.begin());
  };

  registerAliases({kCanonicalNameEucJp, "cseucpkdfmtjapanese", "x-euc-jp"});

  registerAliases({kCanonicalNameShiftJis, "csshiftjis", "ms932", "ms_kanji",
                   "shift-jis", "sjis", "windows-31j", "x-sjis"});

  registerAliases({
      kCanonicalNameEucKr,
      "cseuckr",
      "csksc56011987",
      "iso-ir-149",
      "korean",
      "ks_c_5601-1987",
      "ks_c_5601-1989",
      "ksc5601",
      "ksc_5601",
      "windows-949",
  });

  registerAliases({kCanonicalNameIso2022Jp, "csiso2022jp"});

  registerAliases({kCanonicalNameGbk, "chinese", "csgb2312", "csiso58gb231280",
                   "gb2312", "gb_2312", "gb_2312-80", "iso-ir-58", "x-gbk"});

  registerAliases({kCanonicalNameGb18030});
}

void TextCodecCJK::RegisterCodecs(TextCodecRegistrar registrar) {
  for (auto* name : kSupportedCanonicalNames) {
    registrar(name, Create, nullptr);
  }
}

std::unique_ptr<TextCodec> TextCodecCJK::Create(const TextEncoding& encoding,
                                                const void*) {
  const AtomicString& name = encoding.GetName();

  // To keep the `TextCodecCJK` constructor private, we intend to `new`
  // it and use `base::WrapUnique`. Note that we cannot use `std::make_unique`
  // for a private constructor.
  if (name == kCanonicalNameEucJp) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kEucJp));
  }
  if (name == kCanonicalNameShiftJis) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kShiftJis));
  }
  if (name == kCanonicalNameEucKr) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kEucKr));
  }
  if (name == kCanonicalNameIso2022Jp) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kIso2022Jp));
  }
  if (name == kCanonicalNameGbk) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kGbk));
  }
  if (name == kCanonicalNameGb18030) {
    return base::WrapUnique(new TextCodecCJK(Encoding::kGb18030));
  }
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

String TextCodecCJK::Decoder::Decode(base::span<const uint8_t> bytes,
                                     bool flush,
                                     bool stop_on_error,
                                     bool& saw_error) {
  StringBuilder result;
  result.ReserveCapacity(bytes.size());

  if (prepended_byte_ &&
      ParseByte(*std::exchange(prepended_byte_, std::nullopt), result) ==
          SawError::kYes) {
    saw_error = true;
    result.Append(kReplacementCharacter);
    if (stop_on_error) {
      lead_ = 0x00;
      return result.ToString();
    }
  }
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (ParseByte(bytes[i], result) == SawError::kYes) {
      saw_error = true;
      result.Append(kReplacementCharacter);
      if (stop_on_error) {
        lead_ = 0x00;
        return result.ToString();
      }
    }
    if (prepended_byte_ &&
        ParseByte(*std::exchange(prepended_byte_, std::nullopt), result) ==
            SawError::kYes) {
      saw_error = true;
      result.Append(kReplacementCharacter);
      if (stop_on_error) {
        lead_ = 0x00;
        return result.ToString();
      }
    }
  }

  if (flush && lead_) {
    lead_ = 0x00;
    saw_error = true;
    result.Append(kReplacementCharacter);
  }

  Finalize(flush, result);
  return result.ToString();
}

String TextCodecCJK::Decode(base::span<const uint8_t> data,
                            FlushBehavior flush_behavior,
                            bool stop_on_error,
                            bool& saw_error) {
  bool flush = flush_behavior != FlushBehavior::kDoNotFlush;
  if (!decoder_) {
    switch (encoding_) {
      case Encoding::kEucJp:
        decoder_ = std::make_unique<EucJpDecoder>();
        break;
      case Encoding::kShiftJis:
        decoder_ = std::make_unique<ShiftJisDecoder>();
        break;
      case Encoding::kIso2022Jp:
        decoder_ = std::make_unique<Iso2022JpDecoder>();
        break;
      case Encoding::kEucKr:
        decoder_ = std::make_unique<EucKrDecoder>();
        break;
      // GBK and GB18030 use the same decoder.
      case Encoding::kGbk:
        ABSL_FALLTHROUGH_INTENDED;
      case Encoding::kGb18030:
        decoder_ = std::make_unique<Gb18030Decoder>();
        break;
    }
  }
  return decoder_->Decode(data, flush, stop_on_error, saw_error);
}

Vector<uint8_t> TextCodecCJK::EncodeCommon(StringView string,
                                           UnencodableHandling handling) const {
  switch (encoding_) {
    case Encoding::kEucJp:
      return EncodeEucJp(string, handling);
    case Encoding::kShiftJis:
      return EncodeShiftJis(string, handling);
    case Encoding::kIso2022Jp:
      return EncodeIso2022Jp(string, handling);
    case Encoding::kEucKr:
      return EncodeEucKr(string, handling);
    case Encoding::kGbk:
      return EncodeGbk(string, handling);
    case Encoding::kGb18030:
      return EncodeGb18030(string, handling);
  }
  NOTREACHED_IN_MIGRATION();
  return {};
}

std::string TextCodecCJK::Encode(base::span<const UChar> characters,
                                 UnencodableHandling handling) {
  Vector<uint8_t> v = EncodeCommon(StringView(characters), handling);
  return std::string(v.begin(), v.end());
}

std::string TextCodecCJK::Encode(base::span<const LChar> characters,
                                 UnencodableHandling handling) {
  Vector<uint8_t> v = EncodeCommon(StringView(characters), handling);
  return std::string(v.begin(), v.end());
}

// static
bool TextCodecCJK::IsSupported(StringView name) {
  for (auto* e : kSupportedCanonicalNames) {
    if (e == name) {
      return true;
    }
  }
  return false;
}

}  // namespace WTF
