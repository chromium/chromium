// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/browser/media/capture/capture_util.h"
#include "content/browser/media/capture/capture_util_mac.h"
#include "content/browser/media/capture/pip_screen_capture_coordinator_proxy.h"

namespace content {

class WebContents;

class CONTENT_EXPORT PipScreenCaptureCoordinatorImpl {
 public:
  static PipScreenCaptureCoordinatorImpl* GetInstance();

  static void AddCapture(
      PipScreenCaptureCoordinatorProxy::CaptureInfo capture_info);
  static void RemoveCapture(const base::UnguessableToken& session_id);

  class Observer : public base::CheckedObserver {
   public:
    // Called when the state of the coordinator changes.
    virtual void OnStateChanged(
        std::optional<NativeWindowId> new_pip_window_id,
        const GlobalRenderFrameHostId& new_pip_owner_render_frame_host_id,
        const std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo>&
            captures) = 0;
  };

  ~PipScreenCaptureCoordinatorImpl();

  PipScreenCaptureCoordinatorImpl(const PipScreenCaptureCoordinatorImpl&) =
      delete;
  PipScreenCaptureCoordinatorImpl& operator=(
      const PipScreenCaptureCoordinatorImpl&) = delete;

  void OnPipShown(
      WebContents& pip_web_contents,
      const GlobalRenderFrameHostId& pip_owner_render_frame_host_id);
  void OnPipShown(
      NativeWindowId pip_window_id,
      const GlobalRenderFrameHostId& pip_owner_render_frame_host_id);
  void OnPipClosed();

  std::optional<NativeWindowId> PipWindowId() const;
  GlobalRenderFrameHostId GetPipOwnerRenderFrameHostId() const;
  std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> Captures() const;

  std::unique_ptr<PipScreenCaptureCoordinatorProxy> CreateProxy();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void ResetForTesting();

 private:
  void AddCaptureOnUIThread(
      PipScreenCaptureCoordinatorProxy::CaptureInfo capture_info);
  void RemoveCaptureOnUIThread(const base::UnguessableToken& session_id);
  void NotifyStateChanged();
  friend class base::NoDestructor<PipScreenCaptureCoordinatorImpl>;
  PipScreenCaptureCoordinatorImpl();

  std::optional<NativeWindowId> pip_window_id_;
  GlobalRenderFrameHostId pip_owner_render_frame_host_id_;
  base::ObserverList<Observer> observers_;
  std::vector<PipScreenCaptureCoordinatorProxy::CaptureInfo> captures_;
  base::WeakPtrFactory<PipScreenCaptureCoordinatorImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_PIP_SCREEN_CAPTURE_COORDINATOR_IMPL_H_
