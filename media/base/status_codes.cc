// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/status_codes.h"

namespace media {

// TODO(tmathmeyer) consider a way to get the names, since we don't have
// the easy c++20 way yet.
std::ostream& operator<<(std::ostream& os, const StatusCode& code) {
  return os << std::hex << static_cast<StatusCodeType>(code);
}

}  // namespace media
