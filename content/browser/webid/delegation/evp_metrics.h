// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_
#define CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/webid/email_verification_request.mojom-forward.h"

namespace content::webid {

CONTENT_EXPORT void RecordEvpRequestStatus(
    blink::mojom::EmailVerificationRequestResult status);

}  // namespace content::webid

#endif  // CONTENT_BROWSER_WEBID_DELEGATION_EVP_METRICS_H_
