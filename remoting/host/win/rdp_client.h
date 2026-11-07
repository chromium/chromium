// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_RDP_CLIENT_H_
#define REMOTING_HOST_WIN_RDP_CLIENT_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopSize;
}  // namespace webrtc

namespace remoting {

class ScreenResolution;

// Scales `width` and `height` down proportionally to fit within the 4K RDP
// limit (3840x2160 in landscape or 2160x3840 in portrait), clamps to the
// minimum supported RDP size (800x600), and aligns to even pixel boundaries.
webrtc::DesktopSize GetBoundedRdpDesktopSize(int width, int height);

// Bounds `resolution`'s dimensions via `GetBoundedRdpDesktopSize()` and, when
// dimensions exceed the 4K RDP limit, scales DPI down by the same factor (with
// a 96-DPI floor) so the effective logical desktop size is preserved.
ScreenResolution GetBoundedRdpResolution(const ScreenResolution& resolution);

// Establishes a loopback RDP connection to spawn a new Windows session.
class RdpClient {
 public:
  class EventHandler {
   public:
    virtual ~EventHandler() {}

    // Notifies the event handler that an RDP connection has been established
    // successfully.
    virtual void OnRdpConnected() = 0;

    // Notifies that the RDP connection has been closed.
    virtual void OnRdpClosed() = 0;
  };

  RdpClient(scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner,
            scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
            const ScreenResolution& resolution,
            const std::string& terminal_id,
            DWORD port_number,
            EventHandler* event_handler);

  RdpClient(const RdpClient&) = delete;
  RdpClient& operator=(const RdpClient&) = delete;

  virtual ~RdpClient();

  // Sends Secure Attention Sequence to the session.
  void InjectSas();

  // Change the resolution of the desktop.
  void ChangeResolution(const ScreenResolution& resolution);

 private:
  // The actual implementation resides in Core class.
  class Core;
  scoped_refptr<Core> core_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_RDP_CLIENT_H_
