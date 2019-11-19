// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/interstitial_page_delegate.h"

namespace content {
InterstitialPageDelegate::TypeID InterstitialPageDelegate::GetTypeForTesting() {
  return nullptr;
}

}  // namespace content
