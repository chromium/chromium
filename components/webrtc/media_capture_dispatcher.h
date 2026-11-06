// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER_H_
#define COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "content/public/browser/media_request_state.h"
#include "content/public/browser/media_stream_request.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace webrtc {

// Lets code outside //chrome observe media capture request, stream and device
// list updates, and ask the embedder to process or check media access. The
// embedder creates and owns at most one instance.
class COMPONENT_EXPORT(COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER)
    MediaCaptureDispatcher {
 public:
  class Observer {
   public:
    virtual ~Observer() = default;

    // Handle an information update consisting of an up-to-date audio capture
    // device list. This happens when a microphone is plugged in or unplugged.
    virtual void OnUpdateAudioDevices(
        const blink::MediaStreamDevices& devices) {}

    // Handle an information update consisting of an up-to-date video capture
    // device list. This happens when a camera is plugged in or unplugged.
    virtual void OnUpdateVideoDevices(
        const blink::MediaStreamDevices& devices) {}

    // Handle an information update related to a media stream request.
    virtual void OnRequestUpdate(int render_process_id,
                                 int render_frame_id,
                                 blink::mojom::MediaStreamType stream_type,
                                 const content::MediaRequestState state) {}

    // Handle an information update that a new stream is being created.
    virtual void OnCreatingAudioStream(int render_process_id,
                                       int render_frame_id) {}
  };

  // Returns the instance, or null if the embedder has not created it yet.
  static MediaCaptureDispatcher* GetInstance();

  MediaCaptureDispatcher(const MediaCaptureDispatcher&) = delete;
  MediaCaptureDispatcher& operator=(const MediaCaptureDispatcher&) = delete;

  // Adds or removes an observer. Called on the UI thread. Adding an observer
  // that is already added does nothing.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Decides whether `web_contents` may capture the camera, microphone or
  // screen as asked in `request`, and runs `callback` with the granted
  // devices or an error. Depending on the request, this decides by policy,
  // shows a permission prompt, or shows a screen picker. `callback` may run
  // before this method returns. Not for requests from extensions.
  virtual void ProcessMediaAccessRequest(
      content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) = 0;

  // Returns whether `render_frame_host` may capture media of `type` for
  // `security_origin` without asking the user, because the permission is
  // already granted or policy allows it. Not for extensions.
  virtual bool CheckMediaAccessPermission(
      content::RenderFrameHost* render_frame_host,
      const url::Origin& security_origin,
      blink::mojom::MediaStreamType type) = 0;

 protected:
  MediaCaptureDispatcher();
  virtual ~MediaCaptureDispatcher();

  // For the embedder to notify the observers.
  const base::ObserverList<Observer>::Unchecked& observers() const {
    return observers_;
  }

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER_H_
