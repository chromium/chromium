// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/test/h265_dpb.h"

#include <algorithm>

#include "base/logging.h"
#include "media/gpu/macros.h"
#include "media/gpu/vaapi/test/macros.h"

namespace media::vaapi_test {

H265Picture::H265Picture(scoped_refptr<SharedVASurface> target_surface)
    : surface(target_surface) {}
H265Picture::~H265Picture() = default;

H265DPB::H265DPB() = default;
H265DPB::~H265DPB() = default;

void H265DPB::set_max_num_pics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, static_cast<size_t>(kMaxDpbSize));
  max_num_pics_ = max_num_pics;
  if (pics_.size() > max_num_pics_)
    pics_.resize(max_num_pics_);
}

void H265DPB::Clear() {
  for (auto it = pics_.begin(); it != pics_.end(); it++) {
    LOG_PIC_INFO(__func__, it->get(), fetch_policy_);
  }
  pics_.clear();
}

void H265DPB::StorePicture(scoped_refptr<H265Picture> pic,
                           H265Picture::ReferenceType ref) {
  DCHECK_LT(pics_.size(), max_num_pics_);
  LOG_PIC_INFO(__func__, pic, fetch_policy_);
  DVLOG(3) << "Adding PicNum: " << pic->pic_order_cnt_val_
           << " ref: " << static_cast<int>(ref);
  pic->ref_ = ref;
  pics_.push_back(std::move(pic));
}

void H265DPB::MarkAllUnusedForReference() {
  for (const auto& pic : pics_)
    pic->ref_ = H265Picture::kUnused;
}

void H265DPB::DeleteUnused() {
  for (auto it = pics_.begin(); it != pics_.end();) {
    auto& pic = *it;
    if ((!pic->pic_output_flag_ || pic->outputted_) &&
        (pic->ref_ == H265Picture::kUnused)) {
      LOG_PIC_INFO(__func__, pic, fetch_policy_);
      std::swap(pic, *(pics_.end() - 1));
      pics_.pop_back();
    } else {
      it++;
    }
  }
}

int H265DPB::GetReferencePicCount() {
  int count = 0;
  for (const auto& pic : pics_) {
    if (pic->ref_ != H265Picture::kUnused)
      count++;
  }
  return count;
}

scoped_refptr<H265Picture> H265DPB::GetPicByPocAndMark(
    int poc,
    H265Picture::ReferenceType ref) {
  return GetPicByPocMaskedAndMark(poc, 0, ref);
}

scoped_refptr<H265Picture> H265DPB::GetPicByPocMaskedAndMark(
    int poc,
    int mask,
    H265Picture::ReferenceType ref) {
  for (const auto& pic : pics_) {
    if ((mask && (pic->pic_order_cnt_val_ & mask) == poc) ||
        (!mask && pic->pic_order_cnt_val_ == poc)) {
      pic->ref_ = ref;
      return pic;
    }
  }

  LOG(ERROR) << "Missing " << H265Picture::GetReferenceName(ref)
             << " ref pic num: " << poc;
  return nullptr;
}

void H265DPB::AppendPendingOutputPics(H265Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->pic_output_flag_ && !pic->outputted_)
      out->push_back(pic);
  }
}

void H265DPB::AppendReferencePics(H265Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->ref_ != H265Picture::kUnused)
      out->push_back(pic);
  }
}

}  // namespace media::vaapi_test
