// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/color_page_popup_controller.h"

#include "third_party/blink/renderer/core/html/forms/color_chooser_popup_ui_controller.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/page/page_popup_client.h"
#include "third_party/blink/renderer/core/page/page_popup_controller.h"

namespace blink {

ColorPagePopupController::ColorPagePopupController(
    Page& page,
    PagePopup& popup,
    ColorChooserPopupUIController* client)
    : PagePopupController(page, popup, client) {}

void ColorPagePopupController::openEyeDropper() {
  if (popup_client_) {
    static_cast<ColorChooserPopupUIController*>(popup_client_)
        ->OpenEyeDropper();
  }
}

}  // namespace blink
