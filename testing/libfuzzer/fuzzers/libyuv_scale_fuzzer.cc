// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/libfuzzer/fuzzers/libyuv_scale_fuzzer.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <random>
#include <string>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "third_party/libyuv/include/libyuv.h"

static void FillBufferWithRandomData(base::span<uint8_t> dst,
                                     std::minstd_rand0& rng) {
  // Maximize fuzzer throughput by writing in 32-bit chunks to minimize RNG
  // calls.
  const size_t aligned_bytes = (dst.size() / 4) * 4;
  auto [aligned_dst, trailing_dst] = dst.split_at(aligned_bytes);
  for (uint32_t& val : base::subtle::reinterpret_span<uint32_t>(aligned_dst)) {
    val = rng();
  }
  uint32_t trailing_rand = rng();
  trailing_dst.copy_from(base::as_bytes(base::span_from_ref(trailing_rand))
                             .first(trailing_dst.size()));
}

void Scale(bool is420,
           int src_width,
           int src_height,
           int dst_width,
           int dst_height,
           int filter_num,
           std::string seed_str) {
  int src_width_uv, src_height_uv;
  if (is420) {
    src_width_uv = (std::abs(src_width) + 1) >> 1;
    src_height_uv = (std::abs(src_height) + 1) >> 1;
  } else {
    src_width_uv = (std::abs(src_width));
    src_height_uv = (std::abs(src_height));
  }

  size_t src_y_plane_size = (std::abs(src_width)) * (std::abs(src_height));
  size_t src_uv_plane_size = (src_width_uv) * (src_height_uv);

  int src_stride_y = std::abs(src_width);
  int src_stride_uv = src_width_uv;

  auto src_y = base::HeapArray<uint8_t>::Uninit(src_y_plane_size);
  auto src_u = base::HeapArray<uint8_t>::Uninit(src_uv_plane_size);
  auto src_v = base::HeapArray<uint8_t>::Uninit(src_uv_plane_size);

  auto p_src_y_16 = base::HeapArray<uint16_t>::Uninit(src_y_plane_size);
  auto p_src_u_16 = base::HeapArray<uint16_t>::Uninit(src_uv_plane_size);
  auto p_src_v_16 = base::HeapArray<uint16_t>::Uninit(src_uv_plane_size);

  std::seed_seq seed(seed_str.begin(), seed_str.end());
  std::minstd_rand0 rng(seed);

  // TODO - consider taking directly as parameters when this code
  // is being run using FuzzTest, though it would probably require
  // complex domains to ensure they're the right size.
  FillBufferWithRandomData(src_y, rng);
  FillBufferWithRandomData(src_u, rng);
  FillBufferWithRandomData(src_v, rng);

  for (size_t i = 0; i < src_y_plane_size; ++i) {
    p_src_y_16[i] = src_y[i];
  }
  for (size_t i = 0; i < src_uv_plane_size; ++i) {
    p_src_u_16[i] = src_u[i];
    p_src_v_16[i] = src_v[i];
  }

  int dst_width_uv, dst_height_uv;
  if (is420) {
    dst_width_uv = (dst_width + 1) >> 1;
    dst_height_uv = (dst_height + 1) >> 1;
  } else {
    dst_width_uv = dst_width;
    dst_height_uv = dst_height;
  }

  size_t dst_y_plane_size = (dst_width) * (dst_height);
  size_t dst_uv_plane_size = (dst_width_uv) * (dst_height_uv);

  int dst_stride_y = dst_width;
  int dst_stride_uv = dst_width_uv;

  auto dst_y_c = base::HeapArray<uint8_t>::Uninit(dst_y_plane_size);
  auto dst_u_c = base::HeapArray<uint8_t>::Uninit(dst_uv_plane_size);
  auto dst_v_c = base::HeapArray<uint8_t>::Uninit(dst_uv_plane_size);

  auto p_dst_y_16 = base::HeapArray<uint16_t>::Uninit(dst_y_plane_size);
  auto p_dst_u_16 = base::HeapArray<uint16_t>::Uninit(dst_uv_plane_size);
  auto p_dst_v_16 = base::HeapArray<uint16_t>::Uninit(dst_uv_plane_size);

  if (is420) {
    I420Scale(src_y.data(), src_stride_y, src_u.data(), src_stride_uv,
              src_v.data(), src_stride_uv, src_width, src_height,
              dst_y_c.data(), dst_stride_y, dst_u_c.data(), dst_stride_uv,
              dst_v_c.data(), dst_stride_uv, dst_width, dst_height,
              static_cast<libyuv::FilterMode>(filter_num));

    I420Scale_16(p_src_y_16.data(), src_stride_y, p_src_u_16.data(),
                 src_stride_uv, p_src_v_16.data(), src_stride_uv, src_width,
                 src_height, p_dst_y_16.data(), dst_stride_y, p_dst_u_16.data(),
                 dst_stride_uv, p_dst_v_16.data(), dst_stride_uv, dst_width,
                 dst_height, static_cast<libyuv::FilterMode>(filter_num));
  } else {
    I444Scale(src_y.data(), src_stride_y, src_u.data(), src_stride_uv,
              src_v.data(), src_stride_uv, src_width, src_height,
              dst_y_c.data(), dst_stride_y, dst_u_c.data(), dst_stride_uv,
              dst_v_c.data(), dst_stride_uv, dst_width, dst_height,
              static_cast<libyuv::FilterMode>(filter_num));

    I444Scale_16(p_src_y_16.data(), src_stride_y, p_src_u_16.data(),
                 src_stride_uv, p_src_v_16.data(), src_stride_uv, src_width,
                 src_height, p_dst_y_16.data(), dst_stride_y, p_dst_u_16.data(),
                 dst_stride_uv, p_dst_v_16.data(), dst_stride_uv, dst_width,
                 dst_height, static_cast<libyuv::FilterMode>(filter_num));
  }
}
