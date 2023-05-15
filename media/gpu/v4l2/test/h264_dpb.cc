// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_dpb.h"

namespace media {
namespace v4l2_test {

H264SliceMetadata::H264SliceMetadata() = default;

H264SliceMetadata::H264SliceMetadata(const H264SliceMetadata&) = default;

int H264DPB::CountRefPics() {
  int ret = 0;
  for (auto& i : *this) {
    if (i.second.ref) {
      ret++;
    }
  }
  return ret;
}

void H264DPB::Delete(const H264SliceMetadata& pic) {
  erase(pic.ref_ts_nsec);
}

void H264DPB::DeleteUnused() {
  std::vector<uint64_t> keys;
  for (auto& i : *this) {
    if (i.second.outputted && !i.second.ref) {
      keys.push_back(i.first);
    }
  }
  for (auto i : keys) {
    erase(i);
  }
}

void H264DPB::UnmarkLowestFrameNumWrapShortRefPic() {
  int key = -1;
  for (auto& i : *this) {
    H264SliceMetadata pic = i.second;
    if (pic.ref && !pic.long_term_reference_flag &&
        (key < 0 || pic.frame_num_wrap < this->at(key).frame_num_wrap)) {
      key = i.first;
    }
  }

  if (key >= 0) {
    this->at(key).ref = false;
  }
}

std::vector<H264SliceMetadata*> H264DPB::GetNotOutputtedPicsAppending() {
  std::vector<H264SliceMetadata*> notOutputtedSlices;
  for (auto& i : *this) {
    if (!i.second.outputted) {
      notOutputtedSlices.push_back(&(i.second));
    }
  }

  return notOutputtedSlices;
}

void H264DPB::MarkAllUnusedRef() {
  for (auto& i : *this) {
    i.second.ref = false;
  }
}

void H264DPB::UpdatePicNums(const int curr_frame_num, const int max_frame_num) {
  for (auto& i : *this) {
    if (!i.second.ref) {
      continue;
    }

    // Update picture numbers as defined in section 8.2.4.1.
    if (i.second.long_term_reference_flag) {
      i.second.long_term_pic_num = i.second.long_term_frame_idx;
    } else {
      if (i.second.frame_num > curr_frame_num) {
        i.second.frame_num_wrap = i.second.frame_num - max_frame_num;
      } else {
        i.second.frame_num_wrap = i.second.frame_num;
      }

      i.second.pic_num = i.second.frame_num_wrap;
    }
  }
}

void H264DPB::UnmarkPicByPicNum(const int pic_num) {
  for (auto& i : *this) {
    if (i.second.pic_num == pic_num && i.second.ref &&
        !i.second.long_term_reference_flag) {
      i.second.ref = false;
      break;
    }
  }
}

void H264DPB::UnmarkLongTerm(const int pic_num) {
  for (auto& i : *this) {
    if (i.second.long_term_pic_num == pic_num && i.second.ref &&
        i.second.long_term_reference_flag) {
      i.second.ref = false;
      break;
    }
  }
}

H264SliceMetadata* H264DPB::GetShortRefPicByPicNum(const int pic_num) {
  for (auto& i : *this) {
    if (i.second.ref && !i.second.long_term_reference_flag &&
        i.second.pic_num == pic_num) {
      return &i.second;
    }
  }
  return nullptr;
}

H264SliceMetadata* H264DPB::GetLongRefPicByFrameIdx(const int frame_index) {
  for (auto& i : *this) {
    if (i.second.ref && i.second.long_term_reference_flag &&
        i.second.long_term_frame_idx == frame_index) {
      return &i.second;
    }
  }
  return nullptr;
}

void H264DPB::UnmarkLongTermPicsGreaterThanFrameIndex(const int index) {
  for (auto& i : *this) {
    if (i.second.long_term_reference_flag &&
        i.second.long_term_frame_idx > index) {
      i.second.ref = false;
    }
  }
}

std::set<int> H264DPB::GetHeldCaptureIds() const {
  std::set<int> dpb_ids;
  for (auto& i : *this) {
    if (i.second.ref || !i.second.outputted) {
      dpb_ids.insert(i.second.capture_queue_buffer_id);
    }
  }

  return dpb_ids;
}

}  // namespace v4l2_test
}  // namespace media
