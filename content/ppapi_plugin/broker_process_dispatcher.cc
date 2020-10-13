// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/broker_process_dispatcher.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/child/child_process.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {
namespace {

// How long we wait before releasing the broker process.
const int kBrokerReleaseTimeSeconds = 30;

std::string ConvertPluginDataPath(const base::FilePath& plugin_data_path) {
  // The string is always 8-bit, convert on Windows.
#if defined(OS_WIN)
  return base::WideToUTF8(plugin_data_path.value());
#else
  return plugin_data_path.value();
#endif
}

struct GetPermissionSettingsContext {
  GetPermissionSettingsContext(
      const base::WeakPtr<BrokerProcessDispatcher> in_dispatcher,
      uint32_t in_request_id)
      : dispatcher(in_dispatcher), request_id(in_request_id) {}

  base::WeakPtr<BrokerProcessDispatcher> dispatcher;
  uint32_t request_id;
};

void GetPermissionSettingsCallback(
    void* user_data,
    PP_Bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    uint32_t site_count,
    const PP_Flash_BrowserOperations_SiteSetting sites[]) {
  std::unique_ptr<GetPermissionSettingsContext> context(
      reinterpret_cast<GetPermissionSettingsContext*>(user_data));

  if (!context->dispatcher.get())
    return;

  ppapi::FlashSiteSettings site_vector;
  if (success) {
    site_vector.reserve(site_count);
    for (uint32_t i = 0; i < site_count; ++i) {
      if (!sites[i].site) {
        success = PP_FALSE;
        break;
      }
      site_vector.push_back(
          ppapi::FlashSiteSetting(sites[i].site, sites[i].permission));
    }

    if (!success)
      site_vector.clear();
  }
  context->dispatcher->OnGetPermissionSettingsCompleted(
      context->request_id, PP_ToBool(success), default_permission, site_vector);
}

}  // namespace

BrokerProcessDispatcher::BrokerProcessDispatcher(
    PP_GetInterface_Func get_plugin_interface,
    PP_ConnectInstance_Func connect_instance,
    bool peer_is_browser)
    : ppapi::proxy::BrokerSideDispatcher(connect_instance),
      get_plugin_interface_(get_plugin_interface),
      flash_browser_operations_1_3_(nullptr),
      flash_browser_operations_1_2_(nullptr),
      flash_browser_operations_1_0_(nullptr),
      peer_is_browser_(peer_is_browser) {
  if (get_plugin_interface) {
    flash_browser_operations_1_0_ =
        static_cast<const PPP_Flash_BrowserOperations_1_0*>(
            get_plugin_interface_(PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_0));

    flash_browser_operations_1_2_ =
        static_cast<const PPP_Flash_BrowserOperations_1_2*>(
            get_plugin_interface_(PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_2));

    flash_browser_operations_1_3_ =
        static_cast<const PPP_Flash_BrowserOperations_1_3*>(
            get_plugin_interface_(PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_3));
  }
}

BrokerProcessDispatcher::~BrokerProcessDispatcher() {
  DVLOG(1) << "BrokerProcessDispatcher::~BrokerProcessDispatcher()";
  // Don't free the process right away. This timer allows the child process
  // to be re-used if the user rapidly goes to a new page that requires this
  // plugin. This is the case for common plugins where they may be used on a
  // source and destination page of a navigation. We don't want to tear down
  // and re-start processes each time in these cases.
  process_ref_.ReleaseWithDelay(
      base::TimeDelta::FromSeconds(kBrokerReleaseTimeSeconds));
}

bool BrokerProcessDispatcher::OnMessageReceived(const IPC::Message& msg) {
  if (BrokerSideDispatcher::OnMessageReceived(msg))
    return true;

  if (!peer_is_browser_) {
    // We might want to consider killing the peer instead is we see problems in
    // the future.
    return false;
  }

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrokerProcessDispatcher, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_GetSitesWithData, OnGetSitesWithData)
    IPC_MESSAGE_HANDLER(PpapiMsg_ClearSiteData, OnClearSiteData)
    IPC_MESSAGE_HANDLER(PpapiMsg_DeauthorizeContentLicenses,
                        OnDeauthorizeContentLicenses)
    IPC_MESSAGE_HANDLER(PpapiMsg_GetPermissionSettings,
                        OnGetPermissionSettings)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetDefaultPermission, OnSetDefaultPermission)
    IPC_MESSAGE_HANDLER(PpapiMsg_SetSitePermission, OnSetSitePermission)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrokerProcessDispatcher::OnGetPermissionSettingsCompleted(
    uint32_t request_id,
    bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    const ppapi::FlashSiteSettings& sites) {
  Send(new PpapiHostMsg_GetPermissionSettingsResult(
      request_id, success, default_permission, sites));
}

void BrokerProcessDispatcher::OnGetSitesWithData(
    uint32_t request_id,
    const base::FilePath& plugin_data_path) {
  std::vector<std::string> sites;
  GetSitesWithData(plugin_data_path, &sites);
  Send(new PpapiHostMsg_GetSitesWithDataResult(request_id, sites));
}

void BrokerProcessDispatcher::OnClearSiteData(
    uint32_t request_id,
    const base::FilePath& plugin_data_path,
    const std::string& site,
    uint64_t flags,
    uint64_t max_age) {
  Send(new PpapiHostMsg_ClearSiteDataResult(
      request_id, ClearSiteData(plugin_data_path, site, flags, max_age)));
}

void BrokerProcessDispatcher::OnDeauthorizeContentLicenses(
    uint32_t request_id,
    const base::FilePath& plugin_data_path) {
  Send(new PpapiHostMsg_DeauthorizeContentLicensesResult(
      request_id, DeauthorizeContentLicenses(plugin_data_path)));
}

void BrokerProcessDispatcher::OnGetPermissionSettings(
    uint32_t request_id,
    const base::FilePath& plugin_data_path,
    PP_Flash_BrowserOperations_SettingType setting_type) {
  if (flash_browser_operations_1_3_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    // The GetPermissionSettingsContext object will be deleted in
    // GetPermissionSettingsCallback().
    flash_browser_operations_1_3_->GetPermissionSettings(
        data_str.c_str(), setting_type, &GetPermissionSettingsCallback,
        new GetPermissionSettingsContext(AsWeakPtr(), request_id));
    return;
  }

  if (flash_browser_operations_1_2_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    // The GetPermissionSettingsContext object will be deleted in
    // GetPermissionSettingsCallback().
    flash_browser_operations_1_2_->GetPermissionSettings(
        data_str.c_str(), setting_type, &GetPermissionSettingsCallback,
        new GetPermissionSettingsContext(AsWeakPtr(), request_id));
    return;
  }

  OnGetPermissionSettingsCompleted(
      request_id, false, PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT,
      ppapi::FlashSiteSettings());
  return;
}

void BrokerProcessDispatcher::OnSetDefaultPermission(
    uint32_t request_id,
    const base::FilePath& plugin_data_path,
    PP_Flash_BrowserOperations_SettingType setting_type,
    PP_Flash_BrowserOperations_Permission permission,
    bool clear_site_specific) {
  Send(new PpapiHostMsg_SetDefaultPermissionResult(
      request_id,
      SetDefaultPermission(plugin_data_path, setting_type, permission,
                           clear_site_specific)));
}

void BrokerProcessDispatcher::OnSetSitePermission(
    uint32_t request_id,
    const base::FilePath& plugin_data_path,
    PP_Flash_BrowserOperations_SettingType setting_type,
    const ppapi::FlashSiteSettings& sites) {
  Send(new PpapiHostMsg_SetSitePermissionResult(
      request_id, SetSitePermission(plugin_data_path, setting_type, sites)));
}

void BrokerProcessDispatcher::GetSitesWithData(
    const base::FilePath& plugin_data_path,
    std::vector<std::string>* site_vector) {
  std::string data_str = ConvertPluginDataPath(plugin_data_path);
  if (flash_browser_operations_1_3_) {
    char** sites = nullptr;
    flash_browser_operations_1_3_->GetSitesWithData(data_str.c_str(), &sites);
    if (!sites)
      return;

    for (size_t i = 0; sites[i]; ++i)
      site_vector->push_back(sites[i]);

    flash_browser_operations_1_3_->FreeSiteList(sites);
  }
}

bool BrokerProcessDispatcher::ClearSiteData(
    const base::FilePath& plugin_data_path,
    const std::string& site,
    uint64_t flags,
    uint64_t max_age) {
  std::string data_str = ConvertPluginDataPath(plugin_data_path);
  if (flash_browser_operations_1_3_) {
    flash_browser_operations_1_3_->ClearSiteData(
        data_str.c_str(), site.empty() ? nullptr : site.c_str(), flags,
        max_age);
    return true;
  }

  // TODO(viettrungluu): Remove this (and the 1.0 interface) sometime after M21
  // goes to Stable.
  if (flash_browser_operations_1_2_) {
    flash_browser_operations_1_2_->ClearSiteData(
        data_str.c_str(), site.empty() ? nullptr : site.c_str(), flags,
        max_age);
    return true;
  }

  if (flash_browser_operations_1_0_) {
    flash_browser_operations_1_0_->ClearSiteData(
        data_str.c_str(), site.empty() ? nullptr : site.c_str(), flags,
        max_age);
    return true;
  }

  return false;
}

bool BrokerProcessDispatcher::DeauthorizeContentLicenses(
    const base::FilePath& plugin_data_path) {
  if (flash_browser_operations_1_3_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    return PP_ToBool(flash_browser_operations_1_3_->DeauthorizeContentLicenses(
        data_str.c_str()));
  }

  if (flash_browser_operations_1_2_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    return PP_ToBool(flash_browser_operations_1_2_->DeauthorizeContentLicenses(
        data_str.c_str()));
  }

  return false;
}

bool BrokerProcessDispatcher::SetDefaultPermission(
    const base::FilePath& plugin_data_path,
    PP_Flash_BrowserOperations_SettingType setting_type,
    PP_Flash_BrowserOperations_Permission permission,
    bool clear_site_specific) {
  if (flash_browser_operations_1_3_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    return PP_ToBool(flash_browser_operations_1_3_->SetDefaultPermission(
        data_str.c_str(), setting_type, permission,
        PP_FromBool(clear_site_specific)));
  }

  if (flash_browser_operations_1_2_) {
    std::string data_str = ConvertPluginDataPath(plugin_data_path);
    return PP_ToBool(flash_browser_operations_1_2_->SetDefaultPermission(
        data_str.c_str(), setting_type, permission,
        PP_FromBool(clear_site_specific)));
  }

  return false;
}

bool BrokerProcessDispatcher::SetSitePermission(
    const base::FilePath& plugin_data_path,
    PP_Flash_BrowserOperations_SettingType setting_type,
    const ppapi::FlashSiteSettings& sites) {
  if (sites.empty())
    return true;

  std::string data_str = ConvertPluginDataPath(plugin_data_path);
  std::unique_ptr<PP_Flash_BrowserOperations_SiteSetting[]> site_array(
      new PP_Flash_BrowserOperations_SiteSetting[sites.size()]);

  for (size_t i = 0; i < sites.size(); ++i) {
    site_array[i].site = sites[i].site.c_str();
    site_array[i].permission = sites[i].permission;
  }

  if (flash_browser_operations_1_3_) {
    PP_Bool result = flash_browser_operations_1_3_->SetSitePermission(
        data_str.c_str(), setting_type,
        static_cast<uint32_t>(sites.size()), site_array.get());

    return PP_ToBool(result);
  }

  if (flash_browser_operations_1_2_) {
    PP_Bool result = flash_browser_operations_1_2_->SetSitePermission(
        data_str.c_str(), setting_type,
        static_cast<uint32_t>(sites.size()), site_array.get());

    return PP_ToBool(result);
  }

  return false;
}

}  // namespace content
