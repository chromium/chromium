// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_COMMON_PACKET_H_
#define MEDIA_CAST_COMMON_PACKET_H_

#include <stdint.h>

#include <vector>

namespace media::cast {

using Packet = std::vector<uint8_t>;

}  // namespace media::cast

#endif  // MEDIA_CAST_COMMON_PACKET_H_
