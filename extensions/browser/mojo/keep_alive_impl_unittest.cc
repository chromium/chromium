// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/mojo/keep_alive_impl.h"

#include <utility>

#include "base/run_loop.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension_builder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {

class KeepAliveTest : public ExtensionsTest {
 public:
  KeepAliveTest() : mojo_activity_(Activity::MOJO, "") {}

  KeepAliveTest(const KeepAliveTest&) = delete;
  KeepAliveTest& operator=(const KeepAliveTest&) = delete;

  ~KeepAliveTest() override {}

  void SetUp() override {
    ExtensionsTest::SetUp();
    extension_ =
        ExtensionBuilder()
            .SetManifest(
                base::Value::Dict()
                    .Set("name", "app")
                    .Set("version", "1")
                    .Set("manifest_version", 2)
                    .Set("app", base::Value::Dict().Set(
                                    "background",
                                    base::Value::Dict().Set(
                                        "scripts", base::Value::List().Append(
                                                       "background.js")))))
            .SetID("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")
            .Build();
  }

  void WaitUntilLazyKeepAliveChanges() {
    int initial_keep_alive_count = GetKeepAliveCount();
    while (GetKeepAliveCount() == initial_keep_alive_count) {
      base::RunLoop().RunUntilIdle();
    }
  }

  void CreateKeepAlive(mojo::PendingReceiver<KeepAlive> receiver) {
    KeepAliveImpl::Create(browser_context(), extension_.get(), nullptr,
                          std::move(receiver));
  }

  const Extension* extension() { return extension_.get(); }

  int GetKeepAliveCount() {
    return ProcessManager::Get(browser_context())
        ->GetLazyKeepaliveCount(extension());
  }

  using ActivitiesMultiset = ProcessManager::ActivitiesMultiset;

  const std::pair<Activity::Type, std::string> mojo_activity_;

  ActivitiesMultiset GetActivities() {
    return ProcessManager::Get(browser_context())
        ->GetLazyKeepaliveActivities(extension());
  }

 private:
  scoped_refptr<const Extension> extension_;
};

TEST_F(KeepAliveTest, Basic) {
  mojo::PendingRemote<KeepAlive> keep_alive;
  CreateKeepAlive(keep_alive.InitWithNewPipeAndPassReceiver());
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(0, GetKeepAliveCount());
  EXPECT_EQ(0u, GetActivities().count(mojo_activity_));
}

TEST_F(KeepAliveTest, TwoKeepAlives) {
  mojo::PendingRemote<KeepAlive> keep_alive;
  CreateKeepAlive(keep_alive.InitWithNewPipeAndPassReceiver());
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  mojo::PendingRemote<KeepAlive> other_keep_alive;
  CreateKeepAlive(other_keep_alive.InitWithNewPipeAndPassReceiver());
  EXPECT_EQ(2, GetKeepAliveCount());
  EXPECT_EQ(2u, GetActivities().count(mojo_activity_));

  keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  other_keep_alive.reset();
  WaitUntilLazyKeepAliveChanges();
  EXPECT_EQ(0, GetKeepAliveCount());
  EXPECT_EQ(0u, GetActivities().count(mojo_activity_));
}

TEST_F(KeepAliveTest, UnloadExtension) {
  mojo::Remote<KeepAlive> keep_alive;
  CreateKeepAlive(keep_alive.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  scoped_refptr<const Extension> other_extension =
      ExtensionBuilder()
          .SetManifest(
              base::Value::Dict()
                  .Set("name", "app")
                  .Set("version", "1")
                  .Set("manifest_version", 2)
                  .Set("app", base::Value::Dict().Set(
                                  "background",
                                  base::Value::Dict().Set(
                                      "scripts", base::Value::List().Append(
                                                     "background.js")))))
          .SetID("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")
          .Build();

  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUnloaded(other_extension.get(),
                          UnloadedExtensionReason::DISABLE);
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  ExtensionRegistry::Get(browser_context())
      ->TriggerOnUnloaded(extension(), UnloadedExtensionReason::DISABLE);
  // When its extension is unloaded, the KeepAliveImpl should not modify the
  // keep-alive count for its extension. However, ProcessManager resets its
  // keep-alive count for an unloaded extension.
  EXPECT_EQ(0, GetKeepAliveCount());
  EXPECT_EQ(0u, GetActivities().count(mojo_activity_));

  // Wait for |keep_alive| to disconnect.
  base::RunLoop run_loop;
  keep_alive.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(KeepAliveTest, ShutdownExtensionRegistry) {
  mojo::Remote<KeepAlive> keep_alive;
  CreateKeepAlive(keep_alive.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  ExtensionRegistry::Get(browser_context())->Shutdown();
  // After a shutdown event, the KeepAliveImpl should not access its
  // ProcessManager and so the keep-alive count should remain unchanged.
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  // Wait for |keep_alive| to disconnect.
  base::RunLoop run_loop;
  keep_alive.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
}

TEST_F(KeepAliveTest, ShutdownProcessManager) {
  mojo::Remote<KeepAlive> keep_alive;
  CreateKeepAlive(keep_alive.BindNewPipeAndPassReceiver());
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  ProcessManager::Get(browser_context())->Shutdown();
  // After a shutdown event, the KeepAliveImpl should not access its
  // ProcessManager and so the keep-alive count should remain unchanged.
  EXPECT_EQ(1, GetKeepAliveCount());
  EXPECT_EQ(1u, GetActivities().count(mojo_activity_));

  // Wait for |keep_alive| to disconnect.
  base::RunLoop run_loop;
  keep_alive.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();
}

}  // namespace extensions
