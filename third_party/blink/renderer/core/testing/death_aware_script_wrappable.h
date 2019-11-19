// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DEATH_AWARE_SCRIPT_WRAPPABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DEATH_AWARE_SCRIPT_WRAPPABLE_H_

#include <signal.h>
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

class DeathAwareScriptWrappable;

namespace internal {

class InObjectContainer {
  DISALLOW_NEW();

 public:
  explicit InObjectContainer(DeathAwareScriptWrappable* dependency)
      : dependency_(dependency) {}

  virtual ~InObjectContainer() {}

  virtual void Trace(Visitor* visitor) { visitor->Trace(dependency_); }

 private:
  Member<DeathAwareScriptWrappable> dependency_;
};

}  // namespace internal

// ScriptWrappable that can be used to
// (a) build a graph of ScriptWrappables, and
// (b) observe a single DeathAwareScriptWrappable for being garbage collected.
class DeathAwareScriptWrappable : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();
  static DeathAwareScriptWrappable* instance_;
  static bool has_died_;

 public:
  typedef Member<DeathAwareScriptWrappable> Wrapper;

  static DeathAwareScriptWrappable* Create() {
    return MakeGarbageCollected<DeathAwareScriptWrappable>();
  }

  static bool HasDied() { return has_died_; }
  static void ObserveDeathsOf(DeathAwareScriptWrappable* instance) {
    has_died_ = false;
    instance_ = instance;
  }

  DeathAwareScriptWrappable() = default;
  ~DeathAwareScriptWrappable() override {
    if (this == instance_) {
      has_died_ = true;
    }
  }

  void Trace(blink::Visitor* visitor) override {
    visitor->Trace(wrapped_dependency_);
    visitor->Trace(wrapped_vector_dependency_);
    visitor->Trace(wrapped_hash_map_dependency_);
    visitor->Trace(in_object_dependency_);
    ScriptWrappable::Trace(visitor);
  }

  void SetWrappedDependency(DeathAwareScriptWrappable* dependency) {
    wrapped_dependency_ = dependency;
  }

  void AddWrappedVectorDependency(DeathAwareScriptWrappable* dependency) {
    wrapped_vector_dependency_.push_back(dependency);
  }

  void AddWrappedHashMapDependency(DeathAwareScriptWrappable* key,
                                   DeathAwareScriptWrappable* value) {
    wrapped_hash_map_dependency_.insert(key, value);
  }

  void AddInObjectDependency(DeathAwareScriptWrappable* dependency) {
    in_object_dependency_.push_back(internal::InObjectContainer(dependency));
  }

 private:
  Wrapper wrapped_dependency_;
  HeapVector<Wrapper> wrapped_vector_dependency_;
  HeapHashMap<Wrapper, Wrapper> wrapped_hash_map_dependency_;
  HeapVector<internal::InObjectContainer> in_object_dependency_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_DEATH_AWARE_SCRIPT_WRAPPABLE_H_
