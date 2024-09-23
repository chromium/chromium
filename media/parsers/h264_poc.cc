// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/parsers/h264_poc.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "media/parsers/h264_parser.h"

namespace media {

namespace {

// Check if a slice includes memory management control operation 5, which
// results in some |pic_order_cnt| state being cleared.
bool HasMMCO5(const media::H264SliceHeader& slice_hdr) {
  // Require that the frame actually has memory management control operations.
  if (slice_hdr.nal_ref_idc == 0 ||
      slice_hdr.idr_pic_flag ||
      !slice_hdr.adaptive_ref_pic_marking_mode_flag) {
    return false;
  }

  for (size_t i = 0; i < std::size(slice_hdr.ref_pic_marking); i++) {
    int32_t op = slice_hdr.ref_pic_marking[i].memory_mgmnt_control_operation;
    if (op == 5)
      return true;

    // Stop at the end of the list.
    if (op == 0)
      return false;
  }

  // Should not get here, the list is always zero terminated.
  return false;
}

}  // namespace

H264POC::H264POC() {
  Reset();
}

H264POC::~H264POC() = default;

void H264POC::Reset() {
  // It shouldn't be necessary to reset these values, but doing so will improve
  // reproducibility for buggy streams.
  ref_pic_order_cnt_msb_ = 0;
  ref_pic_order_cnt_lsb_ = 0;
  prev_frame_num_ = 0;
  prev_frame_num_offset_ = 0;
  pending_mmco5_ = false;
}

std::optional<int32_t> H264POC::ComputePicOrderCnt(
    const H264SPS* sps,
    const H264SliceHeader& slice_hdr) {
  if (slice_hdr.field_pic_flag) {
    DLOG(ERROR) << "Interlaced frames are not supported";
    return std::nullopt;
  }

  int32_t pic_order_cnt = 0;
  bool mmco5 = HasMMCO5(slice_hdr);
  int32_t max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
  int32_t max_pic_order_cnt_lsb =
      1 << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

  // Based on T-REC-H.264 8.2.1, "Decoding process for picture order
  // count", available from http://www.itu.int/rec/T-REC-H.264.
  //
  // Reorganized slightly from spec pseudocode to handle MMCO5 when storing
  // state instead of when loading it.
  //
  // Note: Gaps in frame numbers are ignored. They do not affect POC
  // computation.
  switch (sps->pic_order_cnt_type) {
    case 0: {
      int32_t prev_pic_order_cnt_msb = ref_pic_order_cnt_msb_;
      int32_t prev_pic_order_cnt_lsb = ref_pic_order_cnt_lsb_;

      // For an IDR picture, clear the state.
      if (slice_hdr.idr_pic_flag) {
        prev_pic_order_cnt_msb = 0;
        prev_pic_order_cnt_lsb = 0;
      }

      // 8-3. Derive |pic_order_cnt_msb|, accounting for wrapping which is
      //      detected when |pic_order_cnt_lsb| increases or decreases by at
      //      least half of its maximum.
      int32_t pic_order_cnt_msb;
      if ((slice_hdr.pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
          (prev_pic_order_cnt_lsb - slice_hdr.pic_order_cnt_lsb >=
           max_pic_order_cnt_lsb / 2)) {
        pic_order_cnt_msb = prev_pic_order_cnt_msb + max_pic_order_cnt_lsb;
      } else if ((slice_hdr.pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
          (slice_hdr.pic_order_cnt_lsb - prev_pic_order_cnt_lsb >
           max_pic_order_cnt_lsb / 2)) {
        pic_order_cnt_msb = prev_pic_order_cnt_msb - max_pic_order_cnt_lsb;
      } else {
        pic_order_cnt_msb = prev_pic_order_cnt_msb;
      }

      // 8-4, 8-5. Derive |top_field_order_count| and |bottom_field_order_cnt|
      //           (assuming no interlacing).
      int32_t top_foc = pic_order_cnt_msb + slice_hdr.pic_order_cnt_lsb;
      int32_t bottom_foc = top_foc + slice_hdr.delta_pic_order_cnt_bottom;

      // Compute POC.
      //
      // MMCO5, like IDR, starts a new reordering group. The POC is specified to
      // change to 0 after decoding; we change it immediately and set the
      // |pending_mmco5_| flag.
      if (mmco5)
        pic_order_cnt = 0;
      else
        pic_order_cnt = std::min(top_foc, bottom_foc);

      // Store state.
      pending_mmco5_ = mmco5;
      prev_frame_num_ = slice_hdr.frame_num;
      if (slice_hdr.nal_ref_idc != 0) {
        if (mmco5) {
          ref_pic_order_cnt_msb_ = 0;
          ref_pic_order_cnt_lsb_ = top_foc;
        } else {
          ref_pic_order_cnt_msb_ = pic_order_cnt_msb;
          ref_pic_order_cnt_lsb_ = slice_hdr.pic_order_cnt_lsb;
        }
      }

      break;
    }

    case 1: {
      // 8-6. Derive |frame_num_offset|.
      int32_t frame_num_offset;
      if (slice_hdr.idr_pic_flag)
        frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr.frame_num)
        frame_num_offset = prev_frame_num_offset_ + max_frame_num;
      else
        frame_num_offset = prev_frame_num_offset_;

      // 8-7. Derive |abs_frame_num|.
      int32_t abs_frame_num;
      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
        abs_frame_num = frame_num_offset + slice_hdr.frame_num;
      else
        abs_frame_num = 0;

      if (slice_hdr.nal_ref_idc == 0 && abs_frame_num > 0)
        abs_frame_num--;

      // 8-9. Derive |expected_pic_order_cnt| (the |pic_order_cnt| indicated
      //      by the cycle described in the SPS).
      int32_t expected_pic_order_cnt = 0;
      if (abs_frame_num > 0) {
        // 8-8. Derive pic_order_cnt_cycle_cnt and
        //      frame_num_in_pic_order_cnt_cycle.
        // Moved inside 8-9 to avoid division when this check is not done.
        if (sps->num_ref_frames_in_pic_order_cnt_cycle == 0) {
          DLOG(ERROR) << "Invalid num_ref_frames_in_pic_order_cnt_cycle";
          return std::nullopt;
        }

        // H264Parser checks that num_ref_frames_in_pic_order_cnt_cycle < 255.
        int32_t pic_order_cnt_cycle_cnt =
            (abs_frame_num - 1) / sps->num_ref_frames_in_pic_order_cnt_cycle;
        int32_t frame_num_in_pic_order_cnt_cycle =
            (abs_frame_num - 1) % sps->num_ref_frames_in_pic_order_cnt_cycle;

        // 8-9 continued.
        expected_pic_order_cnt = pic_order_cnt_cycle_cnt *
                                 sps->expected_delta_per_pic_order_cnt_cycle;
        for (int32_t i = 0; i <= frame_num_in_pic_order_cnt_cycle; i++)
          expected_pic_order_cnt += sps->offset_for_ref_frame[i];
      }
      if (slice_hdr.nal_ref_idc == 0)
        expected_pic_order_cnt += sps->offset_for_non_ref_pic;

      // 8-10. Derive |top_field_order_cnt| and |bottom_field_order_cnt|
      //       (assuming no interlacing).
      int32_t top_foc = expected_pic_order_cnt + slice_hdr.delta_pic_order_cnt0;
      int32_t bottom_foc = top_foc + sps->offset_for_top_to_bottom_field +
                           slice_hdr.delta_pic_order_cnt1;

      // Compute POC. MMCO5 handling is the same as |pic_order_cnt_type| == 0.
      if (mmco5)
        pic_order_cnt = 0;
      else
        pic_order_cnt = std::min(top_foc, bottom_foc);

      // Store state.
      pending_mmco5_ = mmco5;
      prev_frame_num_ = slice_hdr.frame_num;
      if (mmco5)
        prev_frame_num_offset_ = 0;
      else
        prev_frame_num_offset_ = frame_num_offset;

      break;
    }

    case 2: {
      // 8-11. Derive |frame_num_offset|.
      int32_t frame_num_offset;
      if (slice_hdr.idr_pic_flag)
        frame_num_offset = 0;
      else if (prev_frame_num_ > slice_hdr.frame_num)
        frame_num_offset = prev_frame_num_offset_ + max_frame_num;
      else
        frame_num_offset = prev_frame_num_offset_;

      // 8-12, 8-13. Derive |temp_pic_order_count| (it's always the
      // |pic_order_cnt|, regardless of interlacing).
      int32_t temp_pic_order_count;
      if (slice_hdr.idr_pic_flag)
        temp_pic_order_count = 0;
      else if (slice_hdr.nal_ref_idc == 0)
        temp_pic_order_count = 2 * (frame_num_offset + slice_hdr.frame_num) - 1;
      else
        temp_pic_order_count = 2 * (frame_num_offset + slice_hdr.frame_num);

      // Compute POC. MMCO5 handling is the same as |pic_order_cnt_type| == 0.
      if (mmco5)
        pic_order_cnt = 0;
      else
        pic_order_cnt = temp_pic_order_count;

      // Store state.
      pending_mmco5_ = mmco5;
      prev_frame_num_ = slice_hdr.frame_num;
      if (mmco5)
        prev_frame_num_offset_ = 0;
      else
        prev_frame_num_offset_ = frame_num_offset;

      break;
    }

    default:
      DLOG(ERROR) << "Invalid pic_order_cnt_type: " << sps->pic_order_cnt_type;
      return std::nullopt;
  }

  return pic_order_cnt;
}

}  // namespace media
