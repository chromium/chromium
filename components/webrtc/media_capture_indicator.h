// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBRTC_MEDIA_CAPTURE_INDICATOR_H_
#define COMPONENTS_WEBRTC_MEDIA_CAPTURE_INDICATOR_H_

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace webrtc {

// Lets code outside //chrome ask which WebContents are capturing media and
// observe when that changes. The embedder implements it, and
// MediaCaptureDispatcher::GetCaptureIndicator() returns the instance.
class COMPONENT_EXPORT(COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER)
    MediaCaptureIndicator {
 public:
  class COMPONENT_EXPORT(COMPONENTS_WEBRTC_MEDIA_CAPTURE_DISPATCHER) Observer
      : public base::CheckedObserver {
   public:
    virtual void OnIsCapturingVideoChanged(content::WebContents* web_contents,
                                           bool is_capturing_video) {}
    virtual void OnIsCapturingAudioChanged(content::WebContents* web_contents,
                                           bool is_capturing_audio) {}
    virtual void OnIsBeingMirroredChanged(content::WebContents* web_contents,
                                          bool is_being_mirrored) {}
    virtual void OnIsCapturingTabChanged(content::WebContents* web_contents,
                                         bool is_capturing_tab) {}
    virtual void OnIsCapturingWindowChanged(content::WebContents* web_contents,
                                            bool is_capturing_window) {}
    virtual void OnIsCapturingDisplayChanged(content::WebContents* web_contents,
                                             bool is_capturing_display) {}

   protected:
    ~Observer() override;
  };

  MediaCaptureIndicator(const MediaCaptureIndicator&) = delete;
  MediaCaptureIndicator& operator=(const MediaCaptureIndicator&) = delete;

  // Adds or removes an observer. Observers need to be removed during the
  // lifetime of this object.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns true if `web_contents` or a WebContents inside it is capturing
  // video (e.g., webcam).
  virtual bool IsCapturingVideo(content::WebContents* web_contents) const = 0;

  // Returns true if `web_contents` or a WebContents inside it is capturing
  // audio (e.g., microphone).
  virtual bool IsCapturingAudio(content::WebContents* web_contents) const = 0;

 protected:
  MediaCaptureIndicator();
  ~MediaCaptureIndicator();

  // For the embedder to notify the observers.
  const base::ObserverList<Observer, /*check_empty=*/true>& observers() const {
    return observers_;
  }

 private:
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace webrtc

#endif  // COMPONENTS_WEBRTC_MEDIA_CAPTURE_INDICATOR_H_
