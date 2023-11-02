// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/federated_identity_api_permission_context_delegate.h"

namespace content {

bool FederatedIdentityApiPermissionContextDelegate::
    ShouldCompleteRequestImmediately() const {
  return false;
}

}  // namespace content
