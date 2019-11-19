// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_
#define SERVICES_DEVICE_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/public/mojom/screen_orientation.mojom.h"

namespace device {

class ScreenOrientationListenerAndroid
    : public mojom::ScreenOrientationListener {
 public:
  static void Create(
      mojo::PendingReceiver<mojom::ScreenOrientationListener> receiver);

  ~ScreenOrientationListenerAndroid() override;

 private:
  ScreenOrientationListenerAndroid();

  // mojom::ScreenOrientationListener:
  void IsAutoRotateEnabledByUser(
      IsAutoRotateEnabledByUserCallback callback) override;

  DISALLOW_COPY_AND_ASSIGN(ScreenOrientationListenerAndroid);
};

}  // namespace device

#endif  // SERVICES_DEVICE_SCREEN_ORIENTATION_SCREEN_ORIENTATION_LISTENER_ANDROID_H_
