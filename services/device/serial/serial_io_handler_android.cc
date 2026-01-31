// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_io_handler_android.h"

#include "base/task/thread_pool.h"

namespace device {

SerialIoHandlerAndroid::SerialIoHandlerAndroid(
    const base::FilePath& port,
    scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner,
    base::WeakPtr<SerialDeviceEnumeratorAndroid> enumerator)
    : SerialIoHandlerPosix(port, std::move(ui_thread_task_runner)),
      enumerator_(std::move(enumerator)) {}

SerialIoHandlerAndroid::~SerialIoHandlerAndroid() = default;

void SerialIoHandlerAndroid::OpenImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  if (!enumerator_) {
    OnPathOpenError(task_runner, "No enumerator to open port", port().value());
    return;
  }
  // base::Unretained is safe because |enumerator_| is owned by
  // |SerialPortManagerImpl| object, which is only destroyed on shutdown.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(
          &SerialDeviceEnumeratorAndroid::OpenPath,
          base::Unretained(enumerator_.get()), port(),
          base::BindOnce(&SerialIoHandler::OnPathOpened, this, task_runner),
          base::BindOnce(&SerialIoHandler::OnPathOpenError, this,
                         task_runner)));
}

bool SerialIoHandlerAndroid::PostOpen() {
  // Override SerialIoHandlerPosix::PostOpen() with no-op, because the exclusive
  // mode and non-blocking flag are set in ChromeSerialManager.openPort().
  return true;
}

}  // namespace device
