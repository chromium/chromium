// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_script_identifier.h"

namespace blink {

AdScriptIdentifier::AdScriptIdentifier(
    const v8_inspector::V8DebuggerId& context_id,
    int id)
    : context_id(context_id), id(id) {}

}  // namespace blink
