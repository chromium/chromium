// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_io_handler_win.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/sequence_checker.h"
#include "base/task/current_thread.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"

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

bool SerialIoHandlerWin::PostOpen() {
  DCHECK(!read_context_);
  DCHECK(!write_context_);

  base::CurrentIOThread::Get()->RegisterIOHandler(file().GetPlatformFile(),
                                                  this);

  read_context_ = std::make_unique<base::MessagePumpForIO::IOContext>();
  write_context_ = std::make_unique<base::MessagePumpForIO::IOContext>();

  // Based on the MSDN documentation setting both ReadIntervalTimeout and
  // ReadTotalTimeoutMultiplier to MAXDWORD should cause ReadFile() to return
  // immediately if there is data in the buffer or when a byte arrives while
  // waiting.
  //
  // ReadTotalTimeoutConstant is set to a value low enough to ensure that the
  // timeout case is exercised frequently but high enough to avoid unnecessary
  // wakeups as it is not possible to have ReadFile() return immediately when a
  // byte is received without specifying a timeout.
  //
  // https://docs.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-commtimeouts#remarks
  COMMTIMEOUTS timeouts = {0};
  timeouts.ReadIntervalTimeout = MAXDWORD;
  timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
  timeouts.ReadTotalTimeoutConstant = base::Minutes(5).InMilliseconds();
  if (!::SetCommTimeouts(file().GetPlatformFile(), &timeouts)) {
    SERIAL_PLOG(DEBUG) << "Failed to set serial timeouts";
    return false;
  }

  return true;
}

void SerialIoHandlerWin::ReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsReadPending());

  ClearPendingError();
  if (!IsReadPending())
    return;

  if (!ReadFile(file().GetPlatformFile(), pending_read_buffer().data(),
                pending_read_buffer().size(), nullptr,
                &read_context_->overlapped) &&
      GetLastError() != ERROR_IO_PENDING) {
    OnIOCompleted(read_context_.get(), 0, GetLastError());
  }
}

void SerialIoHandlerWin::WriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(IsWritePending());

  if (!WriteFile(file().GetPlatformFile(), pending_write_buffer().data(),
                 pending_write_buffer().size(), nullptr,
                 &write_context_->overlapped) &&
      GetLastError() != ERROR_IO_PENDING) {
    OnIOCompleted(write_context_.get(), 0, GetLastError());
  }
}

void SerialIoHandlerWin::CancelReadImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());

  if (!PurgeComm(file().GetPlatformFile(), PURGE_RXABORT))
    SERIAL_PLOG(DEBUG) << "RX abort failed";
}

void SerialIoHandlerWin::CancelWriteImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(file().IsValid());
  if (!PurgeComm(file().GetPlatformFile(), PURGE_TXABORT))
    SERIAL_PLOG(DEBUG) << "TX abort failed";
}

bool SerialIoHandlerWin::ConfigurePortImpl() {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    SERIAL_PLOG(DEBUG) << "Failed to get serial port info";
    return false;
  }

  // Set up some sane default options that are not configurable.
  config.fBinary = TRUE;
  config.fParity = TRUE;
  config.fAbortOnError = FALSE;
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
    SERIAL_PLOG(DEBUG) << "Failed to set serial port info";
    return false;
  }
  return true;
}

SerialIoHandlerWin::SerialIoHandlerWin(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner)
    : SerialIoHandler(port, std::move(ui_thread_task_runner)),
      base::MessagePumpForIO::IOHandler(FROM_HERE) {}

SerialIoHandlerWin::~SerialIoHandlerWin() = default;

void SerialIoHandlerWin::OnIOCompleted(
    base::MessagePumpForIO::IOContext* context,
    DWORD bytes_transferred,
    DWORD error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (context == read_context_.get()) {
    if (read_canceled()) {
      ReadCompleted(bytes_transferred, read_cancel_reason());
    } else if (error == ERROR_SUCCESS || error == ERROR_OPERATION_ABORTED) {
      ReadCompleted(bytes_transferred, mojom::SerialReceiveError::NONE);
    } else if (error == ERROR_ACCESS_DENIED || error == ERROR_BAD_COMMAND ||
               error == ERROR_DEVICE_REMOVED) {
      ReadCompleted(0, mojom::SerialReceiveError::DEVICE_LOST);
    } else {
      SERIAL_LOG(DEBUG) << "Read failed: "
                        << logging::SystemErrorCodeToString(error);
      ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
    }
  } else if (context == write_context_.get()) {
    DCHECK(IsWritePending());
    if (write_canceled()) {
      WriteCompleted(0, write_cancel_reason());
    } else if (error == ERROR_SUCCESS || error == ERROR_OPERATION_ABORTED) {
      WriteCompleted(bytes_transferred, mojom::SerialSendError::NONE);
    } else if (error == ERROR_GEN_FAILURE) {
      WriteCompleted(0, mojom::SerialSendError::DISCONNECTED);
    } else {
      SERIAL_LOG(DEBUG) << "Write failed: "
                        << logging::SystemErrorCodeToString(error);
      if (error == ERROR_GEN_FAILURE && IsReadPending()) {
        // For devices using drivers such as FTDI, CP2xxx, when device is
        // disconnected, the context is |read_context_| and the error is
        // ERROR_OPERATION_ABORTED.
        // However, for devices using CDC-ACM driver, when device is
        // disconnected, the context is |write_context_| and the error is
        // ERROR_GEN_FAILURE. In this situation, in addition to a write error
        // signal, also need to generate a read error signal
        // mojom::SerialOnReceiveError which will notify the app about the
        // disconnection.
        CancelRead(mojom::SerialReceiveError::SYSTEM_ERROR);
      }
      WriteCompleted(0, mojom::SerialSendError::SYSTEM_ERROR);
    }
  } else {
    NOTREACHED_IN_MIGRATION() << "Invalid IOContext";
  }
}

void SerialIoHandlerWin::ClearPendingError() {
  DWORD errors;
  if (!ClearCommError(file().GetPlatformFile(), &errors, nullptr)) {
    SERIAL_PLOG(DEBUG) << "Failed to clear communication error";
    return;
  }

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
  } else if (errors != 0) {
    NOTIMPLEMENTED() << "Unexpected communication error: " << std::hex
                     << errors;
    ReadCompleted(0, mojom::SerialReceiveError::SYSTEM_ERROR);
  }
}

void SerialIoHandlerWin::Flush(mojom::SerialPortFlushMode mode) const {
  DWORD flags;
  switch (mode) {
    case mojom::SerialPortFlushMode::kReceiveAndTransmit:
      flags = PURGE_RXCLEAR | PURGE_TXCLEAR;
      break;
    case mojom::SerialPortFlushMode::kReceive:
      flags = PURGE_RXCLEAR;
      break;
    case mojom::SerialPortFlushMode::kTransmit:
      flags = PURGE_TXCLEAR;
      break;
  }

  if (!PurgeComm(file().GetPlatformFile(), flags))
    SERIAL_PLOG(DEBUG) << "Failed to flush serial port";
}

void SerialIoHandlerWin::Drain() {
  if (!FlushFileBuffers(file().GetPlatformFile()))
    SERIAL_PLOG(DEBUG) << "Failed to drain serial port";
}

mojom::SerialPortControlSignalsPtr SerialIoHandlerWin::GetControlSignals()
    const {
  DWORD status;
  if (!GetCommModemStatus(file().GetPlatformFile(), &status)) {
    SERIAL_PLOG(DEBUG) << "Failed to get port control signals";
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
    SERIAL_PLOG(DEBUG) << "Failed to configure data-terminal-ready signal";
    return false;
  }
  if (signals.has_rts && !EscapeCommFunction(file().GetPlatformFile(),
                                             signals.rts ? SETRTS : CLRRTS)) {
    SERIAL_PLOG(DEBUG) << "Failed to configure request-to-send signal";
    return false;
  }
  if (signals.has_brk &&
      !EscapeCommFunction(file().GetPlatformFile(),
                          signals.brk ? SETBREAK : CLRBREAK)) {
    SERIAL_PLOG(DEBUG) << "Failed to configure break signal";
    return false;
  }

  return true;
}

mojom::SerialConnectionInfoPtr SerialIoHandlerWin::GetPortInfo() const {
  DCB config = {0};
  config.DCBlength = sizeof(config);
  if (!GetCommState(file().GetPlatformFile(), &config)) {
    SERIAL_PLOG(DEBUG) << "Failed to get serial port info";
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
