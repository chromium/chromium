// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_H265_DPB_H_
#define MEDIA_GPU_VAAPI_TEST_H265_DPB_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/parsers/h265_parser.h"
#include "ui/gfx/geometry/rect.h"

namespace media::vaapi_test {

#define LOG_PIC_INFO(func, pic, fetch_policy)                                \
  do {                                                                       \
    VLOG(3) << func << "," << pic->pic_order_cnt_val_ << ","                 \
            << pic->nal_unit_type_ << ","                                    \
            << pic->surface->GetMD5Sum(fetch_policy) << ","                  \
            << pic->pic_output_flag_ << ","                                  \
            << pic->no_output_of_prior_pics_flag_ << "," << pic->outputted_; \
  } while (0)

// A picture (a frame or a field) in the H.265 spec sense.
// See spec at http://www.itu.int/rec/T-REC-H.265
class H265Picture : public base::RefCountedThreadSafe<H265Picture> {
 public:
  using Vector = std::vector<scoped_refptr<H265Picture>>;

  explicit H265Picture(scoped_refptr<SharedVASurface> target_surface);

  H265Picture(const H265Picture&) = delete;
  H265Picture& operator=(const H265Picture&) = delete;

  enum ReferenceType {
    kUnused = 0,
    kShortTermCurrBefore = 1,
    kShortTermCurrAfter = 2,
    kShortTermFoll = 3,
    kLongTermCurr = 4,
    kLongTermFoll = 5,
  };

  static std::string GetReferenceName(ReferenceType ref) {
    if (ref == kUnused)
      return "Unused";
    else if (ref == kLongTermCurr || ref == kLongTermFoll)
      return "LongTerm";
    else
      return "ShortTerm";
  }

  bool IsLongTermRef() const {
    return ref_ == kLongTermCurr || ref_ == kLongTermFoll;
  }
  bool IsShortTermRef() const {
    return ref_ == kShortTermCurrBefore || ref_ == kShortTermCurrAfter ||
           ref_ == kShortTermFoll;
  }
  bool IsUnused() const { return ref_ == kUnused; }

  const gfx::Rect visible_rect() const { return visible_rect_; }
  void set_visible_rect(const gfx::Rect& rect) { visible_rect_ = rect; }

  // Values calculated per H.265 specification or taken from slice header.
  // See spec for more details on each (some names have been converted from
  // CamelCase in spec to Chromium-style names).
  int nal_unit_type_;
  bool no_rasl_output_flag_{false};
  bool no_output_of_prior_pics_flag_{false};
  bool pic_output_flag_{false};
  bool valid_for_prev_tid0_pic_{false};
  int slice_pic_order_cnt_lsb_{0};
  int pic_order_cnt_msb_{0};
  int pic_order_cnt_val_{0};

  // Our own state variables.
  bool irap_pic_;
  bool first_picture_;
  bool processed_{false};

  ReferenceType ref_{kUnused};

  bool outputted_{false};

  scoped_refptr<SharedVASurface> surface;

 protected:
  friend class base::RefCountedThreadSafe<H265Picture>;
  virtual ~H265Picture();

 private:
  gfx::Rect visible_rect_;
};

// DPB - Decoded Picture Buffer.
// Stores decoded pictures that will be used for future display and/or
// reference.
class H265DPB {
 public:
  H265DPB();

  H265DPB(const H265DPB&) = delete;
  H265DPB& operator=(const H265DPB&) = delete;

  ~H265DPB();

  void set_max_num_pics(size_t max_num_pics);
  size_t max_num_pics() const { return max_num_pics_; }

  // Removes all entries from the DPB.
  void Clear();

  // Stores |pic| in the DPB. If |used_for_long_term| is true it'll be marked as
  // used for long term reference, otherwise it'll be marked as used for short
  // term reference.
  void StorePicture(scoped_refptr<H265Picture> pic,
                    H265Picture::ReferenceType ref);

  // Mark all pictures in DPB as unused for reference.
  void MarkAllUnusedForReference();

  // Removes all pictures from the DPB that do not have |pic_output_flag_| set
  // and are marked Unused for reference.
  void DeleteUnused();

  // Returns the number of pictures in the DPB that are marked for reference.
  int GetReferencePicCount();

  // Returns a picture in the DPB which has a POC equal to |poc| and marks it
  // with |ref| reference type. If not found, returns nullptr.
  scoped_refptr<H265Picture> GetPicByPocAndMark(int poc,
                                                H265Picture::ReferenceType ref);

  // Returns a picture in the DPB which has a POC bitmasked by |mask| which
  // equals |poc| and marks it with |ref| reference type. If not found, returns
  // nullptr. If |mask| is zero, then no bitmasking is done.
  scoped_refptr<H265Picture>
  GetPicByPocMaskedAndMark(int poc, int mask, H265Picture::ReferenceType ref);

  // Appends to |out| all of the pictures in the DPB that are flagged for output
  // but have not be outputted yet.
  void AppendPendingOutputPics(H265Picture::Vector* out);

  // Appends to |out| all of the pictures in the DPB that are not marked as
  // unused for reference.
  void AppendReferencePics(H265Picture::Vector* out);

  size_t size() const { return pics_.size(); }
  bool IsFull() const { return pics_.size() >= max_num_pics_; }

  // Sets the fetch_policy, which allow the dpb to get the MD5
  // value of the picture.
  void SetFetchPolicy(SharedVASurface::FetchPolicy fetch_policy) {
    fetch_policy_ = fetch_policy;
  }

 private:
  H265Picture::Vector pics_;
  size_t max_num_pics_{0};
  SharedVASurface::FetchPolicy fetch_policy_;
};

}  // namespace media::vaapi_test

#endif  // MEDIA_GPU_VAAPI_TEST_H265_DPB_H_
