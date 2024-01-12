// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/libvpx_thread_wrapper.h"

#include "media/base/codec_worker_impl.h"
#include "third_party/libvpx/source/libvpx/vpx_util/vpx_thread.h"

namespace media {

void InitLibVpxThreadWrapper() {
  const VPxWorkerInterface interface =
      CodecWorkerImpl<VPxWorkerInterface, VPxWorkerImpl, VPxWorker,
                      VPxWorkerStatus, NOT_OK, OK,
                      WORK>::GetCodecWorkerInterface();

  CHECK(vpx_set_worker_interface(&interface));
}

}  // namespace media
