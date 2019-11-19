// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/common/content_export.h"
#include "services/device/public/mojom/hid.mojom-forward.h"

namespace content {

// Token representing an open HID device chooser prompt. Destroying this
// object should cancel the prompt.
class CONTENT_EXPORT HidChooser {
 public:
  // Callback type used to report the user action. Passed |nullptr| if no device
  // was selected.
  using Callback = base::OnceCallback<void(device::mojom::HidDeviceInfoPtr)>;

  HidChooser() = default;
  virtual ~HidChooser() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(HidChooser);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_
