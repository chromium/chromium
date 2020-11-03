// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_H265_DPB_H_
#define MEDIA_GPU_H265_DPB_H_

namespace media {

// DPB - Decoded Picture Buffer.
// Stores decoded pictures that will be used for future display
// and/or reference.
class H265DPB {
 public:
  H265DPB();

  H265DPB(const H265DPB&) = delete;
  H265DPB& operator=(const H265DPB&) = delete;

  ~H265DPB();

  void set_max_num_pics(size_t max_num_pics);
  size_t max_num_pics() const { return max_num_pics_; }

 private:
  size_t max_num_pics_;
};

}  // namespace media

#endif  // MEDIA_GPU_H265_DPB_H_