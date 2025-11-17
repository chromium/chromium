// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CPU_X86_WEBGL_IMAGE_CONVERSION_SSE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CPU_X86_WEBGL_IMAGE_CONVERSION_SSE_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(ARCH_CPU_X86_FAMILY)
#include <emmintrin.h>

namespace blink {

namespace simd {

ALWAYS_INLINE void UnpackOneRowOfRGBA4444LittleToRGBA8(
    const uint16_t*& source,
    uint8_t*& destination,
    unsigned& pixels_per_row) {
  __m128i immediate0x0f = _mm_set1_epi16(0x0F);
  unsigned pixels_per_row_trunc = (pixels_per_row / 8) * 8;
  for (unsigned i = 0; i < pixels_per_row_trunc; i += 8) {
    __m128i packed_value = _mm_loadu_si128((__m128i*)source);
    __m128i r = _mm_srli_epi16(packed_value, 12);
    __m128i g = _mm_and_si128(_mm_srli_epi16(packed_value, 8), immediate0x0f);
    __m128i b = _mm_and_si128(_mm_srli_epi16(packed_value, 4), immediate0x0f);
    __m128i a = _mm_and_si128(packed_value, immediate0x0f);
    __m128i component_r = _mm_or_si128(_mm_slli_epi16(r, 4), r);
    __m128i component_g = _mm_or_si128(_mm_slli_epi16(g, 4), g);
    __m128i component_b = _mm_or_si128(_mm_slli_epi16(b, 4), b);
    __m128i component_a = _mm_or_si128(_mm_slli_epi16(a, 4), a);

    __m128i component_rg =
        _mm_or_si128(_mm_slli_epi16(component_g, 8), component_r);
    __m128i component_ba =
        _mm_or_si128(_mm_slli_epi16(component_a, 8), component_b);
    __m128i component_rgba1 = _mm_unpackhi_epi16(component_rg, component_ba);
    __m128i component_rgba2 = _mm_unpacklo_epi16(component_rg, component_ba);

    _mm_storeu_si128((__m128i*)destination, component_rgba2);
    _mm_storeu_si128((__m128i*)(UNSAFE_TODO(destination + 16)),
                     component_rgba1);

    UNSAFE_TODO(source += 8);
    UNSAFE_TODO(destination += 32);
  }
  pixels_per_row -= pixels_per_row_trunc;
}

ALWAYS_INLINE void UnpackOneRowOfRGBA5551LittleToRGBA8(
    const uint16_t*& source,
    uint8_t*& destination,
    unsigned& pixels_per_row) {
  __m128i immediate0x1f = _mm_set1_epi16(0x1F);
  __m128i immediate0x7 = _mm_set1_epi16(0x7);
  __m128i immediate0x1 = _mm_set1_epi16(0x1);
  unsigned pixels_per_row_trunc = (pixels_per_row / 8) * 8;
  for (unsigned i = 0; i < pixels_per_row_trunc; i += 8) {
    __m128i packed_value = _mm_loadu_si128((__m128i*)source);
    __m128i r = _mm_srli_epi16(packed_value, 11);
    __m128i g = _mm_and_si128(_mm_srli_epi16(packed_value, 6), immediate0x1f);
    __m128i b = _mm_and_si128(_mm_srli_epi16(packed_value, 1), immediate0x1f);
    __m128i component_r =
        _mm_or_si128(_mm_slli_epi16(r, 3), _mm_and_si128(r, immediate0x7));
    __m128i component_g =
        _mm_or_si128(_mm_slli_epi16(g, 3), _mm_and_si128(g, immediate0x7));
    __m128i component_b =
        _mm_or_si128(_mm_slli_epi16(b, 3), _mm_and_si128(b, immediate0x7));
    __m128i component_a = _mm_cmpeq_epi16(
        _mm_and_si128(packed_value, immediate0x1), immediate0x1);

    __m128i component_rg =
        _mm_or_si128(_mm_slli_epi16(component_g, 8), component_r);
    __m128i component_ba =
        _mm_or_si128(_mm_slli_epi16(component_a, 8), component_b);
    __m128i component_rgba1 = _mm_unpackhi_epi16(component_rg, component_ba);
    __m128i component_rgba2 = _mm_unpacklo_epi16(component_rg, component_ba);

    _mm_storeu_si128((__m128i*)destination, component_rgba2);
    _mm_storeu_si128((__m128i*)(UNSAFE_TODO(destination + 16)),
                     component_rgba1);

    UNSAFE_TODO(source += 8);
    UNSAFE_TODO(destination += 32);
  }
  pixels_per_row -= pixels_per_row_trunc;
}

ALWAYS_INLINE void PackOneRowOfRGBA8LittleToRA8(const uint8_t*& source,
                                                uint8_t*& destination,
                                                unsigned& pixels_per_row) {
  float tmp[4];
  unsigned pixels_per_row_trunc = (pixels_per_row / 4) * 4;
  const __m128 max_pixel_value = _mm_set1_ps(255.0);
  for (unsigned i = 0; i < pixels_per_row_trunc; i += 4) {
    __m128 scale_factor =
        _mm_set_ps(UNSAFE_TODO(source[15]) ? UNSAFE_TODO(source[15]) : 255.0,
                   UNSAFE_TODO(source[11]) ? UNSAFE_TODO(source[11]) : 255.0,
                   UNSAFE_TODO(source[7]) ? UNSAFE_TODO(source[7]) : 255.0,
                   UNSAFE_TODO(source[3]) ? UNSAFE_TODO(source[3]) : 255.0);
    __m128 source_r =
        _mm_set_ps(UNSAFE_TODO(source[12]), UNSAFE_TODO(source[8]),
                   UNSAFE_TODO(source[4]), source[0]);

    source_r = _mm_mul_ps(source_r, _mm_div_ps(max_pixel_value, scale_factor));

    _mm_storeu_ps(tmp, source_r);

    destination[0] = static_cast<uint8_t>(tmp[0]);
    UNSAFE_TODO(destination[1]) = static_cast<uint8_t>(UNSAFE_TODO(source[3]));
    UNSAFE_TODO(destination[2]) = static_cast<uint8_t>(tmp[1]);
    UNSAFE_TODO(destination[3]) = static_cast<uint8_t>(UNSAFE_TODO(source[7]));
    UNSAFE_TODO(destination[4]) = static_cast<uint8_t>(tmp[2]);
    UNSAFE_TODO(destination[5]) = static_cast<uint8_t>(UNSAFE_TODO(source[11]));
    UNSAFE_TODO(destination[6]) = static_cast<uint8_t>(tmp[3]);
    UNSAFE_TODO(destination[7]) = static_cast<uint8_t>(UNSAFE_TODO(source[15]));

    UNSAFE_TODO(source += 16);
    UNSAFE_TODO(destination += 8);
  }
  pixels_per_row -= pixels_per_row_trunc;
}

ALWAYS_INLINE void UnpackOneRowOfBGRA8LittleToRGBA8(const uint32_t*& source,
                                                    uint32_t*& destination,
                                                    unsigned& pixels_per_row) {
  __m128i bgra, rgba;
  __m128i br_mask = _mm_set1_epi32(0x00ff00ff);
  __m128i ga_mask = _mm_set1_epi32(0xff00ff00);
  unsigned pixels_per_row_trunc = (pixels_per_row / 4) * 4;

  for (unsigned i = 0; i < pixels_per_row_trunc; i += 4) {
    bgra = _mm_loadu_si128((const __m128i*)(source));
    rgba = _mm_shufflehi_epi16(_mm_shufflelo_epi16(bgra, 0xB1), 0xB1);

    rgba = _mm_or_si128(_mm_and_si128(rgba, br_mask),
                        _mm_and_si128(bgra, ga_mask));
    _mm_storeu_si128((__m128i*)(destination), rgba);

    UNSAFE_TODO(source += 4);
    UNSAFE_TODO(destination += 4);
  }

  pixels_per_row -= pixels_per_row_trunc;
}

ALWAYS_INLINE void PackOneRowOfRGBA8LittleToR8(const uint8_t*& source,
                                               uint8_t*& destination,
                                               unsigned& pixels_per_row) {
  float tmp[4];
  unsigned pixels_per_row_trunc = (pixels_per_row / 4) * 4;

  for (unsigned i = 0; i < pixels_per_row_trunc; i += 4) {
    __m128 scale =
        _mm_set_ps(UNSAFE_TODO(source[15]) ? UNSAFE_TODO(source[15]) : 255,
                   UNSAFE_TODO(source[11]) ? UNSAFE_TODO(source[11]) : 255,
                   UNSAFE_TODO(source[7]) ? UNSAFE_TODO(source[7]) : 255,
                   UNSAFE_TODO(source[3]) ? UNSAFE_TODO(source[3]) : 255);

    __m128 source_r =
        _mm_set_ps(UNSAFE_TODO(source[12]), UNSAFE_TODO(source[8]),
                   UNSAFE_TODO(source[4]), source[0]);
    source_r = _mm_mul_ps(source_r, _mm_div_ps(_mm_set1_ps(255), scale));

    _mm_storeu_ps(tmp, source_r);
    destination[0] = static_cast<uint8_t>(tmp[0]);
    UNSAFE_TODO(destination[1]) = static_cast<uint8_t>(tmp[1]);
    UNSAFE_TODO(destination[2]) = static_cast<uint8_t>(tmp[2]);
    UNSAFE_TODO(destination[3]) = static_cast<uint8_t>(tmp[3]);

    UNSAFE_TODO(source += 16);
    UNSAFE_TODO(destination += 4);
  }

  pixels_per_row -= pixels_per_row_trunc;
}

// This function always handles the full row.
ALWAYS_INLINE void PackOneRowOfRGBA8LittleToRGBA8(const uint8_t*& source,
                                                  uint8_t*& destination,
                                                  unsigned& pixels_per_row) {
  float tmp[4];
  float scale;

  for (unsigned i = 0; i < pixels_per_row; i++) {
    scale = UNSAFE_TODO(source[3]) ? UNSAFE_TODO(source[3]) : 255;
    __m128 source_r = _mm_set_ps(0, UNSAFE_TODO(source[2]),
                                 UNSAFE_TODO(source[1]), source[0]);

    source_r =
        _mm_mul_ps(source_r, _mm_div_ps(_mm_set1_ps(255), _mm_set1_ps(scale)));

    _mm_storeu_ps(tmp, source_r);
    destination[0] = static_cast<uint8_t>(tmp[0]);
    UNSAFE_TODO(destination[1]) = static_cast<uint8_t>(tmp[1]);
    UNSAFE_TODO(destination[2]) = static_cast<uint8_t>(tmp[2]);
    UNSAFE_TODO(destination[3]) = UNSAFE_TODO(source[3]);

    UNSAFE_TODO(source += 4);
    UNSAFE_TODO(destination += 4);
  }
  pixels_per_row = 0;
}

}  // namespace simd
}  // namespace blink

#endif  // ARCH_CPU_X86_FAMILY

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CPU_X86_WEBGL_IMAGE_CONVERSION_SSE_H_
