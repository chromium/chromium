// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_renderer_factory.h"

namespace blink {

class MediaStreamRendererFactoryImpl : public WebMediaStreamRendererFactory {
 public:
  MediaStreamRendererFactoryImpl();
  ~MediaStreamRendererFactoryImpl() override;

  scoped_refptr<WebMediaStreamVideoRenderer> GetVideoRenderer(
      const WebMediaStream& web_stream,
      const WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      override;

  scoped_refptr<WebMediaStreamAudioRenderer> GetAudioRenderer(
      const WebMediaStream& web_stream,
      WebLocalFrame* web_frame,
      const WebString& device_id) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaStreamRendererFactoryImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIASTREAM_MEDIA_STREAM_RENDERER_FACTORY_IMPL_H_
