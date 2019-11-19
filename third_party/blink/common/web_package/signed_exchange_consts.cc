// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/web_package/signed_exchange_consts.h"

namespace blink {

const char kSignedExchangeMimeType[] = "application/signed-exchange;v=b3";

// Currently we are using "-04" suffix in case Variants spec changes.
// https://httpwg.org/http-extensions/draft-ietf-httpbis-variants.html#variants
const char kSignedExchangeVariantsHeader[] = "variants-04";
const char kSignedExchangeVariantKeyHeader[] = "variant-key-04";

}  // namespace blink
