// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_APPLE_PIXEL_BUFFER_TRANSFERER_H_
#define MEDIA_CAPTURE_VIDEO_APPLE_PIXEL_BUFFER_TRANSFERER_H_

#import <VideoToolbox/VideoToolbox.h>

#include "base/apple/scoped_cftyperef.h"
#include "media/capture/capture_export.h"

namespace media {

// The PixelBufferTransferer copies and/or converts image buffers from a source
// buffer to a destination buffer. The desired resolution and pixel format is
// configured on the destination pixel buffer, not the transferer. See also
// PixelBufferPool for creating recyclable pixel buffers.
//
// If the destination pixel buffer is set up with a different resolution than
// the source, scaling happens.
// If the destination pixel buffer is set up with a different pixel format than
// thee source, conversion happens.
class CAPTURE_EXPORT PixelBufferTransferer {
 public:
  PixelBufferTransferer();
  ~PixelBufferTransferer();

  // Copies and/or converts from source to destination. If the transfer is not
  // supported, false is returned.
  bool TransferImage(CVPixelBufferRef source, CVPixelBufferRef destination);

 private:
  base::apple::ScopedCFTypeRef<VTPixelTransferSessionRef> transfer_session_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_APPLE_PIXEL_BUFFER_TRANSFERER_H_
