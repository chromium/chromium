// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_PARSERS_H265_POC_H_
#define MEDIA_PARSERS_H265_POC_H_

#include <stdint.h>

#include <optional>

#include "media/base/media_export.h"

namespace media {

struct H265SPS;
struct H265PPS;
struct H265SliceHeader;

class MEDIA_EXPORT H265POC {
 public:
  H265POC();

  H265POC(const H265POC&) = delete;
  H265POC& operator=(const H265POC&) = delete;

  ~H265POC();

  // Returns the picture order count for a slice.
  int32_t ComputePicOrderCnt(const H265SPS* sps,
                             const H265PPS* pps,
                             const H265SliceHeader& slice_hdr);
  // Reset computation state.
  void Reset();

 private:
  int32_t ref_pic_order_cnt_msb_;
  int32_t ref_pic_order_cnt_lsb_;
  // Indicate whether or not this is the first picture processed
  bool first_picture_;
};

}  // namespace media

#endif  // MEDIA_PARSERS_H265_POC_H_
