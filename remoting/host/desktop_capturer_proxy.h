// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
#define REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "remoting/host/desktop_display_info.h"
#include "remoting/host/desktop_display_info_loader.h"
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
// GetSourceList() is not implemented by this class, it always returns false.
// This class also loads the list of desktop displays on the UI thread, and
// notifies the ClientSessionControl if the displays have changed.
class DesktopCapturerProxy : public webrtc::DesktopCapturer {
 public:
  explicit DesktopCapturerProxy(
      scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      base::WeakPtr<ClientSessionControl> client_session_control);

  DesktopCapturerProxy(const DesktopCapturerProxy&) = delete;
  DesktopCapturerProxy& operator=(const DesktopCapturerProxy&) = delete;

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
  void OnDisplayInfoLoaded(DesktopDisplayInfo info);

  base::ThreadChecker thread_checker_;

  std::unique_ptr<Core> core_;
  scoped_refptr<base::SingleThreadTaskRunner> capture_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;

  raw_ptr<webrtc::DesktopCapturer::Callback> callback_;

  // Used to disconnect the client session.
  // Note: This cannot be used on Windows because the ClientSession is not in
  // the same process as the DesktopCapturerProxy.
  base::WeakPtr<ClientSessionControl> client_session_control_;

  // Contains the most recently gathered info about the desktop displays.
  DesktopDisplayInfo desktop_display_info_;

  // Created on the calling thread, but accessed and destroyed on the UI thread.
  std::unique_ptr<DesktopDisplayInfoLoader> desktop_display_info_loader_;

  base::WeakPtrFactory<DesktopCapturerProxy> weak_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_CAPTURER_PROXY_H_
