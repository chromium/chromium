// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_ui_controller_interface_binder.h"

#include "content/browser/bad_message.h"

namespace content::internal {

void ReceivedInvalidWebUIControllerMessage(RenderFrameHost* rfh) {
  ReceivedBadMessage(
      rfh->GetProcess(),
      bad_message::BadMessageReason::RFH_INVALID_WEB_UI_CONTROLLER);
}

}  // namespace content::internal
