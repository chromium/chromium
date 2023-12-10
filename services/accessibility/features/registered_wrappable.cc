
// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/registered_wrappable.h"

#include "gin/public/isolate_holder.h"
#include "services/accessibility/features/v8_manager.h"

namespace ax {

RegisteredWrappable::RegisteredWrappable(v8::Local<v8::Context> context) {
  observer_.Observe(V8Environment::GetFromContext(context));
}

RegisteredWrappable::~RegisteredWrappable() = default;

void RegisteredWrappable::OnIsolateWillDestroy() {
  delete this;
}

void RegisteredWrappable::StopObserving() {
  observer_.Reset();
}

}  // namespace ax
