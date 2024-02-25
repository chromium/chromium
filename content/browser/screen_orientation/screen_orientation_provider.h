// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_

#include <optional>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_frame_host_receiver_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "ui/display/mojom/screen_orientation.mojom.h"

namespace content {

class ScreenOrientationDelegate;
class WebContents;

// Handles screen orientation lock/unlock. Platforms which wish to provide
// custom implementations can provide a factory for ScreenOrientationDelegate.
class CONTENT_EXPORT ScreenOrientationProvider
    : public device::mojom::ScreenOrientation,
      public WebContentsObserver {
 public:
  ScreenOrientationProvider(WebContents* web_contents);

  ScreenOrientationProvider(const ScreenOrientationProvider&) = delete;
  ScreenOrientationProvider& operator=(const ScreenOrientationProvider&) =
      delete;

  ~ScreenOrientationProvider() override;

  void BindScreenOrientation(
      RenderFrameHost* rfh,
      mojo::PendingAssociatedReceiver<device::mojom::ScreenOrientation>
          receiver);

  // device::mojom::ScreenOrientation:
  void LockOrientation(device::mojom::ScreenOrientationLockType,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

  // Inform about a screen orientation update. It is called to let the provider
  // know if a lock has been resolved.
  void OnOrientationChange();

  // Provide a delegate which creates delegates for platform implementations.
  // The delegate is not owned by ScreenOrientationProvider.
  static void SetDelegate(ScreenOrientationDelegate* delegate);
  static ScreenOrientationDelegate* GetDelegateForTesting();
  static bool LockMatchesOrientation(
      device::mojom::ScreenOrientationLockType lock,
      display::mojom::ScreenOrientation orientation);

  // WebContentsObserver
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void PrimaryPageChanged(Page& page) override;

 private:
  // Calls on |on_result_callback_| with |result|, followed by resetting
  // |on_result_callback_| and |pending_lock_orientation_|.
  void NotifyLockResult(device::mojom::ScreenOrientationLockResult result);

  // Returns the lock type that should be associated with 'natural' lock.
  // Returns device::mojom::ScreenOrientationLockType if the natural lock type
  // can't be found.
  device::mojom::ScreenOrientationLockType GetNaturalLockType() const;

  // Whether the passed |lock| matches the current orientation. In other words,
  // whether the orientation will need to change to match the |lock|.
  bool LockMatchesCurrentOrientation(
      device::mojom::ScreenOrientationLockType lock);

  // Not owned, responsible for platform implementations.
  static ScreenOrientationDelegate* delegate_;

  // Whether the ScreenOrientationProvider currently has a lock applied.
  bool lock_applied_;

  // Lock that require orientation changes are not completed until
  // OnOrientationChange.
  std::optional<device::mojom::ScreenOrientationLockType>
      pending_lock_orientation_;

  LockOrientationCallback pending_callback_;

  RenderFrameHostReceiverSet<device::mojom::ScreenOrientation> receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
