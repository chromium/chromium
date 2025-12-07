// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_USB_USB_INTERFACE_DETACH_ALLOWLIST_H_
#define SERVICES_DEVICE_USB_USB_INTERFACE_DETACH_ALLOWLIST_H_

#include <string>
#include <vector>

#include "base/no_destructor.h"
#include "services/device/public/mojom/usb_device.mojom.h"

namespace device {

class UsbInterfaceDetachAllowlist final {
 public:
  struct Protocol {
    uint8_t class_code;
    uint8_t subclass_code;
    uint8_t protocol_code;
  };

  // Element of allowlist for detaching USB kernel drivers
  struct Entry {
    Entry(const std::string& driver_name,
          const std::vector<Protocol>& protocols);
    Entry(const Entry& other);
    ~Entry();

    // The name of the driver that is allowed to detach
    std::string driver_name;

    // The list of protocols that are allowed to detach (empty means "all").
    std::vector<Protocol> protocols;
  };

  // For testing
  explicit UsbInterfaceDetachAllowlist(std::vector<Entry> entries);

  UsbInterfaceDetachAllowlist(const UsbInterfaceDetachAllowlist&) = delete;
  UsbInterfaceDetachAllowlist& operator=(const UsbInterfaceDetachAllowlist&) =
      delete;

  ~UsbInterfaceDetachAllowlist();

  // Returns a singleton instance of the allowlist.
  static const UsbInterfaceDetachAllowlist& Get();

  bool CanDetach(std::string_view driver_name,
                 const mojom::UsbAlternateInterfaceInfo& interface_info) const;

 private:
  // friend NoDestructor to permit access to private constructor.
  friend base::NoDestructor<UsbInterfaceDetachAllowlist>;

  UsbInterfaceDetachAllowlist();

  // Allowlist entries.
  const std::vector<Entry> entries_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_USB_USB_INTERFACE_DETACH_ALLOWLIST_H_
