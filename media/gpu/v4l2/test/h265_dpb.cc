// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h265_dpb.h"

#include <algorithm>

#include "base/logging.h"
#include "media/gpu/macros.h"

namespace media::v4l2_test {

H265Picture::H265Picture() = default;
H265Picture::~H265Picture() = default;

H265DPB::H265DPB() = default;
H265DPB::~H265DPB() = default;

void H265DPB::SetMaxNumPics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, static_cast<size_t>(kMaxDpbSize));
  max_num_pics_ = max_num_pics;
  // reserve() is a no-op when |pics_.size() <= max_num_pics_|
  pics_.reserve(max_num_pics_);
}

void H265DPB::Clear() {
  pics_.clear();
}

void H265DPB::StorePicture(scoped_refptr<H265Picture> pic) {
  DCHECK_LT(pics_.size(), max_num_pics_);

  LOG(INFO) << "Adding PicNum: " << pic->pic_order_cnt_val_
            << " reference type: " << static_cast<int>(pic->reference_type_);
  pics_.push_back(std::move(pic));
}

void H265DPB::MarkAllUnusedForReference() {
  for (const auto& pic : pics_) {
    pic->reference_type_ = H265Picture::kUnused;
  }
}

void H265DPB::DeleteUnused() {
  // Note that |pic| is removed from the DPB during the for loop.
  // |pic| is removed after swapping with the last one in |pics_|.
  // The for loop will continue with the swapped one.
  for (auto it = pics_.begin(); it != pics_.end();) {
    auto& pic = *it;
    if ((!pic->pic_output_flag_ || pic->outputted_) &&
        (pic->reference_type_ == H265Picture::kUnused)) {
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
    if (pic->reference_type_ != H265Picture::kUnused) {
      count++;
    }
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
      pic->reference_type_ = ref;
      return pic;
    }
  }

  LOG(ERROR) << "Missing " << H265Picture::GetReferenceName(ref)
             << " ref pic num: " << poc;
  return nullptr;
}

void H265DPB::AppendPendingOutputPics(H265Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->pic_output_flag_ && !pic->outputted_) {
      out->push_back(pic);
    }
  }
}

void H265DPB::AppendReferencePics(H265Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->reference_type_ != H265Picture::kUnused) {
      out->push_back(pic);
    }
  }
}

std::set<uint32_t> H265DPB::GetBufferIdsInUse() const {
  std::set<uint32_t> buffer_ids_in_use;

  for (const auto& pic : pics_) {
    if (pic->reference_type_ != H265Picture::kUnused || !pic->outputted_) {
      buffer_ids_in_use.insert(pic->capture_queue_buffer_id_);
    }
  }

  return buffer_ids_in_use;
}

}  // namespace media::v4l2_test
