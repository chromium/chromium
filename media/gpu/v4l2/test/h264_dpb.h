// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MEDIA_GPU_V4L2_TEST_H264_DPB_H_
#define MEDIA_GPU_V4L2_TEST_H264_DPB_H_

#include <map>
#include <set>

#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"

namespace media {
namespace v4l2_test {

// H264SliceMetadata contains metadata about an H.264 picture
// slice including how the slice is reordered. It is used as
// elements in the decoded picture buffer class H264DPB.
struct H264SliceMetadata {
  H264SliceMetadata();
  H264SliceMetadata(const H264SliceMetadata&);

  H264SliceHeader slice_header;
  int bottom_field_order_cnt = 0;
  int frame_num = -1;
  int frame_num_offset = 0;
  int frame_num_wrap = 0;
  uint64_t ref_ts_nsec = 0;  // Reference Timestamp in nanoseconds.
  int pic_num = -1;
  int pic_order_cnt = 0;
  int pic_order_cnt_lsb = 0;
  int pic_order_cnt_msb = 0;
  int top_field_order_cnt = 0;
  bool outputted = false;  // Whether this slice has been outputted.
  bool ref = false;        // Whether this slice is a reference element.
  H264DecRefPicMarking ref_pic_marking[H264SliceHeader::kRefListSize];
  bool long_term_reference_flag = false;
  int long_term_frame_idx = 0;
  // Picture number for picture which is marked as long term as defined in
  // section 7.4.3.1.
  int long_term_pic_num = 0;
  // The CAPTURE queue index this slice is queued in.
  int capture_queue_buffer_id = -1;
  gfx::Rect visible_rect_;
};

// H264DPB is a class representing a Decoded Picture Buffer (DPB).
// The DPB is a map of H264 picture slice metadata objects that
// describe the pictures used in the H.264 decoding process.
class H264DPB : public std::map<uint64_t, H264SliceMetadata> {
 public:
  H264DPB() = default;
  ~H264DPB() = default;

  H264DPB(const H264DPB&) = delete;
  H264DPB& operator=(const H264DPB&) = delete;

  // Returns number of Reference |H264SliceMetadata| elements
  // in the DPB.
  int CountRefPics();
  // Deletes input |H264SliceMetadata| object from the DPB.
  void Delete(const H264SliceMetadata& pic);
  // Deletes any |H264SliceMetadata| object from DPB that is considered
  // to be unused by the decoder.
  // An |H264SliceMetadata| is unused if it has been outputted and is not a
  // reference picture.
  void DeleteUnused();
  // Removes the reference picture marking from the lowest frame
  // number |H264SliceMetadata| object in the DPB. This is used for
  // implementing a sliding window DPB replacement algorithm.
  void UnmarkLowestFrameNumWrapShortRefPic();
  // Returns a vector of |H264SliceMetadata| objects that have not been output
  // by the H264 Decoder.
  std::vector<H264SliceMetadata*> GetNotOutputtedPicsAppending();
  // Updates every |H264SliceMetadata| object in the DPB to indicate that they
  // are not reference elements.
  void MarkAllUnusedRef();
  // Updates each |H264SliceMetadata| object in DPB's frame num wrap
  // based on the max frame num.
  void UpdatePicNums(const int curr_frame_num, const int max_frame_num);
  // Removes the reference picture marking from the |H264SliceMetadata| object
  // in the DPB which has the same picture number as pic_num and is not a long
  // term picture.
  void UnmarkPicByPicNum(const int pic_num);
  // Removes the long term reference marking from a |H264SliceMetadata| object
  // that has a long term picture number equal to pic_num.
  void UnmarkLongTerm(const int pic_num);
  // Returns a short term reference picture from the |H264SliceMetadata|
  // objects that has a picture number equal to pic_num.
  H264SliceMetadata* GetShortRefPicByPicNum(const int pic_num);
  // Returns a long term reference picture from the |H264SliceMetadata|
  // objects that has a long term frame index equal to frame_index.
  H264SliceMetadata* GetLongRefPicByFrameIdx(const int frame_index);
  // Removes long term reference picture marking from |H264SliceMetadata|
  // objects that have a long term frame index greater than index.
  void UnmarkLongTermPicsGreaterThanFrameIndex(const int index);
  // Returns a set of indices on the CAPTURE queue which are
  // currently in use and cannot be refreshed.
  std::set<int> GetHeldCaptureIds() const;
  // Maximum number of elements in the DPB. This is utilized by the
  // decoder for updating elements in the DPB.
  size_t max_dpb_size_ = -1;
};

}  // namespace v4l2_test
}  // namespace media

#endif  // MEDIA_GPU_V4L2_TEST_H264_DPB_H_
