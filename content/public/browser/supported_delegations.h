// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SUPPORTED_DELEGATIONS_H_
#define CONTENT_PUBLIC_BROWSER_SUPPORTED_DELEGATIONS_H_

#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace content {

// This class represents the supported delegations of the StoredPaymentApp.
struct CONTENT_EXPORT SupportedDelegations {
  SupportedDelegations();
  ~SupportedDelegations();

  bool shipping_address = false;
  bool payer_name = false;
  bool payer_phone = false;
  bool payer_email = false;

  bool ProvidesAll(
      const payments::mojom::PaymentOptionsPtr& payment_options) const;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SUPPORTED_DELEGATIONS_H_
