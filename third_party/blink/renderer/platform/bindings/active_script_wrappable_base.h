// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_

#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8.h"

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

  // See trait below.
  //
  // Registering the ActiveScriptWrappableBase after construction means that
  // the garbage collector does not need to deal with objects that are
  // currently under construction. This is important as checking whether ASW
  // should be treated as active involves calling virtual functions which may
  // not work during construction. The objects in construction are kept alive
  // via conservative stack scanning.
  void ActiveScriptWrappableBaseConstructed();

 protected:
  ActiveScriptWrappableBase() = default;
};

}  // namespace blink

#if BUILDFLAG(USE_V8_OILPAN)

namespace cppgc {
template <typename T, typename Unused>
struct PostConstructionCallbackTrait;

template <typename T>
struct PostConstructionCallbackTrait<
    T,
    base::void_t<decltype(
        std::declval<T>().ActiveScriptWrappableBaseConstructed())>> {
  static void Call(T* object) {
    static_assert(std::is_base_of<blink::ActiveScriptWrappableBase, T>::value,
                  "Only ActiveScriptWrappableBase should use the "
                  "post-construction hook.");
    object->ActiveScriptWrappableBaseConstructed();
  }
};
}  // namespace cppgc

#else  // !USE_V8_OILPAN

namespace blink {
template <typename T>
struct PostConstructionHookTrait<
    T,
    base::void_t<decltype(
        std::declval<T>().ActiveScriptWrappableBaseConstructed())>> {
  static void Call(T* object) {
    static_assert(std::is_base_of<ActiveScriptWrappableBase, T>::value,
                  "Only ActiveScriptWrappableBase should use the "
                  "post-construction hook.");
    object->ActiveScriptWrappableBaseConstructed();
  }
};

}  // namespace blink

#endif  // !USE_V8_OILPAN

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ACTIVE_SCRIPT_WRAPPABLE_BASE_H_
