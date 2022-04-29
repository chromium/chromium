// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_CONDITIONAL_UI_DELEGATE_ANDROID_H_
#define CONTENT_PUBLIC_BROWSER_CONDITIONAL_UI_DELEGATE_ANDROID_H_

#include <vector>

#include "base/callback_forward.h"

namespace device {
class DiscoverableCredentialMetadata;
}

namespace content {

// Interface for providing the embedder with a list of Web Authentication
// credentials for use in Conditional UI.
class ConditionalUiDelegateAndroid {
 public:
  virtual ~ConditionalUiDelegateAndroid() = default;

  // Called when a Web Authentication Conditional UI request is received. This
  // provides the callback that will complete the request if and when a user
  // selects a credential from a form autofill dialog.
  virtual void OnWebAuthnRequestPending(
      const std::vector<device::DiscoverableCredentialMetadata>& credentials,
      base::OnceCallback<void(const std::vector<uint8_t>& id)> callback) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_CONDITIONAL_UI_DELEGATE_ANDROID_H_
