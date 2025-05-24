// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/shared_storage/shared_storage_worklet_navigator.h"

namespace blink {

SharedStorageWorkletNavigator::SharedStorageWorkletNavigator(
    ExecutionContext* execution_context)
    : NavigatorBase(execution_context) {}

SharedStorageWorkletNavigator::~SharedStorageWorkletNavigator() = default;

String SharedStorageWorkletNavigator::GetAcceptLanguages() {
  NOTREACHED();
}

}  // namespace blink
