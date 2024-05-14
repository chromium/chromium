// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include <algorithm>

#include "base/logging.h"
#include "base/notreached.h"
#include "media/gpu/h264_dpb.h"

namespace media {

H264Picture::H264Picture()
    : pic_order_cnt_type(0),
      top_field_order_cnt(0),
      bottom_field_order_cnt(0),
      pic_order_cnt(0),
      pic_order_cnt_msb(0),
      pic_order_cnt_lsb(0),
      delta_pic_order_cnt_bottom(0),
      delta_pic_order_cnt0(0),
      delta_pic_order_cnt1(0),
      pic_num(0),
      long_term_pic_num(0),
      frame_num(0),
      frame_num_offset(0),
      frame_num_wrap(0),
      long_term_frame_idx(0),
      type(H264SliceHeader::kPSlice),
      nal_ref_idc(0),
      idr(false),
      idr_pic_id(0),
      ref(false),
      ref_pic_list_modification_flag_l0(0),
      abs_diff_pic_num_minus1(0),
      long_term(false),
      outputted(false),
      mem_mgmt_5(false),
      nonexisting(false),
      field(FIELD_NONE),
      long_term_reference_flag(false),
      adaptive_ref_pic_marking_mode_flag(false),
      dpb_position(0) {
  memset(&ref_pic_marking, 0, sizeof(ref_pic_marking));
}

H264Picture::~H264Picture() = default;

V4L2H264Picture* H264Picture::AsV4L2H264Picture() {
  return nullptr;
}

VaapiH264Picture* H264Picture::AsVaapiH264Picture() {
  return nullptr;
}

D3D11H264Picture* H264Picture::AsD3D11H264Picture() {
  return nullptr;
}

H264DPB::H264DPB() : max_num_pics_(0) {}
H264DPB::~H264DPB() = default;

void H264DPB::Clear() {
  pics_.clear();
}

void H264DPB::set_max_num_pics(size_t max_num_pics) {
  DCHECK_LE(max_num_pics, static_cast<size_t>(kDPBMaxSize));
  max_num_pics_ = max_num_pics;
  if (pics_.size() > max_num_pics_)
    pics_.resize(max_num_pics_);
}

void H264DPB::UpdatePicPositions() {
  size_t i = 0;
  for (auto& pic : pics_) {
    pic->dpb_position = i;
    ++i;
  }
}

void H264DPB::Delete(scoped_refptr<H264Picture> pic) {
  for (auto it = pics_.begin(); it != pics_.end(); ++it) {
    if ((*it) == pic) {
      pics_.erase(it);
      UpdatePicPositions();
      return;
    }
  }
  NOTREACHED_IN_MIGRATION() << "Missing pic with POC: " << pic->pic_order_cnt;
}

void H264DPB::DeleteUnused() {
  for (auto it = pics_.begin(); it != pics_.end();) {
    if ((*it)->outputted && !(*it)->ref)
      it = pics_.erase(it);
    else
      ++it;
  }
  UpdatePicPositions();
}

void H264DPB::StorePic(scoped_refptr<H264Picture> pic) {
  DCHECK_LT(pics_.size(), max_num_pics_);
  DVLOG(3) << "Adding PicNum: " << pic->pic_num << " ref: " << (int)pic->ref
           << " longterm: " << (int)pic->long_term << " to DPB";
  pic->dpb_position = pics_.size();
  pics_.push_back(std::move(pic));
}

int H264DPB::CountRefPics() {
  int ret = 0;
  for (size_t i = 0; i < pics_.size(); ++i) {
    if (pics_[i]->ref)
      ++ret;
  }
  return ret;
}

void H264DPB::MarkAllUnusedForRef() {
  for (size_t i = 0; i < pics_.size(); ++i)
    pics_[i]->ref = false;
}

scoped_refptr<H264Picture> H264DPB::GetShortRefPicByPicNum(int pic_num) {
  for (const auto& pic : pics_) {
    if (pic->ref && !pic->long_term && pic->pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing short ref pic num: " << pic_num;
  return nullptr;
}

scoped_refptr<H264Picture> H264DPB::GetLongRefPicByLongTermPicNum(int pic_num) {
  for (const auto& pic : pics_) {
    if (pic->ref && pic->long_term && pic->long_term_pic_num == pic_num)
      return pic;
  }

  DVLOG(1) << "Missing long term pic num: " << pic_num;
  return nullptr;
}

scoped_refptr<H264Picture> H264DPB::GetLongRefPicByLongTermIdx(int idx) {
  for (const auto& pic : pics_) {
    if (pic->ref && pic->long_term && pic->long_term_frame_idx == idx) {
      return pic;
    }
  }

  return nullptr;
}

scoped_refptr<H264Picture> H264DPB::GetLowestFrameNumWrapShortRefPic() {
  scoped_refptr<H264Picture> ret;
  for (const auto& pic : pics_) {
    if (pic->ref && !pic->long_term &&
        (!ret || pic->frame_num_wrap < ret->frame_num_wrap))
      ret = pic;
  }
  return ret;
}

void H264DPB::GetNotOutputtedPicsAppending(H264Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (!pic->outputted)
      out->push_back(pic);
  }
}

void H264DPB::GetShortTermRefPicsAppending(H264Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->ref && !pic->long_term)
      out->push_back(pic);
  }
}

void H264DPB::GetLongTermRefPicsAppending(H264Picture::Vector* out) {
  for (const auto& pic : pics_) {
    if (pic->ref && pic->long_term)
      out->push_back(pic);
  }
}

}  // namespace media
