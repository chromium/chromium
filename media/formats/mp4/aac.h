// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_AAC_H_
#define MEDIA_FORMATS_MP4_AAC_H_

#include <stdint.h>

#include <vector>

#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "media/base/audio_codecs.h"
#include "media/base/channel_layout.h"
#include "media/base/media_export.h"
#include "media/base/media_log.h"

namespace media {

class BitReader;

namespace mp4 {

// This class parses the AAC information from decoder specific information
// embedded in the esds box in an ISO BMFF file.
// Please refer to ISO 14496 Part 3 Table 1.13 - Syntax of AudioSpecificConfig
// for more details.
class MEDIA_EXPORT AAC {
 public:
  // AAC has a few nested profile types, and we often have to add special
  // behavior for XHE AAC typed media. Specifically, this is the USAC
  // Audio-Object type from
  // https://wiki.multimedia.cx/index.php/MPEG-4_Audio#Audio_Object_Types.
  static constexpr uint8_t kXHeAAcType = 42;

  AAC();
  AAC(const AAC& other);
  ~AAC();

  // Parse the AAC config from the raw binary data embedded in esds box.
  // The function will parse the data and get the ElementaryStreamDescriptor,
  // then it will parse the ElementaryStreamDescriptor to get audio stream
  // configurations.
  bool Parse(base::span<const uint8_t> data, MediaLog* media_log);

  // Gets the output sample rate for the AAC stream.
  // |sbr_in_mimetype| should be set to true if the SBR mode is
  // signalled in the mimetype. (ie mp4a.40.5 in the codecs parameter).
  // Returns the samples_per_second value that should used in an
  // AudioDecoderConfig.
  int GetOutputSamplesPerSecond(bool sbr_in_mimetype) const;

  // Gets the channel layout for the AAC stream.
  // |sbr_in_mimetype| should be set to true if the SBR mode is
  // signalled in the mimetype. (ie mp4a.40.5 in the codecs parameter).
  // Returns the channel_layout value that should used in an
  // AudioDecoderConfig.
  ChannelLayout GetChannelLayout(bool sbr_in_mimetype) const;

  // Converts a raw AAC frame into an AAC frame with an ADTS header. Allocates
  // new memory and copies the data from `buffer`, with the appropriate ADTS
  // header. The size of the returned array is `buffer.size` +
  // `adts_header_size`. Returns an empty HeapArray<uint8_t> on failure.
  base::HeapArray<uint8_t> CreateAdtsFromEsds(base::span<const uint8_t> buffer,
                                              int* adts_header_size) const;

  // If known, returns the AudioCodecProfile.
  AudioCodecProfile GetProfile() const;

  // Returns the codec specific data needed by android MediaCodec.
  std::vector<uint8_t> codec_specific_data() const {
    return codec_specific_data_;
  }

 private:
  bool SkipDecoderGASpecificConfig(BitReader* bit_reader) const;
  bool SkipErrorSpecificConfig() const;
  bool SkipGASpecificConfig(BitReader* bit_reader) const;

  // Sets the ADTS header into the given span.
  void SetAdtsHeader(base::span<uint8_t> adts, size_t total_size) const;

  // The following variables store the AAC specific configuration information
  // that are used to generate the ADTS header.
  uint8_t profile_;
  uint8_t frequency_index_;
  uint8_t channel_config_;

  // The codec specific data needed by the android MediaCodec.
  std::vector<uint8_t> codec_specific_data_;

  // The following variables store audio configuration information that
  // can be used by Chromium. They are based on the AAC specific
  // configuration but can be overridden by extensions in elementary
  // stream descriptor.
  int frequency_;
  int extension_frequency_;
  ChannelLayout channel_layout_;
};

}  // namespace mp4

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_AAC_H_
