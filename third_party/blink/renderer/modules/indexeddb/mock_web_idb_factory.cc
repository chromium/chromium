// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

MockWebIDBFactory::MockWebIDBFactory() = default;

MockWebIDBFactory::~MockWebIDBFactory() = default;

void MockWebIDBFactory::GetDatabaseInfo(
    std::unique_ptr<WebIDBCallbacks> callbacks) {
  *callbacks_ptr_ = std::move(callbacks);
}

void MockWebIDBFactory::SetCallbacksPointer(
    std::unique_ptr<WebIDBCallbacks>* callbacks_ptr) {
  callbacks_ptr_ = callbacks_ptr;
}

}  // namespace blink
