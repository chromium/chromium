// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_UNICODE_TO_KEYSYM_H_
#define REMOTING_HOST_LINUX_UNICODE_TO_KEYSYM_H_

#include <stdint.h>

#include <vector>

namespace remoting {

std::vector<uint32_t> GetKeySymsForUnicode(uint32_t code_point);

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_UNICODE_TO_KEYSYM_H_
