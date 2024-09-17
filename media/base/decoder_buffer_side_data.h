// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_
#define MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/time/time.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/video_decoder_config.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
  //
  // Required by some hardware content protection mechanisms to ensure the
  // decrypted buffer never leaves secure memory. When set, this DecoderBuffer
  // generally carries the partially (headers in the clear) encrypted payload;
  uint64_t secure_handle = 0;

  // Duration of (audio) samples from the beginning and end of this frame
  // which should be discarded after decoding. A value of kInfiniteDuration
  // for the first value indicates the entire frame should be discarded; the
  // second value must be base::TimeDelta() in this case.
  using DiscardPadding = std::pair<base::TimeDelta, base::TimeDelta>;
  DiscardPadding discard_padding;

  // If set, it signals that the current end of stream buffer is for a config
  // change. The upcoming config may be used by the decoder to make more optimal
  // decisions around reallocation and flushing. Only set on EOS buffers.
  using ConfigVariant = absl::variant<AudioDecoderConfig, VideoDecoderConfig>;
  std::optional<ConfigVariant> next_config;
};

}  // namespace media

#endif  // MEDIA_BASE_DECODER_BUFFER_SIDE_DATA_H_
