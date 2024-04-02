// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_MOJO_VIDEO_CAPTURER_LIST_H_
#define REMOTING_HOST_MOJO_VIDEO_CAPTURER_LIST_H_

#include <map>
#include <memory>

#include "base/memory/scoped_refptr.h"
#include "remoting/host/mojom/desktop_session.mojom.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"

namespace webrtc {
class DesktopVector;
class MouseCursor;
}  // namespace webrtc

namespace remoting {

class AutoThreadTaskRunner;
class DesktopEnvironment;
class MojoVideoCapturer;

// This manages a list of MojoVideoCapturer instances, up to one per screen. It
// provides methods for setting properties of all managed capturers. It also
// automatically deletes any capturers whose Mojo endpoints become
// disconnected.
class MojoVideoCapturerList {
 public:
  MojoVideoCapturerList();
  MojoVideoCapturerList(const MojoVideoCapturerList&) = delete;
  MojoVideoCapturerList& operator=(const MojoVideoCapturerList&) = delete;
  ~MojoVideoCapturerList();

  // Creates and starts a new video-capturer for a display. Any capturer
  // previously created for the screen_id will be deleted. This returns the new
  // Mojo endpoints for communicating with the capturer.
  mojom::CreateVideoCapturerResultPtr CreateVideoCapturer(
      webrtc::ScreenId screen_id,
      DesktopEnvironment* environment,
      scoped_refptr<AutoThreadTaskRunner> caller_task_runner);

  // Returns true if there are no video-capturers in the list.
  bool IsEmpty() const;

  // Deletes and removes all capturers from the list.
  void Clear();

  // Sets the mouse-cursor on all capturers. This creates internal copies and
  // does not take ownership.
  void SetMouseCursor(const webrtc::MouseCursor& cursor);

  // Sets the mouse-cursor position on all capturers.
  void SetMouseCursorPosition(const webrtc::DesktopVector& position);

  // Sets compose-enabled on all capturers.
  void SetComposeEnabled(bool enabled);

 private:
  std::map<webrtc::ScreenId, std::unique_ptr<MojoVideoCapturer>>
      video_capturers_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_MOJO_VIDEO_CAPTURER_LIST_H_
