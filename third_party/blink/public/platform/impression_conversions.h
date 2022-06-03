// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_PLATFORM_IMPRESSION_CONVERSIONS_H_
#define THIRD_PARTY_BLINK_PUBLIC_PLATFORM_IMPRESSION_CONVERSIONS_H_

#include "third_party/blink/public/common/navigation/impression.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_impression.h"

namespace blink {

BLINK_PLATFORM_EXPORT blink::Impression ConvertWebImpressionToImpression(
    const blink::WebImpression& web_impression);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_PLATFORM_IMPRESSION_CONVERSIONS_H_
