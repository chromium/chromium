// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_broker_helper.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"

namespace content {

void NotifyFrameHostOfAudioStreamStarted(int render_process_id,
                                         int render_frame_id) {
  auto impl = [](int render_process_id, int render_frame_id) {
    if (auto* host =
            RenderFrameHostImpl::FromID(render_process_id, render_frame_id)) {
      host->OnMediaStreamAdded();
    }
  };
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(impl, render_process_id, render_frame_id));
}

void NotifyFrameHostOfAudioStreamStopped(int render_process_id,
                                         int render_frame_id) {
  auto impl = [](int render_process_id, int render_frame_id) {
    if (auto* host =
            RenderFrameHostImpl::FromID(render_process_id, render_frame_id)) {
      host->OnMediaStreamRemoved();
    }
  };
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(impl, render_process_id, render_frame_id));
}

}  // namespace content
