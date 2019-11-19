// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_MOCK_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_VIDEO_MOCK_VIDEO_DECODE_ACCELERATOR_H_

#include "video_decode_accelerator.h"

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/video/picture.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Remember to use the default action of Destroy() when using EXPECT_CALL.
// Otherwise, the mock will be leaked and the test runner will complain.
class MockVideoDecodeAccelerator : public VideoDecodeAccelerator {
 public:
  MockVideoDecodeAccelerator();
  ~MockVideoDecodeAccelerator() override;

  MOCK_METHOD2(Initialize, bool(const Config& config, Client* client));
  MOCK_METHOD1(Decode, void(BitstreamBuffer bitstream_buffer));
  MOCK_METHOD2(Decode,
               void(scoped_refptr<DecoderBuffer> buffer, int32_t bitstream_id));
  MOCK_METHOD1(AssignPictureBuffers,
               void(const std::vector<PictureBuffer>& buffers));
  MOCK_METHOD1(ReusePictureBuffer, void(int32_t picture_buffer_id));
  MOCK_METHOD0(Flush, void());
  MOCK_METHOD0(Reset, void());
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD2(TryToSetupDecodeOnSeparateThread,
               bool(const base::WeakPtr<Client>&,
                    const scoped_refptr<base::SingleThreadTaskRunner>&));

 private:
  void DeleteThis();
  DISALLOW_COPY_AND_ASSIGN(MockVideoDecodeAccelerator);
};

}  // namespace media

#endif  // MEDIA_VIDEO_MOCK_VIDEO_DECODE_ACCELERATOR_H_
