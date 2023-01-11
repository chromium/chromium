// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_SCOPED_VPX_CODEC_H_
#define REMOTING_CODEC_SCOPED_VPX_CODEC_H_

#include <memory>

extern "C" {
typedef struct vpx_codec_ctx vpx_codec_ctx_t;
}

namespace remoting {

struct VpxCodecDeleter {
  void operator()(vpx_codec_ctx_t* codec);
};

typedef std::unique_ptr<vpx_codec_ctx_t, VpxCodecDeleter> ScopedVpxCodec;

}  // namespace remoting

#endif  // REMOTING_CODEC_SCOPED_VPX_CODEC_H_
