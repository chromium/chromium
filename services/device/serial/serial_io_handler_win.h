// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/task/single_thread_task_runner.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

class SerialIoHandlerWin : public SerialIoHandler,
                           public base::MessagePumpForIO::IOHandler {
 public:
  SerialIoHandlerWin(const SerialIoHandlerWin&) = delete;
  SerialIoHandlerWin& operator=(const SerialIoHandlerWin&) = delete;

 protected:
  // SerialIoHandler implementation.
  void ReadImpl() override;
  void WriteImpl() override;
  void CancelReadImpl() override;
  void CancelWriteImpl() override;
  bool ConfigurePortImpl() override;
  void Flush(mojom::SerialPortFlushMode mode) const override;
  void Drain() override;
  mojom::SerialPortControlSignalsPtr GetControlSignals() const override;
  bool SetControlSignals(
      const mojom::SerialHostControlSignals& control_signals) override;
  mojom::SerialConnectionInfoPtr GetPortInfo() const override;
  bool PostOpen() override;

 private:
  class UiThreadHelper;
  friend class SerialIoHandler;

  explicit SerialIoHandlerWin(
      const base::FilePath& port,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~SerialIoHandlerWin() override;

  // base::MessagePumpForIO::IOHandler implementation.
  void OnIOCompleted(base::MessagePumpForIO::IOContext* context,
                     DWORD bytes_transfered,
                     DWORD error) override;

  void ClearPendingError();
  void OnDeviceRemoved(const std::wstring& device_path);

  // Context used for overlapped reads.
  std::unique_ptr<base::MessagePumpForIO::IOContext> read_context_;

  // Context used for overlapped writes.
  std::unique_ptr<base::MessagePumpForIO::IOContext> write_context_;

  // The helper lives on the UI thread and holds a weak reference back to the
  // handler that owns it.
  raw_ptr<UiThreadHelper> helper_ = nullptr;
  base::WeakPtrFactory<SerialIoHandlerWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_IO_HANDLER_WIN_H_
