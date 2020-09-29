// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/serial/serial_device_enumerator.h"

#include <utility>

#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/device_event_log/device_event_log.h"

#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#include "services/device/serial/serial_device_enumerator_linux.h"
#elif defined(OS_MAC)
#include "services/device/serial/serial_device_enumerator_mac.h"
#elif defined(OS_WIN)
#include "services/device/serial/serial_device_enumerator_win.h"
#endif

namespace device {

// static
std::unique_ptr<SerialDeviceEnumerator> SerialDeviceEnumerator::Create(
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner) {
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
  return SerialDeviceEnumeratorLinux::Create();
#elif defined(OS_MAC)
  return std::make_unique<SerialDeviceEnumeratorMac>();
#elif defined(OS_WIN)
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

base::Optional<base::FilePath> SerialDeviceEnumerator::GetPathFromToken(
    const base::UnguessableToken& token,
    bool use_alternate_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto it = ports_.find(token);
  if (it == ports_.end())
    return base::nullopt;

#if defined(OS_MAC)
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
