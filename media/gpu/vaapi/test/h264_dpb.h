// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H264_DPB_H_
#define MEDIA_GPU_VAAPI_TEST_H264_DPB_H_

#include <stddef.h>

#include <vector>

#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/parsers/h264_parser.h"
#include "ui/gfx/geometry/rect.h"

namespace media::vaapi_test {

// Abstract reference of a decoded picture. Helpful for keeping track of not
// only the decoded surface, but metadata necessary for memory management. Note
// that this object owns both the surface of the decoded buffer and the
// compressed slice data buffers memory.
class H264Picture : public base::RefCountedThreadSafe<H264Picture> {
 public:
  using Vector = std::vector<scoped_refptr<H264Picture>>;

  enum Field {
    FIELD_NONE,
    FIELD_TOP,
    FIELD_BOTTOM,
  };

  // Values calculated per H.264 specification or taken from slice header.
  // See spec for more details on each (some names have been converted from
  // CamelCase in spec to Chromium-style names).
  int pic_order_cnt_type = 0;
  int top_field_order_cnt = 0;
  int bottom_field_order_cnt = 0;
  int pic_order_cnt = 0;  // note that this can wrap around
  int pic_order_cnt_msb = 0;
  int pic_order_cnt_lsb = 0;
  int delta_pic_order_cnt_bottom = 0;
  int delta_pic_order_cnt0 = 0;
  int delta_pic_order_cnt1 = 0;

  int pic_num = 0;
  int long_term_pic_num = 0;
  int frame_num = 0;  // from slice header, not picture order count
  int frame_num_offset = 0;
  int frame_num_wrap = 0;
  int long_term_frame_idx = 0;

  // Keep the underlying data for the VAAPI buffers around as long as the
  // surface is around.
  std::vector<std::unique_ptr<uint8_t[]>> slice_data_buffers;

  H264SliceHeader::Type type;
  int nal_ref_idc = 0;
  bool idr = false;    // IDR picture?
  int idr_pic_id = 0;  // Valid only if idr == true.
  bool ref = false;    // reference picture?
  int ref_pic_list_modification_flag_l0 = 0;
  int abs_diff_pic_num_minus1 = 0;
  bool long_term = false;  // long term reference picture?
  bool outputted = false;
  // Does memory management op 5 needs to be executed after this
  // picture has finished decoding?
  bool mem_mgmt_5 = false;

  // Created by the decoding process for gaps in frame_num.
  // Not for decode or output.
  bool nonexisting = false;

  Field field;

  // Values from slice_hdr to be used during reference marking and
  // memory management after finishing this picture.
  bool long_term_reference_flag = false;
  bool adaptive_ref_pic_marking_mode_flag = false;
  H264DecRefPicMarking ref_pic_marking[H264SliceHeader::kRefListSize];

  // Position in DPB (i.e. index in DPB).
  int dpb_position = 0;

  // Visible rectangle. Note that this may not match the size of the coded
  // picture, since H.264 pads them to be 16 pixel aligned in all dimensions.
  // This is the source of truth for the output image size however.
  gfx::Rect visible_rect;

  scoped_refptr<SharedVASurface> surface;

  explicit H264Picture(scoped_refptr<SharedVASurface> target_surface);

 protected:
  friend class base::RefCountedThreadSafe<H264Picture>;

  virtual ~H264Picture();
};

// DPB - Decoded Picture Buffer.
// Stores decoded pictures that will be used for future display
// and/or reference.
class H264DPB {
 public:
  H264DPB();
  ~H264DPB();

  H264DPB(const H264DPB&) = delete;
  H264DPB& operator=(const H264DPB&) = delete;

  void set_max_num_pics(size_t max_num_pics);
  size_t max_num_pics() const { return max_num_pics_; }

  // Remove unused (not reference and already outputted) pictures from DPB
  // and free it.
  void DeleteUnused();

  // Remove a picture by its pic_order_cnt and free it.
  void DeleteByPOC(int poc);

  // Clear DPB.
  void Clear();

  // Store picture in DPB. DPB takes ownership of its resources.
  void StorePic(scoped_refptr<H264Picture> pic);

  // Return the number of reference pictures in DPB.
  int CountRefPics();

  // Mark all pictures in DPB as unused for reference.
  void MarkAllUnusedForRef();

  // Return a short-term reference picture by its pic_num.
  scoped_refptr<H264Picture> GetShortRefPicByPicNum(int pic_num);

  // Return a long-term reference picture by its long_term_pic_num.
  scoped_refptr<H264Picture> GetLongRefPicByLongTermPicNum(int pic_num);

  // Return a long-term reference picture by its long term reference index.
  scoped_refptr<H264Picture> GetLongRefPicByLongTermIdx(int idx);

  // Return the short reference picture with lowest frame_num. Used for sliding
  // window memory management.
  scoped_refptr<H264Picture> GetLowestFrameNumWrapShortRefPic();

  // Append all pictures that have not been outputted yet to the passed |out|
  // vector, sorted by lowest pic_order_cnt (in output order).
  void GetNotOutputtedPicsAppending(H264Picture::Vector* out);

  // Append all short term reference pictures to the passed |out| vector.
  void GetShortTermRefPicsAppending(H264Picture::Vector* out);

  // Append all long term reference pictures to the passed |out| vector.
  void GetLongTermRefPicsAppending(H264Picture::Vector* out);

  // Iterators for direct access to DPB contents.
  // Will be invalidated after any of Remove* calls.
  H264Picture::Vector::iterator begin() { return pics_.begin(); }
  H264Picture::Vector::iterator end() { return pics_.end(); }
  H264Picture::Vector::const_iterator begin() const { return pics_.begin(); }
  H264Picture::Vector::const_iterator end() const { return pics_.end(); }
  H264Picture::Vector::const_reverse_iterator rbegin() const {
    return pics_.rbegin();
  }
  H264Picture::Vector::const_reverse_iterator rend() const {
    return pics_.rend();
  }

  size_t size() const { return pics_.size(); }
  bool IsFull() const { return pics_.size() >= max_num_pics_; }

  // Per H264 spec, increase to 32 if interlaced video is supported.
  enum {
    kDPBMaxSize = 16,
  };

 private:
  void UpdatePicPositions();

  H264Picture::Vector pics_;
  size_t max_num_pics_ = 0;
};

}  // namespace media::vaapi_test

#endif  // MEDIA_GPU_VAAPI_TEST_H264_DPB_H_
