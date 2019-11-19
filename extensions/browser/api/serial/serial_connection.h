// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_
#define EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/cancelable_callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/serial.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/public/cpp/system/simple_watcher.h"
#include "net/base/io_buffer.h"
#include "services/device/public/mojom/serial.mojom.h"

using content::BrowserThread;

namespace extensions {

// Encapsulates an mojo interface ptr of device::mojom::SerialPort, which
// corresponds with an open serial port in remote side(Device Service). NOTE:
// Instances of this object should only be constructed on the IO thread, and all
// methods should only be called on the IO thread unless otherwise noted.
class SerialConnection : public ApiResource,
                         public device::mojom::SerialPortClient {
 public:
  using OpenCompleteCallback = device::mojom::SerialPort::OpenCallback;
  using GetInfoCompleteCallback =
      base::OnceCallback<void(bool,
                              std::unique_ptr<api::serial::ConnectionInfo>)>;

  // This is the callback type expected by Receive. Note that an error result
  // does not necessarily imply an empty |data| string, since a receive may
  // complete partially before being interrupted by an error condition.
  using ReceiveEventCallback =
      base::RepeatingCallback<void(std::vector<uint8_t> data,
                                   api::serial::ReceiveError error)>;
  // This is the callback type expected by Send. Note that an error result
  // does not necessarily imply 0 bytes sent, since a send may complete
  // partially before being interrupted by an error condition.
  using SendCompleteCallback =
      base::OnceCallback<void(uint32_t bytes_sent,
                              api::serial::SendError error)>;

  using ConfigureCompleteCallback =
      device::mojom::SerialPort::ConfigurePortCallback;

  using FlushCompleteCallback = device::mojom::SerialPort::FlushCallback;

  using GetControlSignalsCompleteCallback = base::OnceCallback<void(
      std::unique_ptr<api::serial::DeviceControlSignals>)>;

  using SetControlSignalsCompleteCallback =
      device::mojom::SerialPort::SetControlSignalsCallback;

  SerialConnection(const std::string& owner_extension_id,
                   mojo::PendingRemote<device::mojom::SerialPort> serial_port);
  ~SerialConnection() override;

  // ApiResource override.
  bool IsPersistent() const override;

  void set_persistent(bool persistent) { persistent_ = persistent; }
  bool persistent() const { return persistent_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_buffer_size(int buffer_size);
  int buffer_size() const { return buffer_size_; }

  void set_receive_timeout(int receive_timeout);
  int receive_timeout() const { return receive_timeout_; }

  void set_send_timeout(int send_timeout);
  int send_timeout() const { return send_timeout_; }

  void SetPaused(bool paused);
  bool paused() const { return paused_; }

  void SetConnectionErrorHandler(base::OnceClosure connection_error_handler);

  // Initiates an asynchronous Open of the device. It is the caller's
  // responsibility to ensure that this SerialConnection stays alive
  // until |callback| is run.
  virtual void Open(const api::serial::ConnectionOptions& options,
                    OpenCompleteCallback callback);

  // Begins an asynchronous send operation. Calling this while a Send
  // is already pending is a no-op and returns |false| without calling
  // |callback|.
  virtual bool Send(const std::vector<uint8_t>& data,
                    SendCompleteCallback callback);

  // Start to the polling process from |receive_pipe_|.
  virtual void StartPolling(const ReceiveEventCallback& callback);

  // Flushes input and output buffers.
  void Flush(FlushCompleteCallback callback) const;

  // Configures some subset of port options for this connection.
  // Omitted options are unchanged.
  void Configure(const api::serial::ConnectionOptions& options,
                 ConfigureCompleteCallback callback);

  // Connection configuration query. Returns retrieved ConnectionInfo value via
  // |callback|, and indicates whether it's complete info. Some ConnectionInfo
  // fields are filled with local info from |this|, while some other fields must
  // be retrieved from remote SerialPort interface, which may fail.
  void GetInfo(GetInfoCompleteCallback callback) const;

  // Reads current control signals (DCD, CTS, etc.) and returns via |callback|.
  // Returns nullptr if we failed in getting values.
  void GetControlSignals(GetControlSignalsCompleteCallback callback) const;

  // Sets one or more control signals (DTR, RTS, Break). Returns result success
  // or not via |callback|.
  void SetControlSignals(device::mojom::SerialHostControlSignalsPtr signals,
                         SetControlSignalsCompleteCallback callback);

  // Initiates an asynchronous close of the device.
  void Close(base::OnceClosure callback);

  static const BrowserThread::ID kThreadId = BrowserThread::UI;

 private:
  friend class ApiResourceManager<SerialConnection>;
  static const char* service_name() { return "SerialConnectionManager"; }

  // device::mojom::SerialPortClient override.
  void OnReadError(device::mojom::SerialReceiveError error) override;
  void OnSendError(device::mojom::SerialSendError error) override;

  void OnOpen(
      mojo::ScopedDataPipeConsumerHandle consumer,
      mojo::ScopedDataPipeProducerHandle producer,
      mojo::PendingReceiver<device::mojom::SerialPortClient> client_receiver,
      OpenCompleteCallback callback,
      bool success);

  // Read data from |receive_pipe_| when the data is ready or dispatch error
  // events in error cases.
  void OnReadPipeReadableOrClosed(MojoResult result,
                                  const mojo::HandleSignalsState& state);
  void OnReadPipeClosed();

  void CreatePipe(mojo::ScopedDataPipeProducerHandle* producer,
                  mojo::ScopedDataPipeConsumerHandle* consumer);
  void SetUpReceiveDataPipe(mojo::ScopedDataPipeConsumerHandle producer);
  void SetUpSendDataPipe(mojo::ScopedDataPipeProducerHandle consumer);

  void SetTimeoutCallback();

  // Handles a receive timeout.
  void OnReceiveTimeout();

  // Handles a send timeout.
  void OnSendTimeout();

  void OnSendPipeWritableOrClosed(MojoResult result,
                                  const mojo::HandleSignalsState& state);
  void OnSendPipeClosed();

  // Handles |serial_port_| connection error.
  void OnConnectionError();

  // Handles |client_receiver_| connection error.
  void OnClientReceiverClosed();

  // Flag indicating whether or not the connection should persist when
  // its host app is suspended.
  bool persistent_;

  // User-specified connection name.
  std::string name_;

  // Size of the receive and send buffer.
  int buffer_size_;

  // Amount of time (in ms) to wait for a Read to succeed before triggering a
  // timeout response via onReceiveError.
  int receive_timeout_;

  // Amount of time (in ms) to wait for a Write to succeed before triggering
  // a timeout response.
  int send_timeout_;

  // Flag indicating that the connection is paused. A paused connection will not
  // raise new onReceive events.
  bool paused_;

  // Callback to handle the completion of a pending Receive() request.
  ReceiveEventCallback receive_event_cb_;
  base::Optional<device::mojom::SerialReceiveError> read_error_;

  // Callback to handle the completion of a pending Send() request.
  SendCompleteCallback send_complete_;
  size_t bytes_written_;

  // The data needs to be sent.
  std::vector<uint8_t> data_to_send_;

  // Closure which will trigger a receive timeout unless cancelled. Reset on
  // initialization and after every successful Receive().
  base::CancelableClosure receive_timeout_task_;

  // Write timeout closure. Reset on initialization and after every successful
  // Send().
  base::CancelableClosure send_timeout_task_;

  // Mojo interface remote corresponding with remote asynchronous I/O handler.
  mojo::Remote<device::mojom::SerialPort> serial_port_;

  // Pipe for read.
  mojo::ScopedDataPipeConsumerHandle receive_pipe_;
  mojo::SimpleWatcher receive_pipe_watcher_;

  // Pipe for send.
  mojo::ScopedDataPipeProducerHandle send_pipe_;
  mojo::SimpleWatcher send_pipe_watcher_;

  mojo::Receiver<device::mojom::SerialPortClient> client_receiver_{this};

  // Closure which is set by client and will be called when |serial_port_|
  // connection encountered an error.
  base::OnceClosure connection_error_handler_;

  base::WeakPtrFactory<SerialConnection> weak_factory_{this};
};

}  // namespace extensions

namespace mojo {

template <>
struct TypeConverter<device::mojom::SerialHostControlSignalsPtr,
                     extensions::api::serial::HostControlSignals> {
  static device::mojom::SerialHostControlSignalsPtr Convert(
      const extensions::api::serial::HostControlSignals& input);
};

template <>
struct TypeConverter<device::mojom::SerialConnectionOptionsPtr,
                     extensions::api::serial::ConnectionOptions> {
  static device::mojom::SerialConnectionOptionsPtr Convert(
      const extensions::api::serial::ConnectionOptions& input);
};

}  // namespace mojo

#endif  // EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_
