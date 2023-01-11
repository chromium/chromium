// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "services/device/public/mojom/hid.mojom-forward.h"

namespace content {

// Token representing an open HID device chooser prompt. Destroying this
// object should cancel the prompt.
class CONTENT_EXPORT HidChooser {
 public:
  // Callback type used to report the user action. An empty vector is passed if
  // no device was selected.
  using Callback =
      base::OnceCallback<void(std::vector<device::mojom::HidDeviceInfoPtr>)>;

  HidChooser() = default;

  HidChooser(const HidChooser&) = delete;
  HidChooser& operator=(const HidChooser&) = delete;

  virtual ~HidChooser() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HID_CHOOSER_H_
