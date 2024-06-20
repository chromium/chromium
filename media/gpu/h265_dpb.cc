// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/h265_dpb.h"

#include <algorithm>

#include "base/logging.h"
#include "media/parsers/h265_parser.h"

namespace media {

H265Picture::H265Picture() = default;
H265Picture::~H265Picture() = default;

H265DPB::H265DPB() = default;
H265DPB::~H265DPB() = default;

V4L2H265Picture* H265Picture::AsV4L2H265Picture() {
  return nullptr;
}

VaapiH265Picture* H265Picture::AsVaapiH265Picture() {
  return nullptr;
}

D3D11H265Picture* H265Picture::AsD3D11H265Picture() {
  return nullptr;
}

void H265DPB::set_max_num_pics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, static_cast<size_t>(kMaxDpbSize));
  max_num_pics_ = max_num_pics;
  if (pics_.size() > max_num_pics_)
    pics_.resize(max_num_pics_);
}

void H265DPB::Clear() {
  pics_.clear();
}

void H265DPB::StorePicture(scoped_refptr<H265Picture> pic,
                           H265Picture::ReferenceType ref) {
  DCHECK_LT(pics_.size(), max_num_pics_);
  pic->ref_ = ref;
  DVLOG(3) << "Adding PicNum: " << pic->pic_order_cnt_val_
           << " ref: " << static_cast<int>(pic->ref_);
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

  DVLOG(1) << "Missing " << H265Picture::GetReferenceName(ref)
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

}  // namespace media