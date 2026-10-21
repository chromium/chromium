// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/registry_util.h"

#include <windows.h>

#include <aclapi.h>

#include <array>
#include <optional>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_localalloc.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "remoting/base/registry_test_helper_win.h"
#include "remoting/base/version.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/native_messaging_entries.h"
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/security_descriptor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting::installer {

namespace {

inline constexpr wchar_t kTestInstallDir[] =
    L"C:\\Program Files\\Google\\Chrome Remote Desktop";
inline constexpr wchar_t kEventLogKeyPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application\\chromoting";
inline constexpr wchar_t kSecurityKeyPath[] = L"SOFTWARE\\Google\\SecurityKey";
inline constexpr wchar_t kRegisteredApplicationsKeyPath[] =
    L"SOFTWARE\\RegisteredApplications";

}  // namespace

class RegistryUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  const base::FilePath install_dir_{kTestInstallDir};
  registry_util::RegistryOverrideManager registry_override_manager_;
};

TEST_F(RegistryUtilTest, RegisterNativeMessagingHosts) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    std::wstring expected_manifest =
        install_dir_.Append(entry.manifest_file_name).value();
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(key_path, nullptr, view),
                expected_manifest)
          << "Mismatch for key: " << key_path << " (view=" << view << ")";
    }
  }
}

TEST_F(RegistryUtilTest, UnregisterNativeMessagingHosts) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      EXPECT_FALSE(RegistryTestHelperWin::KeyExists(key_path, view))
          << "Key was not unregistered: " << key_path << " (view=" << view
          << ")";
    }
  }
}

TEST_F(RegistryUtilTest, RegisterEventLogSource) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  std::wstring expected_core_dll = install_dir_.Append(kCoreBinaryName).value();
  constexpr DWORD kExpectedTypes =
      EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

  EXPECT_EQ(
      RegistryTestHelperWin::ReadDwordValue(kEventLogKeyPath, L"CategoryCount"),
      1u);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kEventLogKeyPath,
                                                   L"CategoryMessageFile"),
            expected_core_dll);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kEventLogKeyPath,
                                                   L"EventMessageFile"),
            expected_core_dll);
  EXPECT_EQ(RegistryTestHelperWin::ReadDwordValue(kEventLogKeyPath,
                                                  L"TypesSupported"),
            kExpectedTypes);
}

TEST_F(RegistryUtilTest, UnregisterEventLogSource) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(kEventLogKeyPath));
}

TEST_F(RegistryUtilTest, RegisterSecurityKey) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kSecurityKeyPath,
                                                   L"RemoteSecurityKeyPath"),
            install_dir_.Append(kRemoteSecurityKeyBinaryName).value());
}

TEST_F(RegistryUtilTest, UnregisterSecurityKeyWhenEmpty) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(kSecurityKeyPath));
}

TEST_F(RegistryUtilTest, UnregisterPreservesNonEmptySecurityKey) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));
  {
    base::win::RegKey security_key(HKEY_LOCAL_MACHINE, kSecurityKeyPath,
                                   KEY_SET_VALUE);
    ASSERT_TRUE(security_key.Valid());
    ASSERT_EQ(security_key.WriteValue(L"OtherValue", L"keep_me"),
              ERROR_SUCCESS);
  }

  UnregisterHostRegistryEntries();

  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(kSecurityKeyPath));
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kSecurityKeyPath,
                                                   L"RemoteSecurityKeyPath"),
            std::nullopt);
  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(kSecurityKeyPath, L"OtherValue"),
      L"keep_me");
}

TEST_F(RegistryUtilTest, UnregisterPreservesSecurityKeyWithSubkey) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));
  std::wstring subkey_path =
      base::StrCat({kSecurityKeyPath, L"\\CustomSubKey"});
  {
    base::win::RegKey subkey;
    ASSERT_EQ(subkey.Create(HKEY_LOCAL_MACHINE, subkey_path.c_str(), KEY_WRITE),
              ERROR_SUCCESS);
  }

  UnregisterHostRegistryEntries();

  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(kSecurityKeyPath));
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(kSecurityKeyPath,
                                                   L"RemoteSecurityKeyPath"),
            std::nullopt);
  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(subkey_path));
}

TEST_F(RegistryUtilTest, RegisterUrlForwarder) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  std::wstring core_dll_path = install_dir_.Append(kCoreBinaryName).value();
  std::wstring open_url_exe_path =
      install_dir_.Append(kRemoteOpenUrlBinaryName).value();
  std::wstring expected_name_res =
      base::StrCat({L"@", core_dll_path, L",-",
                    base::NumberToWString(IDS_URL_FORWARDER_NAME)});
  std::wstring expected_desc_res =
      base::StrCat({L"@", core_dll_path, L",-",
                    base::NumberToWString(IDS_URL_FORWARDER_DESCRIPTION)});
  std::wstring expected_icon_res = base::StrCat(
      {core_dll_path, L",-", base::NumberToWString(IDI_CHROME_REMOTE_DESKTOP)});
  std::wstring expected_open_cmd =
      base::StrCat({L"\"", open_url_exe_path, L"\" \"%1\""});

  std::wstring prog_id_base =
      base::StrCat({L"SOFTWARE\\Classes\\", kUrlForwarderProgId});
  std::wstring prog_id_app = base::StrCat({prog_id_base, L"\\Application"});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(prog_id_app,
                                                   L"ApplicationCompany"),
            kManufacturer);
  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(prog_id_app, L"ApplicationName"),
      expected_name_res);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(prog_id_app,
                                                   L"ApplicationDescription"),
            expected_desc_res);
  EXPECT_EQ(
      RegistryTestHelperWin::ReadStringValue(prog_id_app, L"ApplicationIcon"),
      expected_icon_res);

  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({prog_id_base, L"\\DefaultIcon"})),
            expected_icon_res);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({prog_id_base, L"\\shell\\open\\command"})),
            expected_open_cmd);

  std::wstring capabilities_path = base::StrCat(
      {L"SOFTWARE\\", kChromotingKeyPath, L"\\UrlForwarder\\Capabilities"});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(capabilities_path,
                                                   L"ApplicationDescription"),
            expected_desc_res);

  std::wstring assoc_path =
      base::StrCat({capabilities_path, L"\\URLAssociations"});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(assoc_path, L"http"),
            kUrlForwarderProgId);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(assoc_path, L"https"),
            kUrlForwarderProgId);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(assoc_path, L"mailto"),
            kUrlForwarderProgId);

  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                kRegisteredApplicationsKeyPath, kUrlForwarderName),
            capabilities_path);
}

TEST_F(RegistryUtilTest, UnregisterUrlForwarder) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));
  {
    base::win::RegKey reg_apps_key(
        HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyPath, KEY_SET_VALUE);
    ASSERT_TRUE(reg_apps_key.Valid());
    ASSERT_EQ(
        reg_apps_key.WriteValue(L"OtherApp", L"SOFTWARE\\Other\\Capabilities"),
        ERROR_SUCCESS);
  }

  UnregisterHostRegistryEntries();

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({L"SOFTWARE\\Classes\\", kUrlForwarderProgId})));
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(base::StrCat(
      {L"SOFTWARE\\", kChromotingKeyPath, L"\\UrlForwarder\\Capabilities"})));
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                kRegisteredApplicationsKeyPath, kUrlForwarderName),
            std::nullopt);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                kRegisteredApplicationsKeyPath, L"OtherApp"),
            L"SOFTWARE\\Other\\Capabilities");
}

TEST_F(RegistryUtilTest, RegisterComServer) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  std::wstring core_dll_path = install_dir_.Append(kCoreBinaryName).value();
  std::wstring host_exe_path = install_dir_.Append(kHostBinaryName).value();
  std::wstring expected_local_server_cmd =
      base::StrCat({L"\"", host_exe_path, L"\" --type=rdp_desktop_session"});

  // Verify AppId and binary security descriptors.
  std::wstring app_id_path =
      base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(app_id_path),
            L"ChromotingRdpDesktopSession");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(app_id_path, L"RunAs"),
            L"NT AUTHORITY\\LocalService");

  ScopedSd expected_sd = ConvertSddlToSd(kRdpDesktopSessionComPermissionsSddl);
  ASSERT_TRUE(expected_sd);
  std::optional<std::wstring> expected_sddl =
      RegistryTestHelperWin::SdToSddl(expected_sd.get());
  ASSERT_TRUE(expected_sddl.has_value());
  EXPECT_EQ(RegistryTestHelperWin::ReadBinarySdAsSddl(app_id_path,
                                                      L"AccessPermission"),
            expected_sddl);
  EXPECT_EQ(RegistryTestHelperWin::ReadBinarySdAsSddl(app_id_path,
                                                      L"LaunchPermission"),
            expected_sddl);

  // Verify RdpDesktopSession CLSID.
  std::wstring rdp_clsid = base::win::WStringFromGUID(CLSID_RdpDesktopSession);
  std::wstring clsid_base =
      base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(clsid_base),
            L"RdpDesktopSession Class");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(clsid_base, L"AppID"),
            kRdpAppId);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({clsid_base, L"\\LocalServer32"})),
            expected_local_server_cmd);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({clsid_base, L"\\TypeLib"})),
            kChromotingTypeLibId);

  // Verify PSFactory CLSIDs.
  for (const auto& [ps_clsid, ps_name] :
       std::to_array<std::pair<const wchar_t*, const wchar_t*>>({
           {kIRdpDesktopSessionId, L"IRdpDesktopSession PSFactory"},
           {kIRdpDesktopSessionEventHandlerId,
            L"IRdpDesktopSessionEventHandler PSFactory"},
       })) {
    std::wstring ps_path =
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", ps_clsid});
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(ps_path), ps_name);
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                  base::StrCat({ps_path, L"\\InprocServer32"})),
              core_dll_path);
  }

  // Verify Interfaces.
  for (const wchar_t* iid : {kIDaemonControlId, kIDaemonControl2Id}) {
    std::wstring iface_path =
        base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid});
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                  base::StrCat({iface_path, L"\\ProxyStubClsid32"})),
              kUniversalMarshalerClsid);
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                  base::StrCat({iface_path, L"\\TypeLib"})),
              kChromotingTypeLibId);
  }

  std::wstring rdp_iface_path =
      base::StrCat({L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionId});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(rdp_iface_path),
            L"IRdpDesktopSession");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({rdp_iface_path, L"\\ProxyStubClsid32"})),
            kIRdpDesktopSessionId);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({rdp_iface_path, L"\\TypeLib"})),
            kChromotingTypeLibId);

  std::wstring handler_iface_path = base::StrCat(
      {L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionEventHandlerId});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(handler_iface_path),
            L"IRdpDesktopSessionEventHandler");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({handler_iface_path, L"\\ProxyStubClsid32"})),
            kIRdpDesktopSessionEventHandlerId);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({handler_iface_path, L"\\TypeLib"})),
            kChromotingTypeLibId);

  // Verify TypeLib.
  std::wstring typelib_v1 = base::StrCat(
      {L"SOFTWARE\\Classes\\TypeLib\\", kChromotingTypeLibId, L"\\1.0"});
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(typelib_v1),
            L"Chromoting 1.0 Type Library");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({typelib_v1, L"\\0\\win32"})),
            core_dll_path);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({typelib_v1, L"\\0\\win64"})),
            core_dll_path);
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({typelib_v1, L"\\FLAGS"})),
            L"0");
  EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(
                base::StrCat({typelib_v1, L"\\HELPDIR"})),
            install_dir_.value());
}

TEST_F(RegistryUtilTest, UnregisterComServer) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId})));
  std::wstring rdp_clsid = base::win::WStringFromGUID(CLSID_RdpDesktopSession);
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid})));
  for (const wchar_t* clsid :
       {kIRdpDesktopSessionId, kIRdpDesktopSessionEventHandlerId}) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", clsid})));
  }
  for (const wchar_t* iid :
       {kIDaemonControlId, kIDaemonControl2Id, kIRdpDesktopSessionId,
        kIRdpDesktopSessionEventHandlerId}) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
        base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid})));
  }
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({L"SOFTWARE\\Classes\\TypeLib\\", kChromotingTypeLibId})));
}

TEST_F(RegistryUtilTest, RegisterHostConfigAndLoggingKeys) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  std::wstring base_key = base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  EXPECT_TRUE(RegistryTestHelperWin::KeyExists(
      base::StrCat({base_key, L"\\paired-clients\\clients"})));
  EXPECT_TRUE(
      RegistryTestHelperWin::KeyExists(base::StrCat({base_key, L"\\logging"})));

  std::wstring secrets_path =
      base::StrCat({base_key, L"\\paired-clients\\secrets"});
  base::win::RegKey secrets_key(HKEY_LOCAL_MACHINE, secrets_path.c_str(),
                                KEY_READ | READ_CONTROL);
  ASSERT_TRUE(secrets_key.Valid());

  PSECURITY_DESCRIPTOR raw_sd = nullptr;
  PACL dacl = nullptr;
  ASSERT_EQ(::GetSecurityInfo(secrets_key.Handle(), SE_REGISTRY_KEY,
                              DACL_SECURITY_INFORMATION,
                              /*ppsidOwner=*/nullptr, /*ppsidGroup=*/nullptr,
                              &dacl, /*ppSacl=*/nullptr, &raw_sd),
            static_cast<DWORD>(ERROR_SUCCESS));
  base::win::ScopedLocalAlloc scoped_sd = base::win::TakeLocalAlloc(raw_sd);
  ASSERT_NE(dacl, nullptr);

  SECURITY_DESCRIPTOR_CONTROL control = 0;
  DWORD revision = 0;
  ASSERT_TRUE(
      ::GetSecurityDescriptorControl(scoped_sd.get(), &control, &revision));
  EXPECT_NE(control & SE_DACL_PROTECTED, 0);
}

TEST_F(RegistryUtilTest, UnregisterHostConfigAndLoggingKeys) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  std::wstring base_key = base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({base_key, L"\\paired-clients\\secrets"})));
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({base_key, L"\\paired-clients\\clients"})));
  EXPECT_FALSE(
      RegistryTestHelperWin::KeyExists(base::StrCat({base_key, L"\\logging"})));
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(base_key));
}

TEST_F(RegistryUtilTest, RegisterOmahaClient) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  std::wstring omaha_path =
      base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId});
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::wstring expected_version = base::UTF8ToWide(REMOTING_VERSION_STRING);
  for (REGSAM view : kBothRegistryViews) {
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(omaha_path, L"pv", view),
              expected_version);
    EXPECT_EQ(RegistryTestHelperWin::ReadStringValue(omaha_path, L"name", view),
              kProductName);
  }
#else
  for (REGSAM view : kBothRegistryViews) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(omaha_path, view));
  }
#endif
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(RegistryUtilTest, UnregisterOmahaClient) {
  ASSERT_TRUE(RegisterHostRegistryEntries(install_dir_));

  UnregisterHostRegistryEntries();

  std::wstring omaha_path =
      base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId});
  for (REGSAM view : kBothRegistryViews) {
    EXPECT_FALSE(RegistryTestHelperWin::KeyExists(omaha_path, view));
  }
}
#endif

TEST_F(RegistryUtilTest, UnregisterWhenNotRegisteredIsNoOp) {
  UnregisterHostRegistryEntries();

  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(
      base::StrCat({L"SOFTWARE\\", kChromotingKeyPath})));
  EXPECT_FALSE(RegistryTestHelperWin::KeyExists(kEventLogKeyPath));
}

}  // namespace remoting::installer
