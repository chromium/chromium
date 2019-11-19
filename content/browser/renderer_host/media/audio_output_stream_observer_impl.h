// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_STREAM_OBSERVER_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_STREAM_OBSERVER_IMPL_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/audio_output_stream.mojom.h"

namespace content {

class CONTENT_EXPORT AudioOutputStreamObserverImpl
    : public media::mojom::AudioOutputStreamObserver {
 public:
  AudioOutputStreamObserverImpl(int render_process_id,
                                int render_frame_id,
                                int stream_id);
  ~AudioOutputStreamObserverImpl() override;

  // media::mojom::AudioOutputStreamObserver implementation
  void DidStartPlaying() override;
  void DidStopPlaying() override;
  void DidChangeAudibleState(bool is_audible) override;

 private:
  const int render_process_id_;
  const int render_frame_id_;
  const int stream_id_;
  bool did_start_playing_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioOutputStreamObserverImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_AUDIO_OUTPUT_STREAM_OBSERVER_IMPL_H_
