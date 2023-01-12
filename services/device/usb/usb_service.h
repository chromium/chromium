// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_SERVICE_H_
#define SERVICES_DEVICE_USB_USB_SERVICE_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"

namespace device {

class UsbDevice;

// The USB service handles creating and managing an event handler thread that is
// used to manage and dispatch USB events. It is also responsible for device
// discovery on the system, which allows it to re-use device handles to prevent
// competition for the same USB device.
class UsbService {
 public:
  using GetDevicesCallback =
      base::OnceCallback<void(const std::vector<scoped_refptr<UsbDevice>>&)>;

  class Observer {
   public:
    virtual ~Observer();

    // These events are delivered from the thread on which the UsbService object
    // was created.
    virtual void OnDeviceAdded(scoped_refptr<UsbDevice> device);
    virtual void OnDeviceRemoved(scoped_refptr<UsbDevice> device);
    // For observers that need to process device removal after others have run.
    // Should not depend on any other service's knowledge of connected devices.
    virtual void OnDeviceRemovedCleanup(scoped_refptr<UsbDevice> device);

    // Notifies the observer that the UsbService it depends on is shutting down.
    virtual void WillDestroyUsbService();
  };

  // These task traits are to be used for posting blocking tasks to the thread
  // pool.
  static constexpr base::TaskTraits kBlockingTaskTraits = {
      base::MayBlock(), base::TaskPriority::USER_VISIBLE,
      base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN};

  // Returns nullptr when initialization fails.
  static std::unique_ptr<UsbService> Create();

  // Creates a SequencedTaskRunner with kBlockingTaskTraits.
  static scoped_refptr<base::SequencedTaskRunner> CreateBlockingTaskRunner();

  UsbService(const UsbService&) = delete;
  UsbService& operator=(const UsbService&) = delete;

  virtual ~UsbService();

  scoped_refptr<UsbDevice> GetDevice(const std::string& guid);

  // Enumerates available devices.
  virtual void GetDevices(GetDevicesCallback callback);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Methods to add and remove devices for testing purposes. Only a device added
  // by this method can be removed by RemoveDeviceForTesting().
  void AddDeviceForTesting(scoped_refptr<UsbDevice> device);
  void RemoveDeviceForTesting(const std::string& device_guid);
  void GetTestDevices(std::vector<scoped_refptr<UsbDevice>>* devices);

 protected:
  UsbService();

  void NotifyDeviceAdded(scoped_refptr<UsbDevice> device);
  void NotifyDeviceRemoved(scoped_refptr<UsbDevice> device);

  // Subclasses must call this method as the first line of their destructor so
  // that observers can clean up before any fields are destroyed.
  void NotifyWillDestroyUsbService();

  std::unordered_map<std::string, scoped_refptr<UsbDevice>>& devices() {
    return devices_;
  }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  std::unordered_map<std::string, scoped_refptr<UsbDevice>> devices_;
  std::unordered_set<std::string> testing_devices_;
  base::ObserverList<Observer, true>::Unchecked observer_list_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_SERVICE_H_
