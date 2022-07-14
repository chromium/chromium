// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/test/mock_identity_request_dialog_controller.h"

#include "url/gurl.h"

namespace content {

MockIdentityRequestDialogController::MockIdentityRequestDialogController() =
    default;

MockIdentityRequestDialogController::~MockIdentityRequestDialogController() {
  DestructorCalled();
}

}  // namespace content
