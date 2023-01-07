// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/testing/death_aware_script_wrappable.h"

namespace blink {

DeathAwareScriptWrappable* DeathAwareScriptWrappable::instance_;
bool DeathAwareScriptWrappable::has_died_;

}  // namespace blink
