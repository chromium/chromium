// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_H_
#define CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_H_

#include <memory>
#include "base/unguessable_token.h"
#include "content/browser/media/active_media_session_controller.h"
#include "content/browser/media/system_media_controls_notifier.h"

namespace system_media_controls {
class SystemMediaControls;
}  // namespace system_media_controls

namespace content {

// WebAppSystemMediaControls is intended as a data storage class. It holds:
//  - A requestID
//  - SystemMediaControls
//  - SystemMediaControlsNotifier
//  - ActiveMediaSessionController
//
// This class is typically owned by a WebAppSystemMediaControlsManager. See
// web_app_system_media_controls_manager.h for more detailed documentation.
//
// This class is not to be confused with SystemMediaControls. This is a wrapper
// around a SystemMediaControls and other classes - not a derived class of
// SystemMediaControls.
class CONTENT_EXPORT WebAppSystemMediaControls {
 public:
  WebAppSystemMediaControls(
      base::UnguessableToken request_id,
      std::unique_ptr<system_media_controls::SystemMediaControls>
          system_media_controls,
      std::unique_ptr<SystemMediaControlsNotifier> notifier,
      std::unique_ptr<ActiveMediaSessionController> controller);

  // For tests only.
  WebAppSystemMediaControls();

  WebAppSystemMediaControls(const WebAppSystemMediaControls&) = delete;
  WebAppSystemMediaControls& operator=(const WebAppSystemMediaControls&) =
      delete;

  ~WebAppSystemMediaControls();

  // Getter functions:
  base::UnguessableToken GetRequestID() const { return request_id_; }

  // While the following functions can potentially return nullptr, they
  // typically should not due to the constructor expecting 3 unique_ptrs.
  system_media_controls::SystemMediaControls* GetSystemMediaControls() const;

  SystemMediaControlsNotifier* GetNotifier() const;
  void SetNotifier(std::unique_ptr<SystemMediaControlsNotifier> notifier) {
    notifier_ = std::move(notifier);
  }

  ActiveMediaSessionController* GetController() const;
  void SetController(std::unique_ptr<ActiveMediaSessionController> controller) {
    controller_ = std::move(controller);
  }

 private:
  base::UnguessableToken request_id_;
  std::unique_ptr<system_media_controls::SystemMediaControls>
      system_media_controls_;
  std::unique_ptr<SystemMediaControlsNotifier> notifier_;
  std::unique_ptr<ActiveMediaSessionController> controller_;

  friend class WebAppSystemMediaControlsManagerTest;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_WEB_APP_SYSTEM_MEDIA_CONTROLS_H_
