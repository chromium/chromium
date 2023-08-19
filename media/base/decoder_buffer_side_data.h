// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_
#define MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_

#include <vector>

#include "media/base/media_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

struct MEDIA_EXPORT DecoderBufferSideData {
  DecoderBufferSideData();
  ~DecoderBufferSideData();

  DecoderBufferSideData(const DecoderBufferSideData& other);

  // Returns true if all the fields in the other struct match ours.
  bool Matches(const DecoderBufferSideData& other) const;

  // VP9 specific information.
  std::vector<uint32_t> spatial_layers;
  std::vector<uint8_t> alpha_data;

  // Secure buffer handle corresponding to the decrypted contents of the
  // associated DecoderBuffer. A non-zero value indicates this was set.
  uint64_t secure_handle = 0;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_
