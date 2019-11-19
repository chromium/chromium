// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_DYNAMIC_MODULE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_DYNAMIC_MODULE_RESOLVER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class Modulator;
class ReferrerScriptInfo;
class ScriptPromiseResolver;

// DynamicModuleResolver implements "Runtime Semantics:
// HostImportModuleDynamically" per spec.
// https://tc39.github.io/proposal-dynamic-import/#sec-hostimportmoduledynamically
class CORE_EXPORT DynamicModuleResolver final
    : public GarbageCollected<DynamicModuleResolver> {
 public:
  void Trace(Visitor*);

  explicit DynamicModuleResolver(Modulator* modulator)
      : modulator_(modulator) {}

  // Implements "HostImportModuleDynamically" semantics.
  // Should be called w/ a valid V8 context.
  void ResolveDynamically(const String& specifier,
                          const KURL& referrer_resource_url,
                          const ReferrerScriptInfo& referrer_info,
                          ScriptPromiseResolver*);

 private:
  Member<Modulator> modulator_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCRIPT_DYNAMIC_MODULE_RESOLVER_H_
