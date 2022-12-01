// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_ITEM_H_
#define SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_ITEM_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"

namespace device {

// An element of a HID report descriptor.
class HidReportDescriptorItem {
 private:
  enum Type {
    kTypeMain = 0,
    kTypeGlobal = 1,
    kTypeLocal = 2,
    kTypeReserved = 3
  };

  enum MainTag {
    kMainTagDefault = 0x00,       // 0000
    kMainTagInput = 0x08,         // 1000
    kMainTagOutput = 0x09,        // 1001
    kMainTagFeature = 0x0B,       // 1011
    kMainTagCollection = 0x0A,    // 1010
    kMainTagEndCollection = 0x0C  // 1100
  };

  enum GlobalTag {
    kGlobalTagUsagePage = 0x00,        // 0000
    kGlobalTagLogicalMinimum = 0x01,   // 0001
    kGlobalTagLogicalMaximum = 0x02,   // 0010
    kGlobalTagPhysicalMinimum = 0x03,  // 0011
    kGlobalTagPhysicalMaximum = 0x04,  // 0100
    kGlobalTagUnitExponent = 0x05,     // 0101
    kGlobalTagUnit = 0x06,             // 0110
    kGlobalTagReportSize = 0x07,       // 0111
    kGlobalTagReportId = 0x08,         // 1000
    kGlobalTagReportCount = 0x09,      // 1001
    kGlobalTagPush = 0x0A,             // 1010
    kGlobalTagPop = 0x0B               // 1011
  };

  enum LocalTag {
    kLocalTagUsage = 0x00,              // 0000
    kLocalTagUsageMinimum = 0x01,       // 0001
    kLocalTagUsageMaximum = 0x02,       // 0010
    kLocalTagDesignatorIndex = 0x03,    // 0011
    kLocalTagDesignatorMinimum = 0x04,  // 0100
    kLocalTagDesignatorMaximum = 0x05,  // 0101
    kLocalTagStringIndex = 0x07,        // 0111
    kLocalTagStringMinimum = 0x08,      // 1000
    kLocalTagStringMaximum = 0x09,      // 1001
    kLocalTagDelimiter = 0x0A           // 1010
  };

  enum ReservedTag {
    kReservedTagLong = 0xF  // 1111
  };

 public:
  enum Tag {
    kTagDefault = kMainTagDefault << 2 | kTypeMain,
    kTagInput = kMainTagInput << 2 | kTypeMain,
    kTagOutput = kMainTagOutput << 2 | kTypeMain,
    kTagFeature = kMainTagFeature << 2 | kTypeMain,
    kTagCollection = kMainTagCollection << 2 | kTypeMain,
    kTagEndCollection = kMainTagEndCollection << 2 | kTypeMain,
    kTagUsagePage = kGlobalTagUsagePage << 2 | kTypeGlobal,
    kTagLogicalMinimum = kGlobalTagLogicalMinimum << 2 | kTypeGlobal,
    kTagLogicalMaximum = kGlobalTagLogicalMaximum << 2 | kTypeGlobal,
    kTagPhysicalMinimum = kGlobalTagPhysicalMinimum << 2 | kTypeGlobal,
    kTagPhysicalMaximum = kGlobalTagPhysicalMaximum << 2 | kTypeGlobal,
    kTagUnitExponent = kGlobalTagUnitExponent << 2 | kTypeGlobal,
    kTagUnit = kGlobalTagUnit << 2 | kTypeGlobal,
    kTagReportSize = kGlobalTagReportSize << 2 | kTypeGlobal,
    kTagReportId = kGlobalTagReportId << 2 | kTypeGlobal,
    kTagReportCount = kGlobalTagReportCount << 2 | kTypeGlobal,
    kTagPush = kGlobalTagPush << 2 | kTypeGlobal,
    kTagPop = kGlobalTagPop << 2 | kTypeGlobal,
    kTagUsage = kLocalTagUsage << 2 | kTypeLocal,
    kTagUsageMinimum = kLocalTagUsageMinimum << 2 | kTypeLocal,
    kTagUsageMaximum = kLocalTagUsageMaximum << 2 | kTypeLocal,
    kTagDesignatorIndex = kLocalTagDesignatorIndex << 2 | kTypeLocal,
    kTagDesignatorMinimum = kLocalTagDesignatorMinimum << 2 | kTypeLocal,
    kTagDesignatorMaximum = kLocalTagDesignatorMaximum << 2 | kTypeLocal,
    kTagStringIndex = kLocalTagStringIndex << 2 | kTypeLocal,
    kTagStringMinimum = kLocalTagStringMinimum << 2 | kTypeLocal,
    kTagStringMaximum = kLocalTagStringMaximum << 2 | kTypeLocal,
    kTagDelimiter = kLocalTagDelimiter << 2 | kTypeLocal,
    kTagLong = kReservedTagLong << 2 | kTypeReserved
  };

  // HID Input/Output/Feature report information.
  // Can be retrieved from GetShortData()
  // when item.tag() == HidReportDescriptorItem::kTagInput
  // or HidReportDescriptorItem::kTagOutput
  // or HidReportDescriptorItem::kTagFeature.
  // The ReportInfo struct matches the layout of the bitfield defined in section
  // 6.2.2.5 of the Device Class Definition for HID v1.11. Pad to 32-bits so it
  // can be safely cast to and from uint32_t.
#pragma pack(push, 1)
  struct ReportInfo {
    uint8_t data_or_constant : 1;
    uint8_t array_or_variable : 1;
    uint8_t absolute_or_relative : 1;
    uint8_t wrap : 1;
    uint8_t linear : 1;
    uint8_t preferred : 1;
    uint8_t null : 1;
    uint8_t is_volatile : 1;
    uint8_t bit_field_or_buffer : 1;
    uint8_t reserved : 7;
    uint8_t reserved2[2];
  };
#pragma pack(pop)
  static_assert(sizeof(ReportInfo) == sizeof(uint32_t),
                "incorrect report info size");

 private:
  HidReportDescriptorItem(base::span<const uint8_t> bytes,
                          HidReportDescriptorItem* previous);

 public:
  ~HidReportDescriptorItem() {}

  static std::unique_ptr<HidReportDescriptorItem> Create(
      base::span<const uint8_t> bytes,
      HidReportDescriptorItem* previous) {
    return std::unique_ptr<HidReportDescriptorItem>(
        new HidReportDescriptorItem(bytes, previous));
  }

  // Previous element in report descriptor.
  // Owned by descriptor instance.
  HidReportDescriptorItem* previous() const { return previous_; }
  // Next element in report descriptor.
  // Owned by descriptor instance.
  HidReportDescriptorItem* next() const { return next_; }
  // Parent element in report descriptor.
  // Owned by descriptor instance.
  // Can be NULL.
  HidReportDescriptorItem* parent() const { return parent_; }
  // Level in Parent-Children relationship tree.
  // 0 for top-level items (parent()==NULL).
  // 1 if parent() is top-level.
  // 2 if parent() has a top-level parent. Etc.
  size_t GetDepth() const;
  Tag tag() const { return tag_; }
  // Returns true for a long item, false otherwise.
  bool IsLong() const;
  // Raw data of a short item.
  // Not valid for a long item.
  uint32_t GetShortData() const;
  // Size of this item in bytes, including the header.
  size_t GetSize() const;
  // Size of this item in bytes, excluding the header.
  size_t payload_size() const { return payload_size_; }

 private:
  size_t GetHeaderSize() const;

  raw_ptr<HidReportDescriptorItem, DanglingUntriaged> previous_;
  raw_ptr<HidReportDescriptorItem, DanglingUntriaged> next_;
  raw_ptr<HidReportDescriptorItem, DanglingUntriaged> parent_;
  Tag tag_;
  uint32_t shortData_;
  size_t payload_size_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_PUBLIC_CPP_HID_HID_REPORT_DESCRIPTOR_ITEM_H_
