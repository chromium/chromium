// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ad_auction/ads.h"

namespace blink {

Ads::Ads() = default;

Ads::~Ads() = default;

bool Ads::IsValid() const {
  return populated_;
}

WTF::String Ads::GetGuid() const {
  DCHECK(populated_);
  return guid_;
}
}  // namespace blink
