// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_
#define SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_

#include <windows.h>

// Must be after windows.h.
#include <hidclass.h>

// NOTE: <hidsdi.h> must be included before <hidpi.h>. clang-format will want to
// reorder them.
// clang-format off
extern "C" {
#include <hidsdi.h>
#include <hidpi.h>
}
// clang-format on

#include <string>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/win/scoped_handle.h"
#include "device/base/device_monitor_win.h"
#include "services/device/hid/hid_service.h"

namespace base {
class SequencedTaskRunner;
}

namespace device {

class HidServiceWin : public HidService, public DeviceMonitorWin::Observer {
 public:
  // Interface for accessing information contained in the opaque
  // HIDP_PREPARSED_DATA object. A PreparsedData instance represents a single
  // HID top-level collection.
  class PreparsedData {
   public:
    struct ReportItem {
      // The report ID, or zero if the device does not use report IDs.
      uint8_t report_id;

      // The bit field for the corresponding main item in the HID report. This
      // bit field is defined in the Device Class Definition for HID v1.11
      // section 6.2.2.5.
      // https://www.usb.org/document-library/device-class-definition-hid-111
      uint32_t bit_field;

      // The size of one field defined by this item, in bits.
      uint16_t report_size;

      // The number of report fields defined by this item.
      uint16_t report_count;

      // The usage page for this item.
      uint16_t usage_page;

      // The usage range for this item. If the item has a single usage instead
      // of a range, |usage_min| and |usage_max| are set to the same usage ID.
      // Both usage IDs must be from the same |usage_page|.
      uint16_t usage_minimum;
      uint16_t usage_maximum;

      // The designator index range for this item. If the item does not have any
      // designators, both |designator_min| and |designator_max| are set to
      // zero.
      uint16_t designator_minimum;
      uint16_t designator_maximum;

      // The string descriptor index range for this item. If the item does not
      // have any associated string descriptors, both |string_min| and
      // |string_max| are set to zero.
      uint16_t string_minimum;
      uint16_t string_maximum;

      // The range for report fields defined by this item in logical units.
      int32_t logical_minimum;
      int32_t logical_maximum;

      // The range for report fields defined by this item in physical units. May
      // be zero if the item does not define physical units.
      int32_t physical_minimum;
      int32_t physical_maximum;

      // The unit definition for this item. The format for this definition is
      // described in the Device Class Definition for HID v1.11 section 6.2.2.7.
      // https://www.usb.org/document-library/device-class-definition-hid-111
      uint32_t unit;
      uint32_t unit_exponent;

      // The index of the first bit of this item within the containing report,
      // omitting the report ID byte. The bit index follows HID report packing
      // order. (Increasing byte index, and least-signficiant bit to
      // most-significant bit within each byte.)
      size_t bit_index;
    };

    virtual ~PreparsedData() = default;

    // Creates a new mojom::HidCollectionInfoPtr representing the top-level HID
    // collection described by this PreparsedData.
    mojom::HidCollectionInfoPtr CreateHidCollectionInfo() const;

    // Returns the maximum length in bytes of reports of type |report_type|.
    // The returned length does not include the report ID byte.
    uint16_t GetReportByteLength(HIDP_REPORT_TYPE report_type) const;

    // Returns information about the top-level collection described by this
    // PreparsedData.
    //
    // See the HIDP_CAPS documentation for more information.
    // https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/hidpi/ns-hidpi-_hidp_caps
    virtual const HIDP_CAPS& GetCaps() const = 0;

    // Returns a vector of ReportItems describing the fields that make up
    // reports of type |report_type|.
    virtual std::vector<ReportItem> GetReportItems(
        HIDP_REPORT_TYPE report_type) const = 0;
  };

  HidServiceWin();
  HidServiceWin(const HidServiceWin&) = delete;
  HidServiceWin& operator=(const HidServiceWin&) = delete;
  ~HidServiceWin() override;

  void Connect(const std::string& device_id,
               bool allow_protected_reports,
               bool allow_fido_reports,
               ConnectCallback callback) override;
  base::WeakPtr<HidService> GetWeakPtr() override;

 private:
  static void EnumerateBlocking(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  static void AddDeviceBlocking(
      base::WeakPtr<HidServiceWin> service,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      const std::wstring& device_path,
      const std::string& physical_device_id,
      const std::wstring& interface_id);

  // DeviceMonitorWin::Observer implementation:
  void OnDeviceAdded(const GUID& class_guid,
                     const std::wstring& device_path) override;
  void OnDeviceRemoved(const GUID& class_guid,
                       const std::wstring& device_path) override;

  // Tries to open the device read-write and falls back to read-only.
  static base::win::ScopedHandle OpenDevice(const std::wstring& device_path);

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  base::ScopedObservation<DeviceMonitorWin, DeviceMonitorWin::Observer>
      device_observation_{this};
  base::WeakPtrFactory<HidServiceWin> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_SERVICE_WIN_H_
