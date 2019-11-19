// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/desktop_display_info.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace webrtc {
class DesktopCaptureOptions;
}  // namespace webrtc

namespace remoting {

class ClientSessionControl;

// DesktopCapturerProxy is responsible for calling webrtc::DesktopCapturer on
// the capturer thread and then returning results to the caller's thread.
// GetSourceList() and SelectSource() functions are not implemented by this
// class, they always return false.
class DesktopCapturerProxy : public webrtc::DesktopCapturer {
 public:
  explicit DesktopCapturerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);
  ~DesktopCapturerProxy() override;

  // CreateCapturer() should be used if the capturer needs to be created on the
  // capturer thread. Alternatively the capturer can be passed to
  // set_capturer().
  void CreateCapturer(const webrtc::DesktopCaptureOptions& options);
  void set_capturer(std::unique_ptr<webrtc::DesktopCapturer> capturer);

  // webrtc::DesktopCapturer interface.
  void Start(Callback* callback) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;

 private:
  class Core;

  void OnFrameCaptured(webrtc::DesktopCapturer::Result result,
                       std::unique_ptr<webrtc::DesktopFrame> frame);

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  webrtc::DesktopCapturer::Callback* callback_;

  // Used to disconnect the client session.
  // Note: This cannot be used on Windows because the ClientSession is not in
  // the same process as the DesktopCapturerProxy.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Contains the most recently gathered info about the desktop displays.
  std::unique_ptr<DesktopDisplayInfo> desktop_display_info_;

  base::WeakPtrFactory<DesktopCapturerProxy> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DesktopCapturerProxy);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
