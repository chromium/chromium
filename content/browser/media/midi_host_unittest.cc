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
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "media/midi/midi_manager.h"
#include "media/midi/midi_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

using midi::mojom::PortState;

const uint8_t kNoteOn[] = {0x90, 0x3c, 0x7f};

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
  explicit FakeMidiManager(midi::MidiService* service) : MidiManager(service) {}
  ~FakeMidiManager() override = default;

  base::WeakPtr<FakeMidiManager> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void DispatchSendMidiData(midi::MidiManagerClient* client,
                            uint32_t port_index,
                            const std::vector<uint8_t>& data,
                            base::TimeTicks timestamp) override {
    events_.push_back(
        MidiEvent(DISPATCH_SEND_MIDI_DATA, port_index, data, timestamp));
  }
  std::vector<MidiEvent> events_;

  base::WeakPtrFactory<FakeMidiManager> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManager);
};

class FakeMidiManagerFactory : public midi::MidiService::ManagerFactory {
 public:
  FakeMidiManagerFactory() {}
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

  base::WeakPtrFactory<FakeMidiManagerFactory> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(FakeMidiManagerFactory);
};

class MidiHostForTesting : public MidiHost {
 public:
  MidiHostForTesting(int renderer_process_id, midi::MidiService* midi_service)
      : MidiHost(renderer_process_id, midi_service) {}
  ~MidiHostForTesting() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MidiHostForTesting);
};

class MidiSessionClientForTesting : public midi::mojom::MidiSessionClient {
 public:
  MidiSessionClientForTesting() = default;
  ~MidiSessionClientForTesting() override = default;

  void AddInputPort(midi::mojom::PortInfoPtr info) override {}
  void AddOutputPort(midi::mojom::PortInfoPtr info) override {}
  void SetInputPortState(uint32_t port, PortState state) override {}
  void SetOutputPortState(uint32_t port, PortState state) override {}
  void SessionStarted(midi::mojom::Result result) override {}
  void AcknowledgeSentData(uint32_t bytes) override {}
  void DataReceived(uint32_t port,
                    const std::vector<uint8_t>& data,
                    base::TimeTicks timestamp) override {}
};

class MidiHostTest : public testing::Test {
 public:
  MidiHostTest() : data_(kNoteOn, kNoteOn + base::size(kNoteOn)), port_id_(0) {
    browser_context_ = std::make_unique<TestBrowserContext>();
    rph_ = std::make_unique<MockRenderProcessHost>(browser_context_.get());
    std::unique_ptr<FakeMidiManagerFactory> factory =
        std::make_unique<FakeMidiManagerFactory>();
    factory_ = factory->GetWeakPtr();
    service_ = std::make_unique<midi::MidiService>(std::move(factory));
    host_ = std::make_unique<MidiHostForTesting>(rph_->GetID(), service_.get());
    mojo::PendingRemote<midi::mojom::MidiSessionClient> client_remote;
    mojo::MakeSelfOwnedReceiver(std::make_unique<MidiSessionClientForTesting>(),
                                client_remote.InitWithNewPipeAndPassReceiver());
    host_->StartSession(session_.BindNewPipeAndPassReceiver(),
                        std::move(client_remote));
  }
  ~MidiHostTest() override {
    session_.reset();
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
    host_->SendData(port, data_, base::TimeTicks());
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

  int GetNumberOfBadMessages() { return rph_->bad_msg_count(); }

 private:
  BrowserTaskEnvironment task_environment_;
  std::unique_ptr<BrowserContext> browser_context_;
  std::unique_ptr<MockRenderProcessHost> rph_;

  std::vector<uint8_t> data_;
  int32_t port_id_;
  base::WeakPtr<FakeMidiManagerFactory> factory_;
  std::unique_ptr<midi::MidiService> service_;
  std::unique_ptr<MidiHostForTesting> host_;
  mojo::Remote<midi::mojom::MidiSession> session_;

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
  EXPECT_EQ(1, GetNumberOfBadMessages());

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

}  // namespace content
