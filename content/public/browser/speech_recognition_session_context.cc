// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/speech_recognition_session_context.h"

#include "ipc/constants.mojom.h"

namespace content {

SpeechRecognitionSessionContext::SpeechRecognitionSessionContext()
    : render_process_id(0),
      render_frame_id(IPC::mojom::kRoutingIdNone),
      embedder_render_process_id(0),
      embedder_render_frame_id(IPC::mojom::kRoutingIdNone) {}

SpeechRecognitionSessionContext::SpeechRecognitionSessionContext(
    const SpeechRecognitionSessionContext& other) = default;

SpeechRecognitionSessionContext::~SpeechRecognitionSessionContext() {
}

}  // namespace content
