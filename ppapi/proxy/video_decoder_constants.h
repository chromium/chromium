// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_VIDEO_DECODER_CONSTANTS_H_
#define PPAPI_PROXY_VIDEO_DECODER_CONSTANTS_H_

namespace ppapi {
namespace proxy {

// These constants are shared by the video decoder resource and host.
enum {
  // Maximum number of concurrent decodes which can be pending.
  kMaximumPendingDecodes = 8,

  // Minimum size of shared-memory buffers (100 KB). Make them large since we
  // try to reuse them.
  kMinimumBitstreamBufferSize = 100 << 10,

  // Maximum size of shared-memory buffers (4 MB). This should be enough even
  // for 4K video at reasonable compression levels.
  kMaximumBitstreamBufferSize = 4 << 20,

  // The maximum number of pictures that the client can pass in for
  // min_picture_count, just as a sanity check on the argument.
  // This should match the constant of the same name in test_video_decoder.cc.
  kMaximumPictureCount = 100
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_VIDEO_DECODER_CONSTANTS_H_
