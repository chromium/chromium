// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_

#include <type_traits>

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

/**
 * Classes deriving from ActiveScriptWrappable will be kept alive as long as
 * they have a pending activity. Destroying the corresponding ExecutionContext
 * implicitly releases them to avoid leaks.
 */
class PLATFORM_EXPORT ActiveScriptWrappableBase : public GarbageCollectedMixin {
 public:
  ActiveScriptWrappableBase(const ActiveScriptWrappableBase&) = delete;
  ActiveScriptWrappableBase& operator=(const ActiveScriptWrappableBase&) =
      delete;
  virtual ~ActiveScriptWrappableBase() = default;

  virtual bool IsContextDestroyed() const = 0;
  virtual bool DispatchHasPendingActivity() const = 0;

 protected:
  ActiveScriptWrappableBase() = default;

  void RegisterActiveScriptWrappable(v8::Isolate*);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
