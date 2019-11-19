// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/sms/sms.h"

#include <utility>

namespace blink {

SMS::SMS(const WTF::String& content) : content_(content) {}

SMS::~SMS() = default;

const String& SMS::content() const {
  return content_;
}

}  // namespace blink
