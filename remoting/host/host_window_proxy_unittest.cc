// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/host_window_proxy.h"

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "remoting/host/client_session_control.h"
#include "remoting/host/disconnect_window_base.h"
#include "remoting/host/host_mock_objects.h"
#include "remoting/protocol/errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using ::testing::_;
using ::testing::Eq;
using ::testing::ReturnRef;

constexpr char kTestUserJid[] = "remote.user@gmail.com/chromoting_ftl_1234";
constexpr char kLocalDisconnectReason[] = "Local disconnect";

class FakeClientSessionControl : public MockClientSessionControl {
 public:
  explicit FakeClientSessionControl(std::string client_jid)
      : client_jid_(std::move(client_jid)) {
    EXPECT_CALL(*this, client_jid()).WillRepeatedly(ReturnRef(client_jid_));
  }
  ~FakeClientSessionControl() override = default;

  base::WeakPtr<ClientSessionControl> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  std::string client_jid_;
  base::WeakPtrFactory<FakeClientSessionControl> weak_factory_{this};
};

// Requests a local disconnect as soon as it starts, like a privacy overlay
// whose Disconnect button is clicked, and reports its destruction.
class LocalDisconnectWindow : public DisconnectWindowBase {
 public:
  explicit LocalDisconnectWindow(base::OnceClosure on_destroyed)
      : on_destroyed_(std::move(on_destroyed)) {}
  ~LocalDisconnectWindow() override { std::move(on_destroyed_).Run(); }

  void Start(const base::WeakPtr<ClientSessionControl>& client_session_control)
      override {
    DisconnectWindowBase::Start(client_session_control);
    DisconnectSessionAndAwaitTeardown(
        kLocalDisconnectReason, base::BindOnce([]() {
          ADD_FAILURE() << "Session teardown timeout should not be reached.";
        }));
  }

 private:
  base::OnceClosure on_destroyed_;
};

}  // namespace

// Verifies that a window which requests a disconnect from the UI thread stays
// alive after the request reaches the caller thread, and is destroyed on the UI
// thread only once session teardown destroys the `HostWindowProxy`.
TEST(HostWindowProxyTest, LocalDisconnectKeepsWindowUntilProxyIsDestroyed) {
  base::test::TaskEnvironment task_environment;
  base::Thread ui_thread("UiThread");
  ASSERT_TRUE(ui_thread.Start());

  std::atomic<bool> window_destroyed{false};
  base::RunLoop window_destroyed_loop;
  auto window = std::make_unique<LocalDisconnectWindow>(base::BindOnce(
      [](std::atomic<bool>* destroyed, base::OnceClosure quit) {
        *destroyed = true;
        std::move(quit).Run();
      },
      &window_destroyed,
      base::BindPostTask(task_environment.GetMainThreadTaskRunner(),
                         window_destroyed_loop.QuitClosure())));

  FakeClientSessionControl client_session_control(kTestUserJid);
  auto proxy = std::make_unique<HostWindowProxy>(
      task_environment.GetMainThreadTaskRunner(), ui_thread.task_runner(),
      std::move(window));

  base::RunLoop disconnect_loop;
  EXPECT_CALL(
      client_session_control,
      DisconnectSession(protocol::ErrorCode::OK, Eq(kLocalDisconnectReason), _))
      .WillOnce([&]() {
        EXPECT_TRUE(task_environment.GetMainThreadTaskRunner()
                        ->BelongsToCurrentThread());
        EXPECT_FALSE(window_destroyed);
        disconnect_loop.Quit();
      });
  proxy->Start(client_session_control.GetWeakPtr());
  disconnect_loop.Run();

  // The window outlives the disconnect request.
  ui_thread.FlushForTesting();
  EXPECT_FALSE(window_destroyed);

  // Session teardown destroys the proxy, which destroys the window on the UI
  // thread.
  proxy.reset();
  window_destroyed_loop.Run();
  EXPECT_TRUE(window_destroyed);

  ui_thread.Stop();
}

}  // namespace remoting
