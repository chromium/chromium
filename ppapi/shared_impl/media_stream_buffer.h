// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_MEDIA_STREAM_BUFFER_H_
#define PPAPI_SHARED_IMPL_MEDIA_STREAM_BUFFER_H_

#include <stdint.h>

#include "ppapi/c/ppb_audio_buffer.h"
#include "ppapi/c/ppb_video_frame.h"

namespace ppapi {

union MediaStreamBuffer {
  enum Type {
    TYPE_UNKNOWN = 0,
    TYPE_AUDIO = 1,
    TYPE_VIDEO = 2,
    TYPE_BITSTREAM = 3
  };

  struct Header {
    Type type;
    uint32_t size;
  };

  struct Audio {
    Header header;
    PP_TimeDelta timestamp;
    PP_AudioBuffer_SampleRate sample_rate;
    uint32_t number_of_channels;
    uint32_t number_of_samples;
    uint32_t data_size;
    // Uses 8 bytes to make sure the Audio struct has consistent size between
    // NaCl code and renderer code.
    uint8_t data[8];
  };

  struct Video {
    Header header;
    PP_TimeDelta timestamp;
    PP_VideoFrame_Format format;
    PP_Size size;
    uint32_t data_size;
    // Uses 8 bytes to make sure the Video struct has consistent size between
    // NaCl code and renderer code.
    uint8_t data[8];
  };

  struct Bitstream {
    Header header;
    uint32_t data_size;
    // Uses 8 bytes to make sure the Bitstream struct has consistent size
    // between NaCl code and renderer code.
    uint8_t data[8];
  };

  // Because these structs are written and read in shared memory, we need
  // the size and alighment to be consistent between NaCl and its host trusted
  // platform.
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Header, 8);
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Audio, 40);
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Video, 40);
  PP_COMPILE_ASSERT_SIZE_IN_BYTES(Bitstream, 20);

  Header header;
  Video video;
  Audio audio;
  Bitstream bitstream;
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_MEDIA_STREAM_BUFFER_H_
