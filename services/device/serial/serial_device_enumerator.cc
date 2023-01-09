// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator.h"

#include <utility>

#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "services/device/serial/serial_device_enumerator_linux.h"
#elif BUILDFLAG(IS_MAC)
#include "services/device/serial/serial_device_enumerator_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "services/device/serial/serial_device_enumerator_win.h"
#endif

namespace device {

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return SerialDeviceEnumeratorLinux::Create();
#elif BUILDFLAG(IS_MAC)
  return std::make_unique<SerialDeviceEnumeratorMac>();
#elif BUILDFLAG(IS_WIN)
  return std::make_unique<SerialDeviceEnumeratorWin>(std::move(ui_task_runner));
#else
#error "No implementation of SerialDeviceEnumerator on this platform."
#endif
}

SerialDeviceEnumerator::SerialDeviceEnumerator() = default;

SerialDeviceEnumerator::~SerialDeviceEnumerator() = default;

std::vector<mojom::SerialPortInfoPtr> SerialDeviceEnumerator::GetDevices() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::vector<mojom::SerialPortInfoPtr> ports;
  ports.reserve(ports_.size());
  for (const auto& map_entry : ports_)
    ports.push_back(map_entry.second->Clone());
  return ports;
}

void SerialDeviceEnumerator::AddObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.AddObserver(observer);
}

void SerialDeviceEnumerator::RemoveObserver(Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observer_list_.RemoveObserver(observer);
}

absl::optional<base::FilePath> SerialDeviceEnumerator::GetPathFromToken(
    const base::UnguessableToken& token,
    bool use_alternate_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = ports_.find(token);
  if (it == ports_.end())
    return absl::nullopt;

#if BUILDFLAG(IS_MAC)
  if (use_alternate_path)
    return it->second->alternate_path;
#endif

  return it->second->path;
}

void SerialDeviceEnumerator::AddPort(mojom::SerialPortInfoPtr port) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::UnguessableToken token = port->token;
  auto result = ports_.insert(std::make_pair(token, std::move(port)));
  DCHECK(result.second);  // |ports_| should not already contain |token|.

  for (auto& observer : observer_list_)
    observer.OnPortAdded(*result.first->second);
}

void SerialDeviceEnumerator::RemovePort(base::UnguessableToken token) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = ports_.find(token);
  DCHECK(it != ports_.end());
  mojom::SerialPortInfoPtr port = std::move(it->second);

  SERIAL_LOG(EVENT) << "Serial device removed: path=" << port->path;

  ports_.erase(it);

  for (auto& observer : observer_list_)
    observer.OnPortRemoved(*port);
}

}  // namespace device
