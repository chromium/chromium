// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/text/hyphenation.h"

namespace blink {

scoped_refptr<Hyphenation> Hyphenation::PlatformGetHyphenation(
    const AtomicString&) {
  return nullptr;
}

}  // namespace blink
