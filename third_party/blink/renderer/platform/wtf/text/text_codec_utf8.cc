/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/platform/wtf/text/text_codec_utf8.h"

#include <array>
#include <memory>
#include <optional>
#include <ranges>
#include <variant>

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/types/to_address.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_codec_ascii_fast_path.h"
#include "third_party/simdutf/simdutf.h"

namespace blink {

namespace {

// Minimum chunk size to use TryDecodeWithSimdutf(); smaller inputs stay on the
// inline-buffered scalar path (InlinedStringBuffer uses a 128-byte stack
// buffer) to avoid fast-path setup overhead on tiny chunks.
constexpr size_t kSimdutfMinInputBytes = 256;

// Returns true if `bytes` contains any byte >= 0xC4.
// In valid UTF-8, ASCII bytes are 0x00..0x7F, continuation bytes are
// 0x80..0xBF, and Latin-1 (U+0080..U+00FF) lead bytes are 0xC2 or 0xC3. Any
// code point >= U+0100 begins with a lead byte in 0xC4..0xF4, so valid UTF-8
// decodes to Latin-1 iff it contains no byte >= 0xC4.
bool ContainsByteAboveLatin1Lead(base::span<const uint8_t> bytes) {
  while (bytes.size() >= 64) {
    auto block = bytes.first<64>();
    uint8_t any_above = 0;
    for (uint8_t b : block) {
      any_above |= (b >= 0xC4);
    }
    if (any_above) {
      return true;
    }
    bytes = bytes.subspan(64u);
  }
  for (uint8_t b : bytes) {
    if (b >= 0xC4) {
      return true;
    }
  }
  return false;
}

// We'll use kNonCharacter* constants to signal invalid utf-8.
// The number in the name signals how many input bytes were invalid.
constexpr int kNonCharacter1 = -1;
constexpr int kNonCharacter2 = -2;
constexpr int kNonCharacter3 = -3;

bool IsNonCharacter(int character) {
  return character >= kNonCharacter3 && character <= kNonCharacter1;
}

ALWAYS_INLINE size_t LengthOfNonCharacter(int character) {
  DCHECK(IsNonCharacter(character));
  return -character;
}

constexpr std::array<uint8_t, 256> kNonAsciiSequenceLength = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

inline int DecodeNonAsciiSequence(base::span<const uint8_t> sequence) {
  DCHECK(!IsAscii(sequence[0]));

  const size_t length = sequence.size();
  if (length == 2) {
    DCHECK_GE(sequence[0], 0xC2);
    DCHECK_LE(sequence[0], 0xDF);
    if (sequence[1] < 0x80 || sequence[1] > 0xBF)
      return kNonCharacter1;
    return ((sequence[0] << 6) + sequence[1]) - 0x00003080;
  }
  if (length == 3) {
    DCHECK_GE(sequence[0], 0xE0);
    DCHECK_LE(sequence[0], 0xEF);
    switch (sequence[0]) {
      case 0xE0:
        if (sequence[1] < 0xA0 || sequence[1] > 0xBF)
          return kNonCharacter1;
        break;
      case 0xED:
        if (sequence[1] < 0x80 || sequence[1] > 0x9F)
          return kNonCharacter1;
        break;
      default:
        if (sequence[1] < 0x80 || sequence[1] > 0xBF)
          return kNonCharacter1;
    }
    if (sequence[2] < 0x80 || sequence[2] > 0xBF)
      return kNonCharacter2;
    return ((sequence[0] << 12) + (sequence[1] << 6) + sequence[2]) -
           0x000E2080;
  }
  DCHECK_EQ(length, 4u);
  DCHECK_GE(sequence[0], 0xF0);
  DCHECK_LE(sequence[0], 0xF4);
  switch (sequence[0]) {
    case 0xF0:
      if (sequence[1] < 0x90 || sequence[1] > 0xBF)
        return kNonCharacter1;
      break;
    case 0xF4:
      if (sequence[1] < 0x80 || sequence[1] > 0x8F)
        return kNonCharacter1;
      break;
    default:
      if (sequence[1] < 0x80 || sequence[1] > 0xBF)
        return kNonCharacter1;
  }
  if (sequence[2] < 0x80 || sequence[2] > 0xBF)
    return kNonCharacter2;
  if (sequence[3] < 0x80 || sequence[3] > 0xBF)
    return kNonCharacter3;
  return ((sequence[0] << 18) + (sequence[1] << 12) + (sequence[2] << 6) +
          sequence[3]) -
         0x03C82080;
}

inline base::span<UChar> AppendCharacter(base::span<UChar> destination,
                                         int character) {
  DCHECK(!IsNonCharacter(character));
  DCHECK(!U_IS_SURROGATE(character));
  if (U_IS_BMP(character)) {
    destination.take_first<1ul>()[0] = static_cast<UChar>(character);
  } else {
    auto surrogates = destination.take_first<2u>();
    surrogates[0] = U16_LEAD(character);
    surrogates[1] = U16_TRAIL(character);
  }
  return destination;
}

template <typename CharType>
class InlinedStringBuffer {
 public:
  explicit InlinedStringBuffer(size_t size) {
    if (size >= kInlinedSize) {
      buffer_.template emplace<StringBuffer<CharType>>(size);
      span_ = std::get<OutlinedArray>(buffer_).Span();
    }
  }

  InlinedStringBuffer(const InlinedStringBuffer&) = delete;
  InlinedStringBuffer& operator=(const InlinedStringBuffer&) = delete;

  base::span<CharType> Span() const { return span_; }

  String ToString(size_t end) && {
    if (std::holds_alternative<InlinedArray>(buffer_)) {
      return String(span_.first(end));
    }
    auto& outlined = std::get<OutlinedArray>(buffer_);
    DCHECK_EQ(Span().data(), outlined.Span().data());
    outlined.Shrink(static_cast<wtf_size_t>(end));
    return String::Adopt(outlined);
  }

 private:
  static constexpr size_t kInlinedSize = 128;
  using InlinedArray = std::array<CharType, kInlinedSize>;
  using OutlinedArray = StringBuffer<CharType>;

  std::variant<InlinedArray, OutlinedArray> buffer_;
  base::span<CharType> span_ = base::span(std::get<InlinedArray>(buffer_));
};

}  // namespace

std::unique_ptr<TextCodec> TextCodecUtf8::Create(const TextEncoding&) {
  return base::WrapUnique(new TextCodecUtf8());
}

bool TextCodecUtf8::IsSupported(StringView canonical_name) {
  return EqualIgnoringAsciiCase(canonical_name, "UTF-8");
}

void TextCodecUtf8::RegisterEncodingNames(EncodingNameRegistrar registrar) {
  AtomicString canonical_name("UTF-8");
  registrar("UTF-8", canonical_name);

  // Additional aliases that originally were present in the encoding
  // table in WebKit on Macintosh, and subsequently added by
  // TextCodecICU. Perhaps we can prove some are not used on the web
  // and remove them.
  registrar("unicode11utf8", canonical_name);
  registrar("unicode20utf8", canonical_name);
  registrar("utf8", canonical_name);
  registrar("x-unicode20utf8", canonical_name);

  // Additional aliases present in the WHATWG Encoding Standard
  // (http://encoding.spec.whatwg.org/)
  // and Firefox (24), but not in ICU 4.6.
  registrar("unicode-1-1-utf-8", canonical_name);
}

void TextCodecUtf8::RegisterCodecs(TextCodecRegistrar registrar) {
  registrar("UTF-8", Create);
}

void TextCodecUtf8::SavePartialSequenceBytes(
    base::span<const uint8_t>& source) {
  DCHECK(!partial_sequence_size_);
  partial_sequence_size_ = source.size();
  base::span(partial_sequence_)
      .first(partial_sequence_size_)
      .copy_from(source.take_first(partial_sequence_size_));
}

void TextCodecUtf8::ConsumePartialSequenceBytes(size_t num_bytes) {
  DCHECK_GE(partial_sequence_size_, num_bytes);
  partial_sequence_size_ -= num_bytes;
  base::span(partial_sequence_)
      .first(partial_sequence_size_)
      .copy_from(base::span(partial_sequence_)
                     .subspan(num_bytes, partial_sequence_size_));
}

void TextCodecUtf8::FillPartialSequenceBytes(
    size_t sequence_length,
    base::span<const uint8_t>& source) {
  DCHECK_GT(sequence_length, partial_sequence_size_);
  // Copy from `source` until we have `sequence_length` bytes.
  if (!source.empty()) {
    size_t additional_bytes = std::min<size_t>(
        sequence_length - partial_sequence_size_, source.size());
    base::span(partial_sequence_)
        .subspan(partial_sequence_size_, additional_bytes)
        .copy_from(source.take_first(additional_bytes));
    partial_sequence_size_ += additional_bytes;
  }
  // If we still don't have `sequence_length` bytes, fill the rest with zeros
  // (any other lead byte would do), so we can run `DecodeNonAsciiSequence` to
  // tell if the chunk that we have is valid. These bytes are not part of the
  // partial sequence, so don't increment `partial_sequence_size`.
  if (sequence_length > partial_sequence_size_) {
    std::ranges::fill(base::span(partial_sequence_)
                          .first(sequence_length)
                          .last(sequence_length - partial_sequence_size_),
                      0);
  }
}

bool TextCodecUtf8::NeedMoreData(size_t sequence_length,
                                 int character,
                                 bool flush) const {
  if (sequence_length <= partial_sequence_size_) {
    return false;
  }
  // If at the end, there's no more data.
  if (flush) {
    return false;
  }
  const size_t noncharacter_len = LengthOfNonCharacter(character);
  DCHECK(IsNonCharacter(character));
  DCHECK_LE(noncharacter_len, partial_sequence_size_);
  // The partial sequence that we have is incomplete but otherwise valid, a
  // non-character is not an error.
  return noncharacter_len == partial_sequence_size_;
}

bool TextCodecUtf8::HandlePartialSequence(base::span<LChar>& destination,
                                          base::span<const uint8_t>& source,
                                          bool flush) {
  DCHECK(partial_sequence_size_);
  do {
    if (IsAscii(partial_sequence_[0])) {
      destination.take_first<1u>()[0] = partial_sequence_[0];
      ConsumePartialSequenceBytes(1);
      continue;
    }
    size_t count = kNonAsciiSequenceLength[partial_sequence_[0]];
    int character;
    if (!count) {
      character = kNonCharacter1;
    } else {
      if (count > partial_sequence_size_) {
        FillPartialSequenceBytes(count, source);
      }
      character =
          DecodeNonAsciiSequence(base::span(partial_sequence_).first(count));
      if (NeedMoreData(count, character, flush)) {
        return false;
      }
    }
    // The character is invalid or outside the Latin-1 range. Both of these
    // cases are handled by the UTF-16 code-path.
    if (character & ~0xff) {
      return true;
    }
    // `count` should be always be two here and the partial buffer can't
    // contain more code units than that at this point. ASCII characters can't
    // be partial, and all Latin-1 characters can be encoded with two code
    // units. Anything else (non-Latin-1, invalid characters) would be handled
    // by the UTF-16 code-path (below).
    DCHECK_EQ(count, 2u);
    DCHECK_EQ(partial_sequence_size_, count);
    partial_sequence_size_ -= count;
    destination.take_first<1u>()[0] = static_cast<LChar>(character);
  } while (partial_sequence_size_);

  return false;
}

bool TextCodecUtf8::HandlePartialSequence(base::span<UChar>& destination,
                                          base::span<const uint8_t>& source,
                                          bool flush,
                                          bool stop_on_error,
                                          bool& saw_error) {
  DCHECK(partial_sequence_size_);
  do {
    if (IsAscii(partial_sequence_[0])) {
      destination.take_first<1u>()[0] = partial_sequence_[0];
      ConsumePartialSequenceBytes(1);
      continue;
    }
    size_t count = kNonAsciiSequenceLength[partial_sequence_[0]];
    int character;
    if (!count) {
      character = kNonCharacter1;
    } else {
      if (count > partial_sequence_size_) {
        FillPartialSequenceBytes(count, source);
      }
      character =
          DecodeNonAsciiSequence(base::span(partial_sequence_).first(count));
      if (NeedMoreData(count, character, flush)) {
        return false;
      }
    }
    if (IsNonCharacter(character)) {
      saw_error = true;
      if (stop_on_error)
        return false;
      count = LengthOfNonCharacter(character);
      character = uchar::kReplacementCharacter;
    }
    destination = AppendCharacter(destination, character);
    ConsumePartialSequenceBytes(count);
  } while (partial_sequence_size_);

  return false;
}

bool TextCodecUtf8::IsSimdutfEnabled() {
  if (!simdutf_enabled_.has_value()) {
    simdutf_enabled_ =
        base::FeatureList::IsEnabled(features::kSimdutfTextCodecUtf8Decode);
  }
  return *simdutf_enabled_;
}

std::optional<String> TextCodecUtf8::TryDecodeWithSimdutf(
    base::span<const uint8_t> bytes,
    bool do_flush) {
  DCHECK_GE(bytes.size(), kSimdutfMinInputBytes);

  size_t max_output_len = 0;
  if (!base::CheckAdd(partial_sequence_size_, bytes.size())
           .AssignIfValid(&max_output_len) ||
      !base::IsValueInRangeForNumericType<wtf_size_t>(max_output_len)) {
    return std::nullopt;
  }

  base::span<const uint8_t> remaining_bytes = bytes;
  std::optional<int> leading_char;

  // If the previous streaming chunk left 1..3 incomplete bytes in
  // `partial_sequence_`, complete that code point from the start of `bytes`
  // without mutating `partial_sequence_` until the entire chunk succeeds.
  if (partial_sequence_size_ > 0) {
    const size_t lead_seq_len = kNonAsciiSequenceLength[partial_sequence_[0]];
    if (lead_seq_len <= partial_sequence_size_) {
      return std::nullopt;
    }
    const size_t needed = lead_seq_len - partial_sequence_size_;
    std::array<uint8_t, U8_MAX_LENGTH> combined = {};
    base::span(combined)
        .first(partial_sequence_size_)
        .copy_from(base::span(partial_sequence_).first(partial_sequence_size_));
    base::span(combined)
        .subspan(partial_sequence_size_, needed)
        .copy_from(remaining_bytes.first(needed));
    const int ch =
        DecodeNonAsciiSequence(base::span(combined).first(lead_seq_len));
    if (IsNonCharacter(ch)) {
      return std::nullopt;
    }
    leading_char = ch;
    remaining_bytes = remaining_bytes.subspan(needed);
  }

  DCHECK_GE(remaining_bytes.size(), 4u);
  base::span<const uint8_t> complete_bytes = remaining_bytes;
  base::span<const uint8_t> trailing_partial;

  // When streaming (!do_flush), split off any valid incomplete 1..3 byte UTF-8
  // prefix at the end of the chunk using the same zero-padded check as
  // FillPartialSequenceBytes() + NeedMoreData(). Invalid prefixes (such as
  // 0xE0 0x80 or 0xED 0xA0) have LengthOfNonCharacter(ch) < avail and are kept
  // in `complete_bytes` so fallback emits U+FFFD immediately.
  // `partial_sequence_` is only updated after `complete_bytes` decodes without
  // error.
  if (!do_flush && remaining_bytes.back() >= 0x80) {
    for (size_t avail = 1; avail <= 3; ++avail) {
      base::span<const uint8_t> tail = remaining_bytes.last(avail);
      const size_t seq_len = kNonAsciiSequenceLength[tail[0]];
      if (seq_len > avail) {
        std::array<uint8_t, U8_MAX_LENGTH> padded = {};
        base::span(padded).first(avail).copy_from(tail);
        const int ch =
            DecodeNonAsciiSequence(base::span(padded).first(seq_len));
        if (IsNonCharacter(ch) && LengthOfNonCharacter(ch) == avail) {
          complete_bytes =
              remaining_bytes.first(remaining_bytes.size() - avail);
          trailing_partial = tail;
          break;
        }
      }
    }
  }

  auto commit_partial_state = [&]() {
    partial_sequence_size_ = 0;
    if (!trailing_partial.empty()) {
      SavePartialSequenceBytes(trailing_partial);
    }
  };

  // Scan the ASCII prefix with FindFirstNonASCII() before invoking simdutf so
  // pure-ASCII chunks (such as streamed scripts) avoid simdutf dispatch and
  // remainder-buffer overhead.
  const size_t first_non_ascii = base::FindFirstNonASCII(
      base::as_string_view(base::as_chars(complete_bytes)));
  if (first_non_ascii == complete_bytes.size()) {
    if (!leading_char.has_value()) {
      commit_partial_state();
      return String(complete_bytes);
    }
    if (*leading_char <= 0xFF) {
      StringBuffer<LChar> buf8(
          base::checked_cast<wtf_size_t>(1 + complete_bytes.size()));
      base::span<LChar> dst8 = buf8.Span();
      dst8[0] = static_cast<LChar>(*leading_char);
      dst8.subspan(1u).copy_from(complete_bytes);
      commit_partial_state();
      return String::Adopt(buf8);
    }
    const size_t lead_u16_len = U_IS_BMP(*leading_char) ? 1u : 2u;
    StringBuffer<UChar> buf16(
        base::checked_cast<wtf_size_t>(lead_u16_len + complete_bytes.size()));
    base::span<UChar> dst16 = AppendCharacter(buf16.Span(), *leading_char);
    StringImpl::CopyChars(dst16, complete_bytes);
    commit_partial_state();
    return String::Adopt(buf16);
  }

  const uint8_t first_lead = complete_bytes[first_non_ascii];
  if (kNonAsciiSequenceLength[first_lead] == 0) {
    return std::nullopt;
  }

  const base::span<const uint8_t> suffix =
      complete_bytes.subspan(first_non_ascii);

  // Validate the non-ASCII suffix once via simdutf; any malformed UTF-8 falls
  // back to the scalar state machine with codec state untouched. Upfront
  // validation is also required for memory safety before calling
  // latin1_length_from_utf8() or utf16_length_from_utf8() below, which assume
  // well-formed UTF-8 to size the output buffer for simdutf's unchecked span
  // conversion wrappers.
  if (!simdutf::validate_utf8(suffix)) {
    return std::nullopt;
  }

  // Preserve an 8-bit String when all decoded code points fit in Latin-1
  // (<= U+00FF). Checking !ContainsByteAboveLatin1Lead(suffix) before
  // allocating avoids discarding an 8-bit buffer when Latin-1 characters (such
  // as U+00A0 or U+00B7) precede code points > U+00FF, and sizing via
  // latin1_length_from_utf8() avoids a second Shrink() allocation.
  if (leading_char.value_or(0) <= 0xFF &&
      !ContainsByteAboveLatin1Lead(suffix)) {
    const size_t lead_lchar_len = leading_char.has_value() ? 1u : 0u;
    const size_t suffix_latin1_len = simdutf::latin1_length_from_utf8(suffix);
    const size_t latin1_len =
        lead_lchar_len + first_non_ascii + suffix_latin1_len;
    StringBuffer<LChar> buf8(base::checked_cast<wtf_size_t>(latin1_len));
    base::span<LChar> dst8 = buf8.Span();
    if (leading_char.has_value()) {
      dst8.take_first<1u>()[0] = static_cast<LChar>(*leading_char);
    }
    if (first_non_ascii > 0) {
      dst8.take_first(first_non_ascii)
          .copy_from(complete_bytes.first(first_non_ascii));
    }
    const size_t written = simdutf::convert_utf8_to_latin1(suffix, dst8);
    CHECK_EQ(written, suffix_latin1_len);
    commit_partial_state();
    return String::Adopt(buf8);
  }

  // Allocate a single exact-sized StringBuffer<UChar> and transcode via
  // simdutf instead of allocating and discarding an 8-bit buffer.
  const size_t lead_u16_len =
      leading_char.has_value() ? (U_IS_BMP(*leading_char) ? 1u : 2u) : 0u;
  const size_t suffix_utf16_len = simdutf::utf16_length_from_utf8(suffix);
  const size_t utf16_len = lead_u16_len + first_non_ascii + suffix_utf16_len;
  StringBuffer<UChar> buf16(base::checked_cast<wtf_size_t>(utf16_len));
  base::span<UChar> dst16 = buf16.Span();
  if (leading_char.has_value()) {
    dst16 = AppendCharacter(dst16, *leading_char);
  }
  if (first_non_ascii > 0) {
    StringImpl::CopyChars(dst16.take_first(first_non_ascii),
                          complete_bytes.first(first_non_ascii));
  }
  const size_t written = simdutf::convert_utf8_to_utf16(suffix, dst16);
  CHECK_EQ(written, suffix_utf16_len);

  commit_partial_state();
  return String::Adopt(buf16);
}

String TextCodecUtf8::Decode(base::span<const uint8_t> bytes,
                             FlushBehavior flush,
                             bool stop_on_error,
                             bool& saw_error) {
  const bool do_flush = flush != FlushBehavior::kDoNotFlush;
  if (bytes.size() >= kSimdutfMinInputBytes && IsSimdutfEnabled()) {
    if (std::optional<String> decoded = TryDecodeWithSimdutf(bytes, do_flush)) {
      return *std::move(decoded);
    }
  }
  const bool fast_ascii_range_copy =
      base::FeatureList::IsEnabled(base::features::kUtfConversionAsciiFastPath);

  // Each input byte might turn into a character.
  // That includes all bytes in the partial-sequence buffer because
  // each byte in an invalid sequence will turn into a replacement character.
  InlinedStringBuffer<LChar> buffer(
      base::CheckAdd(partial_sequence_size_, bytes.size()).ValueOrDie());
  base::span<LChar> destination = buffer.Span();

  const uint8_t* aligned_end =
      AlignToMachineWord(base::to_address(bytes.end()));
  auto source = bytes;
  size_t characters_decoded;

  do {
    if (partial_sequence_size_) {
      // Explicitly copy destination and source pointers to avoid taking
      // pointers to the local variables, which may harm code generation by
      // disabling some optimizations in some compilers.
      base::span<LChar> destination_for_handle_partial_sequence = destination;
      base::span<const uint8_t> source_for_handle_partial_sequence = source;
      if (HandlePartialSequence(destination_for_handle_partial_sequence,
                                source_for_handle_partial_sequence, do_flush)) {
        source = source_for_handle_partial_sequence;
        goto upConvertTo16Bit;
      }
      destination = destination_for_handle_partial_sequence;
      source = source_for_handle_partial_sequence;
      if (partial_sequence_size_)
        break;
    }

    while (!source.empty()) {
      if (IsAscii(source[0])) {
        // Fast path for ASCII. Most UTF-8 text will be ASCII.
        if (fast_ascii_range_copy) {
          size_t ascii_len = base::FindFirstNonASCII(
              base::as_string_view(base::as_chars(source)));
          destination.take_first(ascii_len).copy_from(
              source.take_first(ascii_len));
          if (source.empty()) {
            break;
          }
        } else {
          if (IsAlignedToMachineWord(source.data())) {
            while (source.data() < aligned_end) {
              MachineWord chunk =
                  *reinterpret_cast<const MachineWord*>(source.data());
              if (!IsAllAscii<LChar>(chunk)) {
                break;
              }
              CopyAsciiMachineWord(
                  chunk, destination.take_first<sizeof(MachineWord)>().data());
              source.take_first<sizeof(MachineWord)>();
            }
            if (source.empty()) {
              break;
            }
            if (!IsAscii(source[0])) {
              continue;
            }
          }
          destination.take_first<1u>()[0] = source.take_first_elem();
          continue;
        }
      }
      size_t count = kNonAsciiSequenceLength[source[0]];
      int character;
      if (count == 0) {
        character = kNonCharacter1;
      } else {
        if (count > source.size()) {
          SavePartialSequenceBytes(source);
          break;
        }
        character = DecodeNonAsciiSequence(source.first(count));
      }
      if (IsNonCharacter(character)) {
        saw_error = true;
        if (stop_on_error)
          break;

        goto upConvertTo16Bit;
      }
      if (character > 0xff)
        goto upConvertTo16Bit;

      source = source.subspan(count);
      destination.take_first<1u>()[0] = static_cast<LChar>(character);
    }
  } while (partial_sequence_size_);

  characters_decoded = destination.data() - buffer.Span().data();
  return std::move(buffer).ToString(characters_decoded);

upConvertTo16Bit:
  InlinedStringBuffer<UChar> buffer16(
      base::CheckAdd(partial_sequence_size_, bytes.size()).ValueOrDie());
  base::span<UChar> destination16 = buffer16.Span();

  // Copy the already converted characters
  const size_t characters_converted =
      static_cast<size_t>(destination.data() - buffer.Span().data());
  StringImpl::CopyChars(destination16.take_first(characters_converted),
                        buffer.Span().first(characters_converted));

  do {
    if (partial_sequence_size_) {
      // Explicitly copy destination and source pointers to avoid taking
      // pointers to the local variables, which may harm code generation by
      // disabling some optimizations in some compilers.
      base::span<UChar> destination_for_handle_partial_sequence = destination16;
      base::span<const uint8_t> source_for_handle_partial_sequence = source;
      HandlePartialSequence(destination_for_handle_partial_sequence,
                            source_for_handle_partial_sequence, do_flush,
                            stop_on_error, saw_error);
      destination16 = destination_for_handle_partial_sequence;
      source = source_for_handle_partial_sequence;
      if (partial_sequence_size_)
        break;
    }

    while (!source.empty()) {
      if (IsAscii(source[0])) {
        // Fast path for ASCII. Most UTF-8 text will be ASCII.
        if (fast_ascii_range_copy) {
          size_t ascii_len = base::FindFirstNonASCII(
              base::as_string_view(base::as_chars(source)));
          StringImpl::CopyChars(destination16.take_first(ascii_len),
                                source.take_first(ascii_len));
          if (source.empty()) {
            break;
          }
        } else {
          if (IsAlignedToMachineWord(source.data())) {
            while (source.data() < aligned_end) {
              MachineWord chunk =
                  *reinterpret_cast<const MachineWord*>(source.data());
              if (!IsAllAscii<LChar>(chunk)) {
                break;
              }
              // SAFETY: `take_first<sizeof(MachineWord)>()` ensures sufficient
              // size.
              UNSAFE_BUFFERS(CopyAsciiMachineWord(
                  chunk,
                  destination16.take_first<sizeof(MachineWord)>().data()));
              source.take_first<sizeof(MachineWord)>();
            }
            if (source.empty()) {
              break;
            }
            if (!IsAscii(source[0])) {
              continue;
            }
          }
          destination16.take_first<1u>()[0] = source.take_first_elem();
          continue;
        }
      }
      size_t count = kNonAsciiSequenceLength[source[0]];
      int character;
      if (count == 0) {
        character = kNonCharacter1;
      } else {
        if (count > source.size()) {
          SavePartialSequenceBytes(source);
          break;
        }
        character = DecodeNonAsciiSequence(source.first(count));
      }
      if (IsNonCharacter(character)) {
        saw_error = true;
        if (stop_on_error)
          break;
        // Each error generates one replacement character and consumes the
        // 'largest subpart' of the incomplete character. Note that the
        // kNonCharacterX constants go from -1..-3 and contain the negative of
        // number of bytes comprising the broken encoding detected.
        count = LengthOfNonCharacter(character);
        character = uchar::kReplacementCharacter;
      }
      source = source.subspan(count);
      destination16 = AppendCharacter(destination16, character);
    }
  } while (partial_sequence_size_);

  characters_decoded = destination16.data() - buffer16.Span().data();
  return std::move(buffer16).ToString(characters_decoded);
}

template <typename CharType>
std::string TextCodecUtf8::EncodeCommon(base::span<const CharType> characters) {
  // The maximum number of UTF-8 bytes needed per UTF-16 code unit is 3.
  // BMP characters take only one UTF-16 code unit and can take up to 3 bytes
  // (3x).
  // Non-BMP characters take two UTF-16 code units and can take up to 4 bytes
  // (2x).
  CHECK_LE(characters.size(), std::numeric_limits<wtf_size_t>::max() / 3);
  const wtf_size_t length = static_cast<wtf_size_t>(characters.size());
  std::string result;
  result.resize(length * 3);

  wtf_size_t i = 0;
  wtf_size_t bytes_written = 0;
  auto destination = base::as_writable_byte_span(result);
  while (i < length) {
    UChar32 character;
    U16_NEXT(characters, i, length, character);
    // U16_NEXT will simply emit a surrogate code point if an unmatched
    // surrogate is encountered; we must convert it to a
    // U+FFFD (REPLACEMENT CHARACTER) here.
    if (0xD800 <= character && character <= 0xDFFF)
      character = uchar::kReplacementCharacter;
    U8_APPEND_UNSAFE(destination, bytes_written, character);
  }

  result.resize(bytes_written);
  return result;
}

template <typename CharType>
TextCodec::EncodeIntoResult TextCodecUtf8::EncodeIntoCommon(
    base::span<const CharType> source,
    base::span<uint8_t> destination) {
  const wtf_size_t length = base::checked_cast<wtf_size_t>(source.size());
  TextCodec::EncodeIntoResult encode_into_result{0, 0};

  wtf_size_t i = 0;
  wtf_size_t previous_code_unit_index = 0;
  bool is_error = false;
  while (i < length && encode_into_result.bytes_written < destination.size() &&
         !is_error) {
    UChar32 character;
    previous_code_unit_index = i;
    U16_NEXT(source, i, length, character);
    // U16_NEXT will simply emit a surrogate code point if an unmatched
    // surrogate is encountered. See comment in EncodeCommon() for more info.
    if (0xD800 <= character && character <= 0xDFFF)
      character = uchar::kReplacementCharacter;
    U8_APPEND(destination, encode_into_result.bytes_written, destination.size(),
              character, is_error);
  }

  // |is_error| is only true when U8_APPEND cannot append the UTF8 bytes that
  // represent a given UTF16 code point, due to limited capacity. In that case,
  // the last code point read was not used, so we must not include its code
  // units in our final |code_units_read| count.
  if (is_error)
    encode_into_result.code_units_read = previous_code_unit_index;
  else
    encode_into_result.code_units_read = i;

  return encode_into_result;
}

std::string TextCodecUtf8::Encode(base::span<const UChar> characters,
                                  UnencodableHandling) {
  return EncodeCommon(characters);
}

std::string TextCodecUtf8::Encode(base::span<const LChar> characters,
                                  UnencodableHandling) {
  return EncodeCommon(characters);
}

TextCodec::EncodeIntoResult TextCodecUtf8::EncodeInto(
    base::span<const UChar> characters,
    base::span<uint8_t> destination) {
  return EncodeIntoCommon(characters, destination);
}

TextCodec::EncodeIntoResult TextCodecUtf8::EncodeInto(
    base::span<const LChar> characters,
    base::span<uint8_t> destination) {
  return EncodeIntoCommon(characters, destination);
}

}  // namespace blink
