// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
#define MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_

#include "media/formats/mp4/writable_box_definitions.h"

#include "base/time/time.h"
#include "media/base/audio_encoder.h"
#include "media/muxers/muxer.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace media {

class AudioParameters;

// Mp4MuxerDelegate builds the MP4 boxes from the encoded stream.
// The boxes fields will start to be populated from the first stream and
// complete in the `Flush` API call. The created box data is a complete
// MP4 format and internal data will be cleared at the end of `Flush`.
class MEDIA_EXPORT Mp4MuxerDelegate {
 public:
  explicit Mp4MuxerDelegate(Muxer::WriteDataCB write_callback);
  ~Mp4MuxerDelegate() = default;
  Mp4MuxerDelegate(const Mp4MuxerDelegate&) = delete;
  Mp4MuxerDelegate& operator=(const Mp4MuxerDelegate&) = delete;

  void AddVideoFrame(const Muxer::VideoParameters& params,
                     base::StringPiece encoded_data,
                     base::TimeTicks timestamp);

  void AddAudioFrame(const AudioParameters& params,
                     base::StringPiece encoded_data,
                     const AudioEncoder::CodecDescription& codec_description,
                     base::TimeTicks timestamp);
  void Flush();
};

}  // namespace media

#endif  // MEDIA_MUXERS_MP4_MUXER_DELEGATE_H_
