// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/ad_script_identifier.h"

#include "base/check_op.h"

namespace blink {

AdScriptIdentifier::AdScriptIdentifier() : id(kEmptyId) {}

AdScriptIdentifier::AdScriptIdentifier(
    const v8_inspector::V8DebuggerId& context_id,
    int id,
    String name)
    : context_id(context_id), id(id), name(name) {
  CHECK_NE(id, kEmptyId);
}

bool AdScriptIdentifier::operator==(const AdScriptIdentifier& other) const {
  return context_id.pair() == other.context_id.pair() && id == other.id;
}

}  // namespace blink
