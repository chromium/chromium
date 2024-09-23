// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/midi/midi_manager.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/system/system_monitor.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "media/midi/midi_service.h"
#include "media/midi/task_service.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "media/midi/midi_manager_win.h"
#endif  // BUILDFLAG(IS_WIN)

namespace midi {

namespace {

using mojom::PortState;
using mojom::Result;

class FakeMidiManager : public MidiManager {
 public:
  explicit FakeMidiManager(MidiService* service) : MidiManager(service) {}

  FakeMidiManager(const FakeMidiManager&) = delete;
  FakeMidiManager& operator=(const FakeMidiManager&) = delete;

  ~FakeMidiManager() override = default;

  base::WeakPtr<FakeMidiManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // MidiManager implementation.
  void StartInitialization() override {
    DCHECK(!initialized_);
    initialized_ = true;
  }
  void DispatchSendMidiData(MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override {}

  // Utility functions for testing.
  void CallCompleteInitialization(Result result) {
    CompleteInitialization(result);
  }

  size_t GetClientCount() { return GetClientCountForTesting(); }

  size_t GetPendingClientCount() { return GetPendingClientCountForTesting(); }

  bool IsInitialized() const { return initialized_; }

 private:
  bool initialized_ = false;

  base::WeakPtrFactory<FakeMidiManager> weak_factory_{this};
};

class FakeMidiManagerFactory : public MidiService::ManagerFactory {
 public:
  FakeMidiManagerFactory() {}

  FakeMidiManagerFactory(const FakeMidiManagerFactory&) = delete;
  FakeMidiManagerFactory& operator=(const FakeMidiManagerFactory&) = delete;

  ~FakeMidiManagerFactory() override = default;

  std::unique_ptr<MidiManager> Create(MidiService* service) override {
    std::unique_ptr<FakeMidiManager> manager =
        std::make_unique<FakeMidiManager>(service);
    manager_ = manager->GetWeakPtr();
    return manager;
  }

  base::WeakPtr<FakeMidiManagerFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtr<FakeMidiManager> manager() {
#if BUILDFLAG(IS_MAC)
    // To avoid Core MIDI issues, MidiManager won't be destructed on macOS.
    // See https://crbug.com/718140.
    if (!manager_ ||
        (!manager_->GetClientCount() && !manager_->GetPendingClientCount())) {
      return nullptr;
    }
#endif
    return manager_;
  }

 private:
  base::WeakPtr<FakeMidiManager> manager_ = nullptr;
  base::WeakPtrFactory<FakeMidiManagerFactory> weak_factory_{this};
};

class FakeMidiManagerClient : public MidiManagerClient {
 public:
  explicit FakeMidiManagerClient(base::OnceClosure on_session_start_cb)
      : on_session_start_cb_(std::move(on_session_start_cb)) {}

  FakeMidiManagerClient(const FakeMidiManagerClient&) = delete;
  FakeMidiManagerClient& operator=(const FakeMidiManagerClient&) = delete;

  ~FakeMidiManagerClient() override = default;

  // MidiManagerClient implementation.
  void AddInputPort(const mojom::PortInfo& info) override {}
  void AddOutputPort(const mojom::PortInfo& info) override {}
  void SetInputPortState(uint32_t port_index, PortState state) override {}
  void SetOutputPortState(uint32_t port_index, PortState state) override {}
  void CompleteStartSession(Result result) override {
    CHECK(on_session_start_cb_);
    result_ = result;
    std::move(on_session_start_cb_).Run();
  }
  void ReceiveMidiData(uint32_t port_index,
                       const uint8_t* data,
                       size_t size,
                       base::TimeTicks timestamp) override {}
  void AccumulateMidiBytesSent(size_t size) override {}
  void Detach() override {}

  Result result() const { return result_; }

 private:
  Result result_ = Result::NOT_SUPPORTED;
  base::OnceClosure on_session_start_cb_;
};

class MidiManagerTest : public ::testing::Test {
 public:
  MidiManagerTest() {
    std::unique_ptr<FakeMidiManagerFactory> factory =
        std::make_unique<FakeMidiManagerFactory>();
    factory_ = factory->GetWeakPtr();
    service_ = std::make_unique<MidiService>(std::move(factory));
  }

  MidiManagerTest(const MidiManagerTest&) = delete;
  MidiManagerTest& operator=(const MidiManagerTest&) = delete;

  ~MidiManagerTest() override {
    service_->Shutdown();
    env_.RunUntilIdle();
  }

 protected:
  void StartTheFirstSession(FakeMidiManagerClient* client) {
    DCHECK(factory_);

    EXPECT_FALSE(factory_->manager());
    service_->StartSession(client);
    ASSERT_TRUE(factory_->manager());
    EXPECT_TRUE(factory_->manager()->IsInitialized());
    EXPECT_EQ(0U, factory_->manager()->GetClientCount());
    EXPECT_EQ(1U, factory_->manager()->GetPendingClientCount());
  }

  void StartTheNthSession(FakeMidiManagerClient* client, size_t nth) {
    DCHECK(factory_);
    DCHECK_NE(1U, nth);

    ASSERT_TRUE(factory_->manager());
    EXPECT_TRUE(factory_->manager()->IsInitialized());
    EXPECT_EQ(0U, factory_->manager()->GetClientCount());
    EXPECT_EQ(nth - 1U, factory_->manager()->GetPendingClientCount());
    service_->StartSession(client);
    EXPECT_EQ(nth, factory_->manager()->GetPendingClientCount());
  }

  void StartSession(FakeMidiManagerClient* client) {
    service_->StartSession(client);
  }

  void EndSession(FakeMidiManagerClient* client, size_t before, size_t after) {
    DCHECK(factory_);

    ASSERT_TRUE(factory_->manager());
    EXPECT_EQ(before, factory_->manager()->GetClientCount());
    EXPECT_TRUE(service_->EndSession(client));
    if (after) {
      ASSERT_TRUE(factory_->manager());
      EXPECT_EQ(after, factory_->manager()->GetClientCount());
    } else {
      EXPECT_FALSE(factory_->manager());
    }
  }

  bool CompleteInitialization(Result result) {
    DCHECK(factory_);

    if (!factory_->manager())
      return false;

    factory_->manager()->CallCompleteInitialization(result);
    return true;
  }

  base::WeakPtr<FakeMidiManagerFactory> factory() { return factory_; }

 private:
  base::test::TaskEnvironment env_;
  base::WeakPtr<FakeMidiManagerFactory> factory_;
  std::unique_ptr<MidiService> service_;
};

TEST_F(MidiManagerTest, StartAndEndSession) {
  base::test::TestFuture<void> test_future;
  std::unique_ptr<FakeMidiManagerClient> client =
      std::make_unique<FakeMidiManagerClient>(test_future.GetCallback());

  StartTheFirstSession(client.get());
  EXPECT_TRUE(CompleteInitialization(Result::OK));
  ASSERT_TRUE(test_future.Wait());
  EXPECT_EQ(Result::OK, client->result());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartAndEndSessionWithError) {
  base::test::TestFuture<void> test_future;
  std::unique_ptr<FakeMidiManagerClient> client =
      std::make_unique<FakeMidiManagerClient>(test_future.GetCallback());

  StartTheFirstSession(client.get());
  EXPECT_TRUE(CompleteInitialization(Result::INITIALIZATION_ERROR));
  ASSERT_TRUE(test_future.Wait());
  EXPECT_EQ(Result::INITIALIZATION_ERROR, client->result());
  EndSession(client.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, StartMultipleSessions) {
  base::test::TestFuture<void> future1;
  std::unique_ptr<FakeMidiManagerClient> client1 =
      std::make_unique<FakeMidiManagerClient>(future1.GetCallback());
  base::test::TestFuture<void> future2;
  std::unique_ptr<FakeMidiManagerClient> client2 =
      std::make_unique<FakeMidiManagerClient>(future2.GetCallback());
  base::test::TestFuture<void> future3;
  std::unique_ptr<FakeMidiManagerClient> client3 =
      std::make_unique<FakeMidiManagerClient>(future3.GetCallback());

  StartTheFirstSession(client1.get());
  StartTheNthSession(client2.get(), 2);
  StartTheNthSession(client3.get(), 3);
  EXPECT_TRUE(CompleteInitialization(Result::OK));
  ASSERT_TRUE(future1.Wait());
  EXPECT_EQ(Result::OK, client1->result());
  ASSERT_TRUE(future2.Wait());
  EXPECT_EQ(Result::OK, client2->result());
  ASSERT_TRUE(future3.Wait());
  EXPECT_EQ(Result::OK, client3->result());
  EndSession(client1.get(), 3U, 2U);
  EndSession(client2.get(), 2U, 1U);
  EndSession(client3.get(), 1U, 0U);
}

TEST_F(MidiManagerTest, TooManyPendingSessions) {
  // Push as many client requests for starting session as possible.
  std::unique_ptr<base::test::TestFuture<void>>
      test_futures[MidiManager::kMaxPendingClientCount];
  std::unique_ptr<FakeMidiManagerClient>
      many_existing_clients[MidiManager::kMaxPendingClientCount];
  test_futures[0] = std::make_unique<base::test::TestFuture<void>>();
  many_existing_clients[0] =
      std::make_unique<FakeMidiManagerClient>(test_futures[0]->GetCallback());
  StartTheFirstSession(many_existing_clients[0].get());
  for (size_t i = 1; i < MidiManager::kMaxPendingClientCount; ++i) {
    test_futures[i] = std::make_unique<base::test::TestFuture<void>>();
    many_existing_clients[i] =
        std::make_unique<FakeMidiManagerClient>(test_futures[i]->GetCallback());
    StartTheNthSession(many_existing_clients[i].get(), i + 1);
  }
  ASSERT_TRUE(factory()->manager());
  EXPECT_TRUE(factory()->manager()->IsInitialized());

  // Push the last client that should be rejected for too many pending requests.
  std::unique_ptr<FakeMidiManagerClient> additional_client =
      std::make_unique<FakeMidiManagerClient>(base::DoNothing());
  StartSession(additional_client.get());
  EXPECT_EQ(Result::INITIALIZATION_ERROR, additional_client->result());

  // Other clients still should not receive a result.
  for (const auto& existing_client : many_existing_clients) {
    EXPECT_EQ(Result::NOT_SUPPORTED, existing_client->result());
  }

  // The Result::OK should be distributed to other clients.
  EXPECT_TRUE(CompleteInitialization(Result::OK));
  for (size_t i = 0; i < MidiManager::kMaxPendingClientCount; ++i) {
    ASSERT_TRUE(test_futures[i]->Wait());
    EXPECT_EQ(Result::OK, many_existing_clients[i]->result());
  }

  // Close all successful sessions in FIFO order.
  size_t sessions = MidiManager::kMaxPendingClientCount;
  for (size_t i = 0; i < MidiManager::kMaxPendingClientCount; ++i, --sessions) {
    EndSession(many_existing_clients[i].get(), sessions, sessions - 1);
  }
}

TEST_F(MidiManagerTest, AbortSession) {
  // A client starting a session can be destructed while an asynchronous
  // initialization is performed.
  base::test::TestFuture<void> test_future;
  std::unique_ptr<FakeMidiManagerClient> client =
      std::make_unique<FakeMidiManagerClient>(test_future.GetCallback());

  StartTheFirstSession(client.get());
  EndSession(client.get(), 0, 0);
  client.reset();

  // Following function should not call the destructed |client| function.
  EXPECT_FALSE(CompleteInitialization(Result::OK));
  EXPECT_FALSE(test_future.IsReady());
}

class PlatformMidiManagerTest : public ::testing::Test {
 public:
  PlatformMidiManagerTest()
      : client_(std::make_unique<FakeMidiManagerClient>(
            test_future_.GetCallback())),
        service_(std::make_unique<MidiService>()) {}

  PlatformMidiManagerTest(const PlatformMidiManagerTest&) = delete;
  PlatformMidiManagerTest& operator=(const PlatformMidiManagerTest&) = delete;

  ~PlatformMidiManagerTest() override {
    service_->Shutdown();
    env_.RunUntilIdle();
  }

  MidiService* service() { return service_.get(); }
  FakeMidiManagerClient* client() { return client_.get(); }
  base::test::TestFuture<void>* future() { return &test_future_; }

  void StartSession() { service_->StartSession(client_.get()); }
  void EndSession() { service_->EndSession(client_.get()); }

  // This #ifdef needs to be identical to the one in media/midi/midi_manager.cc.
  // Do not change the condition for disabling this test.
  bool IsSupported() {
#if !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_WIN) && \
    !(defined(USE_ALSA) && defined(USE_UDEV)) && !BUILDFLAG(IS_ANDROID)
    return false;
#else
    return true;
#endif
  }

 private:
  // SystemMonitor is needed on Windows.
  base::SystemMonitor system_monitor;

  base::test::TaskEnvironment env_;

  base::test::TestFuture<void> test_future_;
  std::unique_ptr<FakeMidiManagerClient> client_;
  std::unique_ptr<MidiService> service_;
};

#if BUILDFLAG(IS_ANDROID)
// The test sometimes fails on Android. https://crbug.com/844027
#define MAYBE_CreatePlatformMidiManager DISABLED_CreatePlatformMidiManager
#else
#define MAYBE_CreatePlatformMidiManager CreatePlatformMidiManager
#endif
TEST_F(PlatformMidiManagerTest, MAYBE_CreatePlatformMidiManager) {
  StartSession();
  ASSERT_TRUE(future()->Wait());
  Result result = client()->result();

#if defined(USE_ALSA)
  // Temporary until http://crbug.com/371230 is resolved.
  EXPECT_TRUE(result == Result::OK || result == Result::INITIALIZATION_ERROR);
#else
  EXPECT_EQ(IsSupported() ? Result::OK : Result::NOT_SUPPORTED, result);
#endif
}

TEST_F(PlatformMidiManagerTest, InstanceIdOverflow) {
  service()->task_service()->OverflowInstanceIdForTesting();
#if BUILDFLAG(IS_WIN)
  MidiManagerWin::OverflowInstanceIdForTesting();
#endif  // BUILDFLAG(IS_WIN)

  StartSession();
  ASSERT_TRUE(future()->Wait());
  EXPECT_EQ(
      IsSupported() ? Result::INITIALIZATION_ERROR : Result::NOT_SUPPORTED,
      client()->result());

  EndSession();
}

}  // namespace

}  // namespace midi
