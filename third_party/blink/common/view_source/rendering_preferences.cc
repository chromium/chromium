// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/view_source/rendering_preferences.h"

namespace blink {

namespace {
bool preference = false;
}  // namespace

// static
void ViewSourceLineWrappingPreference::Set(bool value) {
  preference = value;
}

// static
bool ViewSourceLineWrappingPreference::Get() {
  return preference;
}

}  // namespace blink
