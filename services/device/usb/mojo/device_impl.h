// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_
#define SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_

#include <stdint.h>

#include <optional>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/mojom/usb_device.mojom.h"
#include "services/device/usb/usb_device.h"
#include "services/device/usb/usb_device_handle.h"

namespace device::usb {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WebUsbControlTransferPermissionOutcome)
enum class WebUsbControlTransferPermissionOutcome {
  kAllowed = 0,
  kBlocked = 1,
  kError_InterfaceNotFound = 2,
  // Failed because the device is not in a configured state
  kError_NoConfiguration = 3,
  kMaxValue = kError_NoConfiguration,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WebUsbStateChangeBlockedContext)
enum class WebUsbStateChangeBlockedContext {
  kUnknown = 0,
  kDeviceStateChangeSameConnection = 1,
  kDeviceStateChangeOtherConnection = 2,
  kInterfaceStateChangeSameConnection = 3,
  kMaxValue = kInterfaceStateChangeSameConnection,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:WebUsbStateChangeBlockedContext)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WebUsbEndpointNotReadyReason)
enum class WebUsbEndpointNotReadyReason {
  kDeviceStateChangeInProgress = 0,
  kDeviceNotOpen = 1,
  kInterfaceNotClaimedOrInvalidEndpoint = 2,
  kInterfaceStateChangeInProgress = 3,
  kMaxValue = kInterfaceStateChangeInProgress,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:WebUsbEndpointNotReadyReason)

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(WebUsbStateChangeBlockedMethod)
enum class WebUsbStateChangeBlockedMethod {
  kSetConfiguration = 0,
  kClaimInterface = 1,
  kReleaseInterface = 2,
  kSetInterfaceAlternateSetting = 3,
  kReset = 4,
  kClearHalt = 5,
  kControlTransferIn = 6,
  kControlTransferOut = 7,
  kGenericTransferIn = 8,
  kGenericTransferOut = 9,
  kIsochronousTransferIn = 10,
  kIsochronousTransferOut = 11,
  kMaxValue = kIsochronousTransferOut,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:WebUsbStateChangeBlockedMethod)

// Implementation of the public Device interface. Instances of this class are
// constructed by DeviceManagerImpl and are strongly bound to their MessagePipe
// lifetime.
class DeviceImpl : public mojom::UsbDevice, public device::UsbDevice::Observer {
 public:
  static void Create(scoped_refptr<device::UsbDevice> device,
                     mojo::PendingReceiver<mojom::UsbDevice> receiver,
                     mojo::PendingRemote<mojom::UsbDeviceClient> client,
                     base::span<const uint8_t> blocked_interface_classes,
                     bool allow_security_key_requests,
                     bool allow_unrestricted_control_transfers);

  DeviceImpl(const DeviceImpl&) = delete;
  DeviceImpl& operator=(const DeviceImpl&) = delete;

  ~DeviceImpl() override;

 private:
  DeviceImpl(scoped_refptr<device::UsbDevice> device,
             mojo::PendingRemote<mojom::UsbDeviceClient> client,
             base::span<const uint8_t> blocked_interface_classes,
             bool allow_security_key_requests,
             bool allow_unrestricted_control_transfers);

  // Closes the device if it's open. This will always set |device_handle_| to
  // null.
  void CloseHandle();

  // Checks interface permissions for control transfers.
  bool HasControlTransferPermission(
      mojom::UsbTransferDirection direction,
      mojom::UsbControlTransferType type,
      mojom::UsbControlTransferRecipient recipient,
      uint8_t request,
      uint16_t index);

  const mojom::UsbInterfaceInfo* FindInterface(
      const mojom::UsbConfigurationInfo* config,
      uint8_t interface_number) const;
  std::optional<uint8_t> FindBlockedClass(
      const mojom::UsbInterfaceInfo* interface) const;
  bool HasProtectedInterface(const mojom::UsbConfigurationInfo* config) const;
  bool AllowAndLog(WebUsbControlTransferPermissionOutcome outcome);
  bool BlockAndLog(WebUsbControlTransferPermissionOutcome outcome);

  // Handles completion of an open request.
  static void OnOpen(base::WeakPtr<DeviceImpl> device,
                     OpenCallback callback,
                     scoped_refptr<device::UsbDeviceHandle> handle);
  void OnPermissionGrantedForOpen(OpenCallback callback, bool granted);

  // Device implementation:
  void Open(OpenCallback callback) override;
  void Close(CloseCallback callback) override;
  void SetConfiguration(uint8_t value,
                        SetConfigurationCallback callback) override;
  void ClaimInterface(uint8_t interface_number,
                      ClaimInterfaceCallback callback) override;
  void ReleaseInterface(uint8_t interface_number,
                        ReleaseInterfaceCallback callback) override;
  void SetInterfaceAlternateSetting(
      uint8_t interface_number,
      uint8_t alternate_setting,
      SetInterfaceAlternateSettingCallback callback) override;
  void Reset(ResetCallback callback) override;
  void ClearHalt(mojom::UsbTransferDirection direction,
                 uint8_t endpoint_number,
                 ClearHaltCallback callback) override;
  void ControlTransferIn(mojom::UsbControlTransferParamsPtr params,
                         uint32_t length,
                         uint32_t timeout,
                         ControlTransferInCallback callback) override;
  void ControlTransferOut(mojom::UsbControlTransferParamsPtr params,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          ControlTransferOutCallback callback) override;
  void GenericTransferIn(uint8_t endpoint_number,
                         uint32_t length,
                         uint32_t timeout,
                         GenericTransferInCallback callback) override;
  void GenericTransferOut(uint8_t endpoint_number,
                          base::span<const uint8_t> data,
                          uint32_t timeout,
                          GenericTransferOutCallback callback) override;
  void IsochronousTransferIn(uint8_t endpoint_number,
                             const std::vector<uint32_t>& packet_lengths,
                             uint32_t timeout,
                             IsochronousTransferInCallback callback) override;
  void IsochronousTransferOut(uint8_t endpoint_number,
                              base::span<const uint8_t> data,
                              const std::vector<uint32_t>& packet_lengths,
                              uint32_t timeout,
                              IsochronousTransferOutCallback callback) override;

  // device::UsbDevice::Observer implementation:
  void OnDeviceRemoved(scoped_refptr<device::UsbDevice> device) override;

  void OnInterfaceClaimed(ClaimInterfaceCallback callback,
                          uint8_t interface_number,
                          bool success);
  void OnInterfaceReleased(ReleaseInterfaceCallback callback,
                           uint8_t interface_number,
                           bool success);
  void OnSetInterfaceAlternateSettingComplete(
      SetInterfaceAlternateSettingCallback callback,
      uint8_t interface_number,
      bool success);
  void OnSetConfigurationComplete(SetConfigurationCallback callback,
                                  bool success);
  void OnResetComplete(ResetCallback callback, bool success);
  void OnClientConnectionError();

  // Reject and report bad mojo messaage if `length` exceeds limit.
  bool ShouldRejectUsbTransferLengthAndReportBadMessage(size_t length);

  // Returns whether the endpoint is ready for a transfer.
  // A transfer is allowed if the device is open, the target interface is
  // claimed, and neither a device-wide state change nor an interface-specific
  // state change for the target interface is in progress.
  // This is used for non-control transfers (Generic and Isochronous) and
  // ClearHalt. Records UMA metrics if the endpoint is not ready.
  bool IsEndpointReadyForTransfer(WebUsbStateChangeBlockedMethod method,
                                  uint8_t endpoint_address);

  bool IsInterfaceStateChangeInProgress(uint8_t interface_number) const;
  bool any_interface_state_change_in_progress() const;
  bool any_state_change_in_progress() const;
  void SetInterfaceStateChangeInProgress(uint8_t interface_number,
                                         bool in_progress);

  void RecordStateChangeBlocked(WebUsbStateChangeBlockedMethod method,
                                bool interface_in_progress);

  const scoped_refptr<device::UsbDevice> device_;
  base::ScopedObservation<device::UsbDevice, device::UsbDevice::Observer>
      observation_{this};

  // The device handle. Will be null before the device is opened and after it
  // has been closed. |opening_| is set to true while the asynchronous open is
  // in progress.
  bool opening_ = false;
  scoped_refptr<UsbDeviceHandle> device_handle_;

  // Tracks whether a device-wide state change (SetConfiguration or Reset) was
  // initiated by THIS specific DeviceImpl instance. The shared lock on
  // UsbDevice (device_->device_state_change_in_progress()) may be set by any
  // connection to the device. This instance-local flag ensures that when this
  // DeviceImpl is destroyed (e.g. tab closed while a state change is in
  // flight), its destructor safely clears the shared lock if and only if this
  // instance was the initiator.
  bool device_state_change_in_progress_ = false;
  base::flat_set<uint8_t> interface_state_changes_in_progress_;

  const base::flat_set<uint8_t> blocked_interface_classes_;
  const bool allow_security_key_requests_;
  const bool allow_unrestricted_control_transfers_;
  mojo::SelfOwnedReceiverRef<mojom::UsbDevice> receiver_;
  mojo::Remote<device::mojom::UsbDeviceClient> client_;
  base::WeakPtrFactory<DeviceImpl> weak_factory_{this};
};

}  // namespace device::usb

#endif  // SERVICES_DEVICE_USB_MOJO_DEVICE_IMPL_H_
