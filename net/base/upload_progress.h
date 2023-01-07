// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_UPLOAD_PROGRESS_H_
#define NET_BASE_UPLOAD_PROGRESS_H_

#include <stdint.h>

namespace net {

class UploadProgress {
 public:
  UploadProgress() : size_(0), position_(0) {}
  UploadProgress(uint64_t position, uint64_t size)
      : size_(size), position_(position) {}

  uint64_t size() const { return size_; }
  uint64_t position() const { return position_; }

 private:
  uint64_t size_;
  uint64_t position_;
};

}  // namespace net

#endif  // NET_BASE_UPLOAD_PROGRESS_H_
