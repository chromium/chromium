// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_io_handler_win.h"

#define INITGUID
#include <devpkey.h>
#include <setupapi.h>
#include <windows.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "device/base/device_info_query_win.h"
#include "device/base/device_monitor_win.h"
#include "services/device/serial/serial_device_enumerator_win.h"

namespace device {

namespace {

int BitrateToSpeedConstant(int bitrate) {
#define BITRATE_TO_SPEED_CASE(x) \
  case x:                        \
    return CBR_##x;
  switch (bitrate) {
    BITRATE_TO_SPEED_CASE(110);
    BITRATE_TO_SPEED_CASE(300);
    BITRATE_TO_SPEED_CASE(600);
    BITRATE_TO_SPEED_CASE(1200);
    BITRATE_TO_SPEED_CASE(2400);
    BITRATE_TO_SPEED_CASE(4800);
    BITRATE_TO_SPEED_CASE(9600);
    BITRATE_TO_SPEED_CASE(14400);
    BITRATE_TO_SPEED_CASE(19200);
    BITRATE_TO_SPEED_CASE(38400);
    BITRATE_TO_SPEED_CASE(57600);
    BITRATE_TO_SPEED_CASE(115200);
    BITRATE_TO_SPEED_CASE(128000);
    BITRATE_TO_SPEED_CASE(256000);
    default:
      // If the bitrate doesn't match that of one of the standard
      // index constants, it may be provided as-is to the DCB
      // structure, according to MSDN.
      return bitrate;
  }
#undef BITRATE_TO_SPEED_CASE
}

int DataBitsEnumToConstant(mojom::SerialDataBits data_bits) {
  switch (data_bits) {
    case mojom::SerialDataBits::SEVEN:
      return 7;
    case mojom::SerialDataBits::EIGHT:
    default:
      return 8;
  }
}

int ParityBitEnumToConstant(mojom::SerialParityBit parity_bit) {
  switch (parity_bit) {
    case mojom::SerialParityBit::EVEN:
      return EVENPARITY;
    case mojom::SerialParityBit::ODD:
      return ODDPARITY;
    case mojom::SerialParityBit::NO_PARITY:
    default:
      return NOPARITY;
  }
}

int StopBitsEnumToConstant(mojom::SerialStopBits stop_bits) {
  switch (stop_bits) {
    case mojom::SerialStopBits::TWO:
      return TWOSTOPBITS;
    case mojom::SerialStopBits::ONE:
    default:
      return ONESTOPBIT;
  }
}

int SpeedConstantToBitrate(int speed) {
#define SPEED_TO_BITRATE_CASE(x) \
  case CBR_##x:                  \
    return x;
  switch (speed) {
    SPEED_TO_BITRATE_CASE(110);
    SPEED_TO_BITRATE_CASE(300);
    SPEED_TO_BITRATE_CASE(600);
    SPEED_TO_BITRATE_CASE(1200);
    SPEED_TO_BITRATE_CASE(2400);
    SPEED_TO_BITRATE_CASE(4800);
    SPEED_TO_BITRATE_CASE(9600);
    SPEED_TO_BITRATE_CASE(14400);
    SPEED_TO_BITRATE_CASE(19200);
    SPEED_TO_BITRATE_CASE(38400);
    SPEED_TO_BITRATE_CASE(57600);
    SPEED_TO_BITRATE_CASE(115200);
    SPEED_TO_BITRATE_CASE(128000);
    SPEED_TO_BITRATE_CASE(256000);
    default:
      // If it's not one of the standard index constants,
      // it should be an integral baud rate, according to
      // MSDN.
      return speed;
  }
#undef SPEED_TO_BITRATE_CASE
}

mojom::SerialDataBits DataBitsConstantToEnum(int data_bits) {
  switch (data_bits) {
    case 7:
      return mojom::SerialDataBits::SEVEN;
    case 8:
    default:
      return mojom::SerialDataBits::EIGHT;
  }
}

mojom::SerialParityBit ParityBitConstantToEnum(int parity_bit) {
  switch (parity_bit) {
    case EVENPARITY:
      return mojom::SerialParityBit::EVEN;
    case ODDPARITY:
      return mojom::SerialParityBit::ODD;
    case NOPARITY:
    default:
      return mojom::SerialParityBit::NO_PARITY;
  }
}

mojom::SerialStopBits StopBitsConstantToEnum(int stop_bits) {
  switch (stop_bits) {
    case TWOSTOPBITS:
      return mojom::SerialStopBits::TWO;
    case ONESTOPBIT:
    default:
      return mojom::SerialStopBits::ONE;
  }
}

}  // namespace

// static
scoped_refptr<SerialIoHandler> SerialIoHandler::Create(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner) {
  return new SerialIoHandlerWin(port, std::move(ui_thread_task_runner));
}

class SerialIoHandlerWin::UiThreadHelper final
    : public DeviceMonitorWin::Observer {
 public:
  UiThreadHelper(
      base::WeakPtr<SerialIoHandlerWin> io_handler,
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner)
      : device_observer_(this),
        io_handler_(io_handler),
        io_thread_task_runner_(io_thread_task_runner) {}

  ~UiThreadHelper() { DCHECK_CALLED_ON_VALID_THREAD(thread_checker_); }

  static void Start(UiThreadHelper* self) {
    DETACH_FROM_THREAD(self->thread_checker_);
    DeviceMonitorWin* device_monitor = DeviceMonitorWin::GetForAllInterfaces();
    if (device_monitor)
      self->device_observer_.Add(device_monitor);
  }

 private:
  // DeviceMonitorWin::Observer
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::string& device_path) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    io_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SerialIoHandlerWin::OnDeviceRemoved,
                                  io_handler_, device_path));
  }

  THREAD_CHECKER(thread_checker_);
  ScopedObserver<DeviceMonitorWin, DeviceMonitorWin::Observer> device_observer_;

  // This weak pointer is only valid when checked on this task runner.
  base::WeakPtr<SerialIoHandlerWin> io_handler_;
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(UiThreadHelper);
};

void SerialIoHandlerWin::OnDeviceRemoved(const std::string& device_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DeviceInfoQueryWin device_info_query;
  if (!device_info_query.device_info_list_valid()) {
    DVPLOG(1) << "Failed to create a device information set";
    return;
  }

  // This will add the device so we can query driver info.
  if (!device_info_query.AddDevice(device_path)) {
    DVPLOG(1) << "Failed to get device interface data for " << device_path;
    return;
  }

  if (!device_info_query.GetDeviceInfo()) {
    DVPLOG(1) << "Failed to get device info for " << device_path;
    return;
  }

  std::string friendly_name;
  if (!device_info_query.GetDeviceStringProperty(DEVPKEY_Device_FriendlyName,
                                                 &friendly_name)) {
    DVPLOG(1) << "Failed to get device service property";
    return;
  }

  base::Optional<base::FilePath> path =
      SerialDeviceEnumeratorWin::GetPath(friendly_name);
  if (!path) {
    DVPLOG(1) << "Failed to get device path from \"" << friendly_name << "\".";
    return;
  }

  if (port() == *path)
    CancelRead(mojom::SerialReceiveError::DEVICE_LOST);
}

bool SerialIoHandlerWin::PostOpen() {
  DCHECK(!comm_context_);
  DCHECK(!read_context_);
  DCHECK(!write_context_);

  base::MessageLoopCurrentForIO::Get()->RegisterIOHandler(
      file().GetPlatformFile(), this);

  comm_context_.reset(new base::MessagePumpForIO::IOContext());
  read_context_.reset(new base::MessagePumpForIO::IOContext());
  write_context_.reset(new base::MessagePumpForIO::IOContext());

  scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner =
      base::ThreadTaskRunnerHandle::Get();
  helper_ =
      new UiThreadHelper(weak_factory_.GetWeakPtr(), io_thread_task_runner);
  ui_thread_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&UiThreadHelper::Start, helper_));

  // A ReadIntervalTimeout of MAXDWORD will cause async reads to complete
  // immediately with any data that's available, even if there is none.
  // This is OK because we never issue a read request until WaitCommEvent
  // signals that data is available.
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  if (!::SetCommTimeouts(file().GetPlatformFile(), &timeouts)) {
    VPLOG(1) << "Failed to set serial timeouts";
    return false;
  }

  return true;
}

void SerialIoHandlerWin::ReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_read_buffer());
  DCHECK(file().IsValid());

  if (!SetCommMask(file().GetPlatformFile(), EV_RXCHAR)) {
    VPLOG(1) << "Failed to set serial event flags";
  }

  event_mask_ = 0;
  BOOL ok = ::WaitCommEvent(file().GetPlatformFile(), &event_mask_,
                            &comm_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    VPLOG(1) << "Failed to receive serial event";
    QueueReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
  }
  is_comm_pending_ = true;
}

void SerialIoHandlerWin::WriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_write_buffer());
  DCHECK(file().IsValid());

  BOOL ok = ::WriteFile(file().GetPlatformFile(), pending_write_buffer(),
                        pending_write_buffer_len(), NULL,
                        &write_context_->overlapped);
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    VPLOG(1) << "Write failed";
    QueueWriteCompleted(0, mojom::SerialSendError::SYSTEM_ERROR);
  }
}

void SerialIoHandlerWin::CancelReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());
  ::CancelIo(file().GetPlatformFile());
}

void SerialIoHandlerWin::CancelWriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());
  ::CancelIo(file().GetPlatformFile());
}

bool SerialIoHandlerWin::ConfigurePortImpl() {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    VPLOG(1) << "Failed to get serial port info";
    return false;
  }

  // Set up some sane default options that are not configurable.
  config.fBinary = TRUE;
  config.fParity = TRUE;
  config.fAbortOnError = TRUE;
  config.fOutxDsrFlow = FALSE;
  config.fDtrControl = DTR_CONTROL_ENABLE;
  config.fDsrSensitivity = FALSE;
  config.fOutX = FALSE;
  config.fInX = FALSE;

  DCHECK(options().bitrate);
  config.BaudRate = BitrateToSpeedConstant(options().bitrate);

  DCHECK(options().data_bits != mojom::SerialDataBits::NONE);
  config.ByteSize = DataBitsEnumToConstant(options().data_bits);

  DCHECK(options().parity_bit != mojom::SerialParityBit::NONE);
  config.Parity = ParityBitEnumToConstant(options().parity_bit);

  DCHECK(options().stop_bits != mojom::SerialStopBits::NONE);
  config.StopBits = StopBitsEnumToConstant(options().stop_bits);

  DCHECK(options().has_cts_flow_control);
  if (options().cts_flow_control) {
    config.fOutxCtsFlow = TRUE;
    config.fRtsControl = RTS_CONTROL_HANDSHAKE;
  } else {
    config.fOutxCtsFlow = FALSE;
    config.fRtsControl = RTS_CONTROL_ENABLE;
  }

  if (!SetCommState(file().GetPlatformFile(), &config)) {
    VPLOG(1) << "Failed to set serial port info";
    return false;
  }
  return true;
}

SerialIoHandlerWin::SerialIoHandlerWin(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : SerialIoHandler(port, std::move(ui_thread_task_runner)),
      event_mask_(0),
      is_comm_pending_(false),
      helper_(nullptr) {}

SerialIoHandlerWin::~SerialIoHandlerWin() {
  ui_thread_task_runner()->DeleteSoon(FROM_HERE, helper_);
}

void SerialIoHandlerWin::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (context == comm_context_.get()) {
    DWORD errors;
    COMSTAT status;
    if (!ClearCommError(file().GetPlatformFile(), &errors, &status) ||
        errors != 0) {
      if (errors & CE_BREAK) {
        ReadCompleted(0, mojom::SerialReceiveError::BREAK);
      } else if (errors & CE_FRAME) {
        ReadCompleted(0, mojom::SerialReceiveError::FRAME_ERROR);
      } else if (errors & CE_OVERRUN) {
        ReadCompleted(0, mojom::SerialReceiveError::OVERRUN);
      } else if (errors & CE_RXOVER) {
        ReadCompleted(0, mojom::SerialReceiveError::BUFFER_OVERFLOW);
      } else if (errors & CE_RXPARITY) {
        ReadCompleted(0, mojom::SerialReceiveError::PARITY_ERROR);
      } else {
        ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
      }
      return;
    }

    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
    } else if (pending_read_buffer()) {
      BOOL ok = ::ReadFile(file().GetPlatformFile(), pending_read_buffer(),
                           pending_read_buffer_len(), NULL,
                           &read_context_->overlapped);
      if (!ok && GetLastError() != ERROR_IO_PENDING) {
        VPLOG(1) << "Read failed";
        ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
      }
    }
  } else if (context == read_context_.get()) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
    } else {
      ReadCompleted(bytes_transferred,
                    error == ERROR_SUCCESS
                        ? mojom::SerialReceiveError::NONE
                        : mojom::SerialReceiveError::SYSTEM_ERROR);
    }
  } else if (context == write_context_.get()) {
    DCHECK(pending_write_buffer());
    if (write_canceled()) {
      WriteCompleted(0, write_cancel_reason());
    } else if (error != ERROR_SUCCESS && error != ERROR_OPERATION_ABORTED) {
      WriteCompleted(0, mojom::SerialSendError::SYSTEM_ERROR);
      if (error == ERROR_GEN_FAILURE && IsReadPending()) {
        // For devices using drivers such as FTDI, CP2xxx, when device is
        // disconnected, the context is comm_context_ and the error is
        // ERROR_OPERATION_ABORTED.
        // However, for devices using CDC-ACM driver, when device is
        // disconnected, the context is write_context_ and the error is
        // ERROR_GEN_FAILURE. In this situation, in addition to a write error
        // signal, also need to generate a read error signal
        // mojom::SerialOnReceiveError which will notify the app about the
        // disconnection.
        CancelRead(mojom::SerialReceiveError::SYSTEM_ERROR);
      }
    } else {
      WriteCompleted(bytes_transferred,
                     error == ERROR_SUCCESS
                         ? mojom::SerialSendError::NONE
                         : mojom::SerialSendError::SYSTEM_ERROR);
    }
  } else {
    NOTREACHED() << "Invalid IOContext";
  }
}

bool SerialIoHandlerWin::Flush() const {
  if (!PurgeComm(file().GetPlatformFile(), PURGE_RXCLEAR | PURGE_TXCLEAR)) {
    VPLOG(1) << "Failed to flush serial port";
    return false;
  }
  return true;
}

mojom::SerialPortControlSignalsPtr SerialIoHandlerWin::GetControlSignals()
    const {
  DWORD status;
  if (!GetCommModemStatus(file().GetPlatformFile(), &status)) {
    VPLOG(1) << "Failed to get port control signals";
    return mojom::SerialPortControlSignalsPtr();
  }

  auto signals = mojom::SerialPortControlSignals::New();
  signals->dcd = (status & MS_RLSD_ON) != 0;
  signals->cts = (status & MS_CTS_ON) != 0;
  signals->dsr = (status & MS_DSR_ON) != 0;
  signals->ri = (status & MS_RING_ON) != 0;
  return signals;
}

bool SerialIoHandlerWin::SetControlSignals(
    const mojom::SerialHostControlSignals& signals) {
  if (signals.has_dtr && !EscapeCommFunction(file().GetPlatformFile(),
                                             signals.dtr ? SETDTR : CLRDTR)) {
    VPLOG(1) << "Failed to configure DTR signal";
    return false;
  }
  if (signals.has_rts && !EscapeCommFunction(file().GetPlatformFile(),
                                             signals.rts ? SETRTS : CLRRTS)) {
    VPLOG(1) << "Failed to configure RTS signal";
    return false;
  }
  if (signals.has_brk &&
      !EscapeCommFunction(file().GetPlatformFile(),
                          signals.brk ? SETBREAK : CLRBREAK)) {
    VPLOG(1) << "Failed to configure break signal";
    return false;
  }

  return true;
}

mojom::SerialConnectionInfoPtr SerialIoHandlerWin::GetPortInfo() const {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    VPLOG(1) << "Failed to get serial port info";
    return mojom::SerialConnectionInfoPtr();
  }
  auto info = mojom::SerialConnectionInfo::New();
  info->bitrate = SpeedConstantToBitrate(config.BaudRate);
  info->data_bits = DataBitsConstantToEnum(config.ByteSize);
  info->parity_bit = ParityBitConstantToEnum(config.Parity);
  info->stop_bits = StopBitsConstantToEnum(config.StopBits);
  info->cts_flow_control = config.fOutxCtsFlow != 0;
  return info;
}

}  // namespace device
