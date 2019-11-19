// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_RENDERER_FACTORY_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_RENDERER_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_renderer.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_video_renderer.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace blink {

class WebMediaStream;
class WebLocalFrame;

// WebMediaStreamRendererFactory is used by WebMediaPlayerMS to create audio and
// video feeds from a MediaStream provided an URL.
// The factory methods are virtual in order for Blink web tests to be able to
// override them.
class BLINK_MODULES_EXPORT WebMediaStreamRendererFactory {
 public:
  virtual ~WebMediaStreamRendererFactory() {}

  // Returns a WebMediaStreamVideoRenderer that uses the given task runners.
  // |io_task_runner| is used for passing video frames.
  virtual scoped_refptr<WebMediaStreamVideoRenderer> GetVideoRenderer(
      const WebMediaStream& web_stream,
      const WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner) = 0;

  virtual scoped_refptr<WebMediaStreamAudioRenderer> GetAudioRenderer(
      const WebMediaStream& web_stream,
      WebLocalFrame* web_frame,
      const WebString& device_id) = 0;
};

BLINK_MODULES_EXPORT std::unique_ptr<WebMediaStreamRendererFactory>
CreateWebMediaStreamRendererFactory();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_MODULES_MEDIASTREAM_WEB_MEDIA_STREAM_RENDERER_FACTORY_H_
