// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/paint_manager_simd.h"

// For highway dynamic dispatch - it must know the path to the current file.
#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "pdf/paint_manager_simd.cc"

#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "third_party/highway/src/hwy/foreach_target.h"
#include "third_party/highway/src/hwy/highway.h"

namespace chrome_pdf {
namespace {
void NonPremulBlendScalar(base::span<const uint8_t> src,
                          base::span<uint8_t> dest);
void PremulBlendScalar(base::span<const uint8_t> src, base::span<uint8_t> dest);
}  // namespace
}  // namespace chrome_pdf

HWY_BEFORE_NAMESPACE();
namespace chrome_pdf {
namespace HWY_NAMESPACE {

void NonPremulBlendImpl(const uint8_t* src_ptr,
                        uint8_t* dest_ptr,
                        size_t n_pixels) {
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<uint8_t> d8;
  const size_t n_lanes = hn::Lanes(d8);

  if (n_pixels < n_lanes) {
    // SAFETY: In the interest of making the scalar implementations memory safe,
    // pass spans to them rather than pointers. The bounds checking of these
    // spans is done in PaintManager::DoPaint().
    UNSAFE_BUFFERS(NonPremulBlendScalar({src_ptr, n_pixels * 4},
                                        {dest_ptr, n_pixels * 4}));
    return;
  }

  const hn::ScalableTag<uint16_t> d16;
  const hn::Half<decltype(d8)> dh;
  using V8 = hn::VFromD<decltype(d8)>;
  using V16 = hn::VFromD<decltype(d16)>;

  size_t col_i = 0;
  // Clamping at `offset_max` allows the vector implementation to handle the
  // remaining pixels (n_pixels % n_lanes) without a scalar implementation by
  // overlapping the last vector with the previous one.
  const size_t offset_max = n_pixels - n_lanes;

  auto blend = [&](V8 src, V16 a_lo, V16 a_hi) {
    auto blend_half = [&](V16 src_half, V16 a_half) {
      const V16 q =
          hn::Add(hn::Mul(hn::Sub(hn::Set(d16, 255), src_half), a_half),
                  hn::Set(d16, 127));
      return hn::Sub(hn::Set(d16, 255),
                     hn::ShiftRight<8>(hn::Add(q, hn::ShiftRight<8>(q))));
    };
    return hn::Combine(
        d8, hn::DemoteTo(dh, blend_half(hn::PromoteUpperTo(d16, src), a_hi)),
        hn::DemoteTo(dh, blend_half(hn::PromoteLowerTo(d16, src), a_lo)));
  };

  auto process_vector = [&](size_t offset) {
    V8 b;
    V8 g;
    V8 r;
    V8 a;
    // SAFETY: see comment in PaintManager::DoPaint() for safety information. It
    // would not be advantageous to rewrite this using spans because of
    // highway's interface, which takes pointers.
    UNSAFE_BUFFERS({
      hn::LoadInterleaved4(d8, src_ptr + (offset * 4), b, g, r, a);
      V16 a_lo = hn::PromoteLowerTo(d16, a);
      V16 a_hi = hn::PromoteUpperTo(d16, a);
      hn::StoreInterleaved4(blend(b, a_lo, a_hi), blend(g, a_lo, a_hi),
                            blend(r, a_lo, a_hi), hn::Set(d8, 255), d8,
                            dest_ptr + (offset * 4));
    });
  };

  for (; col_i <= offset_max; col_i += n_lanes) {
    process_vector(col_i);
  }
  if (col_i < n_pixels) {
    process_vector(offset_max);
  }
}

void PremulBlendImpl(const uint8_t* src_ptr,
                     uint8_t* dest_ptr,
                     size_t n_pixels) {
  namespace hn = hwy::HWY_NAMESPACE;
  const hn::ScalableTag<uint8_t> d8;
  const size_t n_lanes = hn::Lanes(d8);

  if (n_pixels < n_lanes) {
    // SAFETY: In the interest of making the scalar implementations memory safe,
    // pass spans to them rather than pointers. The bounds checking of these
    // spans is done in PaintManager::DoPaint().
    UNSAFE_BUFFERS(
        PremulBlendScalar({src_ptr, n_pixels * 4}, {dest_ptr, n_pixels * 4}));
    return;
  }

  using V8 = hn::VFromD<decltype(d8)>;

  size_t col_i = 0;
  // See above comment for semantics.
  const size_t offset_max = n_pixels - n_lanes;

  auto process_vector = [&](size_t offset) {
    V8 b;
    V8 g;
    V8 r;
    V8 a;
    // SAFETY: see comment in PaintManager::DoPaint() for safety information.
    UNSAFE_BUFFERS({
      hn::LoadInterleaved4(d8, src_ptr + (offset * 4), b, g, r, a);
      auto inv_a = hn::Sub(hn::Set(d8, 255), a);
      hn::StoreInterleaved4(hn::Add(b, inv_a), hn::Add(g, inv_a),
                            hn::Add(r, inv_a), hn::Set(d8, 255), d8,
                            dest_ptr + (offset * 4));
    });
  };

  // `col_i` is incremented by `n_lanes` rather than `n_lanes/4` because each
  // LoadInterleaved4 loads 4 packed byte vectors at once, each of which
  // contains one quarter of the pixel.
  for (; col_i <= offset_max; col_i += n_lanes) {
    process_vector(col_i);
  }
  if (col_i < n_pixels) {
    process_vector(offset_max);
  }
}

}  // namespace HWY_NAMESPACE
}  // namespace chrome_pdf
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace chrome_pdf {
namespace {

void NonPremulBlendScalar(base::span<const uint8_t> src,
                          base::span<uint8_t> dest) {
  CHECK_EQ(src.size(), dest.size());
  CHECK_EQ(src.size() % 4, 0u);
  for (size_t i = 0; i < src.size(); i += 4) {
    uint8_t alpha = src[i + 3];

    auto blend = [alpha](uint8_t src) {
      uint16_t q = (255 - src) * alpha + 127;
      return static_cast<uint8_t>(255 - ((q + (q >> 8)) >> 8));
    };

    dest[i] = blend(src[i]);
    dest[i + 1] = blend(src[i + 1]);
    dest[i + 2] = blend(src[i + 2]);
    // Resulting buffer is opaque.
    dest[i + 3] = 255;
  }
}

void PremulBlendScalar(base::span<const uint8_t> src,
                       base::span<uint8_t> dest) {
  CHECK_EQ(src.size(), dest.size());
  CHECK_EQ(src.size() % 4, 0u);
  for (size_t i = 0; i < src.size(); i += 4) {
    uint8_t inv_alpha = 255 - src[i + 3];
    dest[i] = src[i] + inv_alpha;
    dest[i + 1] = src[i + 1] + inv_alpha;
    dest[i + 2] = src[i + 2] + inv_alpha;
    // Resulting buffer is opaque.
    dest[i + 3] = 255;
  }
}

}  // namespace

HWY_EXPORT(NonPremulBlendImpl);
HWY_EXPORT(PremulBlendImpl);

void NonPremulBlend(const uint8_t* src_ptr,
                    uint8_t* dest_ptr,
                    size_t n_pixels) {
  // SAFETY: Funnily enough, the way that highway implements dynamic dispatch
  // uses pointer arithmetic. This is out of our control.
  UNSAFE_BUFFERS(
      HWY_DYNAMIC_DISPATCH(NonPremulBlendImpl)(src_ptr, dest_ptr, n_pixels););
}
void PremulBlend(const uint8_t* src_ptr, uint8_t* dest_ptr, size_t n_pixels) {
  // SAFETY: Funnily enough, the way that highway implements dynamic dispatch
  // uses pointer arithmetic. This is out of our control.
  UNSAFE_BUFFERS(
      HWY_DYNAMIC_DISPATCH(PremulBlendImpl)(src_ptr, dest_ptr, n_pixels););
}

}  // namespace chrome_pdf
#endif  // HWY_ONCE
