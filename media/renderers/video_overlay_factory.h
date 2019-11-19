// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_
#define MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/unguessable_token.h"
#include "media/base/media_export.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace media {

class VideoFrame;

// Creates video overlay frames - native textures that get turned into
// transparent holes in the browser compositor using overlay system.
class MEDIA_EXPORT VideoOverlayFactory {
 public:
  VideoOverlayFactory();
  ~VideoOverlayFactory();

  scoped_refptr<::media::VideoFrame> CreateFrame(const gfx::Size& size);
  const base::UnguessableToken& overlay_plane_id() const {
    return overlay_plane_id_;
  }

 private:
  // |overlay_plane_id_| identifies the instances of VideoOverlayFactory.
  const base::UnguessableToken overlay_plane_id_;

  DISALLOW_COPY_AND_ASSIGN(VideoOverlayFactory);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_VIDEO_OVERLAY_FACTORY_H_