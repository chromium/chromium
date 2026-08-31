// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IONetworkInterface.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <optional>
#include <string>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"

namespace rlz_lib {

namespace {

using MacAddress = std::array<uint8_t, kIOEthernetAddressSize>;

// See http://developer.apple.com/library/mac/#technotes/tn1103/_index.html

// The caller is responsible for freeing |matching_services|.
bool FindEthernetInterfaces(io_iterator_t* matching_services) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> matching_dict(
      IOServiceMatching(kIOEthernetInterfaceClass));
  if (!matching_dict) {
    return false;
  }

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> primary_interface(
      CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));
  if (!primary_interface) {
    return false;
  }

  CFDictionarySetValue(primary_interface.get(), CFSTR(kIOPrimaryInterface),
                       kCFBooleanTrue);
  CFDictionarySetValue(matching_dict.get(), CFSTR(kIOPropertyMatchKey),
                       primary_interface.get());

  kern_return_t kern_result = IOServiceGetMatchingServices(
      kIOMainPortDefault, matching_dict.release(), matching_services);

  return kern_result == KERN_SUCCESS;
}

std::optional<MacAddress> GetMACAddressFromIterator(
    io_iterator_t primary_interface_iterator) {
  std::optional<MacAddress> mac_address;

  base::mac::ScopedIOObject<io_object_t> primary_interface;
  while (primary_interface.reset(IOIteratorNext(primary_interface_iterator)),
         primary_interface) {
    io_object_t primary_interface_parent;
    kern_return_t kern_result = IORegistryEntryGetParentEntry(
        primary_interface.get(), kIOServicePlane, &primary_interface_parent);
    base::mac::ScopedIOObject<io_object_t> primary_interface_parent_deleter(
        primary_interface_parent);
    if (kern_result != KERN_SUCCESS) {
      continue;
    }

    base::apple::ScopedCFTypeRef<CFTypeRef> mac_data(
        IORegistryEntryCreateCFProperty(primary_interface_parent,
                                        CFSTR(kIOMACAddress),
                                        kCFAllocatorDefault, 0));
    CFDataRef mac_data_data = base::apple::CFCast<CFDataRef>(mac_data.get());
    if (mac_data_data &&
        CFDataGetLength(mac_data_data) >= kIOEthernetAddressSize) {
      MacAddress mac{};
      CFDataGetBytes(mac_data_data, CFRangeMake(0, kIOEthernetAddressSize),
                     mac.data());
      mac_address = mac;
    }
  }

  return mac_address;
}

std::optional<MacAddress> GetMacAddress() {
  io_iterator_t primary_interface_iterator;
  if (!FindEthernetInterfaces(&primary_interface_iterator)) {
    return std::nullopt;
  }
  base::mac::ScopedIOObject<io_iterator_t> primary_interface_iterator_deleter(
      primary_interface_iterator);
  return GetMACAddressFromIterator(primary_interface_iterator);
}

}  // namespace

bool GetRawMachineId(std::u16string* data, int* more_data) {
  data->clear();
  if (std::optional<MacAddress> mac_address = GetMacAddress()) {
    const auto& [m0, m1, m2, m3, m4, m5] = *mac_address;
    *data += base::ASCIIToUTF16(base::StringPrintf(
        "mac:%02x%02x%02x%02x%02x%02x", m0, m1, m2, m3, m4, m5));
  }

  // A MAC address is enough to uniquely identify a machine, but it's only 6
  // bytes, 3 of which are manufacturer-determined. To make brute-forcing the
  // SHA1 of this harder, also append the system's serial number.
  std::string serial = base::mac::GetPlatformSerialNumber();
  if (!serial.empty()) {
    if (!data->empty())
      *data += u" ";
    *data += u"serial:" + base::UTF8ToUTF16(serial);
  }

  // On windows, this is set to the volume id. Since it's not scrambled before
  // being sent, just set it to 1.
  *more_data = 1;
  return true;
}

}  // namespace rlz_lib
