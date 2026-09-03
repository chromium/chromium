// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_

#include <IOKit/IOMessage.h>
#include <stdint.h>

#include <set>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/task/sequenced_task_runner.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/xbox_controller_mac.h"

namespace device {

class XboxDataFetcher : public GamepadDataFetcher,
                        public XboxControllerMac::Delegate {
 public:
  using Factory =
      GamepadDataFetcherFactoryImpl<XboxDataFetcher, GamepadSource::kMacXbox>;

  XboxDataFetcher();
  XboxDataFetcher(const XboxDataFetcher& entry) = delete;
  XboxDataFetcher& operator=(const XboxDataFetcher& entry) = delete;
  ~XboxDataFetcher() override;

  GamepadSource source() override;

  // GamepadDataFetcher overrides
  void GetGamepadData(bool devices_changed_hint) override;
  void PlayEffect(int source_id,
                  mojom::GamepadHapticEffectType,
                  mojom::GamepadEffectParametersPtr,
                  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback,
                  scoped_refptr<base::SequencedTaskRunner>) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback,
      scoped_refptr<base::SequencedTaskRunner>) override;

  XboxControllerMac* ControllerForLocation(UInt32 location_id);

 private:
  static void DeviceAdded(void* context, io_iterator_t iterator);
  static void DeviceRemoved(void* context, io_iterator_t iterator);
  static void InterestCallback(void* context,
                               io_service_t service,
                               IOMessage message_type,
                               void* message_argument);

  bool TryOpenDevice(io_service_t service);
  bool RegisterForNotifications();
  bool RegisterForDeviceNotifications(int vendor_id, int product_id);
  bool RegisterForInterestNotifications(io_service_t service);
  void PendingServiceBecameAvailable(io_service_t service);
  void UnregisterFromNotifications();

  void OnAddedToProvider() override;
  void AddController(XboxControllerMac* controller);
  void RemoveController(XboxControllerMac* controller);
  void RemoveControllerByLocationID(uint32_t id);

  // XboxControllerMac::Delegate methods.
  void XboxControllerGotData(XboxControllerMac* controller,
                             const XboxControllerMac::Data& data) override;
  void XboxControllerGotGuideData(XboxControllerMac* controller,
                                  bool guide) override;
  void XboxControllerError(XboxControllerMac* controller) override;

  // The set of connected controllers.
  std::set<XboxControllerMac*> controllers_;

  // The set of enumerated controllers that received an exclusive access error
  // on opening the device, keyed by registry entry ID. The data fetcher is
  // notified when these devices become available so we can try opening them
  // again.
  base::flat_map<uint64_t, base::mac::ScopedIOObject<io_object_t>>
      pending_services_;

  bool listening_ = false;

  // port_ owns source_, so this doesn't need to be a ScopedCFTypeRef, but we
  // do need to maintain a reference to it so we can invalidate it.
  CFRunLoopSourceRef source_ = nullptr;
  base::mac::ScopedIONotificationPortRef port_;

  // Iterators returned by calls to IOServiceAddMatchingNotification for
  // kIOFirstMatchNotification (connection) and kIOTerminatedNotification
  // (disconnection) events. These iterators are not referenced directly but
  // must be kept alive in order to continue to receive notifications.
  std::vector<base::mac::ScopedIOObject<io_iterator_t>> device_event_iterators_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
