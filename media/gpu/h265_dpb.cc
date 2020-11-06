// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/logging.h"
#include "media/gpu/h265_dpb.h"
#include "media/video/h265_parser.h"

namespace media {

H265Picture::H265Picture() = default;
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
  pics_.clear();
}

}  // namespace media