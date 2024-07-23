// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/shill/shill_device_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "components/account_id/account_id.h"
#include "components/onc/onc_constants.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/api_unittest.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace extensions {

namespace {

const char kUserHash[] = "test_user_hash";
const char kUserProfilePath[] = "/network_profile/user/shill";

const char kWifiDevicePath[] = "/device/stub_wifi_device";
const char kCellularDevicePath[] = "/device/stub_cellular_device";

const char kSharedWifiServicePath[] = "/service/shared_wifi";
const char kSharedWifiGuid[] = "shared_wifi_guid";
const char kSharedWifiName[] = "shared_wifi";

const char kPrivateWifiServicePath[] = "/service/private_wifi";
const char kPrivateWifiGuid[] = "private_wifi_guid";
const char kPrivateWifiName[] = "private_wifi";

const char kManagedUserWifiGuid[] = "managed_user_wifi_guid";
const char kManagedUserWifiSsid[] = "managed_user_wifi";

const char kManagedDeviceWifiGuid[] = "managed_device_wifi_guid";
const char kManagedDeviceWifiSsid[] = "managed_device_wifi";

const char kCellularServicePath[] = "/service/cellular";
const char kCellularGuid[] = "cellular_guid";
const char kCellularName[] = "cellular";

}  // namespace

class NetworkingPrivateApiTest : public ApiUnitTest {
 public:
  NetworkingPrivateApiTest() = default;
  ~NetworkingPrivateApiTest() override = default;

  NetworkingPrivateApiTest(const NetworkingPrivateApiTest&) = delete;
  NetworkingPrivateApiTest& operator=(const NetworkingPrivateApiTest&) = delete;

  void SetUp() override {
    ApiUnitTest::SetUp();

    // TODO(b/278643115) Remove LoginState dependency.
    ash::LoginState::Initialize();

    const AccountId account_id = AccountId::FromUserEmail("test@test");
    auto fake_user_manager = std::make_unique<user_manager::FakeUserManager>();
    fake_user_manager->AddUser(account_id);
    fake_user_manager->UserLoggedIn(account_id, kUserHash,
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        std::move(fake_user_manager));

    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_ACTIVE,
        ash::LoginState::LOGGED_IN_USER_KIOSK);
    base::RunLoop().RunUntilIdle();

    device_test()->ClearDevices();
    service_test()->ClearServices();

    SetUpNetworks();
    SetUpNetworkPolicy();

    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    scoped_user_manager_.reset();

    ash::LoginState::Shutdown();

    ApiUnitTest::TearDown();
  }

  void SetUpNetworks() {
    profile_test()->AddProfile(kUserProfilePath, kUserHash);
    profile_test()->AddProfile(ash::ShillProfileClient::GetSharedProfilePath(),
                               "");

    device_test()->AddDevice(kWifiDevicePath, shill::kTypeWifi, "wifi_device1");

    service_test()->AddService(kSharedWifiServicePath, kSharedWifiGuid,
                               kSharedWifiName, shill::kTypeWifi,
                               shill::kStateOnline, true /* visible */);
    service_test()->SetServiceProperty(kSharedWifiServicePath,
                                       shill::kDeviceProperty,
                                       base::Value(kWifiDevicePath));
    service_test()->SetServiceProperty(kSharedWifiServicePath,
                                       shill::kSecurityClassProperty,
                                       base::Value(shill::kSecurityClassPsk));
    service_test()->SetServiceProperty(
        kSharedWifiServicePath, shill::kPriorityProperty, base::Value(2));
    service_test()->SetServiceProperty(
        kSharedWifiServicePath, shill::kProfileProperty,
        base::Value(ash::ShillProfileClient::GetSharedProfilePath()));
    profile_test()->AddService(ash::ShillProfileClient::GetSharedProfilePath(),
                               kSharedWifiServicePath);

    service_test()->AddService(kPrivateWifiServicePath, kPrivateWifiGuid,
                               kPrivateWifiName, shill::kTypeWifi,
                               shill::kStateOnline, true /* visible */);
    service_test()->SetServiceProperty(kPrivateWifiServicePath,
                                       shill::kDeviceProperty,
                                       base::Value(kWifiDevicePath));
    service_test()->SetServiceProperty(kPrivateWifiServicePath,
                                       shill::kSecurityClassProperty,
                                       base::Value(shill::kSecurityClassPsk));
    service_test()->SetServiceProperty(
        kPrivateWifiServicePath, shill::kPriorityProperty, base::Value(2));

    service_test()->SetServiceProperty(kPrivateWifiServicePath,
                                       shill::kProfileProperty,
                                       base::Value(kUserProfilePath));
    profile_test()->AddService(kUserProfilePath, kPrivateWifiServicePath);
  }

  void SetUpNetworkPolicy() {
    ash::ManagedNetworkConfigurationHandler* config_handler =
        ash::NetworkHandler::Get()->managed_network_configuration_handler();

    const std::string user_policy_ssid = kManagedUserWifiSsid;
    base::Value::List user_policy_onc = base::Value::List().Append(
        base::Value::Dict()
            .Set("GUID", kManagedUserWifiGuid)
            .Set("Type", "WiFi")
            .Set("WiFi", base::Value::Dict()
                             .Set("Passphrase", "fake")
                             .Set("SSID", user_policy_ssid)
                             .Set("HexSSID", base::HexEncode(user_policy_ssid))
                             .Set("Security", "WPA-PSK")));

    config_handler->SetPolicy(::onc::ONC_SOURCE_USER_POLICY, kUserHash,
                              user_policy_onc,
                              /*global_network_config=*/base::Value::Dict());

    const std::string device_policy_ssid = kManagedDeviceWifiSsid;
    base::Value::List device_policy_onc = base::Value::List().Append(
        base::Value::Dict()
            .Set("GUID", kManagedDeviceWifiGuid)
            .Set("Type", "WiFi")
            .Set("WiFi",
                 base::Value::Dict()
                     .Set("SSID", device_policy_ssid)
                     .Set("HexSSID", base::HexEncode(device_policy_ssid))
                     .Set("Security", "None")));
    config_handler->SetPolicy(::onc::ONC_SOURCE_DEVICE_POLICY, "",
                              device_policy_onc,
                              /*global_network_config=*/base::Value::Dict());
  }

  void SetDeviceProperty(const std::string& device_path,
                         const std::string& name,
                         const base::Value& value) {
    device_test()->SetDeviceProperty(device_path, name, value,
                                     /*notify_changed=*/false);
  }

  void SetUpCellular() {
    // Add a Cellular GSM Device.
    device_test()->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                             "stub_cellular_device1");

    base::Value::Dict home_provider;
    home_provider.Set("name", "Cellular1_Provider");
    home_provider.Set("code", "000000");
    home_provider.Set("country", "us");
    SetDeviceProperty(kCellularDevicePath, shill::kHomeProviderProperty,
                      base::Value(std::move(home_provider)));
    SetDeviceProperty(kCellularDevicePath, shill::kTechnologyFamilyProperty,
                      base::Value(shill::kNetworkTechnologyGsm));
    SetDeviceProperty(kCellularDevicePath, shill::kMeidProperty,
                      base::Value("test_meid"));
    SetDeviceProperty(kCellularDevicePath, shill::kImeiProperty,
                      base::Value("test_imei"));
    SetDeviceProperty(kCellularDevicePath, shill::kIccidProperty,
                      base::Value("test_iccid"));
    SetDeviceProperty(kCellularDevicePath, shill::kImsiProperty,
                      base::Value("test_imsi"));
    SetDeviceProperty(kCellularDevicePath, shill::kEsnProperty,
                      base::Value("test_esn"));
    SetDeviceProperty(kCellularDevicePath, shill::kMdnProperty,
                      base::Value("test_mdn"));
    SetDeviceProperty(kCellularDevicePath, shill::kMinProperty,
                      base::Value("test_min"));
    SetDeviceProperty(kCellularDevicePath, shill::kModelIdProperty,
                      base::Value("test_model_id"));
    base::Value apn(base::Value::Dict()
                        .Set(shill::kApnProperty, "test-apn")
                        .Set(shill::kApnUsernameProperty, "test-user")
                        .Set(shill::kApnPasswordProperty, "test-password")
                        .Set(shill::kApnAuthenticationProperty, "chap"));
    base::Value apn_list(base::Value::Type::LIST);
    apn_list.GetList().Append(apn.Clone());
    SetDeviceProperty(kCellularDevicePath, shill::kCellularApnListProperty,
                      apn_list);

    service_test()->AddService(kCellularServicePath, kCellularGuid,
                               kCellularName, shill::kTypeCellular,
                               shill::kStateOnline, true /* visible */);
    service_test()->SetServiceProperty(kCellularServicePath,
                                       shill::kCellularAllowRoamingProperty,
                                       base::Value(false));
    service_test()->SetServiceProperty(
        kCellularServicePath, shill::kAutoConnectProperty, base::Value(true));
    service_test()->SetServiceProperty(
        kCellularServicePath, shill::kIccidProperty, base::Value("test_iccid"));
    service_test()->SetServiceProperty(
        kCellularServicePath, shill::kImsiProperty, base::Value("test_imsi"));
    service_test()->SetServiceProperty(
        kCellularServicePath, shill::kNetworkTechnologyProperty,
        base::Value(shill::kNetworkTechnologyGsm));
    service_test()->SetServiceProperty(kCellularServicePath,
                                       shill::kRoamingStateProperty,
                                       base::Value(shill::kRoamingStateHome));
    service_test()->SetServiceProperty(kCellularServicePath,
                                       shill::kCellularApnProperty, apn);
    service_test()->SetServiceProperty(
        kCellularServicePath, shill::kCellularLastGoodApnProperty, apn);

    profile_test()->AddService(kUserProfilePath, kCellularServicePath);

    base::RunLoop().RunUntilIdle();
  }

  void AddSharedNetworkToUserProfile(const std::string& service_path) {
    service_test()->SetServiceProperty(service_path, shill::kProfileProperty,
                                       base::Value(kUserProfilePath));
    profile_test()->AddService(kUserProfilePath, service_path);

    base::RunLoop().RunUntilIdle();
  }

  int GetNetworkPriority(const ash::NetworkState* network) {
    base::Value::Dict properties;
    network->GetStateProperties(&properties);
    return properties.FindInt(shill::kPriorityProperty).value_or(-1);
  }

  bool HasServiceProfile(const std::string& service_path,
                         std::string* profile_path) {
    return profile_test()->GetService(service_path, profile_path).has_value();
  }

  std::optional<base::Value::Dict> GetNetworkProperties(
      const std::string& service_path) {
    base::RunLoop run_loop;
    std::optional<base::Value::Dict> properties;
    ash::NetworkHandler::Get()
        ->network_configuration_handler()
        ->GetShillProperties(
            service_path,
            base::BindOnce(&NetworkingPrivateApiTest::OnNetworkProperties,
                           base::Unretained(this), service_path,
                           base::Unretained(&properties),
                           run_loop.QuitClosure()));
    run_loop.Run();
    if (!properties) {
      return std::nullopt;
    }
    return std::move(*properties);
  }

  void OnNetworkProperties(const std::string& expected_path,
                           std::optional<base::Value::Dict>* result,
                           base::OnceClosure callback,
                           const std::string& service_path,
                           std::optional<base::Value::Dict> properties) {
    if (!properties) {
      ADD_FAILURE() << "Error calling shill client.";
      std::move(callback).Run();
      return;
    }
    EXPECT_EQ(expected_path, service_path);
    *result = std::move(properties);
    std::move(callback).Run();
  }

  std::optional<base::Value::Dict> GetNetworkUiData(
      std::optional<base::Value::Dict>& properties) {
    const std::string* ui_data_json = properties->FindString("UIData");
    if (!ui_data_json) {
      return std::nullopt;
    }

    JSONStringValueDeserializer deserializer(*ui_data_json);
    auto deserialized = deserializer.Deserialize(nullptr, nullptr);

    if (!deserialized || !deserialized->is_dict()) {
      return std::nullopt;
    }
    return std::move(*deserialized).TakeDict();
  }

  bool GetUserSettingStringData(const std::string& guid,
                                const std::string& key,
                                std::string* value = nullptr) {
    const ash::NetworkState* network = ash::NetworkHandler::Get()
                                           ->network_state_handler()
                                           ->GetNetworkStateFromGuid(guid);

    std::optional<base::Value::Dict> properties =
        GetNetworkProperties(network->path());
    if (!properties.has_value()) {
      return false;
    }

    std::optional<base::Value::Dict> ui_data = GetNetworkUiData(properties);
    if (!ui_data) {
      return false;
    }

    const std::string* user_setting =
        ui_data->FindStringByDottedPath("user_settings." + key);
    if (!user_setting) {
      return false;
    }

    if (value) {
      *value = *user_setting;
    }
    return true;
  }

  ash::ShillServiceClient::TestInterface* service_test() {
    return network_handler_test_helper_.service_test();
  }
  ash::ShillDeviceClient::TestInterface* device_test() {
    return network_handler_test_helper_.device_test();
  }
  ash::ShillProfileClient::TestInterface* profile_test() {
    return network_handler_test_helper_.profile_test();
  }

 private:
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  ash::NetworkHandlerTestHelper network_handler_test_helper_;
};

TEST_F(NetworkingPrivateApiTest, SetSharedNetworkProperties) {
  EXPECT_EQ(networking_private::kErrorAccessToSharedConfig,
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(
                    R"(["%s", {"WiFi": {"Passphrase": "passphrase"}}])",
                    kSharedWifiGuid)));
}

TEST_F(NetworkingPrivateApiTest, SetPrivateNetworkPropertiesWebUI) {
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  set_properties->set_source_context_type(mojom::ContextType::kWebUi);

  RunFunction(
      set_properties.get(),
      base::StringPrintf(R"(["%s", {"Priority": 0}])", kSharedWifiGuid));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kSharedWifiGuid);
  ASSERT_TRUE(network);
  EXPECT_FALSE(network->IsPrivate());
  EXPECT_EQ(0, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, SetPrivateNetworkProperties) {
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  RunFunction(
      set_properties.get(),
      base::StringPrintf(R"(["%s", {"Priority": 0}])", kPrivateWifiGuid));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kPrivateWifiGuid);
  ASSERT_TRUE(network);
  EXPECT_TRUE(network->IsPrivate());
  EXPECT_EQ(0, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, SetNetworkRestrictedProperties) {
  const char kProxySettings[] =
      R"({
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "HTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               },
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           }
         })";

  EXPECT_EQ("Error.PropertiesNotAllowed: [ProxySettings]",
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(R"(["%s", %s])", kPrivateWifiGuid,
                                   kProxySettings)));

  const char kStaticIpConfig[] =
      R"({
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8"],
             "Type": "IPv4"
           }
         })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [StaticIPConfig]",
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(R"(["%s", %s])", kPrivateWifiGuid,
                                   kStaticIpConfig)));

  const char kCombinedSettings[] =
      R"({
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           },
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8"],
             "Type": "IPv4"
           }
         })";
  // Note: The order of properties listed in the error is not really important.
  // If the API implementation changes, the expected order can be changed, too.
  EXPECT_EQ("Error.PropertiesNotAllowed: [ProxySettings, StaticIPConfig]",
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(R"(["%s", %s])", kPrivateWifiGuid,
                                   kCombinedSettings)));

  EXPECT_FALSE(
      GetUserSettingStringData(kPrivateWifiGuid, "ProxySettings.Type"));
  EXPECT_FALSE(
      GetUserSettingStringData(kPrivateWifiGuid, "StaticIPConfig.Type"));
}

TEST_F(NetworkingPrivateApiTest, SetNetworkRestrictedPropertiesFromWebUI) {
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  set_properties->set_source_context_type(mojom::ContextType::kWebUi);
  set_properties->set_source_url(GURL("chrome://os-settings/networkDetail"));

  const char kCombinedSettings[] =
      R"({
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           },
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8"],
             "Type": "IPv4"
           }
         })";
  RunFunction(
      set_properties.get(),
      base::StringPrintf(R"(["%s", %s])", kPrivateWifiGuid, kCombinedSettings));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  EXPECT_TRUE(GetUserSettingStringData(kPrivateWifiGuid, "ProxySettings.Type"));
  EXPECT_TRUE(
      GetUserSettingStringData(kPrivateWifiGuid, "StaticIPConfig.Type"));
}

TEST_F(NetworkingPrivateApiTest, CreateSharedNetwork) {
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "New network",
             "Security": "None"
           }
         })";
  EXPECT_EQ(networking_private::kErrorAccessToSharedConfig,
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[true, %s]", kNetworkConfig)));
}

TEST_F(NetworkingPrivateApiTest, CreateSharedNetworkWebUI) {
  scoped_refptr<NetworkingPrivateCreateNetworkFunction> create_network =
      new NetworkingPrivateCreateNetworkFunction();
  create_network->set_source_context_type(mojom::ContextType::kWebUi);

  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "Priority": 1,
           "WiFi": {
             "SSID": "New network",
             "Security": "None"
           }
         })";
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      create_network.get(), base::StringPrintf("[true, %s]", kNetworkConfig));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  std::string guid = result->GetString();
  const ash::NetworkState* network = ash::NetworkHandler::Get()
                                         ->network_state_handler()
                                         ->GetNetworkStateFromGuid(guid);
  ASSERT_TRUE(network);
  EXPECT_FALSE(network->IsPrivate());
  ASSERT_EQ(1, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, CreatePrivateNetwork) {
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "Priority": 1,
           "WiFi": {
             "SSID": "New WiFi",
             "Security": "WPA-PSK"
           }
         })";
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      new NetworkingPrivateCreateNetworkFunction(),
      base::StringPrintf("[false, %s]", kNetworkConfig));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  // Test the created config can be changed now.
  std::string guid = result->GetString();
  const ash::NetworkState* network = ash::NetworkHandler::Get()
                                         ->network_state_handler()
                                         ->GetNetworkStateFromGuid(guid);
  ASSERT_TRUE(network);
  EXPECT_TRUE(network->IsPrivate());
  EXPECT_EQ(1, GetNetworkPriority(network));

  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();

  RunFunction(set_properties.get(),
              base::StringPrintf(R"(["%s", {"Priority": 2}])", guid.c_str()));
  EXPECT_EQ(ExtensionFunction::SUCCEEDED, *set_properties->response_type());

  EXPECT_EQ(2, GetNetworkPriority(network));
}

TEST_F(NetworkingPrivateApiTest, CreateVpn) {
  const char kL2tpIpsecConfig[] =
      R"({
           "Type": "VPN",
           "VPN": {
             "Type": "L2TP-IPsec",
             "AutoConnect": true,
             "Host": "100.100.0.0",
             "IPsec": {
               "AuthenticationType": "PSK",
               "Group": "group",
               "IKEVersion": 1,
               "PSK": "foobar"
             },
             "L2TP": {
               "Username": "user",
               "Password": "fake_password"
              }
            }
         })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [VPN.Host, VPN.IPsec, VPN.L2TP]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kL2tpIpsecConfig)));

  const char kOpenVpnConfig[] =
      R"({
           "Type": "VPN",
           "VPN": {
             "Type": "OpenVPN",
             "AutoConnect": true,
             "Host": "100.100.0.0",
             "OpenVPN": {
               "ClientCertType": "None",
               "UserAuthenticationType": "PasswordAndOTP",
               "Username": "user",
               "Password": "fake_password",
               "OTP": "fake_otp"
             }
            }
          })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [VPN.Host, VPN.OpenVPN]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kOpenVpnConfig)));

  const char kThirdPartyVpnConfig[] =
      R"({
           "Type": "VPN",
           "VPN": {
             "Type": "ThirdPartyVPN",
             "AutoConnect": true,
             "Host": "100.100.0.0",
             "ThirdPartyVPN": {
               "ExtensionID": "fake_extension_id",
               "ProviderName": "some VPN provider"
             }
           }
         })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [VPN.Host, VPN.ThirdPartyVPN]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kThirdPartyVpnConfig)));
}

TEST_F(NetworkingPrivateApiTest, CreateL2TPVpnFromWebUi) {
  const char kL2tpIpsecConfig[] =
      R"({
           "Type": "VPN",
           "VPN": {
             "Type": "L2TP-IPsec",
             "AutoConnect": true,
             "Host": "100.100.0.0",
             "IPsec": {
               "AuthenticationType": "PSK",
               "Group": "group",
               "IKEVersion": 1,
               "PSK": "foobar"
             },
             "L2TP": {}
           }
         })";

  scoped_refptr<NetworkingPrivateCreateNetworkFunction> create_network =
      new NetworkingPrivateCreateNetworkFunction();
  create_network->set_source_context_type(mojom::ContextType::kWebUi);
  create_network->set_source_url(GURL("chrome://os-settings/networkDetail"));
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      create_network.get(),
      base::StringPrintf("[false, %s]", kL2tpIpsecConfig));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  std::string guid = result->GetString();
  const char kL2tpCredentials[] =
      R"({
           "VPN": {
             "L2TP": {
               "Username": "user",
               "Password": "fake_password"
             }
           }
         })";

  // Setting password should fail from non-webui context.
  EXPECT_EQ(
      "Error.PropertiesNotAllowed: [VPN.L2TP]",
      RunFunctionAndReturnError(
          new NetworkingPrivateSetPropertiesFunction(),
          base::StringPrintf(R"(["%s", %s])", guid.c_str(), kL2tpCredentials)));

  EXPECT_FALSE(GetUserSettingStringData(guid, "VPN.L2TP.Username"));

  // VPN properties should be settable from Web UI.
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  set_properties->set_source_context_type(mojom::ContextType::kWebUi);
  set_properties->set_source_url(GURL("chrome://os-settings/networkDetail"));
  result = RunFunctionAndReturnValue(
      set_properties.get(),
      base::StringPrintf(R"(["%s", %s])", guid.c_str(), kL2tpCredentials));

  std::string username;
  EXPECT_TRUE(GetUserSettingStringData(guid, "VPN.L2TP.Username", &username));
  EXPECT_EQ("user", username);
}

TEST_F(NetworkingPrivateApiTest, CreateOpenVpnFromWebUiAndSetProperties) {
  const char kOpenVpnConfig[] =
      R"({
           "Type": "VPN",
           "VPN": {
             "Type": "OpenVPN",
             "AutoConnect": true,
             "Host": "100.100.0.0",
             "OpenVPN": {
               "ClientCertType": "None",
               "UserAuthenticationType": "PasswordAndOTP"
             }
           }
         })";
  scoped_refptr<NetworkingPrivateCreateNetworkFunction> create_network =
      new NetworkingPrivateCreateNetworkFunction();
  create_network->set_source_context_type(mojom::ContextType::kWebUi);
  create_network->set_source_url(GURL("chrome://os-settings/networkDetail"));
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      create_network.get(), base::StringPrintf("[false, %s]", kOpenVpnConfig));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  std::string guid = result->GetString();
  const char kOpenVpnCredentials[] =
      R"({
           "VPN": {
             "OpenVPN": {
               "Username": "user",
               "Password": "fake_password",
               "OTP": "fake_otp"
             }
           }
         })";

  // Setting OpenVPN properties should fail from non-webui context.
  EXPECT_EQ("Error.PropertiesNotAllowed: [VPN.OpenVPN]",
            RunFunctionAndReturnError(
                new NetworkingPrivateSetPropertiesFunction(),
                base::StringPrintf(R"(["%s", %s])", guid.c_str(),
                                   kOpenVpnCredentials)));

  EXPECT_FALSE(GetUserSettingStringData(guid, "VPN.OpenVPN.Username"));

  // VPN properties should be settable from Web UI.
  scoped_refptr<NetworkingPrivateSetPropertiesFunction> set_properties =
      new NetworkingPrivateSetPropertiesFunction();
  set_properties->set_source_context_type(mojom::ContextType::kWebUi);
  set_properties->set_source_url(GURL("chrome://os-settings/networkDetail"));
  result = RunFunctionAndReturnValue(
      set_properties.get(),
      base::StringPrintf(R"(["%s", %s])", guid.c_str(), kOpenVpnCredentials));

  std::string username;
  EXPECT_TRUE(
      GetUserSettingStringData(guid, "VPN.OpenVPN.Username", &username));
  EXPECT_EQ("user", username);
}

TEST_F(NetworkingPrivateApiTest, CreateNetworkWithRestrictedProperties) {
  const char kConfigWithProxySettings[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "new wifi",
             "Security": "None"
           },
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "HTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               },
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           }
         })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [ProxySettings]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kConfigWithProxySettings)));

  const char kConfigWithStaticIpConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "new wifi",
             "Security": "None"
           },
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8", "8.8.8.8.9"],
             "Type": "IPv4"
           }
         })";
  EXPECT_EQ("Error.PropertiesNotAllowed: [StaticIPConfig]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kConfigWithStaticIpConfig)));

  const char kCombinedConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "new wifi",
             "Security": "None"
           },
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           },
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8", "8.8.8.8.9"],
             "Type": "IPv4"
           }
         })";
  // Note: The order of properties listed in the error is not really important.
  // If the API implementation changes, the expected order can be changed, too.
  EXPECT_EQ("Error.PropertiesNotAllowed: [ProxySettings, StaticIPConfig]",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf("[false, %s]", kCombinedConfig)));
}

TEST_F(NetworkingPrivateApiTest,
       CreateNetworkWithRestrictedPropertiesFromWebUi) {
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "Security": "WPA-PSK",
             "Passphrase": "Fake passphrase",
             "SSID": "New network config"
           },
           "ProxySettings": {
             "Type": "Manual",
             "Manual": {
               "SecureHTTPProxy": {
                 "Host": "111.111.0.0",
                 "Port": 80
               }
             }
           },
           "StaticIPConfig": {
             "Gateway": "111.111.0.0",
             "IPAddress": "123.123.123.1",
             "NameServers": ["8.8.8.8", "8.8.8.8.9"],
             "Type": "IPv4"
           }
         })";

  scoped_refptr<NetworkingPrivateCreateNetworkFunction> create_network =
      new NetworkingPrivateCreateNetworkFunction();
  create_network->set_source_context_type(mojom::ContextType::kWebUi);
  create_network->set_source_url(GURL("chrome://os-settings/networkDetail"));
  std::optional<base::Value> result = RunFunctionAndReturnValue(
      create_network.get(), base::StringPrintf("[false, %s]", kNetworkConfig));
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  std::string guid = result->GetString();
  const ash::NetworkState* network = ash::NetworkHandler::Get()
                                         ->network_state_handler()
                                         ->GetNetworkStateFromGuid(guid);

  std::optional<base::Value::Dict> properties =
      GetNetworkProperties(network->path());
  ASSERT_TRUE(properties.has_value());

  std::optional<base::Value::Dict> ui_data = GetNetworkUiData(properties);
  ASSERT_TRUE(ui_data.has_value());

  EXPECT_TRUE(ui_data->FindByDottedPath("user_settings.ProxySettings"));
  EXPECT_TRUE(ui_data->FindByDottedPath("user_settings.StaticIPConfig"));
}

TEST_F(NetworkingPrivateApiTest, CreatePrivateNetwork_NonMatchingSsids) {
  const std::string ssid = "new_wifi_config";
  const std::string hex_ssid = base::HexEncode(ssid);
  const char kNetworkConfig[] =
      R"({
           "Priority": 1,
           "Type": "WiFi",
           "WiFi": {
             "SSID": "New WiFi",
             "HexSSID": "%s",
             "Security": "WPA-PSK"
           }
         })";

  std::optional<base::Value> result = RunFunctionAndReturnValue(
      new NetworkingPrivateCreateNetworkFunction(),
      base::StringPrintf(
          "[false, %s]",
          base::StringPrintf(kNetworkConfig, hex_ssid.c_str()).c_str()));

  ASSERT_TRUE(result);
  ASSERT_TRUE(result->is_string());

  // Test the created config can be changed now.
  const std::string guid = result->GetString();
  const ash::NetworkState* network = ash::NetworkHandler::Get()
                                         ->network_state_handler()
                                         ->GetNetworkStateFromGuid(guid);
  ASSERT_TRUE(network);
  EXPECT_TRUE(network->IsPrivate());
  EXPECT_EQ(1, GetNetworkPriority(network));
  EXPECT_EQ(hex_ssid, network->GetHexSsid());
  EXPECT_EQ(ssid, network->name());
}

TEST_F(NetworkingPrivateApiTest,
       CreateAlreadyConfiguredUserPrivateNetwork_BySsid) {
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "%s",
             "Security": "WPA-PSK"
           }
         })";
  EXPECT_EQ("NetworkAlreadyConfigured",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf(
                    "[false, %s]",
                    base::StringPrintf(kNetworkConfig, kManagedUserWifiSsid)
                        .c_str())));
}

TEST_F(NetworkingPrivateApiTest,
       CreateAlreadyConfiguredUserPrivateNetwork_ByHexSsid) {
  std::string hex_ssid =
      base::HexEncode(kManagedUserWifiSsid, sizeof(kManagedUserWifiSsid) - 1);
  const char kNetworkConfig[] =
      R"({
           "Priority": 1,
           "Type": "WiFi",
           "WiFi": {
             "HexSSID": "%s",
             "Security": "WPA-PSK"
           }
         })";
  EXPECT_EQ(
      "NetworkAlreadyConfigured",
      RunFunctionAndReturnError(
          new NetworkingPrivateCreateNetworkFunction(),
          base::StringPrintf(
              "[false, %s]",
              base::StringPrintf(kNetworkConfig, hex_ssid.c_str()).c_str())));
}

TEST_F(NetworkingPrivateApiTest,
       CreateAlreadyConfiguredUserPrivateNetwork_NonMatchingSsids) {
  std::string hex_ssid =
      base::HexEncode(kManagedUserWifiSsid, sizeof(kManagedUserWifiSsid) - 1);
  const char kNetworkConfig[] =
      R"({
           "Priority": 1,
           "Type": "WiFi",
           "WiFi": {
             "SSID": "different ssid",
             "HexSSID": "%s",
             "Security": "WPA-PSK"
           }
         })";
  // HexSSID should take presedence over SSID when mathing existing networks.
  EXPECT_EQ(
      "NetworkAlreadyConfigured",
      RunFunctionAndReturnError(
          new NetworkingPrivateCreateNetworkFunction(),
          base::StringPrintf(
              "[false, %s]",
              base::StringPrintf(kNetworkConfig, hex_ssid.c_str()).c_str())));
}

TEST_F(NetworkingPrivateApiTest,
       CreateAlreadyConfiguredUserPrivateNetwork_ByHexSSID) {
  std::string hex_ssid =
      base::HexEncode(kManagedUserWifiSsid, sizeof(kManagedUserWifiSsid) - 1);
  const char kNetworkConfig[] =
      R"({
           "Priority": 1,
           "Type": "WiFi",
           "WiFi": {
             "HexSSID": "%s",
             "Security": "WPA-PSK"
           }
         })";
  EXPECT_EQ(
      "NetworkAlreadyConfigured",
      RunFunctionAndReturnError(
          new NetworkingPrivateCreateNetworkFunction(),
          base::StringPrintf(
              "[false, %s]",
              base::StringPrintf(kNetworkConfig, hex_ssid.c_str()).c_str())));
}

TEST_F(NetworkingPrivateApiTest, CreateAlreadyConfiguredDeviceNetwork) {
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "SSID": "%s"
           }
         })";
  EXPECT_EQ("NetworkAlreadyConfigured",
            RunFunctionAndReturnError(
                new NetworkingPrivateCreateNetworkFunction(),
                base::StringPrintf(
                    "[false, %s]",
                    base::StringPrintf(kNetworkConfig, kManagedDeviceWifiSsid)
                        .c_str())));
}

TEST_F(NetworkingPrivateApiTest,
       CreateAlreadyConfiguredDeviceNetwork_ByHexSSID) {
  std::string hex_ssid = base::HexEncode(kManagedDeviceWifiSsid,
                                         sizeof(kManagedDeviceWifiSsid) - 1);
  const char kNetworkConfig[] =
      R"({
           "Type": "WiFi",
           "WiFi": {
             "HexSSID": "%s",
             "Security": "WPA-PSK"
           }
         })";
  EXPECT_EQ(
      "NetworkAlreadyConfigured",
      RunFunctionAndReturnError(
          new NetworkingPrivateCreateNetworkFunction(),
          base::StringPrintf(
              "[false, %s]",
              base::StringPrintf(kNetworkConfig, hex_ssid.c_str()).c_str())));
}

TEST_F(NetworkingPrivateApiTest, GetCellularProperties) {
  SetUpCellular();

  std::optional<base::Value> result =
      RunFunctionAndReturnValue(new NetworkingPrivateGetPropertiesFunction(),
                                base::StringPrintf(R"(["%s"])", kCellularGuid));

  ASSERT_TRUE(result);

  base::Value::Dict expected_result =
      base::Value::Dict()
          .Set("Cellular",
               base::Value::Dict()
                   .Set("AllowRoaming", false)
                   .Set("AutoConnect", true)
                   .Set("Family", "GSM")
                   .Set("HomeProvider", base::Value::Dict()
                                            .Set("Code", "000000")
                                            .Set("Country", "us")
                                            .Set("Name", "Cellular1_Provider"))
                   .Set("ModelID", "test_model_id")
                   .Set("NetworkTechnology", "GSM")
                   .Set("RoamingState", "Home")
                   .Set("Scanning", false))
          .Set("ConnectionState", "Connected")
          .Set("GUID", "cellular_guid")
          .Set("IPAddressConfigType", "DHCP")
          .Set("Metered", true)
          .Set("Name", "cellular")
          .Set("NameServersConfigType", "DHCP")
          .Set("Source", "User")
          .Set("TrafficCounterResetTime", 0.0)
          .Set("Type", "Cellular");

  EXPECT_EQ(base::Value(std::move(expected_result)), *result);
}

TEST_F(NetworkingPrivateApiTest, GetCellularPropertiesFromWebUi) {
  SetUpCellular();

  scoped_refptr<NetworkingPrivateGetPropertiesFunction> get_properties =
      new NetworkingPrivateGetPropertiesFunction();
  get_properties->set_source_context_type(mojom::ContextType::kWebUi);
  get_properties->set_source_url(GURL("chrome://os-settings/networkDetail"));

  std::optional<base::Value> result = RunFunctionAndReturnValue(
      get_properties.get(), base::StringPrintf(R"(["%s"])", kCellularGuid));

  ASSERT_TRUE(result);

  base::Value::Dict expected_apn = base::Value::Dict()
                                       .Set("AccessPointName", "test-apn")
                                       .Set("Username", "test-user")
                                       .Set("Password", "test-password")
                                       .Set("ApnTypes", base::Value::List())
                                       .Set("Authentication", "CHAP");
  base::Value::Dict expected_result =
      base::Value::Dict()
          .Set("Cellular",
               base::Value::Dict()
                   .Set("AllowRoaming", false)
                   .Set("AutoConnect", true)
                   .Set("ESN", "test_esn")
                   .Set("Family", "GSM")
                   .Set("HomeProvider", base::Value::Dict()
                                            .Set("Code", "000000")
                                            .Set("Country", "us")
                                            .Set("Name", "Cellular1_Provider"))
                   .Set("ModelID", "test_model_id")
                   .Set("ICCID", "test_iccid")
                   .Set("IMEI", "test_imei")
                   .Set("IMSI", "test_imsi")
                   .Set("MDN", "test_mdn")
                   .Set("MEID", "test_meid")
                   .Set("MIN", "test_min")
                   .Set("NetworkTechnology", "GSM")
                   .Set("RoamingState", "Home")
                   .Set("Scanning", false)
                   .Set("APNList",
                        base::Value::List().Append(expected_apn.Clone()))
                   .Set("APN", expected_apn.Clone())
                   .Set("LastGoodAPN", expected_apn.Clone()))
          .Set("ConnectionState", "Connected")
          .Set("GUID", "cellular_guid")
          .Set("IPAddressConfigType", "DHCP")
          .Set("Metered", true)
          .Set("Name", "cellular")
          .Set("NameServersConfigType", "DHCP")
          .Set("Source", "User")
          .Set("TrafficCounterResetTime", 0.0)
          .Set("Type", "Cellular");

  EXPECT_EQ(base::Value(std::move(expected_result)), *result);
}

TEST_F(NetworkingPrivateApiTest, ForgetSharedNetwork) {
  EXPECT_EQ(networking_private::kErrorAccessToSharedConfig,
            RunFunctionAndReturnError(
                new NetworkingPrivateForgetNetworkFunction(),
                base::StringPrintf(R"(["%s"])", kSharedWifiGuid)));

  base::RunLoop().RunUntilIdle();
  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(kSharedWifiServicePath, &profile_path));
  EXPECT_EQ(ash::ShillProfileClient::GetSharedProfilePath(), profile_path);
}

TEST_F(NetworkingPrivateApiTest, ForgetPrivateNetwork) {
  RunFunction(new NetworkingPrivateForgetNetworkFunction(),
              base::StringPrintf(R"(["%s"])", kPrivateWifiGuid));

  std::string profile_path;
  EXPECT_FALSE(HasServiceProfile(kPrivateWifiServicePath, &profile_path));
}

TEST_F(NetworkingPrivateApiTest, ForgetPrivateNetworkWebUI) {
  scoped_refptr<NetworkingPrivateForgetNetworkFunction> forget_network =
      new NetworkingPrivateForgetNetworkFunction();
  forget_network->set_source_context_type(mojom::ContextType::kWebUi);

  RunFunction(forget_network.get(),
              base::StringPrintf(R"(["%s"])", kPrivateWifiGuid));

  std::string profile_path;
  EXPECT_FALSE(HasServiceProfile(kPrivateWifiServicePath, &profile_path));
}

TEST_F(NetworkingPrivateApiTest, ForgetUserPolicyNetwork) {
  EXPECT_EQ(networking_private::kErrorPolicyControlled,
            RunFunctionAndReturnError(
                new NetworkingPrivateForgetNetworkFunction(),
                base::StringPrintf(R"(["%s"])", kManagedUserWifiGuid)));

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kManagedUserWifiGuid);

  base::RunLoop().RunUntilIdle();
  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(network->path(), &profile_path));
  EXPECT_EQ(kUserProfilePath, profile_path);
}

TEST_F(NetworkingPrivateApiTest, ForgetUserPolicyNetworkWebUI) {
  scoped_refptr<NetworkingPrivateForgetNetworkFunction> forget_network =
      new NetworkingPrivateForgetNetworkFunction();
  forget_network->set_source_context_type(mojom::ContextType::kWebUi);

  EXPECT_EQ(networking_private::kErrorPolicyControlled,
            RunFunctionAndReturnError(
                forget_network.get(),
                base::StringPrintf(R"(["%s"])", kManagedUserWifiGuid)));

  const ash::NetworkState* network =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kManagedUserWifiGuid);

  base::RunLoop().RunUntilIdle();
  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(network->path(), &profile_path));
  EXPECT_EQ(kUserProfilePath, profile_path);
}

TEST_F(NetworkingPrivateApiTest, ForgetDevicePolicyNetworkWebUI) {
  const ash::NetworkState* network =
      ash::NetworkHandler::Get()
          ->network_state_handler()
          ->GetNetworkStateFromGuid(kManagedDeviceWifiGuid);
  ASSERT_TRUE(network);
  AddSharedNetworkToUserProfile(network->path());

  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(network->path(), &profile_path));
  ASSERT_EQ(kUserProfilePath, profile_path);

  scoped_refptr<NetworkingPrivateForgetNetworkFunction> forget_network =
      new NetworkingPrivateForgetNetworkFunction();
  forget_network->set_source_context_type(mojom::ContextType::kWebUi);
  RunFunction(forget_network.get(),
              base::StringPrintf(R"(["%s"])", kManagedDeviceWifiGuid));

  EXPECT_TRUE(HasServiceProfile(network->path(), &profile_path));
  EXPECT_EQ(ash::ShillProfileClient::GetSharedProfilePath(), profile_path);
}

// Tests that forgetNetwork in case there is a network config in both user and
// shared profile - only config from the user profile is expected to be removed.
TEST_F(NetworkingPrivateApiTest, ForgetNetworkInMultipleProfiles) {
  AddSharedNetworkToUserProfile(kSharedWifiServicePath);

  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(kSharedWifiServicePath, &profile_path));
  ASSERT_EQ(kUserProfilePath, profile_path);

  RunFunction(new NetworkingPrivateForgetNetworkFunction(),
              base::StringPrintf(R"(["%s"])", kSharedWifiGuid));

  EXPECT_TRUE(HasServiceProfile(kSharedWifiServicePath, &profile_path));
  EXPECT_EQ(ash::ShillProfileClient::GetSharedProfilePath(), profile_path);
}

TEST_F(NetworkingPrivateApiTest, ForgetNetworkInMultipleProfilesWebUI) {
  AddSharedNetworkToUserProfile(kSharedWifiServicePath);

  std::string profile_path;
  EXPECT_TRUE(HasServiceProfile(kSharedWifiServicePath, &profile_path));
  ASSERT_EQ(kUserProfilePath, profile_path);

  scoped_refptr<NetworkingPrivateForgetNetworkFunction> forget_network =
      new NetworkingPrivateForgetNetworkFunction();
  forget_network->set_source_context_type(mojom::ContextType::kWebUi);

  RunFunction(forget_network.get(),
              base::StringPrintf(R"(["%s"])", kSharedWifiGuid));

  EXPECT_FALSE(HasServiceProfile(kSharedWifiServicePath, &profile_path));
}

}  // namespace extensions
