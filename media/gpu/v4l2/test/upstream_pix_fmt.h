// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_TEST_UPSTREAM_PIX_FMT_H_
#define MEDIA_GPU_V4L2_TEST_UPSTREAM_PIX_FMT_H_

// This has not been accepted upstream.
#ifndef V4L2_PIX_FMT_AV1
#define V4L2_PIX_FMT_AV1 v4l2_fourcc('A', 'V', '0', '1') /* AV1 */
#endif
// This has been upstreamed and backported for ChromeOS, but has not been
// picked up by the Chromium sysroots.
#ifndef V4L2_PIX_FMT_AV1_FRAME
#define V4L2_PIX_FMT_AV1_FRAME                        \
  v4l2_fourcc('A', 'V', '1', 'F') /* AV1 parsed frame \
                                   */
#endif

#ifndef V4L2_PIX_FMT_MT2T
// MTK 10-bit block mode, two non-contiguous planes.
#define V4L2_PIX_FMT_MT2T v4l2_fourcc('M', 'T', '2', 'T')
#endif

#endif  // MEDIA_GPU_V4L2_TEST_UPSTREAM_PIX_FMT_H_
