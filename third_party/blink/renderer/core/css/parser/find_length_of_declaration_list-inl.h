// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FIND_LENGTH_OF_DECLARATION_LIST_INL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FIND_LENGTH_OF_DECLARATION_LIST_INL_H_

// This file contains SIMD code to try to heuristically detect
// the length of a CSS declaration block. We use this during parsing
// in order to skip over them quickly (we don't need to parse
// all the properties until the first time something actually matches
// the selector). This is akin to setting a BlockGuard and then
// immediately calling SkipToEndOfBlock(), except that
//
//  a) It is much, much faster (something like 10x), since we don't
//     need to run the full tokenizer.
//  b) It is allowed to error out if there are some cases that are
//     too complicated for it to understand (e.g. cases that would
//     require simulating the entire block stack).
//  c) It knows to detect nested rules, and also similarly error out.
//     All of them have to involve { in some shape or form, so that
//     is a fairly easy check (except that we ignore it within strings).
//
// We _don't_ support these cases (i.e., we just error out), which
// we've empirically found to be rare within declaration blocks:
//
//   - Escaping using \ (possible, but requires counting whether
//     we have an even or odd number of them).
//   - [ and ] (would require the block stack).
//   - Extraneous ) (possible, but adds complications and would be rare)
//   - CSS comments (would require interactions with string parsing).
//   - ' within " or " within ' (complex, see below).
//
// The entry point is FindLengthOfDeclarationList(), which returns
// the number of bytes until the block's ending }, exclusive.
// Returns 0 if some kind of error occurred, which means the caller
// will need to parse the block the normal (slow) way.
//
// On x86, we are a bit hampered by our instruction set support;
// it would be nice to have e.g. AVX2, or possibly a smaller subset
// that includes PSHUFB and/or PCLMULQDQ. However, we do fairly well
// already with base SSE2. On Arm (at least on M1, which is very wide),
// it would help slightly to unroll 2x manually to extract more ILP;
// there are many paths where there are long dependency chains.

#ifdef __SSE2__
#include <immintrin.h>
#elif defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"

#ifdef __SSE2__

// Loads the 16 next UTF-16 runes, but any character that is >= 256
// will be converted into 0xFF, so that it has no chance of matching
// anything that we care about below. This maps elegantly to a
// saturating pack, since our values are in little-endian already.
static inline __m128i LoadAndCollapseHighBytes(const UChar* ptr) {
  __m128i x1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
  __m128i x2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr + 8));
  return _mm_packus_epi16(x1, x2);
}

// For LChar, this is trivial; just load the bytes as-is.
static inline __m128i LoadAndCollapseHighBytes(const LChar* ptr) {
  return _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
}

template <class CharType>
ALWAYS_INLINE static size_t FindLengthOfDeclarationList(const CharType* begin,
                                                        const CharType* end) {
  // If the previous block ended with quote status (see below),
  // the lowest byte of this will be all-ones.
  __m128i prev_quoted = _mm_setzero_si128();

  // Similarly, the lowest byte of this will be the number
  // of open parentheses from the previous block.
  __m128i prev_parens = _mm_setzero_si128();

  const CharType* ptr = begin;
  while (ptr + 17 <= end) {
    __m128i x = LoadAndCollapseHighBytes(ptr);
    __m128i next_x = LoadAndCollapseHighBytes(ptr + 1);

    // We don't want deal with escaped characters within strings,
    // and they are generally rare, so if we see any backslashes,
    // just error out. Note that backslashes are _not_ masked out
    // by being inside strings, but the others are.
    const __m128i eq_backslash = _mm_cmpeq_epi8(x, _mm_set1_epi8('\\'));

    // We need to mask out any characters that are part of strings.
    // Strings are both started and ended with quotation marks.
    // We do a prefix-xor to spread out “are we in a string”
    // status from each quotation mark to the next stop. The standard
    // trick to do this would be PCLMULQDQ, but that requires CPU support
    // that we don't have, and it's also fairly high-latency, so we'd
    // need to find some other useful calculations to do while it works.
    // (Also, it only supports 64-bit inputs, so we'd need to do
    // something like PMOVMSKB and then move _back_ into vector
    // registers.)
    //
    // Thus, we'll simply do a prefix xor-sum instead. Note that we
    // need to factor in the quote status from the previous block;
    // if it had an unclosed quote (whether from a mark in its own
    // block, or carried on from a previous one), we need to
    // include that.
    //
    // There is a complication here. Both " and ' are allowed as
    // quotes, so we need to track both. But if one is within the
    // other, it does not count as a quote and should not start
    // a new span (this is similar to having a comment start token
    // within a string). We don't support this case (it seems hard
    // to do branch-free with SIMD), but we need to detect it.
    // Thus, what we do is that instead of just having all-ones
    // or all-zeros for each byte, we actually use and propagate
    // the byte value of the quote mark itself through the XOR cascade.
    // This leads to four values any byte can take:
    //
    //  - 0x00: We are not within quotes.
    //  - 0x22: We are within double quotes.
    //  - 0x27: We are within single quotes.
    //  - 0x05: We have overlapping single and double quote spans.
    //
    // 0x22 and 0x27 have the same meaning, namely “we're within quotes,
    // and need to mask out all special character handling” (i.e., convert
    // to 0xFF). But 0x05 means that we have ' within " or " within '
    // (mixed quotes), and this is a case we're not prepared to take on.
    // Thus, we treat this as an error condition and abort. It is possible
    // that we could treat these 0x05 bytes as a special kind of “cancel
    // quote” and do a _new_ XOR cascade on those bytes alone, but I haven't
    // checked deeply that it will actually work, and it seems very much
    // not worth it for such a narrow case.
    const __m128i eq_single_quote = _mm_cmpeq_epi8(x, _mm_set1_epi8('\''));
    const __m128i eq_double_quote = _mm_cmpeq_epi8(x, _mm_set1_epi8('"'));
    __m128i quoted = x & (eq_single_quote | eq_double_quote);
    quoted ^= prev_quoted;
    quoted ^= _mm_slli_si128(quoted, 1);
    quoted ^= _mm_slli_si128(quoted, 2);
    quoted ^= _mm_slli_si128(quoted, 4);
    quoted ^= _mm_slli_si128(quoted, 8);
    const __m128i mixed_quote =
        _mm_cmpeq_epi8(quoted, _mm_set1_epi8('\'' ^ '"'));

    // Now we have a mask of bytes that are inside quotes
    // (which happens to include the first quote, though
    // we don't really care). E.g., if we have this string,
    // our mask will be:
    //
    //    abc"def"ghij"kl"m...
    //    00011110000011100...
    //
    // We can use this to simply ignore things inside strings.
    // (We don't need to mask next_x as it's only used for comments;
    // masking x will be sufficient.)
    x = _mm_andnot_si128(_mm_cmpgt_epi8(quoted, _mm_setzero_si128()), x);

    // Look for start of comments; successive / and * characters.
    // We don't support them, as they are fairly rare and we'd need to
    // do similar masking as with strings.
    const __m128i comment_start = _mm_cmpeq_epi8(x, _mm_set1_epi8('/')) &
                                  _mm_cmpeq_epi8(next_x, _mm_set1_epi8('*'));

    // Parentheses within values are fairly common, e.g. due to var(),
    // calc() and similar. We need to keep track of how many of them we have
    // (since we disallow [ and {, they are the only thing that can be
    // on the block stack), because if we see a } when we have a non-zero
    // number of parens open, that } doesn't actually count as block end.
    //
    // Thus, we count opening paren as +1 and closing paren as -1
    // (comparisons always come out with -1, so we simply substitute),
    // and then run a prefix sum across the block, similar to what we
    // did above with prefix-xor. This gives the status in any byte.
    // E.g., say that we have
    //
    //   c a l c  (  ( 1 + 2  ) *  ( 3 + 4  )
    //   0 0 0 0 +1 +1 0 0 0 -1 0 +1 0 0 0 -1
    //
    // After the prefix sum, we have
    //   c a l c  (  ( 1 + 2  ) *  ( 3 + 4  )
    //   0 0 0 0  1  2 2 2 2  1 1  2 2 2 2  1
    //
    // plus the count from the previous block (if any). In other words,
    // if we were to see a } at the end of this block, we would know
    // that we wouldn't be in balance and we'd have to error out.
    //
    // NOTE: If we have an overflow (more than 128 levels of parens)
    // or underflow (right-paren without matching underflow) at any point
    // in the string, the top bit of the relevant byte in parens will be set.
    // The technically correct thing to do in CSS parsing with underflow
    // is to ignore the underflow, and we could have simulated that by
    // using saturating adds and starting at -128 (or +127). However,
    // it would require slightly more constants to load and possibly be
    // slightly more icky for NEON, so we don't bother. In any case,
    // we simply reuse the top bit as overflow detection (we OR it into
    // the general “we need to stop” mask below), even though
    // we could probably have gone all the way to accept 255 with an
    // extra comparison.
    const __m128i opening_paren = _mm_cmpeq_epi8(x, _mm_set1_epi8('('));
    const __m128i closing_paren = _mm_cmpeq_epi8(x, _mm_set1_epi8(')'));
    __m128i parens =
        _mm_sub_epi8(_mm_add_epi8(prev_parens, closing_paren), opening_paren);
    parens = _mm_add_epi8(parens, _mm_slli_si128(parens, 1));
    parens = _mm_add_epi8(parens, _mm_slli_si128(parens, 2));
    parens = _mm_add_epi8(parens, _mm_slli_si128(parens, 4));
    parens = _mm_add_epi8(parens, _mm_slli_si128(parens, 8));

    // We don't support the full block stack, so [ must go (we don't
    // need to worry about ], as it will just be ignored when there are
    // no ] to match against).
    //
    // Also, we cannot do lazy parsing of nested rules, so opening braces
    // need to abort. If we had SSSE3, we could have probably done this
    // (and comments) more efficiently with PSHUFB, but we don't.
    //
    // [ and { happen to be 0x20 apart in ASCII, so we can do with one
    // less comparison.
    const __m128i opening_block =
        _mm_cmpeq_epi8(x | _mm_set1_epi8(0x20), _mm_set1_epi8('{'));

    // Right braces mean (successful) EOF.
    const __m128i eq_rightbrace = _mm_cmpeq_epi8(x, _mm_set1_epi8('}'));

    // We generally combine all of the end-parsing situations together
    // and figure out afterwards what the first one was, to determine
    // the return value.
    const __m128i must_end = eq_backslash | mixed_quote | opening_block |
                             comment_start | eq_rightbrace | parens;
    if (_mm_movemask_epi8(must_end) != 0) {
      unsigned idx = __builtin_ctz(_mm_movemask_epi8(must_end));
      ptr += idx;
      if (*ptr == '}') {
        // Check that we have balanced parens at the end point
        // (the paren counter is zero).
        uint16_t mask =
            _mm_movemask_epi8(_mm_cmpeq_epi8(parens, _mm_setzero_si128()));
        if (((mask >> idx) & 1) == 0) {
          return 0;
        } else {
          return ptr - begin;
        }
      } else {
        // Ended due to something that was not successful EOB.
        return 0;
      }
    }

    ptr += 16;
    prev_quoted = _mm_srli_si128(quoted, 15);
    prev_parens = _mm_srli_si128(parens, 15);
  }
  return 0;  // Premature EOF; we cannot SIMD any further.
}

#elif defined(__ARM_NEON__)

static inline uint8x16_t LoadAndCollapseHighBytes(const UChar* ptr) {
  uint8x16_t x1;
  uint8x16_t x2;
  memcpy(&x1, ptr, sizeof(x1));
  memcpy(&x2, ptr + 8, sizeof(x2));
  return vcombine_u64(vqmovn_u16(x1), vqmovn_u16(x2));
}
static inline uint8x16_t LoadAndCollapseHighBytes(const LChar* ptr) {
  uint8x16_t ret;
  memcpy(&ret, ptr, sizeof(ret));
  return ret;
}

// The NEON implementation follows basically the same pattern as the
// SSE2 implementation; comments will be added only where they differ
// substantially.
//
// For A64, we _do_ have access to the PMULL instruction (the NEON
// equivalent of PCLMULQDQ), but it's supposedly slow, so we use
// the same XOR-shift cascade.
template <class CharType>
ALWAYS_INLINE static size_t FindLengthOfDeclarationList(const CharType* begin,
                                                        const CharType* end) {
  // Since NEON doesn't have a natural way of moving the last element
  // to the first slot (shift right by 15 _bytes_), but _does_ have
  // fairly cheap broadcasting (unlike SSE2 without SSSE3), we use
  // a slightly different convention: The prev_* elements hold the
  // last element in _all_ lanes, and is then applied _after_
  // the prefix sum/prefix XOR. This would create havoc with
  // saturating operations, but works well when they commute.
  uint8x16_t prev_quoted = vdupq_n_u8(0);
  uint8x16_t prev_parens = vdupq_n_u8(0);

  const CharType* ptr = begin;
  while (ptr + 17 <= end) {
    uint8x16_t x = LoadAndCollapseHighBytes(ptr);
    const uint8x16_t next_x = LoadAndCollapseHighBytes(ptr + 1);
    const uint8x16_t eq_backslash = x == '\\';
    const uint8x16_t eq_double_quote = x == '"';
    const uint8x16_t eq_single_quote = x == '\'';
    uint8x16_t quoted = x & (eq_double_quote | eq_single_quote);

    // NEON doesn't have 128-bit bytewise shifts like SSE2 have.
    // We thus need to do the algorithm separately in 64-bit halves,
    // then to a separate duplication step to transfer the result
    // from the highest element of the bottom half to all elements
    // of the top half. (The alternative would be to use TBL
    // instructions to simulate the shifts, but they can be slow
    // on mobile CPUs.)
    quoted ^= vshlq_n_u64(vreinterpretq_u64_u8(quoted), 8);
    quoted ^= vshlq_n_u64(vreinterpretq_u64_u8(quoted), 16);
    quoted ^= vshlq_n_u64(vreinterpretq_u64_u8(quoted), 32);
    quoted ^=
        vcombine_u64(vdup_n_u64(0), vdup_lane_u8(vget_low_u64(quoted), 7));
    quoted ^= prev_quoted;
    const uint8x16_t mixed_quote = quoted == ('\'' ^ '"');

    x &= ~(quoted > vdupq_n_u8(0));

    const uint8x16_t comment_start = (x == '/') & (next_x == '*');
    const uint8x16_t opening_paren = x == '(';
    const uint8x16_t closing_paren = x == ')';
    uint8x16_t parens = closing_paren - opening_paren;
    parens +=
        vreinterpretq_u8_u64(vshlq_n_u64(vreinterpretq_u64_u8(parens), 8));
    parens +=
        vreinterpretq_u8_u64(vshlq_n_u64(vreinterpretq_u64_u8(parens), 16));
    parens +=
        vreinterpretq_u8_u64(vshlq_n_u64(vreinterpretq_u64_u8(parens), 32));
    parens += vreinterpretq_u8_u64(
        vcombine_u64(vdup_n_u64(0), vdup_lane_u8(vget_low_u64(parens), 7)));
    parens += prev_parens;

    // The VSHRN trick below doesn't guarantee the use of the top bit
    // the same way PMOVMSKB does, so we can't just use the parens value
    // directly for overflow check. We could compare directly against 255
    // here, but it's nice to have exactly the same behavior as on Intel,
    // so we do a signed shift to just replicate the top bit into the entire
    // byte. (Supposedly, this also has one cycle better throughput on
    // some CPUs.)
    const uint8x16_t parens_overflow = vshrq_n_s8(parens, 7);

    const uint8x16_t opening_block = (x | vdupq_n_u8(0x20)) == '{';
    const uint8x16_t eq_rightbrace = x == '}';
    uint8x16_t must_end = eq_backslash | mixed_quote | opening_block |
                          comment_start | eq_rightbrace | parens_overflow;

    // https://community.arm.com/arm-community-blogs/b/infrastructure-solutions-blog/posts/porting-x86-vector-bitmask-optimizations-to-arm-neon
    uint64_t must_end_narrowed =
        vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(must_end, 4)), 0);
    if (must_end_narrowed != 0) {
      unsigned idx = __builtin_ctzll(must_end_narrowed) >> 2;
      ptr += idx;
      if (*ptr == '}') {
        // Since we don't have cheap PMOVMSKB, and this is not on
        // the most critical path, we just chicken out here and let
        // the compiler spill the value to the stack, where we can
        // do a normal indexing.
        if (parens[idx] != 0) {
          return 0;
        } else {
          return ptr - begin;
        }
      } else {
        return 0;
      }
    }

    // As mentioned above, broadcast instead of shifting.
    ptr += 16;
    prev_quoted = vdupq_lane_u8(vget_high_u64(quoted), 7);
    prev_parens = vdupq_lane_u8(vget_high_u64(parens), 7);
  }
  return 0;
}

#else

// If we have neither SSE2 nor NEON, we simply return 0 immediately.
// We will then never use lazy parsing.
template <class CharType>
ALWAYS_INLINE static size_t FindLengthOfDeclarationList(const CharType* begin,
                                                        const CharType* end) {
  return 0;
}

#endif

ALWAYS_INLINE static size_t FindLengthOfDeclarationList(StringView str) {
  if (str.Is8Bit()) {
    return FindLengthOfDeclarationList(str.Characters8(),
                                       str.Characters8() + str.length());
  } else {
    return FindLengthOfDeclarationList(str.Characters16(),
                                       str.Characters16() + str.length());
  }
}

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_PARSER_FIND_LENGTH_OF_DECLARATION_LIST_INL_H_
