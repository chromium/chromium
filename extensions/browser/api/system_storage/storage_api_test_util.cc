// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/system_storage/storage_api_test_util.h"

#include "base/strings/utf_string_conversions.h"

namespace extensions::test {

const struct TestStorageUnitInfo kRemovableStorageData = {"dcim:device:001",
                                                          "/media/usb1",
                                                          4098,
                                                          1000};

storage_monitor::StorageInfo BuildStorageInfoFromTestStorageUnitInfo(
    const TestStorageUnitInfo& unit) {
  return storage_monitor::StorageInfo(
      unit.device_id, base::FilePath::StringType(), /* no location */
      base::UTF8ToUTF16(unit.name),                 /* storage label */
      std::u16string(),                             /* no storage vendor */
      std::u16string(),                             /* no storage model */
      unit.capacity);
}

}  // namespace extensions::test
