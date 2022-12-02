// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/accessibility/features/v8_manager.h"

#include "base/test/task_environment.h"
#include "services/accessibility/public/mojom/accessibility_service.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ax {

class V8ManagerTest : public testing::Test {
 public:
  V8ManagerTest() = default;
  V8ManagerTest(const V8ManagerTest&) = delete;
  V8ManagerTest& operator=(const V8ManagerTest&) = delete;
  ~V8ManagerTest() override = default;

  void SetUp() override { BindingsIsolateHolder::InitializeV8(); }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(V8ManagerTest, ExecutesSimpleScript) {
  scoped_refptr<V8Manager> manager = V8Manager::Create();
  manager->AddV8Bindings();
  base::RunLoop script_waiter;
  // Test that this script compiles and runs. That indicates that
  // the atpconsole.log binding was added and that JS works in general.
  manager->ExecuteScript(R"JS(
    const d = 22;
    var m = 1;
    let y = 1973;
    atpconsole.log('Green is the loneliest color');
  )JS",
                         script_waiter.QuitClosure());
  script_waiter.Run();
}

}  // namespace ax
