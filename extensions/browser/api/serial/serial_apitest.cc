// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/containers/extend.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/not_fatal_until.h"
#include "base/unguessable_token.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/api/serial/serial_api.h"
#include "extensions/browser/api/serial/serial_connection.h"
#include "extensions/browser/api/serial/serial_port_manager.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/common/api/serial.h"
#include "extensions/common/switches.h"
#include "extensions/test/result_catcher.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"

// Disable SIMULATE_SERIAL_PORTS only if all the following are true:
//
// 1. You have an Arduino or compatible board attached to your machine and
// properly appearing as the first virtual serial port ("first" is very loosely
// defined as whichever port shows up in serial.getPorts). We've tested only
// the Atmega32u4 Breakout Board and Arduino Leonardo; note that both these
// boards are based on the Atmel ATmega32u4, rather than the more common
// Arduino '328p with either FTDI or '8/16u2 USB interfaces. TODO: test more
// widely.
//
// 2. Your user has permission to read/write the port. For example, this might
// mean that your user is in the "tty" or "uucp" group on Ubuntu flavors of
// Linux, or else that the port's path (e.g., /dev/ttyACM0) has global
// read/write permissions.
//
// 3. You have uploaded a program to the board that does a byte-for-byte echo
// on the virtual serial port at 57600 bps. An example is at
// chrome/test/data/extensions/api_test/serial/api/serial_arduino_test.ino.
//
#define SIMULATE_SERIAL_PORTS (1)

using testing::_;
using testing::Return;

namespace extensions {
namespace {

class FakeSerialPort : public device::mojom::SerialPort {
 public:
  explicit FakeSerialPort(device::mojom::SerialPortInfoPtr info)
      : info_(std::move(info)),
        in_stream_watcher_(FROM_HERE,
                           mojo::SimpleWatcher::ArmingPolicy::MANUAL),
        out_stream_watcher_(FROM_HERE,
                            mojo::SimpleWatcher::ArmingPolicy::MANUAL) {
    options_.bitrate = 9600;
    options_.data_bits = device::mojom::SerialDataBits::EIGHT;
    options_.parity_bit = device::mojom::SerialParityBit::NO_PARITY;
    options_.stop_bits = device::mojom::SerialStopBits::ONE;
    options_.cts_flow_control = false;
    options_.has_cts_flow_control = true;
  }

  FakeSerialPort(const FakeSerialPort&) = delete;
  FakeSerialPort& operator=(const FakeSerialPort&) = delete;

  ~FakeSerialPort() override = default;

  mojo::PendingRemote<device::mojom::SerialPort> Open(
      device::mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<device::mojom::SerialPortClient> client) {
    if (receiver_.is_bound()) {
      // Port is already open.
      return mojo::NullRemote();
    }

    DCHECK(!client_.is_bound());
    DCHECK(client.is_valid());
    client_.Bind(std::move(client));

    DoConfigurePort(*options);

    return receiver_.BindNewPipeAndPassRemote();
  }

  const device::mojom::SerialPortInfo& info() { return *info_; }

 private:
  // device::mojom::SerialPort methods:
  void StartWriting(mojo::ScopedDataPipeConsumerHandle consumer) override {
    if (in_stream_) {
      return;
    }

    in_stream_ = std::move(consumer);
    in_stream_watcher_.Watch(
        in_stream_.get(),
        MOJO_HANDLE_SIGNAL_READABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&FakeSerialPort::DoWrite, base::Unretained(this)));
    in_stream_watcher_.ArmOrNotify();
  }

  void StartReading(mojo::ScopedDataPipeProducerHandle producer) override {
    if (out_stream_) {
      return;
    }

    out_stream_ = std::move(producer);
    out_stream_watcher_.Watch(
        out_stream_.get(),
        MOJO_HANDLE_SIGNAL_WRITABLE | MOJO_HANDLE_SIGNAL_PEER_CLOSED,
        MOJO_TRIGGER_CONDITION_SIGNALS_SATISFIED,
        base::BindRepeating(&FakeSerialPort::DoRead, base::Unretained(this)));
    out_stream_watcher_.ArmOrNotify();
  }

  void Flush(device::mojom::SerialPortFlushMode mode,
             FlushCallback callback) override {
    if (mode == device::mojom::SerialPortFlushMode::kReceiveAndTransmit) {
      std::move(callback).Run();
      return;
    }

    NOTREACHED_IN_MIGRATION();
  }

  void Drain(DrainCallback callback) override { NOTREACHED_IN_MIGRATION(); }

  void GetControlSignals(GetControlSignalsCallback callback) override {
    auto signals = device::mojom::SerialPortControlSignals::New();
    signals->dcd = true;
    signals->cts = true;
    signals->ri = true;
    signals->dsr = true;
    std::move(callback).Run(std::move(signals));
  }
  void SetControlSignals(device::mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCallback callback) override {
    std::move(callback).Run(true);
  }
  void ConfigurePort(device::mojom::SerialConnectionOptionsPtr options,
                     ConfigurePortCallback callback) override {
    DoConfigurePort(*options);
    std::move(callback).Run(true);
  }
  void GetPortInfo(GetPortInfoCallback callback) override {
    auto info = device::mojom::SerialConnectionInfo::New();
    info->bitrate = options_.bitrate;
    info->data_bits = options_.data_bits;
    info->parity_bit = options_.parity_bit;
    info->stop_bits = options_.stop_bits;
    info->cts_flow_control = options_.cts_flow_control;
    std::move(callback).Run(std::move(info));
  }

  void Close(bool flush, CloseCallback callback) override {
    in_stream_watcher_.Cancel();
    in_stream_.reset();
    out_stream_watcher_.Cancel();
    out_stream_.reset();
    client_.reset();
    std::move(callback).Run();
    receiver_.reset();
  }

  void DoWrite(MojoResult result, const mojo::HandleSignalsState& state) {
    base::span<const uint8_t> data;
    if (result == MOJO_RESULT_OK) {
      result = in_stream_->BeginReadData(MOJO_READ_DATA_FLAG_NONE, data);
    }
    if (result == MOJO_RESULT_OK) {
      // Control the bytes read from in_stream_ to trigger a variaty of
      // transfer cases between SerialConnection::send_pipe_.
      write_step_++;
      if ((write_step_ % 4) < 2 && data.size() > 1) {
        data = data.first(1u);
      }
      base::Extend(buffer_, data);
      in_stream_->EndReadData(data.size());
      in_stream_watcher_.ArmOrNotify();

      // Enable the notification to write this data to the out stream.
      out_stream_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_SHOULD_WAIT) {
      // If there is no space to write, wait for more space.
      in_stream_watcher_.ArmOrNotify();
      return;
    }
    if (result == MOJO_RESULT_FAILED_PRECONDITION ||
        result == MOJO_RESULT_CANCELLED) {
      // The |in_stream_| has been closed.
      in_stream_.reset();
      return;
    }
    // The code should not reach other cases.
    NOTREACHED_IN_MIGRATION();
  }

  void DoRead(MojoResult result, const mojo::HandleSignalsState& state) {
    if (result != MOJO_RESULT_OK) {
      out_stream_.reset();
      return;
    }
    if (buffer_.empty()) {
      return;
    }
    read_step_++;
    if (read_step_ == 1) {
      // Write one byte first.
      WriteOutReadData(1);
    } else if (read_step_ == 2) {
      // Write one byte in second step and trigger a break error.
      WriteOutReadData(1);
      DCHECK(client_);
      client_->OnReadError(device::mojom::SerialReceiveError::PARITY_ERROR);
      out_stream_watcher_.Cancel();
      out_stream_.reset();
      return;
    } else {
      // Write out the rest data after reconnecting.
      WriteOutReadData(buffer_.size());
    }
    out_stream_watcher_.ArmOrNotify();
  }

  void WriteOutReadData(size_t num_bytes) {
    base::span<const uint8_t> bytes = buffer_;
    bytes = bytes.first(num_bytes);

    size_t actually_written_bytes = 0;
    MojoResult result = out_stream_->WriteData(bytes, MOJO_WRITE_DATA_FLAG_NONE,
                                               actually_written_bytes);
    if (result == MOJO_RESULT_OK) {
      buffer_.erase(buffer_.begin(), buffer_.begin() + actually_written_bytes);
    }
  }

  void DoConfigurePort(const device::mojom::SerialConnectionOptions& options) {
    // Merge options.
    if (options.bitrate) {
      options_.bitrate = options.bitrate;
    }
    if (options.data_bits != device::mojom::SerialDataBits::NONE) {
      options_.data_bits = options.data_bits;
    }
    if (options.parity_bit != device::mojom::SerialParityBit::NONE) {
      options_.parity_bit = options.parity_bit;
    }
    if (options.stop_bits != device::mojom::SerialStopBits::NONE) {
      options_.stop_bits = options.stop_bits;
    }
    if (options.has_cts_flow_control) {
      DCHECK(options_.has_cts_flow_control);
      options_.cts_flow_control = options.cts_flow_control;
    }
  }

  device::mojom::SerialPortInfoPtr info_;
  mojo::Receiver<device::mojom::SerialPort> receiver_{this};

  // Currently applied connection options.
  device::mojom::SerialConnectionOptions options_;
  std::vector<uint8_t> buffer_;
  int read_step_ = 0;
  int write_step_ = 0;
  mojo::Remote<device::mojom::SerialPortClient> client_;
  mojo::ScopedDataPipeConsumerHandle in_stream_;
  mojo::SimpleWatcher in_stream_watcher_;
  mojo::ScopedDataPipeProducerHandle out_stream_;
  mojo::SimpleWatcher out_stream_watcher_;
};

class FakeSerialPortManager : public device::mojom::SerialPortManager {
 public:
  FakeSerialPortManager() {
    AddPort(base::FilePath(FILE_PATH_LITERAL("/dev/fakeserialmojo")));
    AddPort(base::FilePath(FILE_PATH_LITERAL("\\\\COM800\\")));
  }

  FakeSerialPortManager(const FakeSerialPortManager&) = delete;
  FakeSerialPortManager& operator=(const FakeSerialPortManager&) = delete;

  ~FakeSerialPortManager() override = default;

  void Bind(mojo::PendingReceiver<device::mojom::SerialPortManager> receiver) {
    receivers_.Add(this, std::move(receiver));
  }

 private:
  // device::mojom::SerialPortManager methods:
  void SetClient(mojo::PendingRemote<device::mojom::SerialPortManagerClient>
                     remote) override {
    NOTIMPLEMENTED();
  }

  void GetDevices(GetDevicesCallback callback) override {
    std::vector<device::mojom::SerialPortInfoPtr> ports;
    for (const auto& port : ports_)
      ports.push_back(port.second->info().Clone());
    std::move(callback).Run(std::move(ports));
  }

  void OpenPort(
      const base::UnguessableToken& token,
      bool use_alternate_path,
      device::mojom::SerialConnectionOptionsPtr options,
      mojo::PendingRemote<device::mojom::SerialPortClient> client,
      mojo::PendingRemote<device::mojom::SerialPortConnectionWatcher> watcher,
      OpenPortCallback callback) override {
    DCHECK(!watcher);
    auto it = ports_.find(token);
    CHECK(it != ports_.end(), base::NotFatalUntil::M130);
    std::move(callback).Run(
        it->second->Open(std::move(options), std::move(client)));
  }

  void AddPort(const base::FilePath& path) {
    auto token = base::UnguessableToken::Create();
    auto port = device::mojom::SerialPortInfo::New();
    port->token = token;
    port->path = path;
    ports_.insert(std::make_pair(
        token, std::make_unique<FakeSerialPort>(std::move(port))));
  }

  mojo::ReceiverSet<device::mojom::SerialPortManager> receivers_;
  std::map<base::UnguessableToken, std::unique_ptr<FakeSerialPort>> ports_;
};

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {
#if SIMULATE_SERIAL_PORTS
    api::SerialPortManager::OverrideBinderForTesting(base::BindRepeating(
        &SerialApiTest::BindSerialPortManager, base::Unretained(this)));
#endif
  }

  ~SerialApiTest() override {
#if SIMULATE_SERIAL_PORTS
    api::SerialPortManager::OverrideBinderForTesting(base::NullCallback());
#endif
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    port_manager_ = std::make_unique<FakeSerialPortManager>();
  }

  void FailEnumeratorRequest() { fail_enumerator_request_ = true; }

 protected:
  void BindSerialPortManager(
      mojo::PendingReceiver<device::mojom::SerialPortManager> receiver) {
    if (fail_enumerator_request_)
      return;

    port_manager_->Bind(std::move(receiver));
  }

  bool fail_enumerator_request_ = false;
  std::unique_ptr<FakeSerialPortManager> port_manager_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialFakeHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardware) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  ASSERT_TRUE(RunExtensionTest("serial/real_hardware")) << message_;
}

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialRealHardwareFail) {
  ResultCatcher catcher;
  catcher.RestrictToBrowserContext(browser()->profile());

  // chrome.serial.getDevices() should get an empty list when the serial
  // enumerator interface is unavailable.
  FailEnumeratorRequest();
  ASSERT_TRUE(RunExtensionTest("serial/real_hardware_fail")) << message_;
}

}  // namespace extensions
