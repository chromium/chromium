// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_H264_POC_H_
#define MEDIA_VIDEO_H264_POC_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

struct H264SPS;
struct H264SliceHeader;

class MEDIA_EXPORT H264POC {
 public:
  H264POC();

  H264POC(const H264POC&) = delete;
  H264POC& operator=(const H264POC&) = delete;

  ~H264POC();

  // Returns the picture order count for a slice.
  absl::optional<int32_t> ComputePicOrderCnt(const H264SPS* sps,
                                             const H264SliceHeader& slice_hdr);

  // As specified, the POC of a frame with MMCO5 changes (to zero) after
  // decoding. We instead return 0 immediately, and flag that this has occurred
  // by returning true here until ComputePicOrderCnt() is called again.
  //
  // Frames with MMCO5 do not reorder relative to frames earlier in decode
  // order, but may reorder relative to frames later in decode order (just like
  // IDRs).
  bool IsPendingMMCO5() { return pending_mmco5_; }

  // Reset computation state. It's best (although not strictly required) to call
  // this after a seek.
  void Reset();

 private:
  int32_t ref_pic_order_cnt_msb_;
  int32_t ref_pic_order_cnt_lsb_;
  int32_t prev_frame_num_;
  int32_t prev_frame_num_offset_;
  bool pending_mmco5_;
};

}  // namespace media

#endif  // MEDIA_VIDEO_H264_POC_H_
