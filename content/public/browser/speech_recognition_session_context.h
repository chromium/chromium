// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_

#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "url/origin.h"

namespace content {

// The context information required by clients of the SpeechRecognitionManager
// and its delegates for mapping the recognition session to other browser
// elements involved with it (e.g., the page element that requested the
// recognition). The manager keeps this struct attached to the recognition
// session during all the session lifetime, making its contents available to
// clients. (In this regard, see SpeechRecognitionManager::GetSessionContext().)
struct CONTENT_EXPORT SpeechRecognitionSessionContext {
  SpeechRecognitionSessionContext();
  SpeechRecognitionSessionContext(const SpeechRecognitionSessionContext& other);
  ~SpeechRecognitionSessionContext();

  GlobalRenderFrameHostId global_id;

  // The `embedder_global_id` represents a Browser plugin guest's embedder. This
  // is filled in if the session is from a guest Web Speech API. We use these to
  // check if the embedder (app) is permitted to use audio.
  GlobalRenderFrameHostId embedder_global_id;

  // Origin that is requesting recognition, for prompting security notifications
  // to the user.
  url::Origin security_origin;

  // The label for the permission request, it is used for request abortion.
  std::string label;

  // A list of devices being used by the recognition session.
  blink::MediaStreamDevices devices;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
