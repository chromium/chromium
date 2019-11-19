// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/device/public/mojom/screen_orientation.mojom-blink.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/screen_orientation_controller.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/screen_orientation/web_lock_orientation_callback.h"

namespace blink {

class ScreenOrientation;

using device::mojom::blink::ScreenOrientationLockResult;

class MODULES_EXPORT ScreenOrientationControllerImpl final
    : public ScreenOrientationController,
      public ContextLifecycleObserver,
      public PageVisibilityObserver {
  USING_GARBAGE_COLLECTED_MIXIN(ScreenOrientationControllerImpl);

 public:
  explicit ScreenOrientationControllerImpl(LocalFrame&);
  ~ScreenOrientationControllerImpl() override;

  void SetOrientation(ScreenOrientation*);
  void NotifyOrientationChanged() override;

  // Implementation of ScreenOrientationController.
  void lock(WebScreenOrientationLockType,
            std::unique_ptr<WebLockOrientationCallback>) override;
  void unlock() override;
  bool MaybeHasActiveLock() const override;

  static void ProvideTo(LocalFrame&);
  static ScreenOrientationControllerImpl* From(LocalFrame&);

  void SetScreenOrientationAssociatedRemoteForTests(
      mojo::AssociatedRemote<device::mojom::blink::ScreenOrientation>);

  void Trace(blink::Visitor*) override;

 private:
  friend class MediaControlsOrientationLockAndRotateToFullscreenDelegateTest;
  friend class ScreenOrientationControllerImplTest;

  static WebScreenOrientationType ComputeOrientation(const IntRect&, uint16_t);

  // Inherited from ContextLifecycleObserver and PageVisibilityObserver.
  void ContextDestroyed(ExecutionContext*) override;
  void PageVisibilityChanged() override;

  void UpdateOrientation();

  bool IsActive() const;
  bool IsVisible() const;
  bool IsActiveAndVisible() const;

  void OnLockOrientationResult(int, ScreenOrientationLockResult);
  void CancelPendingLocks();
  int GetRequestIdForTests();

  Member<ScreenOrientation> orientation_;
  bool active_lock_ = false;
  mojo::AssociatedRemote<device::mojom::blink::ScreenOrientation>
      screen_orientation_service_;
  std::unique_ptr<WebLockOrientationCallback> pending_callback_;
  int request_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationControllerImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_SCREEN_ORIENTATION_SCREEN_ORIENTATION_CONTROLLER_IMPL_H_
