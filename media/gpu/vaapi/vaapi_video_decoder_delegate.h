// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_
#define MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

namespace media {

template <class T>
class DecodeSurfaceHandler;
class VaapiWrapper;
class VASurface;

// The common part of each AcceleratedVideoDecoder's Accelerator for VA-API.
// This class allows clients to reset VaapiWrapper in case of a profile change.
// DecodeSurfaceHandler must stay alive for the lifetime of this class.
class VaapiVideoDecoderDelegate {
 public:
  VaapiVideoDecoderDelegate(DecodeSurfaceHandler<VASurface>* const vaapi_dec,
                            scoped_refptr<VaapiWrapper> vaapi_wrapper);
  virtual ~VaapiVideoDecoderDelegate();

  void set_vaapi_wrapper(scoped_refptr<VaapiWrapper> vaapi_wrapper);
  virtual void OnVAContextDestructionSoon();

  VaapiVideoDecoderDelegate(const VaapiVideoDecoderDelegate&) = delete;
  VaapiVideoDecoderDelegate& operator=(const VaapiVideoDecoderDelegate&) =
      delete;

 protected:
  // Both owned by caller.
  DecodeSurfaceHandler<VASurface>* const vaapi_dec_;
  scoped_refptr<VaapiWrapper> vaapi_wrapper_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_VAAPI_VIDEO_DECODER_DELEGATE_H_
