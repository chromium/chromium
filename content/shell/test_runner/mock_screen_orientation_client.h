// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_
#define CONTENT_SHELL_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_lock_type.h"
#include "third_party/blink/public/common/screen_orientation/web_screen_orientation_type.h"

namespace blink {
class WebLocalFrame;
}

namespace test_runner {

class TEST_RUNNER_EXPORT MockScreenOrientationClient
    : public device::mojom::ScreenOrientation {
 public:
  explicit MockScreenOrientationClient();
  ~MockScreenOrientationClient() override;

  void ResetData();
  void UpdateDeviceOrientation(blink::WebLocalFrame* main_frame,
                               blink::WebScreenOrientationType orientation);

  blink::WebScreenOrientationType CurrentOrientationType() const;
  unsigned CurrentOrientationAngle() const;
  bool IsDisabled() const { return is_disabled_; }
  void SetDisabled(bool disabled);

  void AddReceiver(mojo::ScopedInterfaceEndpointHandle handle);
  void OverrideAssociatedInterfaceProviderForFrame(blink::WebLocalFrame* frame);

  // device::mojom::ScreenOrientation implementation.
  void LockOrientation(blink::WebScreenOrientationLockType orientation,
                       LockOrientationCallback callback) override;
  void UnlockOrientation() override;

 private:
  void UpdateLockSync(blink::WebScreenOrientationLockType,
                      LockOrientationCallback callback);
  void ResetLockSync();

  void UpdateScreenOrientation(blink::WebScreenOrientationType);
  bool IsOrientationAllowedByCurrentLock(blink::WebScreenOrientationType);
  blink::WebScreenOrientationType SuitableOrientationForCurrentLock();
  static unsigned OrientationTypeToAngle(blink::WebScreenOrientationType);

  blink::WebLocalFrame* main_frame_;
  blink::WebScreenOrientationLockType current_lock_;
  blink::WebScreenOrientationType device_orientation_;
  blink::WebScreenOrientationType current_orientation_;
  bool is_disabled_;
  mojo::AssociatedReceiverSet<device::mojom::ScreenOrientation> receivers_;

  DISALLOW_COPY_AND_ASSIGN(MockScreenOrientationClient);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_MOCK_SCREEN_ORIENTATION_CLIENT_H_
