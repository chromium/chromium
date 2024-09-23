// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/timing/soft_navigation_context.h"

namespace blink {

SoftNavigationContext::SoftNavigationContext() = default;

void SoftNavigationContext::SetUrl(const String& url) {
  url_ = url;
}

}  // namespace blink
