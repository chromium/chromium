// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "media/parsers/h265_parser.h"
#include "media/parsers/h265_poc.h"

namespace media {

H265POC::H265POC() {
  Reset();
}

H265POC::~H265POC() = default;

void H265POC::Reset() {
  ref_pic_order_cnt_msb_ = 0;
  ref_pic_order_cnt_lsb_ = 0;
  first_picture_ = true;
}

int32_t H265POC::ComputePicOrderCnt(const H265SPS* sps,
                                    const H265PPS* pps,
                                    const H265SliceHeader& slice_hdr) {
  int32_t pic_order_cnt = 0;
  int32_t max_pic_order_cnt_lsb =
      1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);
  int32_t pic_order_cnt_msb;
  int32_t no_rasl_output_flag;
  // Calculate POC for current picture.
  if (slice_hdr.irap_pic) {
    // 8.1.3
    no_rasl_output_flag = (slice_hdr.nal_unit_type >= H265NALU::BLA_W_LP &&
                           slice_hdr.nal_unit_type <= H265NALU::IDR_N_LP) ||
                          first_picture_;
  } else {
    no_rasl_output_flag = false;
  }

  if (!slice_hdr.irap_pic || !no_rasl_output_flag) {
    int32_t prev_pic_order_cnt_lsb = ref_pic_order_cnt_lsb_;
    int32_t prev_pic_order_cnt_msb = ref_pic_order_cnt_msb_;

    if ((slice_hdr.slice_pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
        ((prev_pic_order_cnt_lsb - slice_hdr.slice_pic_order_cnt_lsb) >=
         (max_pic_order_cnt_lsb / 2))) {
      pic_order_cnt_msb = prev_pic_order_cnt_msb + max_pic_order_cnt_lsb;
    } else if ((slice_hdr.slice_pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
               ((slice_hdr.slice_pic_order_cnt_lsb - prev_pic_order_cnt_lsb) >
                (max_pic_order_cnt_lsb / 2))) {
      pic_order_cnt_msb = prev_pic_order_cnt_msb - max_pic_order_cnt_lsb;
    } else {
      pic_order_cnt_msb = prev_pic_order_cnt_msb;
    }
  } else {
    pic_order_cnt_msb = 0;
  }

  // 8.3.1 Decoding process for picture order count.
  if (!slice_hdr.temporal_id &&
      (slice_hdr.nal_unit_type < H265NALU::RADL_N ||
       slice_hdr.nal_unit_type > H265NALU::RSV_VCL_N14)) {
    ref_pic_order_cnt_lsb_ = slice_hdr.slice_pic_order_cnt_lsb;
    ref_pic_order_cnt_msb_ = pic_order_cnt_msb;
  }

  pic_order_cnt = pic_order_cnt_msb + slice_hdr.slice_pic_order_cnt_lsb;
  first_picture_ = false;

  return pic_order_cnt;
}

}  // namespace media
