// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/supported_delegations.h"

namespace content {

SupportedDelegations::SupportedDelegations() = default;

SupportedDelegations::~SupportedDelegations() = default;

bool SupportedDelegations::ProvidesAll(
    const payments::mojom::PaymentOptionsPtr& payment_options) const {
  if (!payment_options)
    return true;
  if (payment_options->request_shipping && !shipping_address)
    return false;
  if (payment_options->request_payer_name && !payer_name)
    return false;
  if (payment_options->request_payer_phone && !payer_phone)
    return false;
  if (payment_options->request_payer_email && !payer_email)
    return false;
  return true;
}

}  // namespace content
