// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/ble_scan_parser/scan_record.h"

namespace bluez {

ScanRecord::ScanRecord() = default;
ScanRecord::~ScanRecord() = default;

ScanRecord::ScanRecord(const ScanRecord&) = default;
ScanRecord& ScanRecord::operator=(const ScanRecord&) = default;

ScanRecord::ScanRecord(ScanRecord&&) = default;
ScanRecord& ScanRecord::operator=(ScanRecord&&) = default;

}  // namespace bluez
