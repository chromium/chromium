// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_AV1_PIX_FMT_H_
#define MEDIA_GPU_V4L2_TEST_AV1_PIX_FMT_H_

// TODO(stevecho): This is temporary until the change to define
// V4L2_PIX_FMT_AV1_FRAME lands in videodev2.h.
// https://patchwork.linuxtv.org/project/linux-media/patch/20210810220552.298140-2-daniel.almeida@collabora.com/
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME                        \
  v4l2_fourcc('A', 'V', '1', 'F') /* AV1 parsed frame \
                                   */
#endif

#endif  // MEDIA_GPU_V4L2_TEST_VIDEO_DECODER_H_
