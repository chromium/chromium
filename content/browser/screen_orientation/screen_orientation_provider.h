// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
#define CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_binding_set.h"
#include "content/public/browser/web_contents_observer.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "services/device/public/mojom/screen_orientation_lock_types.mojom.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"

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

  ~ScreenOrientationProvider() override;

  // device::mojom::ScreenOrientation:
  void LockOrientation(blink::WebScreenOrientationLockType orientation,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

  // Inform about a screen orientation update. It is called to let the provider
  // know if a lock has been resolved.
  void OnOrientationChange();

  // Provide a delegate which creates delegates for platform implementations.
  // The delegate is not owned by ScreenOrientationProvider.
  static void SetDelegate(ScreenOrientationDelegate* delegate);
  static ScreenOrientationDelegate* GetDelegateForTesting();

  // WebContentsObserver
  void DidToggleFullscreenModeForTab(bool entered_fullscreen,
                                     bool will_cause_resize) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;

 private:
  // Calls on |on_result_callback_| with |result|, followed by resetting
  // |on_result_callback_| and |pending_lock_orientation_|.
  void NotifyLockResult(device::mojom::ScreenOrientationLockResult result);

  // Returns the lock type that should be associated with 'natural' lock.
  // Returns WebScreenOrientationLockDefault if the natural lock type can't be
  // found.
  blink::WebScreenOrientationLockType GetNaturalLockType() const;

  // Whether the passed |lock| matches the current orientation. In other words,
  // whether the orientation will need to change to match the |lock|.
  bool LockMatchesCurrentOrientation(blink::WebScreenOrientationLockType lock);

  // Not owned, responsible for platform implementations.
  static ScreenOrientationDelegate* delegate_;

  // Whether the ScreenOrientationProvider currently has a lock applied.
  bool lock_applied_;

  // Lock that require orientation changes are not completed until
  // OnOrientationChange.
  base::Optional<blink::WebScreenOrientationLockType> pending_lock_orientation_;

  LockOrientationCallback pending_callback_;

  WebContentsFrameBindingSet<device::mojom::ScreenOrientation> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationProvider);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SCREEN_ORIENTATION_SCREEN_ORIENTATION_PROVIDER_H_
