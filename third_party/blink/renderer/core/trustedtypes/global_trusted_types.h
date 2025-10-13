// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_GLOBAL_TRUSTED_TYPES_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_GLOBAL_TRUSTED_TYPES_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LocalDOMWindow;
class ScriptState;
class TrustedTypePolicyFactory;
class WorkerGlobalScope;

class CORE_EXPORT GlobalTrustedTypes final {
  STATIC_ONLY(GlobalTrustedTypes);

 public:
  static TrustedTypePolicyFactory* trustedTypes(ScriptState*, LocalDOMWindow&);
  static TrustedTypePolicyFactory* trustedTypes(ScriptState*,
                                                WorkerGlobalScope&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TRUSTEDTYPES_GLOBAL_TRUSTED_TYPES_H_
