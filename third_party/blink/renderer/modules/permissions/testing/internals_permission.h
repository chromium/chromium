// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_TESTING_INTERNALS_PERMISSION_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_TESTING_INTERNALS_PERMISSION_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ExceptionState;
class Internals;
class ScriptPromise;
class ScriptState;
class ScriptValue;

class InternalsPermission {
  STATIC_ONLY(InternalsPermission);

 public:
  static ScriptPromise setPermission(ScriptState*,
                                     Internals&,
                                     const ScriptValue&,
                                     const String& state,
                                     const String& origin,
                                     const String& embedding_origin,
                                     ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PERMISSIONS_TESTING_INTERNALS_PERMISSION_H_
