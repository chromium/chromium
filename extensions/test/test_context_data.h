// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_TEST_TEST_CONTEXT_DATA_H_
#define EXTENSIONS_TEST_TEST_CONTEXT_DATA_H_

#include <memory>

#include "extensions/common/context_data.h"

namespace extensions {

class TestContextData : public ContextData {
 public:
  TestContextData() = default;
  ~TestContextData() override = default;

  std::unique_ptr<ContextData> Clone() const override;
  bool IsIsolatedApplication() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_TEST_TEST_CONTEXT_DATA_H_
