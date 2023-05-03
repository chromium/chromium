// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

#include "services/device/public/cpp/hid/hid_report_descriptor.h"
#include "services/device/public/cpp/hid/hid_report_type.h"
#include "services/device/public/cpp/hid/hid_report_utils.h"
#include "services/device/public/cpp/test/test_report_descriptors.h"
#include "services/device/public/mojom/hid.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {
using HidReport = std::vector<std::unique_ptr<HidReportItem>>;
using HidReportMap = std::unordered_map<uint8_t, HidReport>;
using HidCollectionVector = std::vector<std::unique_ptr<HidCollection>>;

// HID unit values.
constexpr uint32_t kUnitCandela = 0x010000e1;
constexpr uint32_t kUnitDegrees = 0x14;
constexpr uint32_t kUnitInch = 0x13;
constexpr uint32_t kUnitNewton = 0xe111;
constexpr uint32_t kUnitSecond = 0x1001;

// Report info bitfield values. The bits are:
//   bit 0: Data (0) | Constant (1)
//   bit 1: Array (0) | Variable (1)
//   bit 2: Absolute (0) | Relative (1)
//   bit 3: No Wrap (0) | Wrap (1)
//   bit 4: Linear (0) | Non-Linear (1)
//   bit 5: Preferred State (0) | No Preferred State (1)
//   bit 6: No Null Value (0) | Has Null Value (1)
//   bit 7: Non-Volatile (0) | Volatile (1)
//   bit 8: Bit Field (0) | Buffered Bytes (1)
constexpr uint16_t kNonNullableArray = 0x0000;
constexpr uint16_t kConstantArray = 0x0001;
constexpr uint16_t kAbsoluteVariable = 0x0002;
constexpr uint16_t kConstant = 0x0003;
constexpr uint16_t kRelativeVariable = 0x0006;
constexpr uint16_t kConstantRelativeVariable = 0x0007;
constexpr uint16_t kNonLinearVariable = 0x0012;
constexpr uint16_t kAbsoluteVariablePreferredState = 0x0022;
constexpr uint16_t kConstantAbsoluteVariablePreferredState = 0x0023;
constexpr uint16_t kNullableArray = 0x0040;
constexpr uint16_t kNullableAbsoluteVariable = 0x0042;
constexpr uint16_t kVolatileConstant = 0x0083;
constexpr uint16_t kBufferedBytes = 0x0102;

// Bit-width and mask for the usage ID field in a 32-bit usage value.
constexpr size_t kUsageIdSizeBits = sizeof(uint16_t) * CHAR_BIT;
constexpr uint32_t kUsageIdMask = std::numeric_limits<uint16_t>::max();

// Combined usage page and usage ID values. The usage page occupies the high 16
// bits, the usage ID occupies the low 16 bits.
constexpr uint32_t kUsageButton = mojom::kPageButton << kUsageIdSizeBits;
constexpr uint32_t kUsageConsumer = mojom::kPageConsumer << kUsageIdSizeBits;
constexpr uint32_t kUsageConsumerControl = kUsageConsumer | 0x01;
constexpr uint32_t kUsageConsumerModeStep = kUsageConsumer | 0x82;
constexpr uint32_t kUsageConsumerFastForward = kUsageConsumer | 0xb3;
constexpr uint32_t kUsageConsumerRewind = kUsageConsumer | 0xb4;
constexpr uint32_t kUsageConsumerScanNextTrack = kUsageConsumer | 0xb5;
constexpr uint32_t kUsageConsumerScanPreviousTrack = kUsageConsumer | 0xb6;
constexpr uint32_t kUsageConsumerStop = kUsageConsumer | 0xb7;
constexpr uint32_t kUsageConsumerPlayPause = kUsageConsumer | 0xcd;
constexpr uint32_t kUsageConsumerMute = kUsageConsumer | 0xe2;
constexpr uint32_t kUsageConsumerVolumeIncrement = kUsageConsumer | 0xe9;
constexpr uint32_t kUsageConsumerVolumeDecrement = kUsageConsumer | 0xea;
constexpr uint32_t kUsageConsumerACHome = kUsageConsumer | 0x0223;
constexpr uint32_t kUsageConsumerACBack = kUsageConsumer | 0x0224;
constexpr uint32_t kUsageDigitizer = mojom::kPageDigitizer << kUsageIdSizeBits;
constexpr uint32_t kUsageDigitizerDigitizer = kUsageDigitizer | 0x01;
constexpr uint32_t kUsageDigitizerBarrelSwitch = kUsageDigitizer | 0x44;
constexpr uint32_t kUsageDigitizerInRange = kUsageDigitizer | 0x32;
constexpr uint32_t kUsageDigitizerPuck = kUsageDigitizer | 0x21;
constexpr uint32_t kUsageDigitizerStylus = kUsageDigitizer | 0x20;
constexpr uint32_t kUsageDigitizerTipPressure = kUsageDigitizer | 0x30;
constexpr uint32_t kUsageDigitizerTipSwitch = kUsageDigitizer | 0x42;
constexpr uint32_t kUsageGenericDesktop = mojom::kPageGenericDesktop
                                          << kUsageIdSizeBits;
constexpr uint32_t kUsageGenericDesktopDial =
    kUsageGenericDesktop | mojom::kGenericDesktopDial;
constexpr uint32_t kUsageGenericDesktopGamePad =
    kUsageGenericDesktop | mojom::kGenericDesktopGamePad;
constexpr uint32_t kUsageGenericDesktopHatSwitch =
    kUsageGenericDesktop | mojom::kGenericDesktopHatSwitch;
constexpr uint32_t kUsageGenericDesktopJoystick =
    kUsageGenericDesktop | mojom::kGenericDesktopJoystick;
constexpr uint32_t kUsageGenericDesktopKeyboard =
    kUsageGenericDesktop | mojom::kGenericDesktopKeyboard;
constexpr uint32_t kUsageGenericDesktopMouse =
    kUsageGenericDesktop | mojom::kGenericDesktopMouse;
constexpr uint32_t kUsageGenericDesktopPointer =
    kUsageGenericDesktop | mojom::kGenericDesktopPointer;
constexpr uint32_t kUsageGenericDesktopRx =
    kUsageGenericDesktop | mojom::kGenericDesktopRx;
constexpr uint32_t kUsageGenericDesktopRy =
    kUsageGenericDesktop | mojom::kGenericDesktopRy;
constexpr uint32_t kUsageGenericDesktopRz =
    kUsageGenericDesktop | mojom::kGenericDesktopRz;
constexpr uint32_t kUsageGenericDesktopSystemControl =
    kUsageGenericDesktop | mojom::kGenericDesktopSystemControl;
constexpr uint32_t kUsageGenericDesktopSystemMainMenu =
    kUsageGenericDesktop | mojom::kGenericDesktopSystemMainMenu;
constexpr uint32_t kUsageGenericDesktopVbrx =
    kUsageGenericDesktop | mojom::kGenericDesktopVbrx;
constexpr uint32_t kUsageGenericDesktopVbry =
    kUsageGenericDesktop | mojom::kGenericDesktopVbry;
constexpr uint32_t kUsageGenericDesktopVbrz =
    kUsageGenericDesktop | mojom::kGenericDesktopVbrz;
constexpr uint32_t kUsageGenericDesktopVx =
    kUsageGenericDesktop | mojom::kGenericDesktopVx;
constexpr uint32_t kUsageGenericDesktopVy =
    kUsageGenericDesktop | mojom::kGenericDesktopVy;
constexpr uint32_t kUsageGenericDesktopVz =
    kUsageGenericDesktop | mojom::kGenericDesktopVz;
constexpr uint32_t kUsageGenericDesktopWheel =
    kUsageGenericDesktop | mojom::kGenericDesktopWheel;
constexpr uint32_t kUsageGenericDesktopX =
    kUsageGenericDesktop | mojom::kGenericDesktopX;
constexpr uint32_t kUsageGenericDesktopY =
    kUsageGenericDesktop | mojom::kGenericDesktopY;
constexpr uint32_t kUsageGenericDesktopZ =
    kUsageGenericDesktop | mojom::kGenericDesktopZ;
constexpr uint32_t kUsageGenericDeviceBatteryStrength =
    (mojom::kPageGenericDevice << kUsageIdSizeBits) | 0x20;
constexpr uint32_t kUsageKeyboard = mojom::kPageKeyboard << kUsageIdSizeBits;
constexpr uint32_t kUsageKeyboardApplication = kUsageKeyboard | 0x65;
constexpr uint32_t kUsageKeyboardLeftControl = kUsageKeyboard | 0xe0;
constexpr uint32_t kUsageKeyboardRightGui = kUsageKeyboard | 0xe7;
constexpr uint32_t kUsageLed = mojom::kPageLed << kUsageIdSizeBits;
constexpr uint32_t kUsageLedNumLock = kUsageLed | 0x01;
constexpr uint32_t kUsageLedCapsLock = kUsageLed | 0x02;
constexpr uint32_t kUsageLedScrollLock = kUsageLed | 0x03;
constexpr uint32_t kUsageLedCompose = kUsageLed | 0x04;
constexpr uint32_t kUsageLedKana = kUsageLed | 0x05;
constexpr uint32_t kUsageLedMute = kUsageLed | 0x09;
constexpr uint32_t kUsageLedOffHook = kUsageLed | 0x17;
constexpr uint32_t kUsageLedRing = kUsageLed | 0x18;
constexpr uint32_t kUsageLedSpeaker = kUsageLed | 0x1e;
constexpr uint32_t kUsageLedHold = kUsageLed | 0x20;
constexpr uint32_t kUsageLedMicrophone = kUsageLed | 0x21;
constexpr uint32_t kUsageLedOnLine = kUsageLed | 0x2a;
constexpr uint32_t kUsageMonitor0 = mojom::kPageMonitor0 << kUsageIdSizeBits;
constexpr uint32_t kUsageMonitorControl = kUsageMonitor0 | 0x01;
constexpr uint32_t kUsageMonitorEdidInfo = kUsageMonitor0 | 0x02;
constexpr uint32_t kUsageMonitorVdifInfo = kUsageMonitor0 | 0x03;
constexpr uint32_t kUsageMonitor2 = mojom::kPageMonitor2 << kUsageIdSizeBits;
constexpr uint32_t kUsageMonitorBrightness = kUsageMonitor2 | 0x10;
constexpr uint32_t kUsageMonitorContrast = kUsageMonitor2 | 0x12;
constexpr uint32_t kUsageMonitorRedVideoGain = kUsageMonitor2 | 0x16;
constexpr uint32_t kUsageMonitorGreenVideoGain = kUsageMonitor2 | 0x18;
constexpr uint32_t kUsageMonitorBlueVideoGain = kUsageMonitor2 | 0x1a;
constexpr uint32_t kUsageMonitorHorizontalPosition = kUsageMonitor2 | 0x20;
constexpr uint32_t kUsageMonitorHorizontalSize = kUsageMonitor2 | 0x22;
constexpr uint32_t kUsageMonitorVerticalPosition = kUsageMonitor2 | 0x30;
constexpr uint32_t kUsageMonitorVerticalSize = kUsageMonitor2 | 0x32;
constexpr uint32_t kUsageMonitorTrapezoidalDistortion = kUsageMonitor2 | 0x42;
constexpr uint32_t kUsageMonitorTilt = kUsageMonitor2 | 0x44;
constexpr uint32_t kUsageMonitorRedVideoBlackLevel = kUsageMonitor2 | 0x6c;
constexpr uint32_t kUsageMonitorGreenVideoBlackLevel = kUsageMonitor2 | 0x6e;
constexpr uint32_t kUsageMonitorBlueVideoBlackLevel = kUsageMonitor2 | 0x70;
constexpr uint32_t kUsagePid = mojom::kPagePidPage << kUsageIdSizeBits;
constexpr uint32_t kUsagePidSetEffectReport = kUsagePid | 0x21;
constexpr uint32_t kUsagePidDuration = kUsagePid | 0x50;
constexpr uint32_t kUsagePidMagnitude = kUsagePid | 0x70;
constexpr uint32_t kUsagePidLoopCount = kUsagePid | 0x7c;
constexpr uint32_t kUsagePidDCEnableActuators = kUsagePid | 0x97;
constexpr uint32_t kUsagePidStartDelay = kUsagePid | 0xa7;
constexpr uint32_t kUsageSimulation = mojom::kPageSimulation
                                      << kUsageIdSizeBits;
constexpr uint32_t kUsageSimulationAccelerator = kUsageSimulation | 0xc4;
constexpr uint32_t kUsageSimulationBrake = kUsageSimulation | 0xc5;
constexpr uint32_t kUsageTelephony = mojom::kPageTelephony << kUsageIdSizeBits;
constexpr uint32_t kUsageTelephonyHeadset = kUsageTelephony | 0x05;
constexpr uint32_t kUsageTelephonyKeyPad = kUsageTelephony | 0x06;
constexpr uint32_t kUsageTelephonyProgrammableButton = kUsageTelephony | 0x07;
constexpr uint32_t kUsageTelephonyHookSwitch = kUsageTelephony | 0x20;
constexpr uint32_t kUsageTelephonyFlash = kUsageTelephony | 0x21;
constexpr uint32_t kUsageTelephonyRedial = kUsageTelephony | 0x24;
constexpr uint32_t kUsageTelephonyLine = kUsageTelephony | 0x2a;
constexpr uint32_t kUsageTelephonyPhoneMute = kUsageTelephony | 0x2f;
constexpr uint32_t kUsageTelephonySpeedDial = kUsageTelephony | 0x50;
constexpr uint32_t kUsageTelephonyLineBusyTone = kUsageTelephony | 0x97;
constexpr uint32_t kUsageTelephonyRinger = kUsageTelephony | 0x9e;
constexpr uint32_t kUsageTelephonyPhoneKey0 = kUsageTelephony | 0xb0;
constexpr uint32_t kUsageTelephonyPhoneKeyPound = kUsageTelephony | 0xbb;
constexpr uint32_t kUsageVendor = mojom::kPageVendor << kUsageIdSizeBits;
constexpr uint32_t kUsageVendor02 = (mojom::kPageVendor + 0x02)
                                    << kUsageIdSizeBits;
constexpr uint32_t kUsageVendor05 = (mojom::kPageVendor + 0x05)
                                    << kUsageIdSizeBits;
constexpr uint32_t kUsageVendor30 = (mojom::kPageVendor + 0x30)
                                    << kUsageIdSizeBits;
constexpr uint32_t kUsageVendor40 = (mojom::kPageVendor + 0x40)
                                    << kUsageIdSizeBits;
constexpr uint32_t kUsageVendor80 = (mojom::kPageVendor + 0x80)
                                    << kUsageIdSizeBits;

// Report item tags.
constexpr HidReportDescriptorItem::Tag kInput =
    HidReportDescriptorItem::kTagInput;
constexpr HidReportDescriptorItem::Tag kOutput =
    HidReportDescriptorItem::kTagOutput;
constexpr HidReportDescriptorItem::Tag kFeature =
    HidReportDescriptorItem::kTagFeature;
constexpr uint32_t kCollectionTypeApplication =
    mojom::kHIDCollectionTypeApplication;
constexpr uint32_t kCollectionTypeLogical = mojom::kHIDCollectionTypeLogical;
constexpr uint32_t kCollectionTypePhysical = mojom::kHIDCollectionTypePhysical;

}  // namespace

class HidReportDescriptorTest : public testing::Test {
 protected:
  using HidUsageAndPage = mojom::HidUsageAndPage;
  using HidCollectionInfo = mojom::HidCollectionInfo;
  using HidCollectionInfoPtr = mojom::HidCollectionInfoPtr;

  void TearDown() override {
    descriptor_ = nullptr;
    expected_collection_infos_.clear();
    expected_collections_.clear();
    report_id_ = 0;
    globals_ = HidItemStateTable::HidGlobalItemState();
  }

 public:
  // Add a top-level collection to |expected_collection_infos_|.
  void AddTopCollectionInfo(HidCollectionInfoPtr collection_info) {
    expected_collection_infos_.push_back(std::move(collection_info));
  }

  // Create a new collection and append it to |expected_collections_|.
  HidCollection* AddTopCollection(uint32_t usage, uint32_t collection_type) {
    uint16_t usage_page = (usage >> kUsageIdSizeBits) & kUsageIdMask;
    usage = usage & kUsageIdMask;
    expected_collections_.push_back(std::make_unique<HidCollection>(
        nullptr, usage_page, usage, static_cast<uint32_t>(collection_type)));
    return expected_collections_.back().get();
  }

  // Create a new collection as a child of |parent|.
  HidCollection* AddChild(HidCollection* parent,
                          uint32_t usage,
                          uint32_t collection_type) {
    uint16_t usage_page = (usage >> kUsageIdSizeBits) & kUsageIdMask;
    usage = usage & kUsageIdMask;
    parent->AddChildForTesting(std::make_unique<HidCollection>(
        parent, usage_page, usage, static_cast<uint32_t>(collection_type)));
    return parent->GetChildren().back().get();
  }

  // Set the |report_id|. Subsequent report items will be appended to the report
  // with this ID.
  void SetReportId(uint8_t report_id) { report_id_ = report_id; }

  // Set the |unit| and |unit_exponent|. Subsequent report items will inherit
  // these values.
  void SetUnitAndUnitExponent(uint32_t unit, uint32_t unit_exponent) {
    globals_.unit = unit;
    globals_.unit_exponent = unit_exponent;
  }

  // Set the logical and physical minimums and maximums. Subsequent report items
  // will inherit these values.
  void SetLogicalAndPhysicalBounds(int32_t logical_minimum,
                                   int32_t logical_maximum,
                                   int32_t physical_minimum,
                                   int32_t physical_maximum) {
    globals_.logical_minimum = logical_minimum;
    globals_.logical_maximum = logical_maximum;
    globals_.physical_minimum = physical_minimum;
    globals_.physical_maximum = physical_maximum;
  }

  // Set the |report_size| in bits, and the |report_count|. Subsequent report
  // items will inherit these values.
  void SetReportSizeAndCount(uint32_t report_size, uint32_t report_count) {
    globals_.report_size = report_size;
    globals_.report_count = report_count;
  }

  // Add a report item with a size and count but no usage value.
  void AddReportConstant(HidCollection* collection,
                         HidReportDescriptorItem::Tag tag,
                         uint32_t report_info) {
    HidItemStateTable state;
    state.global_stack.push_back(globals_);
    state.report_id = report_id_;
    for (const HidCollection* c = collection; c; c = c->GetParent())
      const_cast<HidCollection*>(c)->AddReportItem(tag, report_info, state);
  }

  // Add a report item for one or more usages with the same size. The size of
  // |usage_ids| is not required to be the same as the report count.
  void AddReportItem(HidCollection* collection,
                     HidReportDescriptorItem::Tag tag,
                     uint32_t report_info,
                     const std::vector<uint32_t>& usage_ids) {
    HidItemStateTable state;
    state.global_stack.push_back(globals_);
    state.report_id = report_id_;
    state.local.usages = usage_ids;
    for (const HidCollection* c = collection; c; c = c->GetParent())
      const_cast<HidCollection*>(c)->AddReportItem(tag, report_info, state);
  }

  // Add a report item for a range of usages. The item may be a variable or an
  // array.
  void AddReportItemRange(HidCollection* collection,
                          HidReportDescriptorItem::Tag tag,
                          uint32_t report_info,
                          uint32_t usage_minimum,
                          uint32_t usage_maximum) {
    HidItemStateTable state;
    state.global_stack.push_back(globals_);
    state.report_id = report_id_;
    state.local.usage_minimum = usage_minimum;
    state.local.usage_maximum = usage_maximum;
    for (const HidCollection* c = collection; c; c = c->GetParent())
      const_cast<HidCollection*>(c)->AddReportItem(tag, report_info, state);
  }

  void ValidateDetails(const bool expected_has_report_id,
                       const size_t expected_max_input_report_size,
                       const size_t expected_max_output_report_size,
                       const size_t expected_max_feature_report_size,
                       base::span<const uint8_t> report_descriptor_data) {
    descriptor_ = std::make_unique<HidReportDescriptor>(report_descriptor_data);
    std::vector<HidCollectionInfoPtr> actual_collection_infos;
    bool actual_has_report_id;
    size_t actual_max_input_report_size;
    size_t actual_max_output_report_size;
    size_t actual_max_feature_report_size;
    descriptor_->GetDetails(&actual_collection_infos, &actual_has_report_id,
                            &actual_max_input_report_size,
                            &actual_max_output_report_size,
                            &actual_max_feature_report_size);

    ASSERT_EQ(expected_collection_infos_.size(),
              actual_collection_infos.size());
    auto actual_info_iter = actual_collection_infos.begin();
    auto expected_info_iter = expected_collection_infos_.begin();
    while (expected_info_iter != expected_collection_infos_.end() &&
           actual_info_iter != actual_collection_infos.end()) {
      const HidCollectionInfoPtr& expected_info = *expected_info_iter;
      const HidCollectionInfoPtr& actual_info = *actual_info_iter;
      ASSERT_EQ(expected_info->usage->usage_page,
                actual_info->usage->usage_page);
      ASSERT_EQ(expected_info->usage->usage, actual_info->usage->usage);
      ASSERT_THAT(actual_info->report_ids,
                  testing::ContainerEq(expected_info->report_ids));
      ++expected_info_iter;
      ++actual_info_iter;
    }
    ASSERT_EQ(expected_has_report_id, actual_has_report_id);
    ASSERT_EQ(expected_max_input_report_size, actual_max_input_report_size);
    ASSERT_EQ(expected_max_output_report_size, actual_max_output_report_size);
    ASSERT_EQ(expected_max_feature_report_size, actual_max_feature_report_size);
  }

  static void ValidateReportItem(const HidReportItem& expected,
                                 const HidReportItem& actual) {
    uint32_t expected_report_info =
        *reinterpret_cast<const uint32_t*>(&expected.GetReportInfo());
    uint32_t actual_report_info =
        *reinterpret_cast<const uint32_t*>(&actual.GetReportInfo());
    ASSERT_EQ(expected.GetTag(), actual.GetTag());
    ASSERT_EQ(expected_report_info, actual_report_info);
    ASSERT_EQ(expected.GetReportId(), actual.GetReportId());
    ASSERT_THAT(actual.GetUsages(), testing::ContainerEq(expected.GetUsages()));
    ASSERT_EQ(expected.GetUsageMinimum(), actual.GetUsageMinimum());
    ASSERT_EQ(expected.GetUsageMaximum(), actual.GetUsageMaximum());
    ASSERT_EQ(expected.GetDesignatorMinimum(), actual.GetDesignatorMinimum());
    ASSERT_EQ(expected.GetDesignatorMaximum(), actual.GetDesignatorMaximum());
    ASSERT_EQ(expected.GetStringMinimum(), actual.GetStringMinimum());
    ASSERT_EQ(expected.GetStringMaximum(), actual.GetStringMaximum());
    ASSERT_EQ(expected.GetLogicalMinimum(), actual.GetLogicalMinimum());
    ASSERT_EQ(expected.GetLogicalMaximum(), actual.GetLogicalMaximum());
    ASSERT_EQ(expected.GetPhysicalMinimum(), actual.GetPhysicalMinimum());
    ASSERT_EQ(expected.GetPhysicalMaximum(), actual.GetPhysicalMaximum());
    ASSERT_EQ(expected.GetUnitExponent(), actual.GetUnitExponent());
    ASSERT_EQ(expected.GetUnit(), actual.GetUnit());
    ASSERT_EQ(expected.GetReportSize(), actual.GetReportSize());
    ASSERT_EQ(expected.GetReportCount(), actual.GetReportCount());
  }

  static void ValidateReportMap(const HidReportMap& expected_reports,
                                const HidReportMap& actual_reports) {
    for (const auto& expected_entry : expected_reports) {
      auto find_it = actual_reports.find(expected_entry.first);
      ASSERT_NE(find_it, actual_reports.end());
      const auto& expected_report = expected_entry.second;
      const auto& actual_report = find_it->second;
      ASSERT_EQ(expected_report.size(), actual_report.size());
      auto expected_item_iter = expected_report.begin();
      auto actual_item_iter = actual_report.begin();
      while (expected_item_iter != expected_report.end() &&
             actual_item_iter != actual_report.end()) {
        ValidateReportItem(**expected_item_iter, **actual_item_iter);
        ++expected_item_iter;
        ++actual_item_iter;
      }
    }
    ASSERT_EQ(expected_reports.size(), actual_reports.size());
  }

  static void ValidateLinkCollection(const HidCollection* expected_collection,
                                     const HidCollection* actual_collection) {
    ASSERT_EQ(expected_collection->GetUsagePage(),
              actual_collection->GetUsagePage());
    ASSERT_EQ(expected_collection->GetUsage(), actual_collection->GetUsage());
    ASSERT_EQ(expected_collection->GetCollectionType(),
              actual_collection->GetCollectionType());
    ValidateReportMap(expected_collection->GetInputReports(),
                      actual_collection->GetInputReports());
    ValidateReportMap(expected_collection->GetOutputReports(),
                      actual_collection->GetOutputReports());
    ValidateReportMap(expected_collection->GetFeatureReports(),
                      actual_collection->GetFeatureReports());
    const auto& expected_children = expected_collection->GetChildren();
    const auto& actual_children = actual_collection->GetChildren();
    auto expected_child_iter = expected_children.begin();
    auto actual_child_iter = actual_children.begin();
    while (expected_child_iter != expected_children.end() &&
           actual_child_iter != actual_children.end()) {
      const HidCollection* expected_child = expected_child_iter->get();
      const HidCollection* actual_child = actual_child_iter->get();
      ASSERT_EQ(actual_child->GetParent(), actual_collection);
      ValidateLinkCollection(expected_child, actual_child);
      ++expected_child_iter;
      ++actual_child_iter;
    }
    ASSERT_EQ(expected_children.size(), actual_children.size());
  }

  void ValidateCollections(base::span<const uint8_t> report_descriptor_data) {
    descriptor_ = std::make_unique<HidReportDescriptor>(report_descriptor_data);
    const auto& actual_collections = descriptor_->collections();
    auto actual_collection_iter = actual_collections.begin();
    auto expected_collection_iter = expected_collections_.begin();
    while (expected_collection_iter != expected_collections_.end() &&
           actual_collection_iter != actual_collections.end()) {
      ValidateLinkCollection(expected_collection_iter->get(),
                             actual_collection_iter->get());
      ++expected_collection_iter;
      ++actual_collection_iter;
    }
    ASSERT_EQ(expected_collections_.size(), actual_collections.size());
  }

 private:
  std::unique_ptr<HidReportDescriptor> descriptor_;
  std::vector<HidCollectionInfoPtr> expected_collection_infos_;
  HidCollectionVector expected_collections_;
  uint8_t report_id_ = 0;
  HidItemStateTable::HidGlobalItemState globals_;
};

TEST_F(HidReportDescriptorTest, ValidateDetails_Digitizer) {
  auto digitizer = HidCollectionInfo::New();
  digitizer->usage = HidUsageAndPage::New(0x01, mojom::kPageDigitizer);
  ASSERT_FALSE(IsAlwaysProtected(*digitizer->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*digitizer->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*digitizer->usage, HidReportType::kFeature));
  digitizer->report_ids = {0x01, 0x02, 0x03};
  AddTopCollectionInfo(std::move(digitizer));
  ValidateDetails(true, 6, 0, 0, TestReportDescriptors::Digitizer());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_Digitizer) {
  auto* top =
      AddTopCollection(kUsageDigitizerDigitizer, kCollectionTypeApplication);
  auto* puck = AddChild(top, kUsageDigitizerPuck, kCollectionTypePhysical);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 12000, 0, 12);
  SetUnitAndUnitExponent(kUnitInch, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(puck, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetUnitAndUnitExponent(0, 0);
  SetReportSizeAndCount(1, 3);
  AddReportItem(puck, kInput, kAbsoluteVariable,
                {kUsageDigitizerInRange, kUsageDigitizerBarrelSwitch,
                 kUsageDigitizerTipSwitch});
  SetReportSizeAndCount(5, 1);
  AddReportConstant(puck, kInput, kConstant);
  SetReportId(0x02);
  auto* stylus_up =
      AddChild(top, kUsageDigitizerStylus, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 12000, 0, 12);
  SetUnitAndUnitExponent(kUnitInch, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(stylus_up, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetUnitAndUnitExponent(0, 0);
  SetReportSizeAndCount(1, 2);
  AddReportItem(stylus_up, kInput, kAbsoluteVariable, {kUsageDigitizerInRange});
  SetLogicalAndPhysicalBounds(0, 16, 0, 1);
  SetReportSizeAndCount(5, 2);
  AddReportItemRange(stylus_up, kInput, kNullableArray, kUsageButton,
                     kUsageButton + 16);
  SetReportSizeAndCount(2, 2);
  AddReportConstant(stylus_up, kInput, kConstantArray);
  SetReportId(0x03);
  auto* stylus_down =
      AddChild(top, kUsageDigitizerStylus, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 12000, 0, 12);
  SetUnitAndUnitExponent(kUnitInch, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(stylus_down, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetUnitAndUnitExponent(0, 0);
  SetReportSizeAndCount(1, 2);
  AddReportItem(stylus_down, kInput, kAbsoluteVariable,
                {kUsageDigitizerInRange, kUsageDigitizerBarrelSwitch});
  SetReportSizeAndCount(1, 6);
  AddReportConstant(stylus_down, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 127, 0, 45);
  SetUnitAndUnitExponent(kUnitNewton, 4);
  SetReportSizeAndCount(8, 1);
  AddReportItem(stylus_down, kInput, kNonLinearVariable,
                {kUsageDigitizerTipPressure});
  ValidateCollections(TestReportDescriptors::Digitizer());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Keyboard) {
  auto keyboard = HidCollectionInfo::New();
  keyboard->usage = HidUsageAndPage::New(mojom::kGenericDesktopKeyboard,
                                         mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*keyboard->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*keyboard->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*keyboard->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(keyboard));
  ValidateDetails(false, 8, 1, 0, TestReportDescriptors::Keyboard());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_Keyboard) {
  auto* top = AddTopCollection(kUsageGenericDesktopKeyboard,
                               kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 8);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageKeyboardLeftControl,
                     kUsageKeyboardRightGui);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(top, kInput, kConstant);
  SetReportSizeAndCount(1, 5);
  AddReportItemRange(top, kOutput, kAbsoluteVariable, kUsageLedNumLock,
                     kUsageLedKana);
  SetReportSizeAndCount(3, 1);
  AddReportConstant(top, kOutput, kConstant);
  SetLogicalAndPhysicalBounds(0, 101, 0, 0);
  SetReportSizeAndCount(8, 6);
  AddReportItemRange(top, kInput, kNonNullableArray, kUsageKeyboard,
                     kUsageKeyboardApplication);
  ValidateCollections(TestReportDescriptors::Keyboard());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Monitor) {
  auto monitor = HidCollectionInfo::New();
  monitor->usage = HidUsageAndPage::New(0x01, mojom::kPageMonitor0);
  ASSERT_FALSE(IsAlwaysProtected(*monitor->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*monitor->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*monitor->usage, HidReportType::kFeature));
  monitor->report_ids = {0x01, 0x02, 0x03, 0x04, 0x05};
  AddTopCollectionInfo(std::move(monitor));
  ValidateDetails(true, 0, 0, 243, TestReportDescriptors::Monitor());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_Monitor) {
  auto* top =
      AddTopCollection(kUsageMonitorControl, kCollectionTypeApplication);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 128);
  AddReportItem(top, kFeature, kBufferedBytes, {kUsageMonitorEdidInfo});
  SetReportId(0x02);
  SetReportSizeAndCount(8, 243);
  AddReportItem(top, kFeature, kBufferedBytes, {kUsageMonitorVdifInfo});
  SetReportId(0x03);
  SetUnitAndUnitExponent(kUnitCandela, 0x0e);
  SetReportSizeAndCount(16, 1);
  SetLogicalAndPhysicalBounds(0, 200, 0, 0);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageMonitorBrightness});
  SetReportId(0x04);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageMonitorContrast});
  SetReportSizeAndCount(16, 6);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  AddReportItem(
      top, kFeature, kAbsoluteVariable,
      {kUsageMonitorRedVideoGain, kUsageMonitorGreenVideoGain,
       kUsageMonitorBlueVideoGain, kUsageMonitorRedVideoBlackLevel,
       kUsageMonitorGreenVideoBlackLevel, kUsageMonitorBlueVideoBlackLevel});
  SetReportId(0x05);
  SetLogicalAndPhysicalBounds(0, 127, 0, 0);
  AddReportItem(top, kFeature, kAbsoluteVariable,
                {kUsageMonitorHorizontalPosition, kUsageMonitorHorizontalSize,
                 kUsageMonitorVerticalPosition, kUsageMonitorVerticalSize,
                 kUsageMonitorTrapezoidalDistortion, kUsageMonitorTilt});
  ValidateCollections(TestReportDescriptors::Monitor());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_Mouse) {
  auto mouse = HidCollectionInfo::New();
  mouse->usage = HidUsageAndPage::New(mojom::kGenericDesktopMouse,
                                      mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*mouse->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*mouse->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*mouse->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(mouse));
  ValidateDetails(false, 3, 0, 0, TestReportDescriptors::Mouse());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_Mouse) {
  auto* top =
      AddTopCollection(kUsageGenericDesktopMouse, kCollectionTypeApplication);
  auto* physical =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 3);
  AddReportItemRange(physical, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 3);
  SetReportSizeAndCount(5, 1);
  AddReportConstant(physical, kInput, kConstant);
  SetLogicalAndPhysicalBounds(-127, 127, 0, 0);
  SetReportSizeAndCount(8, 2);
  AddReportItem(physical, kInput, kRelativeVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  ValidateCollections(TestReportDescriptors::Mouse());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_LogitechUnifyingReceiver) {
  auto hidpp_short = HidCollectionInfo::New();
  hidpp_short->usage = HidUsageAndPage::New(0x01, mojom::kPageVendor);
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_short->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_short->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_short->usage, HidReportType::kFeature));
  hidpp_short->report_ids = {0x10};
  auto hidpp_long = HidCollectionInfo::New();
  hidpp_long->usage = HidUsageAndPage::New(0x02, mojom::kPageVendor);
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_long->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_long->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_long->usage, HidReportType::kFeature));
  hidpp_long->report_ids = {0x11};
  auto hidpp_dj = HidCollectionInfo::New();
  hidpp_dj->usage = HidUsageAndPage::New(0x04, mojom::kPageVendor);
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_dj->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_dj->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*hidpp_dj->usage, HidReportType::kFeature));
  hidpp_dj->report_ids = {0x20, 0x21};
  AddTopCollectionInfo(std::move(hidpp_short));
  AddTopCollectionInfo(std::move(hidpp_long));
  AddTopCollectionInfo(std::move(hidpp_dj));
  ValidateDetails(true, 31, 31, 0,
                  TestReportDescriptors::LogitechUnifyingReceiver());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_LogitechUnifyingReceiver) {
  auto* short_collection =
      AddTopCollection(kUsageVendor + 0x01, kCollectionTypeApplication);
  SetReportId(0x10);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 6);
  AddReportItem(short_collection, kInput, kNonNullableArray,
                {kUsageVendor + 0x01});
  AddReportItem(short_collection, kOutput, kNonNullableArray,
                {kUsageVendor + 0x01});
  auto* long_collection =
      AddTopCollection(kUsageVendor + 0x02, kCollectionTypeApplication);
  SetReportId(0x11);
  SetReportSizeAndCount(8, 19);
  AddReportItem(long_collection, kInput, kNonNullableArray,
                {kUsageVendor + 0x02});
  AddReportItem(long_collection, kOutput, kNonNullableArray,
                {kUsageVendor + 0x02});
  auto* dj_collection =
      AddTopCollection(kUsageVendor + 0x04, kCollectionTypeApplication);
  SetReportId(0x20);
  SetReportSizeAndCount(8, 14);
  AddReportItem(dj_collection, kInput, kNonNullableArray,
                {kUsageVendor + 0x41});
  AddReportItem(dj_collection, kOutput, kNonNullableArray,
                {kUsageVendor + 0x41});
  SetReportId(0x21);
  SetReportSizeAndCount(8, 31);
  AddReportItem(dj_collection, kInput, kNonNullableArray,
                {kUsageVendor + 0x42});
  AddReportItem(dj_collection, kOutput, kNonNullableArray,
                {kUsageVendor + 0x42});
  ValidateCollections(TestReportDescriptors::LogitechUnifyingReceiver());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SonyDualshock3) {
  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopJoystick,
                                         mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kFeature));
  top_info->report_ids = {0x01, 0x02, 0xee, 0xef};
  AddTopCollectionInfo(std::move(top_info));
  ValidateDetails(true, 48, 48, 48, TestReportDescriptors::SonyDualshock3Usb());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_SonyDualshock3) {
  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  auto* joystick = AddChild(top, kUsageGenericDesktop, kCollectionTypeLogical);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(joystick, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetReportSizeAndCount(1, 19);
  AddReportItemRange(joystick, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 19);
  SetReportSizeAndCount(1, 13);
  AddReportConstant(joystick, kInput, kConstant);
  auto* stick_axes =
      AddChild(joystick, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 255, 0, 255);
  SetReportSizeAndCount(8, 4);
  AddReportItem(stick_axes, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopZ, kUsageGenericDesktopRz});
  SetReportSizeAndCount(8, 39);
  AddReportItem(joystick, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  SetReportSizeAndCount(8, 48);
  AddReportItem(joystick, kOutput, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  AddReportItem(joystick, kFeature, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  auto* report_02_collection =
      AddChild(top, kUsageGenericDesktop, kCollectionTypeLogical);
  SetReportId(0x02);
  AddReportItem(report_02_collection, kFeature, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  auto* report_ee_collection =
      AddChild(top, kUsageGenericDesktop, kCollectionTypeLogical);
  SetReportId(0xee);
  AddReportItem(report_ee_collection, kFeature, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  auto* report_ef_collection =
      AddChild(top, kUsageGenericDesktop, kCollectionTypeLogical);
  SetReportId(0xef);
  AddReportItem(report_ef_collection, kFeature, kAbsoluteVariable,
                {kUsageGenericDesktopPointer});
  ValidateCollections(TestReportDescriptors::SonyDualshock3Usb());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SonyDualshock4) {
  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopGamePad,
                                         mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kFeature));
  top_info->report_ids = {0x01, 0x05, 0x04, 0x02, 0x08, 0x10, 0x11, 0x12, 0x13,
                          0x14, 0x15, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86,
                          0x87, 0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0xa0, 0xa1,
                          0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xf0, 0xf1, 0xf2, 0xa7,
                          0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0};
  AddTopCollectionInfo(std::move(top_info));
  ValidateDetails(true, 63, 31, 63, TestReportDescriptors::SonyDualshock4Usb());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_SonyDualshock4) {
  auto* top =
      AddTopCollection(kUsageGenericDesktopGamePad, kCollectionTypeApplication);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 4);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopZ, kUsageGenericDesktopRz});
  SetLogicalAndPhysicalBounds(0, 7, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(top, kInput, kNullableAbsoluteVariable,
                {kUsageGenericDesktopHatSwitch});
  SetLogicalAndPhysicalBounds(0, 1, 0, 315);
  SetUnitAndUnitExponent(0, 0);
  SetReportSizeAndCount(1, 14);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 14);
  SetLogicalAndPhysicalBounds(0, 127, 0, 315);
  SetReportSizeAndCount(6, 1);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageVendor + 0x20});
  SetLogicalAndPhysicalBounds(0, 255, 0, 315);
  SetReportSizeAndCount(8, 2);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopRx, kUsageGenericDesktopRy});
  SetReportSizeAndCount(8, 54);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageVendor + 0x21});
  SetReportId(0x05);
  SetReportSizeAndCount(8, 31);
  AddReportItem(top, kOutput, kAbsoluteVariable, {kUsageVendor + 0x22});
  SetReportId(0x04);
  SetReportSizeAndCount(8, 36);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x23});
  SetReportId(0x02);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x24});
  SetReportId(0x08);
  SetReportSizeAndCount(8, 3);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x25});
  SetReportId(0x10);
  SetReportSizeAndCount(8, 4);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x26});
  SetReportId(0x11);
  SetReportSizeAndCount(8, 2);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x27});
  SetReportId(0x12);
  SetReportSizeAndCount(8, 15);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor02 + 0x21});
  SetReportId(0x13);
  SetReportSizeAndCount(8, 22);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor02 + 0x22});
  SetReportId(0x14);
  SetReportSizeAndCount(8, 16);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor05 + 0x20});
  SetReportId(0x15);
  SetReportSizeAndCount(8, 44);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor05 + 0x21});
  SetReportId(0x80);
  SetReportSizeAndCount(8, 6);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x20});
  SetReportId(0x81);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x21});
  SetReportId(0x82);
  SetReportSizeAndCount(8, 5);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x22});
  SetReportId(0x83);
  SetReportSizeAndCount(8, 1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x23});
  SetReportId(0x84);
  SetReportSizeAndCount(8, 4);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x24});
  SetReportId(0x85);
  SetReportSizeAndCount(8, 6);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x25});
  SetReportId(0x86);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x26});
  SetReportId(0x87);
  SetReportSizeAndCount(8, 35);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x27});
  SetReportId(0x88);
  SetReportSizeAndCount(8, 34);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x28});
  SetReportId(0x89);
  SetReportSizeAndCount(8, 2);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x29});
  SetReportId(0x90);
  SetReportSizeAndCount(8, 5);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x30});
  SetReportId(0x91);
  SetReportSizeAndCount(8, 3);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x31});
  SetReportId(0x92);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x32});
  SetReportId(0x93);
  SetReportSizeAndCount(8, 12);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x33});
  SetReportId(0xa0);
  SetReportSizeAndCount(8, 6);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x40});
  SetReportId(0xa1);
  SetReportSizeAndCount(8, 1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x41});
  SetReportId(0xa2);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x42});
  SetReportId(0xa3);
  SetReportSizeAndCount(8, 48);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x43});
  SetReportId(0xa4);
  SetReportSizeAndCount(8, 13);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x44});
  SetReportId(0xa5);
  SetReportSizeAndCount(8, 21);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x45});
  SetReportId(0xa6);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x46});
  SetReportId(0xf0);
  SetReportSizeAndCount(8, 63);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x47});
  SetReportId(0xf1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x48});
  SetReportId(0xf2);
  SetReportSizeAndCount(8, 15);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x49});
  SetReportId(0xa7);
  SetReportSizeAndCount(8, 1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x4a});
  SetReportId(0xa8);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x4b});
  SetReportId(0xa9);
  SetReportSizeAndCount(8, 8);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x4c});
  SetReportId(0xaa);
  SetReportSizeAndCount(8, 1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x4e});
  SetReportId(0xab);
  SetReportSizeAndCount(8, 57);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x4f});
  SetReportId(0xac);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x50});
  SetReportId(0xad);
  SetReportSizeAndCount(8, 11);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x51});
  SetReportId(0xae);
  SetReportSizeAndCount(8, 1);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x52});
  SetReportId(0xaf);
  SetReportSizeAndCount(8, 2);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x53});
  SetReportId(0xb0);
  SetReportSizeAndCount(8, 63);
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor80 + 0x54});
  ValidateCollections(TestReportDescriptors::SonyDualshock4Usb());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_XboxWirelessController) {
  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopGamePad,
                                         mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kFeature));
  top_info->report_ids = {0x01, 0x02, 0x03, 0x04};
  AddTopCollectionInfo(std::move(top_info));
  ValidateDetails(
      true, 15, 8, 0,
      TestReportDescriptors::MicrosoftXboxWirelessControllerBluetooth());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_XboxWirelessController) {
  auto* top =
      AddTopCollection(kUsageGenericDesktopGamePad, kCollectionTypeApplication);
  SetReportId(0x01);
  auto* left_stick =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(left_stick, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  auto* right_stick =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  AddReportItem(right_stick, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopRx, kUsageGenericDesktopRy});
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopZ});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(top, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopRz});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(top, kInput, kConstant);
  SetLogicalAndPhysicalBounds(1, 8, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(top, kInput, kNullableAbsoluteVariable,
                {kUsageGenericDesktopHatSwitch});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetUnitAndUnitExponent(0, 0);
  AddReportConstant(top, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 10);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 10);
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(top, kInput, kConstant);
  SetReportId(0x02);
  auto* mode_collection =
      AddChild(top, kUsageGenericDesktopSystemControl, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportItem(mode_collection, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopSystemMainMenu});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(7, 1);
  AddReportConstant(mode_collection, kInput, kConstant);
  SetReportId(0x03);
  auto* pid_collection =
      AddChild(top, kUsagePidSetEffectReport, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidDCEnableActuators});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportConstant(pid_collection, kOutput, kConstant);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  SetReportSizeAndCount(8, 4);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidMagnitude});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetUnitAndUnitExponent(kUnitSecond, 0x0e);
  SetReportSizeAndCount(8, 1);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidDuration});
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidStartDelay});
  SetUnitAndUnitExponent(0, 0);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidLoopCount});
  SetReportId(0x04);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDeviceBatteryStrength});
  ValidateCollections(
      TestReportDescriptors::MicrosoftXboxWirelessControllerBluetooth());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_NintendoSwitchProController) {
  auto top_info = HidCollectionInfo::New();
  top_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopJoystick,
                                         mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*top_info->usage, HidReportType::kFeature));
  top_info->report_ids = {0x30, 0x21, 0x81, 0x01, 0x10, 0x80, 0x82};
  AddTopCollectionInfo(std::move(top_info));
  ValidateDetails(true, 63, 63, 0,
                  TestReportDescriptors::NintendoSwitchProControllerUsb());
}

TEST_F(HidReportDescriptorTest,
       ValidateCollections_NintendoSwitchProController) {
  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetReportId(0x30);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetUnitAndUnitExponent(0, 0);
  SetReportSizeAndCount(1, 10);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 10);
  SetReportSizeAndCount(1, 4);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 11,
                     kUsageButton + 14);
  SetReportSizeAndCount(1, 2);
  AddReportConstant(top, kInput, kConstant);
  auto* stick_axes =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  SetReportSizeAndCount(16, 4);
  AddReportItem(stick_axes, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopZ, kUsageGenericDesktopRz});
  SetLogicalAndPhysicalBounds(0, 7, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopHatSwitch});
  SetLogicalAndPhysicalBounds(0, 1, 0, 315);
  SetReportSizeAndCount(1, 4);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 15,
                     kUsageButton + 18);
  SetReportSizeAndCount(8, 52);
  AddReportConstant(top, kInput, kConstant);
  SetReportId(0x21);
  SetReportSizeAndCount(8, 63);
  AddReportItem(top, kInput, kConstant, {kUsageVendor + 0x01});
  SetReportId(0x81);
  AddReportItem(top, kInput, kConstant, {kUsageVendor + 0x02});
  SetReportId(0x01);
  AddReportItem(top, kOutput, kVolatileConstant, {kUsageVendor + 0x03});
  SetReportId(0x10);
  AddReportItem(top, kOutput, kVolatileConstant, {kUsageVendor + 0x04});
  SetReportId(0x80);
  AddReportItem(top, kOutput, kVolatileConstant, {kUsageVendor + 0x05});
  SetReportId(0x82);
  AddReportItem(top, kOutput, kVolatileConstant, {kUsageVendor + 0x06});
  ValidateCollections(TestReportDescriptors::NintendoSwitchProControllerUsb());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_XboxAdaptiveController) {
  auto gamepad_info = HidCollectionInfo::New();
  gamepad_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopGamePad,
                                             mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*gamepad_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*gamepad_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(
      IsAlwaysProtected(*gamepad_info->usage, HidReportType::kFeature));
  gamepad_info->report_ids = {0x01, 0x02, 0x03, 0x04, 0x06,
                              0x07, 0x08, 0x09, 0x0a, 0x0b};
  auto keyboard_info = HidCollectionInfo::New();
  keyboard_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopKeyboard,
                                              mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*keyboard_info->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*keyboard_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(
      IsAlwaysProtected(*keyboard_info->usage, HidReportType::kFeature));
  keyboard_info->report_ids = {0x05};
  AddTopCollectionInfo(std::move(gamepad_info));
  AddTopCollectionInfo(std::move(keyboard_info));
  ValidateDetails(
      true, 54, 8, 64,
      TestReportDescriptors::MicrosoftXboxAdaptiveControllerBluetooth());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_XboxAdaptiveController) {
  auto* gamepad =
      AddTopCollection(kUsageGenericDesktopGamePad, kCollectionTypeApplication);
  SetReportId(0x01);
  auto* left_stick =
      AddChild(gamepad, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(left_stick, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY});
  auto* right_stick =
      AddChild(gamepad, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  AddReportItem(right_stick, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopZ, kUsageGenericDesktopRz});
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(gamepad, kInput, kAbsoluteVariable, {kUsageSimulationBrake});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(gamepad, kInput, kAbsoluteVariable,
                {kUsageSimulationAccelerator});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(1, 8, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(gamepad, kInput, kNullableAbsoluteVariable,
                {kUsageGenericDesktopHatSwitch});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetUnitAndUnitExponent(0, 0);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 15);
  AddReportItemRange(gamepad, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 15);
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  AddReportItem(gamepad, kInput, kAbsoluteVariable, {kUsageConsumerACBack});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(7, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  auto* left_stick2 =
      AddChild(gamepad, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  SetReportSizeAndCount(16, 2);
  AddReportItem(left_stick2, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopVx, kUsageGenericDesktopVy});
  auto* right_stick2 =
      AddChild(gamepad, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 0xffff, 0, 0);
  AddReportItem(right_stick2, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopVbrx, kUsageGenericDesktopVbry});
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(gamepad, kInput, kAbsoluteVariable, {kUsageGenericDesktopVz});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1023, 0, 0);
  SetReportSizeAndCount(10, 1);
  AddReportItem(gamepad, kInput, kAbsoluteVariable, {kUsageGenericDesktopVbrz});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(6, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(1, 8, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(gamepad, kInput, kNullableAbsoluteVariable,
                {kUsageGenericDesktopDial});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetUnitAndUnitExponent(0, 0);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 15);
  AddReportItemRange(gamepad, kInput, kAbsoluteVariable, kUsageButton + 16,
                     kUsageButton + 30);
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  AddReportItem(gamepad, kInput, kAbsoluteVariable, {kUsageConsumerModeStep});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(7, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  auto* consumer_collection =
      AddChild(gamepad, kUsageConsumerControl, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0x81});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportConstant(consumer_collection, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0x84});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportConstant(consumer_collection, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 1);
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0x85});
  SetReportSizeAndCount(4, 1);
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0x99});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportConstant(consumer_collection, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 1);
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0x9e});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xa1});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xa2});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xa3});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xa4});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xb9});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xba});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xbb});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xbe});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc0});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc1});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc2});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc3});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc4});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc5});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc6});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc7});
  AddReportItem(consumer_collection, kInput, kAbsoluteVariable,
                {kUsageConsumer + 0xc8});
  SetReportId(0x02);
  auto* mode_collection =
      AddChild(gamepad, kUsageConsumerControl, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportItem(mode_collection, kInput, kAbsoluteVariable,
                {kUsageConsumerACHome});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  SetReportSizeAndCount(7, 1);
  AddReportConstant(mode_collection, kInput, kConstant);
  SetReportId(0x03);
  auto* pid_collection =
      AddChild(gamepad, kUsagePidSetEffectReport, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidDCEnableActuators});
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportConstant(pid_collection, kOutput, kConstant);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  SetReportSizeAndCount(8, 4);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidMagnitude});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetUnitAndUnitExponent(kUnitSecond, 0x0e);
  SetReportSizeAndCount(8, 1);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidDuration});
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidStartDelay});
  SetUnitAndUnitExponent(0, 0);
  AddReportItem(pid_collection, kOutput, kAbsoluteVariable,
                {kUsagePidLoopCount});
  SetReportId(0x04);
  AddReportItem(gamepad, kInput, kAbsoluteVariable,
                {kUsageGenericDeviceBatteryStrength});
  SetReportId(0x06);
  auto* report_06_collection =
      AddChild(gamepad, kUsageVendor + 0x01, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  AddReportItem(report_06_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x01});
  AddReportItem(report_06_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x02});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  AddReportItem(report_06_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x03});
  SetReportSizeAndCount(8, 60);
  AddReportItem(report_06_collection, kFeature, kBufferedBytes,
                {kUsageVendor + 0x04});
  SetReportId(0x07);
  auto* report_07_collection =
      AddChild(gamepad, kUsageVendor + 0x02, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  SetReportSizeAndCount(8, 1);
  AddReportItem(report_07_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x05});
  AddReportItem(report_07_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x06});
  AddReportItem(report_07_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x07});
  SetReportId(0x08);
  auto* report_08_collection =
      AddChild(gamepad, kUsageVendor + 0x03, kCollectionTypeLogical);
  AddReportItem(report_08_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x08});
  AddReportItem(report_08_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x09});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  AddReportItem(report_08_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x0a});
  SetReportId(0x09);
  auto* report_09_collection =
      AddChild(gamepad, kUsageVendor + 0x04, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);
  AddReportItem(report_09_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x0b});
  AddReportItem(report_09_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x0c});
  AddReportItem(report_09_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x0d});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  AddReportItem(report_09_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x0e});
  SetReportSizeAndCount(8, 60);
  AddReportItem(report_09_collection, kFeature, kBufferedBytes,
                {kUsageVendor + 0x0f});
  SetReportId(0x0a);
  auto* report_0a_collection =
      AddChild(gamepad, kUsageVendor + 0x05, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 0x7fffffff, 0, 0);
  SetReportSizeAndCount(32, 1);
  AddReportItem(report_0a_collection, kInput, kAbsoluteVariable,
                {kUsageVendor + 0x10});
  AddReportItem(report_0a_collection, kInput, kAbsoluteVariable,
                {kUsageVendor + 0x11});
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 2);
  AddReportItem(report_0a_collection, kInput, kAbsoluteVariable,
                {kUsageVendor + 0x12});
  SetReportSizeAndCount(8, 1);
  AddReportItem(report_0a_collection, kInput, kAbsoluteVariable,
                {kUsageVendor + 0x13});
  SetReportId(0x0b);
  auto* report_0b_collection =
      AddChild(gamepad, kUsageVendor + 0x06, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 100, 0, 0);

  AddReportItem(report_0b_collection, kFeature, kAbsoluteVariable,
                {kUsageVendor + 0x14});
  SetReportId(0x05);
  auto* keyboard = AddTopCollection(kUsageGenericDesktopKeyboard,
                                    kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 8);
  AddReportItemRange(keyboard, kInput, kAbsoluteVariable,
                     kUsageKeyboardLeftControl, kUsageKeyboardRightGui);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(keyboard, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 101, 0, 0);
  SetReportSizeAndCount(8, 6);
  AddReportItemRange(keyboard, kInput, kNonNullableArray, kUsageKeyboard,
                     kUsageKeyboardApplication);
  ValidateCollections(
      TestReportDescriptors::MicrosoftXboxAdaptiveControllerBluetooth());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_NexusPlayerController) {
  auto gamepad_info = HidCollectionInfo::New();
  gamepad_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopGamePad,
                                             mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*gamepad_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*gamepad_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(
      IsAlwaysProtected(*gamepad_info->usage, HidReportType::kFeature));
  gamepad_info->report_ids = {0x01, 0x02};
  auto status_info = HidCollectionInfo::New();
  status_info->usage = HidUsageAndPage::New(mojom::kGenericDesktopGamePad,
                                            mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*status_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*status_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*status_info->usage, HidReportType::kFeature));
  status_info->report_ids = {0x03};
  AddTopCollectionInfo(std::move(gamepad_info));
  AddTopCollectionInfo(std::move(status_info));
  ValidateDetails(true, 8, 1, 0,
                  TestReportDescriptors::NexusPlayerController());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_NexusPlayerController) {
  auto* gamepad =
      AddTopCollection(kUsageGenericDesktopGamePad, kCollectionTypeApplication);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 11);
  AddReportItem(
      gamepad, kInput, kAbsoluteVariable,
      {kUsageButton + 1, kUsageButton + 2, kUsageButton + 4, kUsageButton + 5,
       kUsageButton + 7, kUsageButton + 8, kUsageButton + 14, kUsageButton + 15,
       kUsageButton + 13, kUsageConsumerACBack, kUsageConsumerACHome});
  SetReportSizeAndCount(1, 1);
  AddReportConstant(gamepad, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 7, 0, 315);
  SetUnitAndUnitExponent(kUnitDegrees, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItem(gamepad, kInput, kNullableAbsoluteVariable,
                {kUsageGenericDesktopHatSwitch});
  SetUnitAndUnitExponent(0, 0);
  auto* axes_collection =
      AddChild(gamepad, kUsageGenericDesktop, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 255, 0, 255);
  SetReportSizeAndCount(8, 6);
  AddReportItem(axes_collection, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopZ, kUsageGenericDesktopRz,
                 kUsageSimulationBrake, kUsageSimulationAccelerator});
  SetReportId(0x02);
  SetLogicalAndPhysicalBounds(0, 1, 0, 255);
  SetReportSizeAndCount(1, 4);
  AddReportItem(gamepad, kOutput, kAbsoluteVariable,
                {kUsageLedNumLock, kUsageLedCapsLock, kUsageLedScrollLock,
                 kUsageLedCompose});
  SetReportSizeAndCount(4, 1);
  AddReportConstant(gamepad, kOutput, kConstant);
  SetReportId(0x03);
  auto* status =
      AddTopCollection(kUsageGenericDesktopGamePad, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 255, 0, 255);
  SetReportSizeAndCount(8, 1);
  AddReportItem(status, kInput, kAbsoluteVariable,
                {kUsageGenericDeviceBatteryStrength});
  SetReportSizeAndCount(8, 6);
  AddReportItem(status, kInput, kAbsoluteVariable, {0xffbcbdad});
  ValidateCollections(TestReportDescriptors::NexusPlayerController());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SteamControllerKeyboard) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(mojom::kGenericDesktopKeyboard,
                                     mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 8, 1, 0,
                  TestReportDescriptors::SteamControllerKeyboard());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_SteamControllerKeyboard) {
  auto* top = AddTopCollection(kUsageGenericDesktopKeyboard,
                               kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 8);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageKeyboardLeftControl,
                     kUsageKeyboardRightGui);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(top, kInput, kConstantArray);
  SetReportSizeAndCount(1, 5);
  AddReportItemRange(top, kOutput, kAbsoluteVariable, kUsageLedNumLock,
                     kUsageLedKana);
  SetReportSizeAndCount(3, 1);
  AddReportConstant(top, kOutput, kConstantArray);
  SetReportSizeAndCount(8, 6);
  SetLogicalAndPhysicalBounds(0, 101, 0, 0);
  AddReportItemRange(top, kInput, kNonNullableArray, kUsageKeyboard,
                     kUsageKeyboardApplication);
  ValidateCollections(TestReportDescriptors::SteamControllerKeyboard());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SteamControllerMouse) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(mojom::kGenericDesktopMouse,
                                     mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 4, 0, 0,
                  TestReportDescriptors::SteamControllerMouse());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_SteamControllerMouse) {
  auto* top =
      AddTopCollection(kUsageGenericDesktopMouse, kCollectionTypeApplication);
  auto* pointer =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 5);
  AddReportItemRange(pointer, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 5);
  SetReportSizeAndCount(3, 1);
  AddReportConstant(pointer, kInput, kConstantArray);
  SetLogicalAndPhysicalBounds(-127, 127, 0, 0);
  SetReportSizeAndCount(8, 3);
  AddReportItem(pointer, kInput, kRelativeVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopWheel});
  ValidateCollections(TestReportDescriptors::SteamControllerMouse());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_SteamControllerVendor) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(0x01, mojom::kPageVendor);
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 64, 64, 64,
                  TestReportDescriptors::SteamControllerVendor());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_SteamControllerVendor) {
  auto* top = AddTopCollection(kUsageVendor + 0x01, kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 64);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageVendor + 0x01});
  AddReportItem(top, kOutput, kAbsoluteVariable, {kUsageVendor + 0x01});
  AddReportItem(top, kFeature, kAbsoluteVariable, {kUsageVendor + 0x01});
  ValidateCollections(TestReportDescriptors::SteamControllerVendor());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_XSkillsUsbAdapter) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(mojom::kGenericDesktopJoystick,
                                     mojom::kPageGenericDesktop);
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 7, 4, 0, TestReportDescriptors::XSkillsUsbAdapter());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_XSkillsUsbAdapter) {
  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetReportSizeAndCount(1, 12);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 12);
  SetReportSizeAndCount(1, 4);
  AddReportConstant(top, kInput, kConstant);
  SetLogicalAndPhysicalBounds(0, 255, 0, 255);
  SetReportSizeAndCount(8, 4);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopRz, kUsageGenericDesktopZ});
  SetLogicalAndPhysicalBounds(0, 15, 0, 15);
  SetReportSizeAndCount(4, 2);
  AddReportItem(top, kInput, kAbsoluteVariable,
                {kUsageGenericDesktopRx, kUsageGenericDesktopRy});
  SetReportSizeAndCount(8, 4);
  AddReportItemRange(top, kOutput, kAbsoluteVariable, kUsageVendor + 0x01,
                     kUsageVendor + 0x04);
  ValidateCollections(TestReportDescriptors::XSkillsUsbAdapter());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_BelkinNostromoKeyboard) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(mojom::kGenericDesktopKeyboard,
                                     mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 8, 0, 0,
                  TestReportDescriptors::BelkinNostromoKeyboard());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_BelkinNostromoKeyboard) {
  auto* top = AddTopCollection(kUsageGenericDesktopKeyboard,
                               kCollectionTypeApplication);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 8);
  AddReportItemRange(top, kInput, kAbsoluteVariable, kUsageKeyboardLeftControl,
                     kUsageKeyboardRightGui);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(top, kInput, kConstantArray);
  SetLogicalAndPhysicalBounds(0, 101, 0, 0);
  SetReportSizeAndCount(8, 6);
  AddReportItemRange(top, kInput, kNonNullableArray, kUsageKeyboard,
                     kUsageKeyboardApplication);
  ValidateCollections(TestReportDescriptors::BelkinNostromoKeyboard());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_BelkinNostromoMouseAndExtra) {
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(mojom::kGenericDesktopMouse,
                                     mojom::kPageGenericDesktop);
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kInput));
  ASSERT_TRUE(IsAlwaysProtected(*info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*info->usage, HidReportType::kFeature));
  AddTopCollectionInfo(std::move(info));
  ValidateDetails(false, 4, 1, 0,
                  TestReportDescriptors::BelkinNostromoMouseAndExtra());
}

TEST_F(HidReportDescriptorTest,
       ValidateCollections_BelkinNostromoMouseAndExtra) {
  auto* top =
      AddTopCollection(kUsageGenericDesktopMouse, kCollectionTypeApplication);
  auto* pointer =
      AddChild(top, kUsageGenericDesktopPointer, kCollectionTypePhysical);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 3);
  AddReportItemRange(pointer, kInput, kAbsoluteVariable, kUsageButton + 1,
                     kUsageButton + 3);
  SetReportSizeAndCount(5, 1);
  AddReportConstant(pointer, kInput, kConstantArray);
  SetLogicalAndPhysicalBounds(-127, 127, 0, 0);
  SetReportSizeAndCount(8, 3);
  AddReportItem(pointer, kInput, kRelativeVariable,
                {kUsageGenericDesktopX, kUsageGenericDesktopY,
                 kUsageGenericDesktopWheel});
  SetLogicalAndPhysicalBounds(0, 1, 0, 1);
  SetReportSizeAndCount(1, 3);
  AddReportItemRange(pointer, kOutput, kAbsoluteVariable, kUsageLedNumLock,
                     kUsageLedScrollLock);
  SetReportSizeAndCount(5, 1);
  AddReportConstant(pointer, kOutput, kConstantArray);
  ValidateCollections(TestReportDescriptors::BelkinNostromoMouseAndExtra());
}

TEST_F(HidReportDescriptorTest, ValidateDetails_JabraLink380c) {
  auto telephony_info = HidCollectionInfo::New();
  telephony_info->usage = HidUsageAndPage::New(0x05, mojom::kPageTelephony);
  ASSERT_FALSE(
      IsAlwaysProtected(*telephony_info->usage, HidReportType::kInput));
  ASSERT_FALSE(
      IsAlwaysProtected(*telephony_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(
      IsAlwaysProtected(*telephony_info->usage, HidReportType::kFeature));
  telephony_info->report_ids = {0x02};
  auto vendor_info = HidCollectionInfo::New();
  vendor_info->usage = HidUsageAndPage::New(0x01, mojom::kPageVendor);
  ASSERT_FALSE(IsAlwaysProtected(*vendor_info->usage, HidReportType::kInput));
  ASSERT_FALSE(IsAlwaysProtected(*vendor_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(IsAlwaysProtected(*vendor_info->usage, HidReportType::kFeature));
  vendor_info->report_ids = {0x05, 0x04};
  auto consumer_info = HidCollectionInfo::New();
  consumer_info->usage = HidUsageAndPage::New(0x01, mojom::kPageConsumer);
  ASSERT_FALSE(IsAlwaysProtected(*consumer_info->usage, HidReportType::kInput));
  ASSERT_FALSE(
      IsAlwaysProtected(*consumer_info->usage, HidReportType::kOutput));
  ASSERT_FALSE(
      IsAlwaysProtected(*consumer_info->usage, HidReportType::kFeature));
  consumer_info->report_ids = {0x01};
  AddTopCollectionInfo(std::move(telephony_info));
  AddTopCollectionInfo(std::move(vendor_info));
  AddTopCollectionInfo(std::move(consumer_info));
  ValidateDetails(true, 62, 62, 1, TestReportDescriptors::JabraLink380c());
}

TEST_F(HidReportDescriptorTest, ValidateCollections_JabraLink380c) {
  auto* telephony =
      AddTopCollection(kUsageTelephonyHeadset, kCollectionTypeApplication);
  SetReportId(0x02);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 3);
  AddReportItem(telephony, kInput, kConstantAbsoluteVariablePreferredState,
                {kUsageTelephonyHookSwitch, kUsageTelephonyLineBusyTone,
                 kUsageTelephonyLine});
  SetReportSizeAndCount(1, 4);
  AddReportItem(telephony, kInput, kConstantRelativeVariable,
                {kUsageTelephonyPhoneMute, kUsageTelephonyFlash,
                 kUsageTelephonyRedial, kUsageTelephonySpeedDial});
  auto* keypad =
      AddChild(telephony, kUsageTelephonyKeyPad, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 12, 0, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItemRange(keypad, kInput, kNullableArray, kUsageTelephonyPhoneKey0,
                     kUsageTelephonyPhoneKeyPound);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportItem(telephony, kInput, kAbsoluteVariable,
                {kUsageTelephonyProgrammableButton});
  SetReportSizeAndCount(1, 4);
  AddReportConstant(telephony, kInput, kConstantArray);
  SetReportSizeAndCount(1, 7);
  AddReportItem(
      telephony, kOutput, kAbsoluteVariablePreferredState,
      {kUsageLedOffHook, kUsageLedSpeaker, kUsageLedMute, kUsageLedRing,
       kUsageLedHold, kUsageLedMicrophone, kUsageLedOnLine});
  SetReportSizeAndCount(1, 1);
  AddReportItem(telephony, kOutput, kAbsoluteVariablePreferredState,
                {kUsageTelephonyRinger});
  SetReportSizeAndCount(1, 8);
  AddReportConstant(telephony, kOutput, kConstantArray);
  auto* vendor = AddTopCollection(kUsageVendor + 1, kCollectionTypeApplication);
  SetReportId(0x05);
  SetLogicalAndPhysicalBounds(0, 255, 0, 0);
  SetReportSizeAndCount(8, 62);
  AddReportItem(vendor, kOutput, kBufferedBytes, {kUsageVendor + 1});
  AddReportItem(vendor, kInput, kBufferedBytes, {kUsageVendor + 1});
  SetReportId(0x04);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 4);
  AddReportItem(vendor, kInput, kConstantAbsoluteVariablePreferredState,
                {kUsageVendor30 + 0x20, kUsageVendor30 + 0xfffb,
                 kUsageVendor30 + 0x97, kUsageVendor30 + 0x2a});
  SetReportSizeAndCount(1, 5);
  AddReportItem(
      vendor, kInput, kConstantRelativeVariable,
      {kUsageVendor30 + 0x2f, kUsageVendor30 + 0x21, kUsageVendor30 + 0x24,
       kUsageVendor30 + 0xfffd, kUsageVendor30 + 0x50});
  auto* vendor_child =
      AddChild(vendor, kUsageVendor30 + 6, kCollectionTypeLogical);
  SetLogicalAndPhysicalBounds(0, 12, 0, 0);
  SetReportSizeAndCount(4, 1);
  AddReportItemRange(vendor_child, kInput, kNullableArray,
                     kUsageVendor30 + 0xb0, kUsageVendor30 + 0xbb);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 1);
  AddReportItem(vendor, kInput, kConstantAbsoluteVariablePreferredState,
                {kUsageVendor30 + 0xfffc});
  SetReportSizeAndCount(1, 2);
  AddReportConstant(vendor, kInput, kConstantArray);
  SetReportSizeAndCount(1, 7);
  AddReportItem(
      vendor, kOutput, kAbsoluteVariablePreferredState,
      {kUsageVendor40 + 0x17, kUsageVendor40 + 0xfffb, kUsageVendor40 + 0x09,
       kUsageVendor40 + 0x18, kUsageVendor40 + 0x20, kUsageVendor40 + 0x21,
       kUsageVendor40 + 0x2a});
  SetReportSizeAndCount(1, 1);
  AddReportItem(vendor, kOutput, kAbsoluteVariablePreferredState,
                {kUsageVendor30 + 0x9e});
  SetReportSizeAndCount(1, 8);
  AddReportConstant(vendor, kOutput, kConstantArray);
  SetReportSizeAndCount(1, 1);
  AddReportItem(vendor, kFeature, kAbsoluteVariablePreferredState,
                {kUsageVendor30 + 0xffff});
  SetReportSizeAndCount(1, 7);
  AddReportConstant(vendor, kFeature, kConstantArray);
  auto* consumer =
      AddTopCollection(kUsageConsumerControl, kCollectionTypeApplication);
  SetReportId(0x01);
  SetLogicalAndPhysicalBounds(0, 1, 0, 0);
  SetReportSizeAndCount(1, 9);
  AddReportItem(
      consumer, kInput, kAbsoluteVariable,
      {kUsageConsumerVolumeDecrement, kUsageConsumerVolumeIncrement,
       kUsageConsumerMute, kUsageConsumerPlayPause, kUsageConsumerStop,
       kUsageConsumerScanNextTrack, kUsageConsumerScanPreviousTrack,
       kUsageConsumerFastForward, kUsageConsumerRewind});
  SetReportSizeAndCount(1, 7);
  AddReportConstant(consumer, kInput, kConstantArray);
  ValidateCollections(TestReportDescriptors::JabraLink380c());
}

TEST_F(HidReportDescriptorTest, InvalidReportSizePermitted) {
  // Report size can be at most 32 bits. However, some devices specify larger
  // sizes. Make sure an invalid report is still reflected in the maximum
  // report size. The descriptor below describes a report with one 128-bit
  // constant field.
  static const uint8_t kInvalidReportSizeDescriptor[] = {
      0xA0,        // Collection
      0x95, 0x01,  //   Report Count (1)
      0x75, 0x80,  //   Report Size (128)
      0x90         //   Output
  };
  auto report_descriptor_data = base::make_span(kInvalidReportSizeDescriptor);
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(0, 0);
  AddTopCollectionInfo(std::move(info));
  // Maximum report sizes should include the invalid report.
  ValidateDetails(false, 0, 16, 0, report_descriptor_data);

  // The report item with invalid size should still be included in the
  // collection info.
  auto* top = AddTopCollection(0, kCollectionTypePhysical);
  SetReportSizeAndCount(128, 1);
  AddReportConstant(top, kOutput, kNonNullableArray);
  ValidateCollections(report_descriptor_data);
}

TEST_F(HidReportDescriptorTest, ReasonablyHugeReportNotIgnored) {
  // The descriptor below defines a 2^16-1 byte output report. Larger reports
  // are considered unreasonable and are ignored in the max report size
  // calculation.
  static const uint8_t kReasonablyHugeReportDescriptor[] = {
      0xA0,              // Collection
      0x96, 0xff, 0xff,  //   Report Count (65535)
      0x75, 0x08,        //   Report Size (8)
      0x90               //   Output
  };
  auto report_descriptor_data =
      base::make_span(kReasonablyHugeReportDescriptor);
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(0, 0);
  AddTopCollectionInfo(std::move(info));
  // Maximum report sizes should include the huge report.
  ValidateDetails(false, 0, 65535, 0, report_descriptor_data);

  auto* top = AddTopCollection(0, kCollectionTypePhysical);
  SetReportSizeAndCount(8, 65535);
  AddReportConstant(top, kOutput, kNonNullableArray);
  ValidateCollections(report_descriptor_data);
}

TEST_F(HidReportDescriptorTest, UnreasonablyHugeReportIgnored) {
  // The descriptor below defines a 2^16 byte output report. The report is
  // larger than the maximum report size considered reasonable and will be
  // ignored when computing the max report size.
  static const uint8_t kUnreasonablyHugeReportDescriptor[] = {
      0xA0,                          // Collection
      0x97, 0x00, 0x00, 0x01, 0x00,  //   Report Count (65536)
      0x75, 0x08,                    //   Report Size (8)
      0x90                           //   Output
  };
  auto report_descriptor_data =
      base::make_span(kUnreasonablyHugeReportDescriptor);
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(0, 0);
  AddTopCollectionInfo(std::move(info));
  // Maximum report sizes should not be affected by the huge report.
  ValidateDetails(false, 0, 0, 0, report_descriptor_data);

  // The unreasonably huge report item should still be included in the
  // collection info.
  auto* top = AddTopCollection(0, kCollectionTypePhysical);
  SetReportSizeAndCount(8, 65536);
  AddReportConstant(top, kOutput, kNonNullableArray);
  ValidateCollections(report_descriptor_data);
}

TEST_F(HidReportDescriptorTest, HighlyNestedReportLimitsDepth) {
  // The HID report descriptor parser sets a maximum depth to prevent issues
  // with descriptors that define many nested collections. The descriptor below
  // nests a single constant inside 51 collections.
  static const uint8_t kHighlyNestedReportDescriptor[] = {
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,

      0x95, 0x01,  // Report Count (1)
      0x75, 0x08,  // Report Size (8)
      0x90         // Output
  };
  auto report_descriptor_data = base::make_span(kHighlyNestedReportDescriptor);
  auto info = HidCollectionInfo::New();
  info->usage = HidUsageAndPage::New(0, 0);
  AddTopCollectionInfo(std::move(info));
  // The item in the innermost collection should still be reflected in the
  // maximum report size.
  ValidateDetails(false, 0, 1, 0, report_descriptor_data);

  // Construct nested collections up to the depth limit. The item from the
  // innermost collection should be propagated to all its parents even though
  // the depth limit has been reached.
  auto* parent = AddTopCollection(0, kCollectionTypePhysical);
  for (size_t i = 1; i < 50; ++i)
    parent = AddChild(parent, 0, kCollectionTypePhysical);
  SetReportSizeAndCount(8, 1);
  AddReportConstant(parent, kOutput, kNonNullableArray);
  ValidateCollections(report_descriptor_data);
}

TEST_F(HidReportDescriptorTest, ExtraEndCollectionIgnored) {
  // When the report descriptor parser encounters an End Collection item,
  // it should decrement the collection depth only if a matching Collection
  // item was previously encountered. If no Collection item was encountered,
  // the End Collection item should be ignored.
  static const uint8_t kExtraEndCollectionDescriptor[] = {
      0xC0,  // End Collection

      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0,
      0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0, 0xA0};

  // Construct nested collections up to the depth limit. If the extra End
  // Collection item was ignored, the depth limit should have prevented the
  // innermost collection from being created.
  auto* parent = AddTopCollection(0, kCollectionTypePhysical);
  for (size_t i = 1; i < 50; ++i)
    parent = AddChild(parent, 0, kCollectionTypePhysical);
  ValidateCollections(base::make_span(kExtraEndCollectionDescriptor));
}

TEST_F(HidReportDescriptorTest, ZeroByteLogicalMinMax) {
  static const uint8_t kZeroByteLogicalMinMaxDescriptor[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,  // Usage (Joystick)
      0xA1, 0x01,  // Collection (Application)
      0x75, 0x08,  //   Report Size (8)
      0x95, 0x01,  //   Report Count (1)
      0x09, 0x30,  //   Usage (X)
      0x14,        //   Logical Minimum (0)
      0x24,        //   Logical Maximum (0)
      0x34,        //   Physical Minimum (0)
      0x44,        //   Physical Maximum (0)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                   //   Null Position)
      0xC0,        // End Collection
  };

  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetReportSizeAndCount(8, 1);
  SetLogicalAndPhysicalBounds(0, 0, 0, 0);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopX});
  ValidateCollections(base::make_span(kZeroByteLogicalMinMaxDescriptor));
}

TEST_F(HidReportDescriptorTest, OneByteLogicalMinMax) {
  static const uint8_t kOneByteLogicalMinMaxDescriptor[] = {
      0x05, 0x01,  // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,  // Usage (Joystick)
      0xA1, 0x01,  // Collection (Application)
      0x75, 0x08,  //   Report Size (8)
      0x95, 0x01,  //   Report Count (1)
      0x09, 0x30,  //   Usage (X)
      0x15, 0x81,  //   Logical Minimum (-127)
      0x25, 0x7F,  //   Logical Maximum (127)
      0x35, 0x81,  //   Physical Minimum (-127)
      0x45, 0x7F,  //   Physical Maximum (127)
      0x81, 0x02,  //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No
                   //   Null Position)
      0xC0,        // End Collection
  };

  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetReportSizeAndCount(8, 1);
  SetLogicalAndPhysicalBounds(-127, 127, -127, 127);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopX});
  ValidateCollections(base::make_span(kOneByteLogicalMinMaxDescriptor));
}

TEST_F(HidReportDescriptorTest, TwoByteLogicalMinMax) {
  static const uint8_t kTwoByteLogicalMinMaxDescriptor[] = {
      0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,        // Usage (Joystick)
      0xA1, 0x01,        // Collection (Application)
      0x75, 0x10,        //   Report Size (16)
      0x95, 0x01,        //   Report Count (1)
      0x09, 0x30,        //   Usage (X)
      0x16, 0x01, 0x80,  //   Logical Minimum (-32767)
      0x26, 0xFF, 0x7F,  //   Logical Maximum (32767)
      0x36, 0x01, 0x80,  //   Physical Minimum (-32767)
      0x46, 0xFF, 0x7F,  //   Physical Maximum (32767)
      0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred
                         //   State,No Null Position)
      0xC0,              // End Collection
  };

  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetReportSizeAndCount(16, 1);
  SetLogicalAndPhysicalBounds(-32767, 32767, -32767, 32767);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopX});
  ValidateCollections(base::make_span(kTwoByteLogicalMinMaxDescriptor));
}

TEST_F(HidReportDescriptorTest, FourByteLogicalMinMax) {
  static const uint8_t kFourByteLogicalMinMaxDescriptor[] = {
      0x05, 0x01,                    // Usage Page (Generic Desktop Ctrls)
      0x09, 0x04,                    // Usage (Joystick)
      0xA1, 0x01,                    // Collection (Application)
      0x75, 0x20,                    //   Report Size (32)
      0x95, 0x01,                    //   Report Count (1)
      0x09, 0x30,                    //   Usage (X)
      0x17, 0x01, 0x00, 0x00, 0x80,  //   Logical Minimum (-2147483647)
      0x27, 0xFF, 0xFF, 0xFF, 0x7F,  //   Logical Maximum (2147483647)
      0x37, 0x01, 0x00, 0x00, 0x80,  //   Physical Minimum (-2147483647)
      0x47, 0xFF, 0xFF, 0xFF, 0x7F,  //   Physical Maximum (2147483647)
      0x81, 0x02,                    //   Input (Data,Var,Abs,No Wrap,Linear,
                                     //   Preferred State,No Null Position)
      0xC0,                          // End Collection
  };

  auto* top = AddTopCollection(kUsageGenericDesktopJoystick,
                               kCollectionTypeApplication);
  SetReportSizeAndCount(32, 1);
  SetLogicalAndPhysicalBounds(-2147483647, 2147483647, -2147483647, 2147483647);
  AddReportItem(top, kInput, kAbsoluteVariable, {kUsageGenericDesktopX});
  ValidateCollections(base::make_span(kFourByteLogicalMinMaxDescriptor));
}

}  // namespace device
