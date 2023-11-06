// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/preferences/tracked/device_id.h"

#include <IOKit/IOKitLib.h>

#include "base/apple/foundation_util.h"
#include "base/apple/scoped_cftyperef.h"
#include "base/mac/scoped_ioobject.h"
#include "base/strings/sys_string_conversions.h"

MachineIdStatus GetDeterministicMachineSpecificId(std::string* machine_id) {
  base::mac::ScopedIOObject<io_service_t> platform_expert(
      IOServiceGetMatchingService(kIOMasterPortDefault,
                                  IOServiceMatching("IOPlatformExpertDevice")));
  if (!platform_expert.get())
    return MachineIdStatus::FAILURE;

  base::apple::ScopedCFTypeRef<CFTypeRef> uuid(IORegistryEntryCreateCFProperty(
      platform_expert.get(), CFSTR(kIOPlatformUUIDKey), kCFAllocatorDefault,
      0));
  if (!uuid.get())
    return MachineIdStatus::FAILURE;

  CFStringRef uuid_string = base::apple::CFCast<CFStringRef>(uuid.get());
  if (!uuid_string)
    return MachineIdStatus::FAILURE;

  *machine_id = base::SysCFStringRefToUTF8(uuid_string);
  return MachineIdStatus::SUCCESS;
}
