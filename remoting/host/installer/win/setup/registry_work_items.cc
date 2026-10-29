// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Chrome Remote Desktop product-specific Windows registry `WorkItem`s and
// registration schemas for the Windows host installer.
//
// See `registry_work_items.h` for the boundary between CRD product-specific
// registry schemas (which belong here) and generic registry primitives (which
// belong in `registry_util.{h,cc}`).

#include "remoting/host/installer/win/setup/registry_work_items.h"

#include <windows.h>

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "build/branding_buildflags.h"
#include "chrome/installer/util/work_item.h"
#include "chrome/installer/util/work_item_list.h"
#include "remoting/base/registry_util_win.h"
#include "remoting/base/version.h"
#include "remoting/host/installer/win/setup/installer_constants.h"
#include "remoting/host/installer/win/setup/native_messaging_entries.h"
#include "remoting/host/installer/win/setup/registry_util.h"
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

void AddNativeMessagingWorkItems(const base::FilePath& install_dir,
                                 WorkItemList* list) {
  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    std::wstring manifest_path =
        install_dir.Append(entry.manifest_file_name).value();
    for (REGSAM view : kBothRegistryViews) {
      AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE,
                                              key_path, manifest_path, view);
    }
  }
}

void AddEventLogWorkItems(const base::FilePath& install_dir,
                          WorkItemList* list) {
  std::wstring key_path =
      base::StrCat({kEventLogApplicationKeyPath, L"\\", kEventSourceName});
  std::wstring core_dll_path = install_dir.Append(kCoreBinaryName).value();
  constexpr DWORD kTypesSupported =
      EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;

  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, key_path,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, key_path,
                               WorkItem::kWow64Default, L"CategoryCount",
                               static_cast<DWORD>(1u), /*overwrite=*/true);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, key_path,
                               WorkItem::kWow64Default, L"CategoryMessageFile",
                               core_dll_path, /*overwrite=*/true);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, key_path,
                               WorkItem::kWow64Default, L"EventMessageFile",
                               core_dll_path, /*overwrite=*/true);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, key_path,
                               WorkItem::kWow64Default, L"TypesSupported",
                               kTypesSupported, /*overwrite=*/true);
}

void AddRemoteSecurityKeyWorkItems(const base::FilePath& install_dir,
                                   WorkItemList* list) {
  std::wstring binary_path =
      install_dir.Append(kRemoteSecurityKeyBinaryName).value();
  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath,
                               WorkItem::kWow64Default,
                               L"RemoteSecurityKeyPath", binary_path,
                               /*overwrite=*/true);
}

void AddUrlForwarderWorkItems(const base::FilePath& install_dir,
                              WorkItemList* list) {
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
  std::wstring app_key_path = base::StrCat({prog_id_base, L"\\Application"});
  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, app_key_path,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_key_path,
                               WorkItem::kWow64Default, L"ApplicationCompany",
                               std::wstring(kManufacturer), /*overwrite=*/true);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_key_path,
                               WorkItem::kWow64Default, L"ApplicationName",
                               app_name_res, /*overwrite=*/true);
  list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, app_key_path, WorkItem::kWow64Default,
      L"ApplicationDescription", app_desc_res, /*overwrite=*/true);
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_key_path,
                               WorkItem::kWow64Default, L"ApplicationIcon",
                               app_icon_res, /*overwrite=*/true);

  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({prog_id_base, L"\\DefaultIcon"}),
      app_icon_res);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE,
      base::StrCat({prog_id_base, L"\\shell\\open\\command"}), open_command);

  std::wstring capabilities_path = base::StrCat(
      {L"SOFTWARE\\", kChromotingKeyPath, L"\\UrlForwarder\\Capabilities"});
  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, capabilities_path,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, capabilities_path, WorkItem::kWow64Default,
      L"ApplicationDescription", app_desc_res, /*overwrite=*/true);

  std::wstring assoc_path =
      base::StrCat({capabilities_path, L"\\URLAssociations"});
  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, assoc_path,
                                WorkItem::kWow64Default);
  for (const wchar_t* scheme : {L"http", L"https", L"mailto"}) {
    list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, assoc_path,
                                 WorkItem::kWow64Default, scheme,
                                 std::wstring(kUrlForwarderProgId),
                                 /*overwrite=*/true);
  }

  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE,
                                kRegisteredApplicationsKeyPath,
                                WorkItem::kWow64Default);
  list->AddSetRegValueWorkItem(
      HKEY_LOCAL_MACHINE, kRegisteredApplicationsKeyPath,
      WorkItem::kWow64Default, kUrlForwarderName, capabilities_path,
      /*overwrite=*/true);
}

bool AddComServerWorkItems(const base::FilePath& install_dir,
                           WorkItemList* list) {
  std::wstring rdp_clsid = FormatClsidString(CLSID_RdpDesktopSession);
  if (rdp_clsid.empty()) {
    LOG(ERROR) << "Failed to format CLSID_RdpDesktopSession";
    return false;
  }

  ScopedSd rdp_sd = ConvertSddlToSd(kRdpDesktopSessionComPermissionsSddl);
  if (!rdp_sd) {
    PLOG(ERROR) << "Failed to convert RDP COM SDDL";
    return false;
  }
  // SAFETY: `rdp_sd` owns a heap buffer of `rdp_sd.length()` bytes allocated by
  // `ConvertSddlToSd`.
  base::span<const uint8_t> rdp_sd_bytes = UNSAFE_BUFFERS(base::span(
      reinterpret_cast<const uint8_t*>(rdp_sd.get()), rdp_sd.length()));

  std::wstring core_dll_path = install_dir.Append(kCoreBinaryName).value();
  std::wstring host_exe_path = install_dir.Append(kHostBinaryName).value();
  std::wstring local_server_cmd =
      base::StrCat({L"\"", host_exe_path, L"\" --type=rdp_desktop_session"});

  // Register AppId `{52e6fd1a-f16e-49c0-aacb-5436a915448b}`.
  std::wstring app_id_path =
      base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId});
  AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE, app_id_path,
                                          L"ChromotingRdpDesktopSession");
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, app_id_path,
                               WorkItem::kWow64Default, L"RunAs",
                               L"NT AUTHORITY\\LocalService",
                               /*overwrite=*/true);
  list->AddWorkItem(new SetRegBinaryValueWorkItem(
      HKEY_LOCAL_MACHINE, app_id_path, WorkItem::kWow64Default,
      L"AccessPermission", rdp_sd_bytes));
  list->AddWorkItem(new SetRegBinaryValueWorkItem(
      HKEY_LOCAL_MACHINE, app_id_path, WorkItem::kWow64Default,
      L"LaunchPermission", rdp_sd_bytes));

  // Register CLSID for RdpDesktopSession and PSFactories.
  std::wstring clsid_base =
      base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid});
  AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE, clsid_base,
                                          L"RdpDesktopSession Class");
  list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, clsid_base,
                               WorkItem::kWow64Default, L"AppID",
                               std::wstring(kRdpAppId), /*overwrite=*/true);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({clsid_base, L"\\LocalServer32"}),
      local_server_cmd);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({clsid_base, L"\\TypeLib"}),
      kChromotingTypeLibId);

  for (const auto& [ps_clsid, ps_name] :
       std::to_array<std::pair<const wchar_t*, const wchar_t*>>({
           {kIRdpDesktopSessionId, L"IRdpDesktopSession PSFactory"},
           {kIRdpDesktopSessionEventHandlerId,
            L"IRdpDesktopSessionEventHandler PSFactory"},
       })) {
    std::wstring ps_path =
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", ps_clsid});
    AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE, ps_path,
                                            ps_name);
    AddCreateKeyAndSetDefaultValueWorkItems(
        list, HKEY_LOCAL_MACHINE, base::StrCat({ps_path, L"\\InprocServer32"}),
        core_dll_path);
  }

  // Register Interfaces (`IDaemonControl`, `IDaemonControl2`,
  // `IRdpDesktopSession`, `IRdpDesktopSessionEventHandler`).
  for (const wchar_t* iid : {kIDaemonControlId, kIDaemonControl2Id}) {
    std::wstring iface_path =
        base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid});
    AddCreateKeyAndSetDefaultValueWorkItems(
        list, HKEY_LOCAL_MACHINE,
        base::StrCat({iface_path, L"\\ProxyStubClsid32"}),
        kUniversalMarshalerClsid);
    AddCreateKeyAndSetDefaultValueWorkItems(
        list, HKEY_LOCAL_MACHINE, base::StrCat({iface_path, L"\\TypeLib"}),
        kChromotingTypeLibId);
  }

  std::wstring rdp_iface_path =
      base::StrCat({L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionId});
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, rdp_iface_path, L"IRdpDesktopSession");
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE,
      base::StrCat({rdp_iface_path, L"\\ProxyStubClsid32"}),
      kIRdpDesktopSessionId);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({rdp_iface_path, L"\\TypeLib"}),
      kChromotingTypeLibId);

  std::wstring handler_iface_path = base::StrCat(
      {L"SOFTWARE\\Classes\\Interface\\", kIRdpDesktopSessionEventHandlerId});
  AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE,
                                          handler_iface_path,
                                          L"IRdpDesktopSessionEventHandler");
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE,
      base::StrCat({handler_iface_path, L"\\ProxyStubClsid32"}),
      kIRdpDesktopSessionEventHandlerId);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE,
      base::StrCat({handler_iface_path, L"\\TypeLib"}), kChromotingTypeLibId);

  // Register TypeLib (both win32 and win64 subkeys so 32-bit and 64-bit
  // callers resolve the TypeLib).
  std::wstring typelib_v1 = base::StrCat(
      {L"SOFTWARE\\Classes\\TypeLib\\", kChromotingTypeLibId, L"\\1.0"});
  AddCreateKeyAndSetDefaultValueWorkItems(list, HKEY_LOCAL_MACHINE, typelib_v1,
                                          L"Chromoting 1.0 Type Library");
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({typelib_v1, L"\\0\\win32"}),
      core_dll_path);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({typelib_v1, L"\\0\\win64"}),
      core_dll_path);
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({typelib_v1, L"\\FLAGS"}), L"0");
  AddCreateKeyAndSetDefaultValueWorkItems(
      list, HKEY_LOCAL_MACHINE, base::StrCat({typelib_v1, L"\\HELPDIR"}),
      install_dir.value());

  return true;
}

void AddHostConfigAndLoggingWorkItems(WorkItemList* list) {
  std::wstring base_key = base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  list->AddCreateRegKeyWorkItem(
      HKEY_LOCAL_MACHINE,
      base::StrCat({base_key, L"\\paired-clients\\clients"}),
      WorkItem::kWow64Default);
  list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE,
                                base::StrCat({base_key, L"\\logging"}),
                                WorkItem::kWow64Default);
  list->AddWorkItem(new CreateProtectedRegKeyWorkItem(
      HKEY_LOCAL_MACHINE,
      base::StrCat({base_key, L"\\paired-clients\\secrets"}),
      WorkItem::kWow64Default, std::string(kPairingRegistrySecretsSddl)));
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
void AddOmahaClientWorkItems(WorkItemList* list) {
  std::wstring omaha_key_path =
      base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId});
  std::wstring version = base::UTF8ToWide(REMOTING_VERSION_STRING);

  for (REGSAM view : kBothRegistryViews) {
    list->AddCreateRegKeyWorkItem(HKEY_LOCAL_MACHINE, omaha_key_path, view);
    list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, omaha_key_path, view,
                                 L"pv", version, /*overwrite=*/true);
    list->AddSetRegValueWorkItem(HKEY_LOCAL_MACHINE, omaha_key_path, view,
                                 L"name", std::wstring(kProductName),
                                 /*overwrite=*/true);
  }
}
#endif

bool AddHostRegistryWorkItems(const base::FilePath& install_dir,
                              WorkItemList* list) {
  CHECK(list);

  AddNativeMessagingWorkItems(install_dir, list);
  AddEventLogWorkItems(install_dir, list);
  AddRemoteSecurityKeyWorkItems(install_dir, list);
  AddUrlForwarderWorkItems(install_dir, list);
  if (!AddComServerWorkItems(install_dir, list)) {
    return false;
  }
  AddHostConfigAndLoggingWorkItems(list);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  AddOmahaClientWorkItems(list);
#endif

  return true;
}

void AddUnregisterHostRegistryWorkItems(WorkItemList* list) {
  CHECK(list);

  for (const auto& entry : kNativeMessagingEntries) {
    std::wstring key_path =
        base::StrCat({entry.registry_parent_path, L"\\", entry.host_name});
    for (REGSAM view : kBothRegistryViews) {
      list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, key_path, view);
    }
  }

  std::wstring event_log_key =
      base::StrCat({kEventLogApplicationKeyPath, L"\\", kEventSourceName});
  list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, event_log_key,
                                WorkItem::kWow64Default);

  // Delete `RemoteSecurityKeyPath` value and remove
  // `SOFTWARE\Google\SecurityKey` only if no other subkeys or values exist.
  list->AddDeleteRegValueWorkItem(
      HKEY_LOCAL_MACHINE, kSecurityKeyRegistryKeyPath, WorkItem::kWow64Default,
      L"RemoteSecurityKeyPath");
  list->AddCallbackWorkItem(base::BindOnce([](const CallbackWorkItem&) {
                              DeleteKeyIfEmpty(HKEY_LOCAL_MACHINE,
                                               kSecurityKeyRegistryKeyPath);
                              return true;
                            }),
                            base::DoNothing());

  std::wstring prog_id_key =
      base::StrCat({L"SOFTWARE\\Classes\\", kUrlForwarderProgId});
  list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, prog_id_key,
                                WorkItem::kWow64Default);
  list->AddDeleteRegValueWorkItem(HKEY_LOCAL_MACHINE,
                                  kRegisteredApplicationsKeyPath,
                                  WorkItem::kWow64Default, kUrlForwarderName);

  // Unregister COM entries.
  list->AddDeleteRegKeyWorkItem(
      HKEY_LOCAL_MACHINE,
      base::StrCat({L"SOFTWARE\\Classes\\AppId\\", kRdpAppId}),
      WorkItem::kWow64Default);
  std::wstring rdp_clsid = FormatClsidString(CLSID_RdpDesktopSession);
  if (!rdp_clsid.empty()) {
    list->AddDeleteRegKeyWorkItem(
        HKEY_LOCAL_MACHINE,
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", rdp_clsid}),
        WorkItem::kWow64Default);
  }
  for (const wchar_t* clsid :
       {kIRdpDesktopSessionId, kIRdpDesktopSessionEventHandlerId}) {
    list->AddDeleteRegKeyWorkItem(
        HKEY_LOCAL_MACHINE,
        base::StrCat({L"SOFTWARE\\Classes\\CLSID\\", clsid}),
        WorkItem::kWow64Default);
  }
  for (const wchar_t* iid :
       {kIDaemonControlId, kIDaemonControl2Id, kIRdpDesktopSessionId,
        kIRdpDesktopSessionEventHandlerId}) {
    list->AddDeleteRegKeyWorkItem(
        HKEY_LOCAL_MACHINE,
        base::StrCat({L"SOFTWARE\\Classes\\Interface\\", iid}),
        WorkItem::kWow64Default);
  }
  list->AddDeleteRegKeyWorkItem(
      HKEY_LOCAL_MACHINE,
      base::StrCat({L"SOFTWARE\\Classes\\TypeLib\\", kChromotingTypeLibId}),
      WorkItem::kWow64Default);

  std::wstring chromoting_key =
      base::StrCat({L"SOFTWARE\\", kChromotingKeyPath});
  list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, chromoting_key,
                                WorkItem::kWow64Default);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  std::wstring omaha_key =
      base::StrCat({L"SOFTWARE\\Google\\Update\\Clients\\", kOmahaAppId});
  for (REGSAM view : kBothRegistryViews) {
    list->AddDeleteRegKeyWorkItem(HKEY_LOCAL_MACHINE, omaha_key, view);
  }
#endif
}

}  // namespace

HostRegistryWorkItem::HostRegistryWorkItem(const base::FilePath& install_dir)
    : install_dir_(install_dir),
      work_item_list_(WorkItem::CreateWorkItemList()) {}

HostRegistryWorkItem::~HostRegistryWorkItem() = default;

bool HostRegistryWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  work_item_list_->set_best_effort(best_effort());
  work_item_list_->set_rollback_enabled(rollback_enabled());
  if (!AddHostRegistryWorkItems(install_dir_, work_item_list_.get())) {
    return false;
  }
  return work_item_list_->Do();
}

void HostRegistryWorkItem::RollbackImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  work_item_list_->Rollback();
}

UnregisterRegistryWorkItem::UnregisterRegistryWorkItem()
    : work_item_list_(WorkItem::CreateWorkItemList()) {
  set_best_effort(true);
  set_rollback_enabled(false);
}

UnregisterRegistryWorkItem::~UnregisterRegistryWorkItem() = default;

bool UnregisterRegistryWorkItem::DoImpl() {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  work_item_list_->set_best_effort(true);
  work_item_list_->set_rollback_enabled(false);
  AddUnregisterHostRegistryWorkItems(work_item_list_.get());
  return work_item_list_->Do();
}

void UnregisterRegistryWorkItem::RollbackImpl() {
  // Uninstallation is best-effort and runs with `set_rollback_enabled(false)`.
}

}  // namespace remoting::installer
