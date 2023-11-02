// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_TEST_V8_TEST_H_
#define GIN_TEST_V8_TEST_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {

class IsolateHolder;

// V8Test is a simple harness for testing interactions with V8. V8Test doesn't
// have any dependencies on Gin's module system.
class V8Test : public testing::Test {
 public:
  V8Test();
  V8Test(const V8Test&) = delete;
  V8Test& operator=(const V8Test&) = delete;
  ~V8Test() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  // This is used during SetUp() to initialize instance_.
  virtual std::unique_ptr<IsolateHolder> CreateIsolateHolder() const;
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IsolateHolder> instance_;
  v8::Persistent<v8::Context> context_;
};

}  // namespace gin

#endif  // GIN_TEST_V8_TEST_H_
