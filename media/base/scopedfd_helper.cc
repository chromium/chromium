// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <vector>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "media/base/scopedfd_helper.h"

namespace media {

std::vector<base::ScopedFD> DuplicateFDs(
    const std::vector<base::ScopedFD>& fds) {
  std::vector<base::ScopedFD> ret;

  for (auto& fd : fds) {
    base::ScopedFD dup_fd = base::ScopedFD(HANDLE_EINTR(dup(fd.get())));
    PCHECK(dup_fd.is_valid());
    ret.push_back(std::move(dup_fd));
  }

  return ret;
}

}  // namespace media
