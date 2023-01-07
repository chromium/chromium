// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/garbage_collected_script_wrappable.h"

namespace blink {

GarbageCollectedScriptWrappable::GarbageCollectedScriptWrappable(
    const String& string)
    : string_(string) {}

GarbageCollectedScriptWrappable::~GarbageCollectedScriptWrappable() = default;

}  // namespace blink
