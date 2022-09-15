// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/codec/scoped_vpx_codec.h"

#include <ostream>

#include "base/check_op.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_codec.h"

namespace remoting {

void VpxCodecDeleter::operator()(vpx_codec_ctx_t* codec) {
  if (codec) {
    vpx_codec_err_t ret = vpx_codec_destroy(codec);
    CHECK_EQ(ret, VPX_CODEC_OK) << "Failed to destroy codec";
    delete codec;
  }
}

}  // namespace remoting
