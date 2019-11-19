// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_

#include <string>

#include "base/callback_forward.h"
#include "third_party/blink/public/web/modules/mediastream/web_media_stream_renderer_factory.h"

namespace content {

// TestMediaStreamClient is a mock implementation of MediaStreamClient used when
// running web tests.
class TestMediaStreamRendererFactory
    : public blink::WebMediaStreamRendererFactory {
 public:
  TestMediaStreamRendererFactory();
  ~TestMediaStreamRendererFactory() override;

  // MediaStreamRendererFactory implementation.
  scoped_refptr<blink::WebMediaStreamVideoRenderer> GetVideoRenderer(
      const blink::WebMediaStream& web_stream,
      const blink::WebMediaStreamVideoRenderer::RepaintCB& repaint_cb,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> main_render_task_runner)
      override;

  scoped_refptr<blink::WebMediaStreamAudioRenderer> GetAudioRenderer(
      const blink::WebMediaStream& web_stream,
      blink::WebLocalFrame* web_frame,
      const blink::WebString& device_id) override;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_TEST_MEDIA_STREAM_RENDERER_FACTORY_H_
