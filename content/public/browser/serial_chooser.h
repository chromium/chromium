// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERIAL_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_SERIAL_CHOOSER_H_

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "services/device/public/mojom/serial.mojom-forward.h"

namespace content {

// Token representing an open serial port chooser prompt. Destroying this
// object should cancel the prompt.
class CONTENT_EXPORT SerialChooser {
 public:
  // Callback type used to report the user action. Passed |nullptr| if no port
  // was selected.
  using Callback = base::OnceCallback<void(device::mojom::SerialPortInfoPtr)>;

  SerialChooser();

  SerialChooser(const SerialChooser&) = delete;
  SerialChooser& operator=(const SerialChooser&) = delete;

  virtual ~SerialChooser();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERIAL_CHOOSER_H_
