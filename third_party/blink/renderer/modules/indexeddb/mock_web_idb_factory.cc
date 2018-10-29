// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/indexeddb/mock_web_idb_factory.h"

#include <memory>

#include "base/memory/ptr_util.h"

namespace blink {

MockWebIDBFactory::MockWebIDBFactory() = default;

MockWebIDBFactory::~MockWebIDBFactory() = default;

std::unique_ptr<MockWebIDBFactory> MockWebIDBFactory::Create() {
  return base::WrapUnique(new MockWebIDBFactory());
}
}  // namespace blink
