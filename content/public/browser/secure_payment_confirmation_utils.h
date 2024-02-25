// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SECURE_PAYMENT_CONFIRMATION_UTILS_H_
#define CONTENT_PUBLIC_BROWSER_SECURE_PAYMENT_CONFIRMATION_UTILS_H_

#include "content/common/content_export.h"

namespace content {

class RenderFrameHost;

// Returns whether or not a given RenderFrameHost is able to use Secure Payment
// Confirmation (that the feature is enabled, that the frame has the necessary
// permissions policy, etc).
CONTENT_EXPORT bool IsFrameAllowedToUseSecurePaymentConfirmation(
    RenderFrameHost* rfh);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SECURE_PAYMENT_CONFIRMATION_UTILS_H_
