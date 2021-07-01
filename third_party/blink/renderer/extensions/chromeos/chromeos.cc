// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/extensions/chromeos/chromeos.h"

namespace blink {

bool ChromeOS::myEmbedderFunction(bool testArg) {
  return !testArg;
}

}  // namespace blink
