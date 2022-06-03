// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_NAVIGATOR_REQUEST_MEDIA_KEY_SYSTEM_ACCESS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_NAVIGATOR_REQUEST_MEDIA_KEY_SYSTEM_ACCESS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_media_key_system_configuration.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;

class NavigatorRequestMediaKeySystemAccess {
  STATIC_ONLY(NavigatorRequestMediaKeySystemAccess);

 public:
  static ScriptPromise requestMediaKeySystemAccess(
      ScriptState*,
      Navigator&,
      const String& key_system,
      const HeapVector<Member<MediaKeySystemConfiguration>>&
          supported_configurations,
      ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_NAVIGATOR_REQUEST_MEDIA_KEY_SYSTEM_ACCESS_H_
