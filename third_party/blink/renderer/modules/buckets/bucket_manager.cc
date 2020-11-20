// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/buckets/bucket_manager.h"

#include "third_party/blink/renderer/core/execution_context/navigator_base.h"

namespace blink {

const char BucketManager::kSupplementName[] = "BucketManager";

BucketManager::BucketManager(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator) {}

BucketManager* BucketManager::storageBuckets(NavigatorBase& navigator,
                                             ExceptionState& exception_state) {
  auto* supplement = Supplement<NavigatorBase>::From<BucketManager>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<BucketManager>(navigator);
    Supplement<NavigatorBase>::ProvideTo(navigator, supplement);
  }
  return supplement;
}

ScriptPromise BucketManager::openOrCreate(ScriptState* script_state,
                                          const String& name,
                                          ExceptionState& exception_state) {
  if (!bucket_list_.Contains(name)) {
    bucket_list_.push_back(name);
    std::sort(bucket_list_.begin(), bucket_list_.end(),
              WTF::CodeUnitCompareLessThan);
  }
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(name);
  return promise;
}

ScriptPromise BucketManager::keys(ScriptState* script_state,
                                  ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();
  resolver->Resolve(bucket_list_);
  return promise;
}

ScriptPromise BucketManager::Delete(ScriptState* script_state,
                                    const String& name,
                                    ExceptionState& exception_state) {
  wtf_size_t index = bucket_list_.Find(name);
  if (index != kNotFound)
    bucket_list_.EraseAt(index);
  return ScriptPromise();
}

void BucketManager::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
