// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USB_CHOOSER_H_
#define CONTENT_PUBLIC_BROWSER_USB_CHOOSER_H_

#include "content/common/content_export.h"

namespace content {

// Token representing an open USB port chooser prompt. Destroying this object
// should cancel the prompt.
class CONTENT_EXPORT UsbChooser {
 public:
  UsbChooser();
  UsbChooser(const UsbChooser&) = delete;
  UsbChooser& operator=(const UsbChooser&) = delete;
  virtual ~UsbChooser();
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USB_CHOOSER_H_
