// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/hid/hid_device.h"

#include "services/device/public/mojom/hid.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/modules/hid/hid.h"
#include "third_party/blink/renderer/platform/bindings/dom_wrapper_world.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

namespace blink {

namespace {

// Construct and return a sample HID report item.
device::mojom::blink::HidReportItemPtr MakeReportItem() {
  auto item = device::mojom::blink::HidReportItem::New();
  item->is_range = false;  // Usages for this item are defined by |usages|.

  // Configure the report item with reasonable values for a button-like input.
  item->is_constant = false;         // Data.
  item->is_variable = true;          // Variable.
  item->is_relative = false;         // Absolute.
  item->wrap = false;                // No wrap.
  item->is_non_linear = false;       // Linear.
  item->no_preferred_state = false;  // Preferred State.
  item->has_null_position = false;   // No Null position.
  item->is_volatile = false;         // Non Volatile.
  item->is_buffered_bytes = false;   // Bit Field.

  // Assign the primary button usage to this item.
  item->usages.push_back(device::mojom::blink::HidUsageAndPage::New(
      0x01, device::mojom::blink::kPageButton));
  // |usage_minimum| and |usage_maximum| are unused.
  item->usage_minimum = device::mojom::blink::HidUsageAndPage::New(0, 0);
  item->usage_maximum = device::mojom::blink::HidUsageAndPage::New(0, 0);

  // Set the designator index and string index extents to zero. This indicates
  // that no physical designators or strings are associated with this item.
  item->designator_minimum = 0;
  item->designator_minimum = 0;
  item->string_minimum = 0;
  item->string_maximum = 0;

  // The report field described by this item can only hold the logical values 0
  // and 1.
  item->logical_minimum = 0;
  item->logical_maximum = 1;
  item->physical_minimum = 0;
  item->physical_maximum = 1;

  // Values reported in this field are unitless.
  item->unit_exponent = 0;
  item->unit = 0;

  // This item defines a single report field, 8 bits wide.
  item->report_size = 8;  // 1 byte.
  item->report_count = 1;

  return item;
}

}  // namespace

TEST(HIDDeviceTest, singleUsageItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  // Check that all item properties are correctly converted for the sample
  // report item.
  EXPECT_TRUE(item->isAbsolute());
  EXPECT_FALSE(item->isArray());
  EXPECT_FALSE(item->isRange());
  EXPECT_FALSE(item->hasNull());
  EXPECT_EQ(1U, item->usages().size());
  EXPECT_EQ(0x00090001U, item->usages()[0]);
  EXPECT_FALSE(item->hasUsageMinimum());
  EXPECT_FALSE(item->hasUsageMaximum());
  EXPECT_FALSE(item->hasStrings());
  EXPECT_EQ(8U, item->reportSize());
  EXPECT_EQ(1U, item->reportCount());
  EXPECT_EQ(0, item->unitExponent());
  EXPECT_EQ(V8HIDUnitSystem::Enum::kNone, item->unitSystem());
  EXPECT_EQ(0, item->unitFactorLengthExponent());
  EXPECT_EQ(0, item->unitFactorMassExponent());
  EXPECT_EQ(0, item->unitFactorTimeExponent());
  EXPECT_EQ(0, item->unitFactorTemperatureExponent());
  EXPECT_EQ(0, item->unitFactorCurrentExponent());
  EXPECT_EQ(0, item->unitFactorLuminousIntensityExponent());
  EXPECT_EQ(0, item->logicalMinimum());
  EXPECT_EQ(1, item->logicalMaximum());
  EXPECT_EQ(0, item->physicalMinimum());
  EXPECT_EQ(1, item->physicalMaximum());
}

TEST(HIDDeviceTest, multiUsageItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Configure the item to use 8 non-consecutive usages.
  mojo_item->usages.clear();
  for (int i = 1; i < 9; ++i) {
    mojo_item->usages.push_back(device::mojom::blink::HidUsageAndPage::New(
        2 * i, device::mojom::blink::kPageButton));
  }
  mojo_item->report_size = 1;  // 1 bit.
  mojo_item->report_count = 8;
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_EQ(8U, item->usages().size());
  EXPECT_EQ(0x00090002U, item->usages()[0]);
  EXPECT_EQ(0x00090004U, item->usages()[1]);
  EXPECT_EQ(0x00090006U, item->usages()[2]);
  EXPECT_EQ(0x00090008U, item->usages()[3]);
  EXPECT_EQ(0x0009000aU, item->usages()[4]);
  EXPECT_EQ(0x0009000cU, item->usages()[5]);
  EXPECT_EQ(0x0009000eU, item->usages()[6]);
  EXPECT_EQ(0x00090010U, item->usages()[7]);
  EXPECT_EQ(1U, item->reportSize());
  EXPECT_EQ(8U, item->reportCount());
}

TEST(HIDDeviceTest, usageRangeItem) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Configure the item to use a usage range. The item defines eight fields,
  // each 1-bit wide, with consecutive usages from the Button usage page.
  mojo_item->is_range = true;
  mojo_item->usages.clear();
  mojo_item->usage_minimum->usage_page = device::mojom::blink::kPageButton;
  mojo_item->usage_minimum->usage = 0x01;  // 1st button usage (primary).
  mojo_item->usage_maximum->usage_page = device::mojom::blink::kPageButton;
  mojo_item->usage_maximum->usage = 0x08;  // 8th button usage.
  mojo_item->report_size = 1;              // 1 bit.
  mojo_item->report_count = 8;
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_FALSE(item->hasStrings());
  EXPECT_FALSE(item->hasUsages());
  EXPECT_EQ(0x00090001U, item->usageMinimum());
  EXPECT_EQ(0x00090008U, item->usageMaximum());
  EXPECT_EQ(1U, item->reportSize());
  EXPECT_EQ(8U, item->reportCount());
}

TEST(HIDDeviceTest, unitDefinition) {
  device::mojom::blink::HidReportItemPtr mojo_item = MakeReportItem();

  // Add a unit definition and check that the unit properties are correctly
  // converted.
  mojo_item->unit_exponent = 0x0C;  // 10^-4
  mojo_item->unit = 0x0000E111;     // g*cm/s^2
  HIDReportItem* item = HIDDevice::ToHIDReportItem(*mojo_item);

  EXPECT_EQ(V8HIDUnitSystem::Enum::kSiLinear, item->unitSystem());
  EXPECT_EQ(-4, item->unitExponent());
  EXPECT_EQ(1, item->unitFactorLengthExponent());
  EXPECT_EQ(1, item->unitFactorMassExponent());
  EXPECT_EQ(-2, item->unitFactorTimeExponent());
  EXPECT_EQ(0, item->unitFactorTemperatureExponent());
  EXPECT_EQ(0, item->unitFactorCurrentExponent());
  EXPECT_EQ(0, item->unitFactorLuminousIntensityExponent());
}

class HIDTestHelper {
 public:
  static HIDDevice* GetOrCreateDevice(
      HID* hid,
      ScriptState* script_state,
      const device::mojom::blink::HidDeviceInfoPtr& info) {
    return hid->GetOrCreateDevice(script_state, info);
  }
  static size_t CacheSize(HID* hid) { return hid->device_caches_.size(); }
  static size_t CacheSizeForWorld(HID* hid, DOMWrapperWorld& world) {
    auto it = hid->device_caches_.find(&world);
    if (it != hid->device_caches_.end()) {
      return it->value->DeviceCache().size();
    }
    return 0;
  }
  static size_t DefaultCacheSize(HID* hid) { return hid->device_cache_.size(); }
};

// Verifies that when WebHIDWorldIsolatedCache is enabled, HIDDevice
// instances are cached per-world. Requesting the same device GUID in different
// worlds (main vs. isolated) must return different C++ objects.
TEST(HIDTest, WorldIsolatedCache) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Enable feature
  ScopedWebHIDWorldIsolatedCacheForTest feature_helper(true);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  HID* hid = HID::hid(*navigator);
  ASSERT_TRUE(hid);

  // Create main world script state
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  EXPECT_TRUE(main_world.IsMainWorld());

  // Create isolated world
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  EXPECT_TRUE(isolated_world->IsIsolatedWorld());

  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  // Create device info representing a physical device
  auto info = device::mojom::blink::HidDeviceInfo::New();
  info->guid = "test-guid";
  info->physical_device_id = "phys-id";

  // GetOrCreateDevice in main world
  HIDDevice* device_main =
      HIDTestHelper::GetOrCreateDevice(hid, main_script_state, info);
  ASSERT_TRUE(device_main);

  // GetOrCreateDevice in isolated world with the same info
  HIDDevice* device_isolated =
      HIDTestHelper::GetOrCreateDevice(hid, isolated_script_state, info);
  ASSERT_TRUE(device_isolated);

  // They should be different C++ objects
  EXPECT_NE(device_main, device_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(HIDTestHelper::CacheSize(hid), 2U);
  EXPECT_EQ(HIDTestHelper::CacheSizeForWorld(hid, main_world), 1U);
  EXPECT_EQ(HIDTestHelper::CacheSizeForWorld(hid, *isolated_world), 1U);
  EXPECT_EQ(HIDTestHelper::DefaultCacheSize(hid), 0U);
}

// Verifies that when WebHIDWorldIsolatedCache is disabled, the cache
// falls back to the legacy behavior where requesting the same device GUID in
// different worlds returns the exact same C++ object.
TEST(HIDTest, WorldIsolatedCacheDisabled) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* isolate = scope.GetIsolate();

  // Disable feature
  ScopedWebHIDWorldIsolatedCacheForTest feature_helper(false);

  Navigator* navigator = scope.GetFrame().DomWindow()->navigator();
  HID* hid = HID::hid(*navigator);
  ASSERT_TRUE(hid);

  // Create main world script state
  ScriptState* main_script_state = scope.GetScriptState();
  DOMWrapperWorld& main_world = main_script_state->World();
  EXPECT_TRUE(main_world.IsMainWorld());

  // Create isolated world
  DOMWrapperWorld* isolated_world =
      DOMWrapperWorld::EnsureIsolatedWorld(isolate, 1);
  EXPECT_TRUE(isolated_world->IsIsolatedWorld());

  scope.GetFrame().GetWindowProxy(*isolated_world);  // Force initialization
  ScriptState* isolated_script_state =
      ToScriptState(&scope.GetFrame(), *isolated_world);
  ASSERT_TRUE(isolated_script_state);

  // Create device info representing a physical device
  auto info = device::mojom::blink::HidDeviceInfo::New();
  info->guid = "test-guid";
  info->physical_device_id = "phys-id";

  // GetOrCreateDevice in main world
  HIDDevice* device_main =
      HIDTestHelper::GetOrCreateDevice(hid, main_script_state, info);
  ASSERT_TRUE(device_main);

  // GetOrCreateDevice in isolated world with the same info
  HIDDevice* device_isolated =
      HIDTestHelper::GetOrCreateDevice(hid, isolated_script_state, info);
  ASSERT_TRUE(device_isolated);

  // They should be the same C++ object
  EXPECT_EQ(device_main, device_isolated);

  // Cache sizes should be updated
  EXPECT_EQ(HIDTestHelper::CacheSize(hid), 0U);
  EXPECT_EQ(HIDTestHelper::DefaultCacheSize(hid), 1U);
}

}  // namespace blink
