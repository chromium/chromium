// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/midi_host.h"

#include <stddef.h>
#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "content/common/media/midi_messages.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using midi::mojom::PortState;

const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};
const int kRenderProcessId = 0;

enum MidiEventType {
  DISPATCH_SEND_MIDI_DATA,
};

struct MidiEvent {
  MidiEvent(MidiEventType in_type,
            uint32_t in_port_index,
            const std::vector<uint8_t>& in_data,
            base::TimeTicks in_timestamp)
      : type(in_type),
        port_index(in_port_index),
        data(in_data),
        timestamp(in_timestamp) {}

  MidiEventType type;
  uint32_t port_index;
  std::vector<uint8_t> data;
  base::TimeTicks timestamp;
};

class FakeMidiManager : public midi::MidiManager {
 public:
  explicit FakeMidiManager(midi::MidiService* service)
      : MidiManager(service), weak_factory_(this) {}
  ~FakeMidiManager() override = default;

  base::WeakPtr<FakeMidiManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void DispatchSendMidiData(midi::MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override {
    events_.push_back(MidiEvent(DISPATCH_SEND_MIDI_DATA,
                                port_index,
                                data,
                                timestamp));
  }
  std::vector<MidiEvent> events_;

  base::WeakPtrFactory<FakeMidiManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerFactory : public midi::MidiService::ManagerFactory {
 public:
  FakeMidiManagerFactory() : weak_factory_(this) {}
  ~FakeMidiManagerFactory() override = default;
  std::unique_ptr<midi::MidiManager> Create(
      midi::MidiService* service) override {
    std::unique_ptr<FakeMidiManager> manager =
        std::make_unique<FakeMidiManager>(service);
    manager_ = manager->GetWeakPtr();
    return manager;
  }

  base::WeakPtr<FakeMidiManagerFactory> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  base::WeakPtr<FakeMidiManager> GetCreatedManager() { return manager_; }

 private:
  base::WeakPtr<FakeMidiManager> manager_;

  base::WeakPtrFactory<FakeMidiManagerFactory> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerFactory);
};

class MidiHostForTesting : public MidiHost {
 public:
  MidiHostForTesting(int renderer_process_id, midi::MidiService* midi_service)
      : MidiHost(renderer_process_id, midi_service) {}

 private:
  ~MidiHostForTesting() override {}

  // BrowserMessageFilter implementation.
  // Override ShutdownForBadMessage() to do nothing since the original
  // implementation to kill a malicious renderer process causes a check failure
  // in unit tests.
  void ShutdownForBadMessage() override {}

  DISALLOW_COPY_AND_ASSIGN(MidiHostForTesting);
};

class MidiHostTest : public testing::Test {
 public:
  MidiHostTest() : data_(kNoteOn, kNoteOn + base::size(kNoteOn)), port_id_(0) {
    std::unique_ptr<FakeMidiManagerFactory> factory =
        std::make_unique<FakeMidiManagerFactory>();
    factory_ = factory->GetWeakPtr();
    service_ = std::make_unique<midi::MidiService>(std::move(factory));
    host_ = new MidiHostForTesting(kRenderProcessId, service_.get());
    host_->OnStartSession();
  }
  ~MidiHostTest() override {
    host_->OnEndSession();
    service_->Shutdown();
    RunLoopUntilIdle();
  }

 protected:
  void AddOutputPort() {
    const std::string id = base::StringPrintf("i-can-%d", port_id_++);
    const std::string manufacturer("yukatan");
    const std::string name("doki-doki-pi-pine");
    const std::string version("3.14159265359");
    PortState state = PortState::CONNECTED;
    midi::mojom::PortInfo info(id, manufacturer, name, version, state);

    host_->AddOutputPort(info);
  }

  void OnSendData(uint32_t port) {
    std::unique_ptr<IPC::Message> message(
        new MidiHostMsg_SendData(port, data_, base::TimeTicks()));
    host_->OnMessageReceived(*message.get());
  }

  size_t GetEventSize() const {
    if (!factory_->GetCreatedManager())
      return 0U;
    return factory_->GetCreatedManager()->events_.size();
  }

  void CheckSendEventAt(size_t at, uint32_t port) {
    base::WeakPtr<FakeMidiManager> manager = factory_->GetCreatedManager();
    ASSERT_TRUE(manager);
    EXPECT_EQ(DISPATCH_SEND_MIDI_DATA, manager->events_[at].type);
    EXPECT_EQ(port, manager->events_[at].port_index);
    EXPECT_EQ(data_, manager->events_[at].data);
    EXPECT_EQ(base::TimeTicks(), manager->events_[at].timestamp);
  }

  void RunLoopUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 private:
  TestBrowserThreadBundle thread_bundle_;

  std::vector<uint8_t> data_;
  int32_t port_id_;
  base::WeakPtr<FakeMidiManagerFactory> factory_;
  std::unique_ptr<midi::MidiService> service_;
  scoped_refptr<MidiHostForTesting> host_;

  DISALLOW_COPY_AND_ASSIGN(MidiHostTest);
};

}  // namespace

// Test if sending data to out of range port is ignored.
TEST_F(MidiHostTest, OutputPortCheck) {
  // Only one output port is available.
  AddOutputPort();

  // Sending data to port 0 should be delivered.
  uint32_t port0 = 0;
  OnSendData(port0);
  RunLoopUntilIdle();
  EXPECT_EQ(1U, GetEventSize());
  CheckSendEventAt(0, port0);

  // Sending data to port 1 should not be delivered.
  uint32_t port1 = 1;
  OnSendData(port1);
  RunLoopUntilIdle();
  EXPECT_EQ(1U, GetEventSize());

  // Two output ports are available from now on.
  AddOutputPort();

  // Sending data to port 0 and 1 should be delivered now.
  OnSendData(port0);
  OnSendData(port1);
  RunLoopUntilIdle();
  EXPECT_EQ(3U, GetEventSize());
  CheckSendEventAt(1, port0);
  CheckSendEventAt(2, port1);
}

}  // namespace conent
