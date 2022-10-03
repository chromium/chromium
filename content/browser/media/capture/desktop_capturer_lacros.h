// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_
#define CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "chromeos/crosapi/mojom/screen_manager.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_options.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capturer.h"
#include "ui/aura/env_observer.h"
#include "ui/gfx/native_widget_types.h"

namespace content {

// This class is responsible for communicating with ash-chrome to get snapshots
// of the desktop.
//
// NOTE: Instances of this class may be allocated and configured on one affine
// sequence and then transferred to another affine sequence (e.g., a worker
// thread) where |Start| gets called. Subsequent methods are allowed to do
// blocking I/O or other expensive operations. The instance, when no longer
// needed, is deleted on the same affine sequence on which |Start| was called.
class DesktopCapturerLacros : public webrtc::DesktopCapturer,
                              public aura::EnvObserver {
 public:
  enum CaptureType { kScreen, kWindow };
  DesktopCapturerLacros(CaptureType capture_type,
                        const webrtc::DesktopCaptureOptions& options);
  DesktopCapturerLacros(const DesktopCapturerLacros&) = delete;
  DesktopCapturerLacros& operator=(const DesktopCapturerLacros&) = delete;
  ~DesktopCapturerLacros() override;

  // DesktopCapturer:
  void Start(Callback* callback) override;
  void CaptureFrame() override;
  bool GetSourceList(SourceList* sources) override;
  bool SelectSource(SourceId id) override;
  bool FocusOnSelectedSource() override;
  bool IsOccluded(const webrtc::DesktopVector& pos) override;
  void SetSharedMemoryFactory(std::unique_ptr<webrtc::SharedMemoryFactory>
                                  shared_memory_factory) override;
  void SetExcludedWindow(webrtc::WindowId window) override;

 private:
  // Callback for when ash-chrome returns a snapshot of the screen or window as
  // a bitmap.
  void DidTakeSnapshot(bool success, const SkBitmap& snapshot);

  void InitializeWidgetMap();

  // EnvObserver and Window Observer overrides. Note that these will *not* be
  // called on the affine sequence.
  void OnHostInitialized(aura::WindowTreeHost* host) override;
  void OnHostDestroyed(aura::WindowTreeHost* host) override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Whether this object is capturing screens or windows.
  const CaptureType capture_type_;

  // TODO(https://crbug.com/1094460): The webrtc options for screen/display
  // capture are currently ignored.
  const webrtc::DesktopCaptureOptions options_;

  // For window capture, this is the source that we want to capture.
  SourceId selected_source_;

  // The webrtc::DesktopCapturer interface expects the implementation to hold
  // onto and call a Callback* object. This instance relies on the assumption
  // that Callback* will outlive this instance.
  //
  // The current media capture implementation expects that the implementation of
  // CaptureFrame() synchronously invokes |callback_| in a re-entrant fashion.
  // Thus, we do not worry about thread safety when invoking callback_.
  raw_ptr<Callback> callback_ = nullptr;

  // A remote for an ash interface that is responsible for either capturing
  // screen snapshots or window snapshots.
  mojo::Remote<crosapi::mojom::SnapshotCapturer> snapshot_capturer_;

  // A lock and map to map the unique window ID to a corresponding accelerated
  // widget. This helps speed up our lookup of Aura windows in GetSourceList,
  // because we're provided the unique window ID by ash, and need to pass along
  // the Accelerated Widget if it's found.
  base::Lock widget_map_lock_;
  std::map<std::string, gfx::AcceleratedWidget> widget_map_
      GUARDED_BY(widget_map_lock_);

  // Helper bool to cache the state of the `kLacrosAuraCapture` feature flag,
  // since the state is the same unless Chrome is restarted. This is mostly to
  // help improve readability, since the feature lookup is fairly cheap.
  const bool is_aura_capture_enabled_;

#if DCHECK_IS_ON()
  bool capturing_frame_ = false;
#endif

  base::WeakPtrFactory<DesktopCapturerLacros> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEDIA_CAPTURE_DESKTOP_CAPTURER_LACROS_H_
