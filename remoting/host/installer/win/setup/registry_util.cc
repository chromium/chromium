// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/installer/win/setup/registry_util.h"

#include <windows.h>

#include <aclapi.h>

#include <array>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "build/branding_buildflags.h"
#include "remoting/base/registry_util_win.h"
#include "remoting/base/version.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/native_messaging_entries.h"
#include "remoting/host/win/chromoting_lib.h"
#include "remoting/host/win/core_resource.h"
#include "remoting/host/win/security_descriptor.h"

namespace remoting::installer {

namespace {

inline constexpr wchar_t kEventLogApplicationKeyPath[] =
    L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\Application";
inline constexpr wchar_t kSecurityKeyRegistryKeyPath[] =
    L"SOFTWARE\\Google\\SecurityKey";
inline constexpr wchar_t kRegisteredApplicationsKeyPath[] =
    L"SOFTWARE\\RegisteredApplications";

std::wstring GetRdpDesktopSessionClsidString() {
  return base::win::WStringFromGUID(CLSID_RdpDesktopSession);
}

bool RegisterNativeMessagingHosts(const base::FilePath& install_dir) {
  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    base::FilePath manifest_path = install_dir.Append(entry.manifest_file_name);
    if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE, key_path,
                                 manifest_path.value(),
                                 KEY_WOW64_32KEY | KEY_WOW64_64KEY)) {
      return false;
    }
  }
  return true;
}

bool RegisterEventLogSource(const base::FilePath& install_dir) {
  std::wstring key_path =
      base::StrCat({kEventLogApplicationKeyPath, L"\\", kEventSourceName});
  base::win::RegKey key;
  LONG result = key.Create(HKEY_LOCAL_MACHINE, key_path.c_str(), KEY_SET_VALUE);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create EventLog key " << key_path << ": "
               << result;
    return false;
  }

  std::wstring core_dll_path = install_dir.Append(kCoreBinaryName).value();
  constexpr DWORD kTypesSupported =
      EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

  if (key.WriteValue(L"CategoryCount", 1u) != ERROR_SUCCESS ||
      key.WriteValue(L"CategoryMessageFile", core_dll_path.c_str()) !=
          ERROR_SUCCESS ||
      key.WriteValue(L"EventMessageFile", core_dll_path.c_str()) !=
          ERROR_SUCCESS ||
      key.WriteValue(L"TypesSupported", kTypesSupported) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write EventLog source values to " << key_path;
    return false;
  }
  return true;
}

bool RegisterRemoteSecurityKey(const base::FilePath& install_dir) {
  base::win::RegKey key;
  LONG result = key.Create(HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath,
                           KEY_SET_VALUE);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create SecurityKey registry key: " << result;
    return false;
  }
  std::wstring binary_path =
      install_dir.Append(kRemoteSecurityKeyBinaryName).value();
  return key.WriteValue(L"RemoteSecurityKeyPath", binary_path.c_str()) ==
         ERROR_SUCCESS;
}

bool RegisterUrlForwarder(const base::FilePath& install_dir) {
  std::wstring core_dll_path = install_dir.Append(kCoreBinaryName).value();
  std::wstring open_url_exe_path =
      install_dir.Append(kRemoteOpenUrlBinaryName).value();

  std::wstring app_name_res =
      base::StrCat({L"@", core_dll_path, L",-",
                    base::NumberToWString(IDS_URL_FORWARDER_NAME)});
  std::wstring app_desc_res =
      base::StrCat({L"@", core_dll_path, L",-",
                    base::NumberToWString(IDS_URL_FORWARDER_DESCRIPTION)});
  std::wstring app_icon_res = base::StrCat(
      {core_dll_path, L",-", base::NumberToWString(IDI_CHROME_REMOTE_DESKTOP)});
  std::wstring open_command =
      base::StrCat({L"\"", open_url_exe_path, L"\" \"%1\""});

  std::wstring prog_id_base =
      base::StrCat({L"SOFTWARE\\Classes\\", kUrlForwarderProgId});
  base::win::RegKey app_key;
  if (app_key.Create(HKEY_LOCAL_MACHINE,
                     base::StrCat({prog_id_base, L"\\Application"}).c_str(),
                     KEY_SET_VALUE) != ERROR_SUCCESS ||
      app_key.WriteValue(L"ApplicationCompany", kManufacturer) !=
          ERROR_SUCCESS ||
      app_key.WriteValue(L"ApplicationName", app_name_res.c_str()) !=
          ERROR_SUCCESS ||
      app_key.WriteValue(L"ApplicationDescription", app_desc_res.c_str()) !=
          ERROR_SUCCESS ||
      app_key.WriteValue(L"ApplicationIcon", app_icon_res.c_str()) !=
          ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to register URL forwarder ProgID Application key.";
    return false;
  }

  if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({prog_id_base, L"\\DefaultIcon"}),
                               app_icon_res) ||
      !WriteDefaultStringValue(
          HKEY_LOCAL_MACHINE,
          base::StrCat({prog_id_base, L"\\shell\\open\\command"}),
          open_command)) {
    return false;
  }

  std::wstring capabilities_path = base::StrCat(
      {L"SOFTWARE\\", kChromotingKeyPath, L"\\UrlForwarder\\Capabilities"});
  base::win::RegKey cap_key;
  if (cap_key.Create(HKEY_LOCAL_MACHINE, capabilities_path.c_str(),
                     KEY_SET_VALUE) != ERROR_SUCCESS ||
      cap_key.WriteValue(L"ApplicationDescription", app_desc_res.c_str()) !=
          ERROR_SUCCESS) {
    return false;
  }

  base::win::RegKey assoc_key;
  std::wstring assoc_path =
      base::StrCat({capabilities_path, L"\\URLAssociations"});
  if (assoc_key.Create(HKEY_LOCAL_MACHINE, assoc_path.c_str(), KEY_SET_VALUE) !=
          ERROR_SUCCESS ||
      assoc_key.WriteValue(L"http", kUrlForwarderProgId) != ERROR_SUCCESS ||
      assoc_key.WriteValue(L"https", kUrlForwarderProgId) != ERROR_SUCCESS ||
      assoc_key.WriteValue(L"mailto", kUrlForwarderProgId) != ERROR_SUCCESS) {
    return false;
  }

  base::win::RegKey reg_apps_key;
  if (reg_apps_key.Create(HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyPath,
                          KEY_SET_VALUE) != ERROR_SUCCESS ||
      reg_apps_key.WriteValue(kUrlForwarderName, capabilities_path.c_str()) !=
          ERROR_SUCCESS) {
    return false;
  }

  return true;
}

bool RegisterComServer(const base::FilePath& install_dir) {
  std::wstring rdp_clsid = GetRdpDesktopSessionClsidString();
  if (rdp_clsid.empty()) {
    LOG(ERROR) << "Failed to format CLSID_RdpDesktopSession";
    return false;
  }

  std::wstring core_dll_path = install_dir.Append(kCoreBinaryName).value();
  std::wstring host_exe_path = install_dir.Append(kHostBinaryName).value();
  std::wstring local_server_cmd =
      base::StrCat({L"\"", host_exe_path, L"\" --type=rdp_desktop_session"});

  // Register AppId `{52e6fd1a-f16e-49c0-aacb-5436a915448b}`.
  std::wstring app_id_path =
      base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId});
  base::win::RegKey app_id_key;
  if (app_id_key.Create(HKEY_LOCAL_MACHINE, app_id_path.c_str(),
                        KEY_SET_VALUE) != ERROR_SUCCESS ||
      app_id_key.WriteValue(nullptr, L"ChromotingRdpDesktopSession") !=
          ERROR_SUCCESS ||
      app_id_key.WriteValue(L"RunAs", L"NT AUTHORITY\\LocalService") !=
          ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create COM AppId key " << app_id_path;
    return false;
  }

  ScopedSd rdp_sd = ConvertSddlToSd(kRdpDesktopSessionComPermissionsSddl);
  if (!rdp_sd) {
    PLOG(ERROR) << "Failed to convert RDP COM SDDL";
    return false;
  }
  if (app_id_key.WriteValue(L"AccessPermission", rdp_sd.get(),
                            static_cast<DWORD>(rdp_sd.length()),
                            REG_BINARY) != ERROR_SUCCESS ||
      app_id_key.WriteValue(L"LaunchPermission", rdp_sd.get(),
                            static_cast<DWORD>(rdp_sd.length()),
                            REG_BINARY) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to write COM permissions for " << app_id_path;
    return false;
  }

  // Register CLSID for RdpDesktopSession and PSFactories.
  std::wstring clsid_base =
      base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid});
  base::win::RegKey clsid_key;
  if (clsid_key.Create(HKEY_LOCAL_MACHINE, clsid_base.c_str(), KEY_SET_VALUE) !=
          ERROR_SUCCESS ||
      clsid_key.WriteValue(nullptr, L"RdpDesktopSession Class") !=
          ERROR_SUCCESS ||
      clsid_key.WriteValue(L"AppID", kRdpAppId) != ERROR_SUCCESS ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({clsid_base, L"\\LocalServer32"}),
                               local_server_cmd) ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({clsid_base, L"\\TypeLib"}),
                               kChromotingTypeLibId)) {
    LOG(ERROR) << "Failed to register RdpDesktopSession CLSID.";
    return false;
  }

  for (const auto& [ps_clsid, ps_name] :
       std::to_array<std::pair<const wchar_t*, const wchar_t*>>({
           {kIRdpDesktopSessionId, L"IRdpDesktopSession PSFactory"},
           {kIRdpDesktopSessionEventHandlerId,
            L"IRdpDesktopSessionEventHandler PSFactory"},
       })) {
    std::wstring ps_path =
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", ps_clsid});
    if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE, ps_path, ps_name) ||
        !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                                 base::StrCat({ps_path, L"\\InprocServer32"}),
                                 core_dll_path)) {
      return false;
    }
  }

  // Register Interfaces (`IDaemonControl`, `IDaemonControl2`,
  // `IRdpDesktopSession`, `IRdpDesktopSessionEventHandler`).
  for (const wchar_t* iid : {kIDaemonControlId, kIDaemonControl2Id}) {
    std::wstring iface_path =
        base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid});
    if (!WriteDefaultStringValue(
            HKEY_LOCAL_MACHINE,
            base::StrCat({iface_path, L"\\ProxyStubClsid32"}),
            kUniversalMarshalerClsid) ||
        !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                                 base::StrCat({iface_path, L"\\TypeLib"}),
                                 kChromotingTypeLibId)) {
      return false;
    }
  }

  std::wstring rdp_iface_path =
      base::StrCat({L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionId});
  if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE, rdp_iface_path,
                               L"IRdpDesktopSession") ||
      !WriteDefaultStringValue(
          HKEY_LOCAL_MACHINE,
          base::StrCat({rdp_iface_path, L"\\ProxyStubClsid32"}),
          kIRdpDesktopSessionId) ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({rdp_iface_path, L"\\TypeLib"}),
                               kChromotingTypeLibId)) {
    return false;
  }

  std::wstring handler_iface_path = base::StrCat(
      {L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionEventHandlerId});
  if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE, handler_iface_path,
                               L"IRdpDesktopSessionEventHandler") ||
      !WriteDefaultStringValue(
          HKEY_LOCAL_MACHINE,
          base::StrCat({handler_iface_path, L"\\ProxyStubClsid32"}),
          kIRdpDesktopSessionEventHandlerId) ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({handler_iface_path, L"\\TypeLib"}),
                               kChromotingTypeLibId)) {
    return false;
  }

  // Register TypeLib (both win32 and win64 subkeys so 32-bit and 64-bit
  // callers resolve the TypeLib).
  std::wstring typelib_v1 = base::StrCat(
      {L"SOFTWARE\\Classes\\TypeLib\\", kChromotingTypeLibId, L"\\1.0"});
  if (!WriteDefaultStringValue(HKEY_LOCAL_MACHINE, typelib_v1,
                               L"Chromoting 1.0 Type Library") ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({typelib_v1, L"\\0\\win32"}),
                               core_dll_path) ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({typelib_v1, L"\\0\\win64"}),
                               core_dll_path) ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({typelib_v1, L"\\FLAGS"}), L"0") ||
      !WriteDefaultStringValue(HKEY_LOCAL_MACHINE,
                               base::StrCat({typelib_v1, L"\\HELPDIR"}),
                               install_dir.value())) {
    return false;
  }

  return true;
}

bool RegisterHostConfigAndLoggingKeys() {
  std::wstring base_key = base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  base::win::RegKey key;
  if (key.Create(HKEY_LOCAL_MACHINE,
                 base::StrCat({base_key, L"\\paired-clients\\clients"}).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS ||
      key.Create(HKEY_LOCAL_MACHINE,
                 base::StrCat({base_key, L"\\logging"}).c_str(),
                 KEY_SET_VALUE) != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create host pairing/logging registry keys.";
    return false;
  }

  // Create `paired-clients\secrets` and protect its DACL so only LocalSystem
  // and Built-in Administrators have access.
  ScopedSd secrets_sd = ConvertSddlToSd(kPairingRegistrySecretsSddl);
  if (!secrets_sd) {
    PLOG(ERROR) << "Failed to convert pairing secrets SDDL";
    return false;
  }

  SECURITY_ATTRIBUTES sa = {sizeof(sa), secrets_sd.get(), FALSE};
  std::wstring secrets_path =
      base::StrCat({base_key, L"\\paired-clients\\secrets"});
  HKEY raw_secrets_key = nullptr;
  LONG result = ::RegCreateKeyExW(
      HKEY_LOCAL_MACHINE, secrets_path.c_str(), /*Reserved=*/0,
      /*lpClass=*/nullptr, REG_OPTION_NON_VOLATILE,
      KEY_SET_VALUE | WRITE_DAC | READ_CONTROL, &sa, &raw_secrets_key,
      /*lpdwDisposition=*/nullptr);
  if (result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to create secrets key " << secrets_path << ": "
               << result;
    return false;
  }
  base::win::RegKey secrets_key(raw_secrets_key);

  PACL dacl = nullptr;
  BOOL dacl_present = FALSE;
  BOOL dacl_defaulted = FALSE;
  if (!::GetSecurityDescriptorDacl(secrets_sd.get(), &dacl_present, &dacl,
                                   &dacl_defaulted) ||
      !dacl_present) {
    PLOG(ERROR) << "Failed to extract DACL for pairing secrets key";
    return false;
  }

  DWORD sec_result = ::SetSecurityInfo(
      secrets_key.Handle(), SE_REGISTRY_KEY,
      DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
      /*psidOwner=*/nullptr, /*psidGroup=*/nullptr, dacl,
      /*pSacl=*/nullptr);
  if (sec_result != ERROR_SUCCESS) {
    LOG(ERROR) << "Failed to set DACL on secrets key: " << sec_result;
    return false;
  }

  return true;
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
bool RegisterOmahaClient() {
  std::wstring omaha_key_path =
      base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId});
  std::wstring version = base::UTF8ToWide(REMOTING_VERSION_STRING);

  for (REGSAM view :
       std::to_array<REGSAM>({KEY_WOW64_32KEY, KEY_WOW64_64KEY})) {
    base::win::RegKey key;
    if (key.Create(HKEY_LOCAL_MACHINE, omaha_key_path.c_str(),
                   KEY_SET_VALUE | view) != ERROR_SUCCESS ||
        key.WriteValue(L"pv", version.c_str()) != ERROR_SUCCESS ||
        key.WriteValue(L"name", kProductName) != ERROR_SUCCESS) {
      LOG(ERROR) << "Failed to register Omaha Clients key: " << omaha_key_path;
      return false;
    }
  }
  return true;
}
#endif

}  // namespace

bool RegisterHostRegistryEntries(const base::FilePath& install_dir) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  if (!RegisterNativeMessagingHosts(install_dir) ||
      !RegisterEventLogSource(install_dir) ||
      !RegisterRemoteSecurityKey(install_dir) ||
      !RegisterUrlForwarder(install_dir) || !RegisterComServer(install_dir) ||
      !RegisterHostConfigAndLoggingKeys()) {
    return false;
  }

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (!RegisterOmahaClient()) {
    return false;
  }
#endif

  return true;
}

void UnregisterHostRegistryEntries() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    DeleteKey(HKEY_LOCAL_MACHINE, key_path, KEY_WOW64_32KEY | KEY_WOW64_64KEY);
  }

  std::wstring event_log_key =
      base::StrCat({kEventLogApplicationKeyPath, L"\\", kEventSourceName});
  DeleteKey(HKEY_LOCAL_MACHINE, event_log_key);

  // Delete `RemoteSecurityKeyPath` value and remove
  // `SOFTWARE\Google\SecurityKey` only if no other subkeys or values exist.
  {
    base::win::RegKey sec_key;
    if (sec_key.Open(HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath,
                     KEY_SET_VALUE) == ERROR_SUCCESS) {
      sec_key.DeleteValue(L"RemoteSecurityKeyPath");
    }
  }
  DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath);

  std::wstring prog_id_key =
      base::StrCat({L"SOFTWARE\\Classes\\", kUrlForwarderProgId});
  DeleteKey(HKEY_LOCAL_MACHINE, prog_id_key);

  {
    base::win::RegKey reg_apps_key;
    if (reg_apps_key.Open(HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyPath,
                          KEY_SET_VALUE) == ERROR_SUCCESS) {
      reg_apps_key.DeleteValue(kUrlForwarderName);
    }
  }

  // Unregister COM entries.
  DeleteKey(HKEY_LOCAL_MACHINE,
            base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId}));
  std::wstring rdp_clsid = GetRdpDesktopSessionClsidString();
  if (!rdp_clsid.empty()) {
    DeleteKey(HKEY_LOCAL_MACHINE,
              base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid}));
  }
  for (const wchar_t* clsid :
       {kIRdpDesktopSessionId, kIRdpDesktopSessionEventHandlerId}) {
    DeleteKey(HKEY_LOCAL_MACHINE,
              base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", clsid}));
  }
  for (const wchar_t* iid :
       {kIDaemonControlId, kIDaemonControl2Id, kIRdpDesktopSessionId,
        kIRdpDesktopSessionEventHandlerId}) {
    DeleteKey(HKEY_LOCAL_MACHINE,
              base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid}));
  }
  DeleteKey(HKEY_LOCAL_MACHINE, base::StrCat({L"SOFTWARE\\Classes\\TypeLib\\",
                                              kChromotingTypeLibId}));

  std::wstring chromoting_key =
      base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  DeleteKey(HKEY_LOCAL_MACHINE, chromoting_key);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  DeleteKey(HKEY_LOCAL_MACHINE,
            base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId}),
            KEY_WOW64_32KEY | KEY_WOW64_64KEY);
#endif
}

}  // namespace remoting::installer
