// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_ACCESSIBILITY_FEATURES_REGISTERED_WRAPPABLE_H_
#define SERVICES_ACCESSIBILITY_FEATURES_REGISTERED_WRAPPABLE_H_

#include "base/scoped_observation.h"
#include "services/accessibility/features/bindings_isolate_holder.h"
#include "services/accessibility/features/v8_manager.h"
#include "v8/include/v8-context.h"

namespace ax {

// Classes that wish to override gin::Wrappable and are constructed with `new`
// should also override RegisteredWrappable to ensure they are cleaned up
// when the isolate they live in is destroyed.
//
// gin::Wrappable objects are allocated with `new` in order to pass memory
// ownership to V8, which will run garbage collection on them when memory
// is getting full. However, V8 does not run garbage collection on teardown.
// Accessibility Service tears down V8 and destroys the isolate when
// features are disabled, so this will happen in both prod and test.
class RegisteredWrappable : public BindingsIsolateHolder::IsolateObserver {
 public:
  explicit RegisteredWrappable(v8::Local<v8::Context> context);
  ~RegisteredWrappable() override;

  // BindingsIsolateHolder::IsolateObserver:
  void OnIsolateWillDestroy() override;

 protected:
  void StopObserving();

 private:
  base::ScopedObservation<BindingsIsolateHolder,
                          BindingsIsolateHolder::IsolateObserver>
      observer_{this};
};

}  // namespace ax

#endif  // SERVICES_ACCESSIBILITY_FEATURES_REGISTERED_WRAPPABLE_H_
