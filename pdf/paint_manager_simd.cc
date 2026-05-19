// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager_simd.h"

// For highway dynamic dispatch - it must know the path to the current file.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "pdf/paint_manager_simd.cc"

#include "base/compiler_specific.h"
#include "third_party/highway/src/hwy/foreach_target.h"
#include "third_party/highway/src/hwy/highway.h"

HWY_BEFORE_NAMESPACE();
namespace chrome_pdf {
namespace HWY_NAMESPACE {

void NonPremulBlendImpl(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels) {
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<uint8_t> d8;
  const hn::ScalableTag<uint16_t> d16;
  const hn::Half<decltype(d8)> dh;
  using V8 = hn::VFromD<decltype(d8)>;
  using V16 = hn::VFromD<decltype(d16)>;

  size_t col_i = 0;

  // SAFETY: see comment in PaintManager::DoPaint() for safety information. It
  // would not be advantageous to rewrite this using spans because of highway's
  // interface, which takes pointers.
  UNSAFE_BUFFERS({
    for (; col_i + hn::Lanes(d8) <= n_pixels; col_i += hn::Lanes(d8)) {
      V8 b;
      V8 g;
      V8 r;
      V8 a;
      hn::LoadInterleaved4(d8, src_ptr + (col_i * 4), b, g, r, a);
      V16 a_lo = hn::PromoteLowerTo(d16, a);
      V16 a_hi = hn::PromoteUpperTo(d16, a);

      auto blend = [&](V8 src) {
        auto blend_half = [&](V16 src_half, V16 a_half) {
          const V16 q =
              hn::Add(hn::Mul(hn::Sub(hn::Set(d16, 255), src_half), a_half),
                      hn::Set(d16, 127));
          return hn::Sub(hn::Set(d16, 255),
                         hn::ShiftRight<8>(hn::Add(q, hn::ShiftRight<8>(q))));
        };
        return hn::Combine(
            d8,
            hn::DemoteTo(dh, blend_half(hn::PromoteUpperTo(d16, src), a_hi)),
            hn::DemoteTo(dh, blend_half(hn::PromoteLowerTo(d16, src), a_lo)));
      };

      hn::StoreInterleaved4(blend(b), blend(g), blend(r), hn::Set(d8, 255), d8,
                            dest_ptr + (col_i * 4));
    }

    // Scalar tail loop to finish the last `width % hn::Lanes(d8)` pixels.
    for (; col_i < n_pixels; ++col_i) {
      uint8_t* src_bgra_ptr = (src_ptr + (col_i * 4));
      uint8_t* dest_bgra_ptr = (dest_ptr + (col_i * 4));
      uint8_t alpha = src_bgra_ptr[3];

      auto blend = [alpha](uint8_t src) {
        uint16_t q = (255 - src) * alpha + 127;
        return static_cast<uint8_t>(255 - ((q + (q >> 8)) >> 8));
      };

      dest_bgra_ptr[0] = blend(src_bgra_ptr[0]);
      dest_bgra_ptr[1] = blend(src_bgra_ptr[1]);
      dest_bgra_ptr[2] = blend(src_bgra_ptr[2]);
      // Resulting buffer is opaque.
      dest_bgra_ptr[3] = 255;
    }
  });
}

void PremulBlendImpl(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels) {
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<uint8_t> d8;

  size_t col_i = 0;
  // Since we're interleaving into four d8 vectors, col_i += Lanes(d8),
  // meaning likely bumping 16 (if 128bit vectors, which SSE3 includes, and
  // is what Chrome is targeting on Windows) 4x8=32bit pixels per iteration.
  //
  // SAFETY: see comment in PaintManager::DoPaint() for safety information.
  UNSAFE_BUFFERS({
    for (; col_i + hn::Lanes(d8) <= n_pixels; col_i += hn::Lanes(d8)) {
      hn::VFromD<decltype(d8)> b;
      hn::VFromD<decltype(d8)> g;
      hn::VFromD<decltype(d8)> r;
      hn::VFromD<decltype(d8)> a;
      hn::LoadInterleaved4(d8, src_ptr + (col_i * 4), b, g, r, a);
      auto inv_a = hn::Sub(hn::Set(d8, 255), a);
      hn::StoreInterleaved4(hn::Add(b, inv_a), hn::Add(g, inv_a),
                            hn::Add(r, inv_a), hn::Set(d8, 255), d8,
                            dest_ptr + (col_i * 4));
    }

    // Scalar tail loop to finish the last `width % hn::Lanes(d8)` pixels.
    for (; col_i < n_pixels; ++col_i) {
      uint8_t* src_bgra_ptr = src_ptr + (col_i * 4);
      uint8_t* dest_bgra_ptr = dest_ptr + (col_i * 4);
      uint8_t inv_alpha = 255 - src_bgra_ptr[3];
      dest_bgra_ptr[0] = src_bgra_ptr[0] + inv_alpha;
      dest_bgra_ptr[1] = src_bgra_ptr[1] + inv_alpha;
      dest_bgra_ptr[2] = src_bgra_ptr[2] + inv_alpha;
      // Resulting buffer is opaque.
      dest_bgra_ptr[3] = 255;
    }
  });
}

}  // namespace HWY_NAMESPACE
}  // namespace chrome_pdf
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace chrome_pdf {

HWY_EXPORT(NonPremulBlendImpl);
HWY_EXPORT(PremulBlendImpl);

void NonPremulBlend(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels) {
  // SAFETY: Funnily enough, the way that highway implements dynamic dispatch
  // uses pointer arithmetic. This is out of our control.
  UNSAFE_BUFFERS(
      HWY_DYNAMIC_DISPATCH(NonPremulBlendImpl)(src_ptr, dest_ptr, n_pixels););
}
void PremulBlend(uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels) {
  // SAFETY: Funnily enough, the way that highway implements dynamic dispatch
  // uses pointer arithmetic. This is out of our control.
  UNSAFE_BUFFERS(
      HWY_DYNAMIC_DISPATCH(PremulBlendImpl)(src_ptr, dest_ptr, n_pixels););
}

}  // namespace chrome_pdf
#endif  // HWY_ONCE
