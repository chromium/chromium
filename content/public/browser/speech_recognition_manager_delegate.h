// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace media::mojom {
class SpeechRecognitionContext;
}
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)

namespace url {
class Origin;
}  // namespace url

namespace content {

class SpeechRecognitionEventListener;

struct GlobalRenderFrameHostId;

// Allows embedders to display the current state of recognition, for getting the
// user's permission and for fetching optional request information.
class CONTENT_EXPORT SpeechRecognitionManagerDelegate {
 public:
  virtual ~SpeechRecognitionManagerDelegate() = default;

  // Checks (asynchronously) if current setup allows speech recognition.
  // This is called on the IO thread.
  virtual void CheckRecognitionIsAllowed(
      int session_id,
      base::OnceCallback<void(bool ask_user, bool is_allowed)> callback) = 0;

  // Checks whether the delegate is interested (returning a non nullptr ptr) or
  // not (returning nullptr) in receiving a copy of all sessions events.
  // This is called on the IO thread.
  virtual SpeechRecognitionEventListener* GetEventListener() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // Returns query parameters that the network speech recognition engine adds
  // to the requests of a session started by `origin`, so that the speech
  // service can pick a recognizer for the caller. Parameters that the engine
  // sets itself, such as "lang", take precedence. The default returns none.
  // This is called on the IO thread.
  virtual base::flat_map<std::string, std::string> GetRoutingParams(
      const url::Origin& origin);
#endif  // !BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
  // This interfaces is used to bind the On-Device Speech Recognition Service on
  // the non-android platforms.
  // This is called on the IO thread.
  virtual void BindSpeechRecognitionContext(
      mojo::PendingReceiver<media::mojom::SpeechRecognitionContext> receiver,
      const std::string& language,
      const GlobalRenderFrameHostId& render_frame_host_id) = 0;
#endif  // !BUILDFLAG(IS_FUCHSIA) && !BUILDFLAG(IS_ANDROID)
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_MANAGER_DELEGATE_H_
