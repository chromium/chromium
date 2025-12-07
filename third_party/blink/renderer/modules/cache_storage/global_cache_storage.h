// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_GLOBAL_CACHE_STORAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_GLOBAL_CACHE_STORAGE_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

class CacheStorage;
class ExceptionState;
class ExecutionContext;
class WindowOrWorkerGlobalScope;

class GlobalCacheStorage final : public GarbageCollected<GlobalCacheStorage>,
                                 public GarbageCollectedMixin {
 public:
  GlobalCacheStorage() = default;

  static bool CanCreateCacheStorage(ExecutionContext*, ExceptionState&);

  static CacheStorage* caches(WindowOrWorkerGlobalScope&, ExceptionState&);

  void Trace(Visitor* visitor) const override;

 private:
  static GlobalCacheStorage& From(WindowOrWorkerGlobalScope&);
  CacheStorage* Caches(WindowOrWorkerGlobalScope&, ExceptionState&);

  Member<CacheStorage> caches_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CACHE_STORAGE_GLOBAL_CACHE_STORAGE_H_
