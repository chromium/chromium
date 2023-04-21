// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/test/h264_dpb.h"

namespace media {
namespace v4l2_test {

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
    if (pic.ref &&
        (key < 0 || pic.frame_num_wrap < this->at(key).frame_num_wrap)) {
      key = i.first;
    }
  }

  if (key >= 0) {
    this->at(key).ref = false;
  }
}

std::vector<H264SliceMetadata*> H264DPB::GetNotOutputtedPicsAppending() {
  std::vector<H264SliceMetadata*> data;
  for (auto& i : *this) {
    H264SliceMetadata pic = i.second;
    if (!pic.outputted) {
      data.push_back(&pic);
    }
  }

  return data;
}

void H264DPB::MarkAllUnusedRef() {
  for (auto& i : *this) {
    i.second.ref = false;
  }
}

void H264DPB::UpdatePicNums(const int curr_frame_num, const int max_frame_num) {
  for (auto& i : *this) {
    if (i.second.ref) {
      continue;
    }

    if (i.second.frame_num > curr_frame_num) {
      i.second.frame_num_wrap = i.second.frame_num - max_frame_num;
    } else {
      i.second.frame_num_wrap = i.second.frame_num;
    }
  }
}

}  // namespace v4l2_test
}  // namespace media
