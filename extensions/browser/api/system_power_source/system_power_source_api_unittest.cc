// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/bind.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "extensions/browser/api/system_power_source/system_power_source_api.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/common/api/system_power_source.h"
#include "extensions/common/extension_builder.h"

using extensions::api::system_power_source::PowerSourceInfo;
using extensions::api_test_utils::RunFunctionAndReturnSingleResult;
namespace power_source_api = extensions::api::system_power_source;

namespace extensions {

namespace {

class SystemPowerSourceApiUnitTest : public ApiUnitTest {
 public:
  SystemPowerSourceApiUnitTest() = default;
  ~SystemPowerSourceApiUnitTest() override = default;

  void SetUp() override {
    ApiUnitTest::SetUp();
    chromeos::PowerManagerClient::InitializeFake();
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
    ApiUnitTest::TearDown();
  }

  std::unique_ptr<base::Value> RunGetPowerSourceInfoFunction() {
    scoped_refptr<SystemPowerSourceGetPowerSourceInfoFunction>
        get_power_source_info_function(
            new SystemPowerSourceGetPowerSourceInfoFunction());

    get_power_source_info_function->set_extension(extension());
    get_power_source_info_function->set_has_callback(true);

    return RunFunctionAndReturnSingleResult(
        get_power_source_info_function.get(), "[]", browser_context());
  }

  std::unique_ptr<base::Value> RunRequestStatusUpdateFunction() {
    scoped_refptr<SystemPowerSourceRequestStatusUpdateFunction>
        request_status_update_function(
            new SystemPowerSourceRequestStatusUpdateFunction());

    request_status_update_function->set_extension(extension());

    return RunFunctionAndReturnSingleResult(
        request_status_update_function.get(), "[]", browser_context());
  }

  chromeos::FakePowerManagerClient* power_manager_client() {
    return chromeos::FakePowerManagerClient::Get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemPowerSourceApiUnitTest);
};

class TestEventRouter : public EventRouter {
 public:
  explicit TestEventRouter(content::BrowserContext* context)
      : EventRouter(context, nullptr) {}
  ~TestEventRouter() override = default;

  void BroadcastEvent(std::unique_ptr<Event> event) override {
    if (event->event_name !=
        api::system_power_source::OnPowerChanged::kEventName) {
      return;
    }
    ASSERT_TRUE(event->event_args);
    ASSERT_EQ(1u, event->event_args->GetList().size());
    power_source_info_.emplace_back(event->event_args->GetList()[0].Clone());
  }

  const std::vector<base::Value>& power_source_info() const {
    return power_source_info_;
  }

  void clear_power_source_info() { power_source_info_.clear(); }

 private:
  std::vector<base::Value> power_source_info_;

  DISALLOW_COPY_AND_ASSIGN(TestEventRouter);
};

std::unique_ptr<KeyedService> TestEventRouterFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<TestEventRouter>(context);
}

power_manager::PowerSupplyProperties MakePowerSupplyProperties(
    const base::Optional<std::string>& external_power_source_id,
    const std::vector<power_manager::PowerSupplyProperties_PowerSource>&
        power_sources) {
  power_manager::PowerSupplyProperties props;
  if (external_power_source_id)
    props.set_external_power_source_id(*external_power_source_id);

  for (const auto& power_source : power_sources)
    *props.add_available_external_power_source() = power_source;

  return props;
}

power_manager::PowerSupplyProperties_PowerSource MakePowerSource(
    const std::string& id,
    power_manager::PowerSupplyProperties_PowerSource_Type type,
    double max_power) {
  power_manager::PowerSupplyProperties_PowerSource power_source;
  power_source.set_id(id);
  power_source.set_type(type);
  power_source.set_max_power(max_power);
  return power_source;
}

}  // namespace

// Barrel jack connected
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceAc) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "AC",
      {MakePowerSource(
          "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
          0)}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", true)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

// USB-C PD charger connected
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceUsb) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "CROS_USB_PD_CHARGER0",
      {{MakePowerSource(
          "CROS_USB_PD_CHARGER0",
          power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, 60)}}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 60.0)
                      .Set("active", true)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

// Barrel Jack + USB-C PD charger connected; Barrel Jack active
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceAcActiveAndUsbInactive) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "AC",
      {{MakePowerSource(
            "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
            0),
        MakePowerSource(
            "CROS_USB_PD_CHARGER0",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C,
            60)}}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", true)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 60.0)
                      .Set("active", false)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

// Barrel Jack + USB-C PD charger connected; USB-C charger active
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceAcInactiveAndUsbActive) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "CROS_USB_PD_CHARGER0",
      {{MakePowerSource(
            "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
            0),
        MakePowerSource(
            "CROS_USB_PD_CHARGER0",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C,
            60)}}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 60.0)
                      .Set("active", true)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

// Barrel Jack + USB-C PD charger connected; neither active
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceNoneActive) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      base::nullopt,
      {{MakePowerSource(
            "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
            0),
        MakePowerSource(
            "CROS_USB_PD_CHARGER0",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C,
            60)}}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 60.0)
                      .Set("active", false)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

// Connected USB chargers with fractional max power gets rounded down to
// two significant digits (for privacy reasons, to reduce the risk of
// finger-printing).  Chargers with a max power that is not a normal value
// larger than zero are reported as not having a max power value.
TEST_F(SystemPowerSourceApiUnitTest, GetPowerSourceRounding) {
  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "CROS_USB_PD_CHARGER0",
      {{MakePowerSource(
            "CROS_USB_PD_CHARGER0",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, 111.0),
        MakePowerSource(
            "CROS_USB_PD_CHARGER1",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, 11.1),
        MakePowerSource(
            "CROS_USB_PD_CHARGER2",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, 1.11),
        MakePowerSource(
            "CROS_USB_PD_CHARGER3",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, 0.111),
        MakePowerSource(
            "CROS_USB_PD_CHARGER4",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C, -1),
        MakePowerSource(
            "CROS_USB_PD_CHARGER5",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C,
            std::nan("")),
        MakePowerSource(
            "CROS_USB_PD_CHARGER6",
            power_manager::PowerSupplyProperties_PowerSource_Type_USB_C,
            std::numeric_limits<double>::infinity())}}));

  std::unique_ptr<base::Value> result = RunGetPowerSourceInfoFunction();
  ASSERT_TRUE(result);

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 110.0)
                      .Set("active", true)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 11.0)
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 1.1)
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("maxPower", 0.11)
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("active", false)
                      .Build())
          .Append(DictionaryBuilder()
                      .Set("type", "usb")
                      .Set("active", false)
                      .Build())
          .Build();

  EXPECT_EQ(*expected_result, *result);
}

TEST_F(SystemPowerSourceApiUnitTest, OnPowerChangedEvent) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  SystemPowerSourceAPI system_power_source_api(browser_context());

  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "AC",
      {MakePowerSource(
          "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
          0)}));

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", true)
                      .Build())
          .Build();

  ASSERT_EQ(1U, event_router->power_source_info().size());
  EXPECT_EQ(*expected_result, event_router->power_source_info()[0]);
}

TEST_F(SystemPowerSourceApiUnitTest, RequestStatusUpdate) {
  TestEventRouter* event_router = static_cast<TestEventRouter*>(
      EventRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          browser_context(),
          base::BindRepeating(&TestEventRouterFactoryFunction)));
  SystemPowerSourceAPI system_power_source_api(browser_context());

  power_manager_client()->UpdatePowerProperties(MakePowerSupplyProperties(
      "AC",
      {MakePowerSource(
          "AC", power_manager::PowerSupplyProperties_PowerSource_Type_MAINS,
          0)}));

  event_router->clear_power_source_info();

  RunRequestStatusUpdateFunction();
  base::RunLoop().RunUntilIdle();

  std::unique_ptr<base::Value> expected_result =
      ListBuilder()
          .Append(DictionaryBuilder()
                      .Set("type", "mains")
                      .Set("active", true)
                      .Build())
          .Build();

  ASSERT_EQ(1U, event_router->power_source_info().size());
  EXPECT_EQ(*expected_result, event_router->power_source_info()[0]);
}

}  // namespace extensions
