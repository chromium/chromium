// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>
#include <random>
#include <string>

#include "testing/libfuzzer/fuzzers/libyuv_scale_fuzzer.h"
#include "third_party/libyuv/include/libyuv.h"

static void FillBufferWithRandomData(uint8_t* dst,
                                     size_t len,
                                     std::minstd_rand0 rng) {
  size_t i;
  for (i = 0; i + 3 < len; i += 4) {
    *reinterpret_cast<uint32_t*>(dst) = rng();
    dst += 4;
  }
  for (; i < len; ++i) {
    *dst++ = rng();
  }
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

  uint8_t* src_y = reinterpret_cast<uint8_t*>(malloc(src_y_plane_size));
  uint8_t* src_u = reinterpret_cast<uint8_t*>(malloc(src_uv_plane_size));
  uint8_t* src_v = reinterpret_cast<uint8_t*>(malloc(src_uv_plane_size));

  uint16_t* p_src_y_16 =
      reinterpret_cast<uint16_t*>(malloc(src_y_plane_size * 2));
  uint16_t* p_src_u_16 =
      reinterpret_cast<uint16_t*>(malloc(src_uv_plane_size * 2));
  uint16_t* p_src_v_16 =
      reinterpret_cast<uint16_t*>(malloc(src_uv_plane_size * 2));

  std::seed_seq seed(seed_str.begin(), seed_str.end());
  std::minstd_rand0 rng(seed);

  // TODO - consider taking directly as parameters when this code
  // is being run using FuzzTest, though it would probably require
  // complex domains to ensure they're the right size.
  FillBufferWithRandomData(src_y, src_y_plane_size, rng);
  FillBufferWithRandomData(src_u, src_uv_plane_size, rng);
  FillBufferWithRandomData(src_v, src_uv_plane_size, rng);

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

  uint8_t* dst_y_c = reinterpret_cast<uint8_t*>(malloc(dst_y_plane_size));
  uint8_t* dst_u_c = reinterpret_cast<uint8_t*>(malloc(dst_uv_plane_size));
  uint8_t* dst_v_c = reinterpret_cast<uint8_t*>(malloc(dst_uv_plane_size));

  uint16_t* p_dst_y_16 =
      reinterpret_cast<uint16_t*>(malloc(dst_y_plane_size * 2));
  uint16_t* p_dst_u_16 =
      reinterpret_cast<uint16_t*>(malloc(dst_uv_plane_size * 2));
  uint16_t* p_dst_v_16 =
      reinterpret_cast<uint16_t*>(malloc(dst_uv_plane_size * 2));

  if (is420) {
    I420Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
              src_width, src_height, dst_y_c, dst_stride_y, dst_u_c,
              dst_stride_uv, dst_v_c, dst_stride_uv, dst_width, dst_height,
              static_cast<libyuv::FilterMode>(filter_num));

    I420Scale_16(p_src_y_16, src_stride_y, p_src_u_16, src_stride_uv,
                 p_src_v_16, src_stride_uv, src_width, src_height, p_dst_y_16,
                 dst_stride_y, p_dst_u_16, dst_stride_uv, p_dst_v_16,
                 dst_stride_uv, dst_width, dst_height,
                 static_cast<libyuv::FilterMode>(filter_num));
  } else {
    I444Scale(src_y, src_stride_y, src_u, src_stride_uv, src_v, src_stride_uv,
              src_width, src_height, dst_y_c, dst_stride_y, dst_u_c,
              dst_stride_uv, dst_v_c, dst_stride_uv, dst_width, dst_height,
              static_cast<libyuv::FilterMode>(filter_num));

    I444Scale_16(p_src_y_16, src_stride_y, p_src_u_16, src_stride_uv,
                 p_src_v_16, src_stride_uv, src_width, src_height, p_dst_y_16,
                 dst_stride_y, p_dst_u_16, dst_stride_uv, p_dst_v_16,
                 dst_stride_uv, dst_width, dst_height,
                 static_cast<libyuv::FilterMode>(filter_num));
  }

  free(src_y);
  free(src_u);
  free(src_v);

  free(p_src_y_16);
  free(p_src_u_16);
  free(p_src_v_16);

  free(dst_y_c);
  free(dst_u_c);
  free(dst_v_c);

  free(p_dst_y_16);
  free(p_dst_u_16);
  free(p_dst_v_16);
}
