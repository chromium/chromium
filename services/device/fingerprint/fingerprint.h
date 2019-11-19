// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_H_
#define SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/device/fingerprint/fingerprint_export.h"
#include "services/device/public/mojom/fingerprint.mojom.h"

namespace device {

class Fingerprint {
 public:
  // This function is implemented in platform-specific subclasses.
  SERVICES_DEVICE_FINGERPRINT_EXPORT static void Create(
      mojo::PendingReceiver<device::mojom::Fingerprint> receiver);
};

}  // namespace device

#endif  // SERVICES_DEVICE_FINGERPRINT_FINGERPRINT_H_
