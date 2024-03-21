// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/secure_payment_confirmation_utils.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"

namespace content {

bool IsFrameAllowedToUseSecurePaymentConfirmation(RenderFrameHost* rfh) {
  return rfh && rfh->IsActive() &&
         (rfh->IsFeatureEnabled(
              blink::mojom::PermissionsPolicyFeature::kPayment) ||
          rfh->IsFeatureEnabled(blink::mojom::PermissionsPolicyFeature::
                                    kPublicKeyCredentialsCreate)) &&
         base::FeatureList::IsEnabled(::features::kSecurePaymentConfirmation);
}

}  // namespace content
