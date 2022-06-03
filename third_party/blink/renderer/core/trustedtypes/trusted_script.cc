// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/trustedtypes/trusted_script.h"

namespace blink {

TrustedScript::TrustedScript(String script) : script_(std::move(script)) {}

const String& TrustedScript::toString() const {
  return script_;
}

}  // namespace blink
