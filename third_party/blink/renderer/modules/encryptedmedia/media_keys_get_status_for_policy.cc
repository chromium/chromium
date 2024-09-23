// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/encryptedmedia/media_keys_get_status_for_policy.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_keys_policy.h"
#include "third_party/blink/renderer/modules/encryptedmedia/media_keys.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

namespace blink {

ScriptPromise<V8MediaKeyStatus> MediaKeysGetStatusForPolicy::getStatusForPolicy(
    ScriptState* script_state,
    MediaKeys& media_keys,
    const MediaKeysPolicy* media_keys_policy,
    ExceptionState& exception_state) {
  DVLOG(1) << __func__;

  return media_keys.getStatusForPolicy(script_state, media_keys_policy,
                                       exception_state);
}

}  // namespace blink
