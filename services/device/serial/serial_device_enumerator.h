// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_

#include <map>
#include <memory>
#include <optional>
#include <vector>

#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/unguessable_token.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_io_handler.h"

namespace base {
class FilePath;
}

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumerator {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnPortAdded(const mojom::SerialPortInfo& port) = 0;
    virtual void OnPortRemoved(const mojom::SerialPortInfo& port) = 0;
    virtual void OnPortConnectedStateChanged(
        const mojom::SerialPortInfo& port) = 0;
  };

  static std::unique_ptr<SerialDeviceEnumerator> Create(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);

  SerialDeviceEnumerator();
  virtual ~SerialDeviceEnumerator();

  std::vector<mojom::SerialPortInfoPtr> GetDevices();
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::optional<base::FilePath> GetPathFromToken(
      const base::UnguessableToken& token,
      bool use_alternate_path);

 protected:
  // These helper methods take care of managing |ports_| and notifying
  // observers. |port|s passed to AddPort() must be unique and the |token|
  // passed to RemovePort() must have previously been added.
  void AddPort(mojom::SerialPortInfoPtr port);
  void RemovePort(base::UnguessableToken token);
  void UpdatePortConnectedState(base::UnguessableToken token,
                                bool is_connected);

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  std::map<base::UnguessableToken, mojom::SerialPortInfoPtr> ports_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
