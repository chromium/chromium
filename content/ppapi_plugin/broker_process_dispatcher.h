// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
#define CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/child/scoped_child_process_reference.h"
#include "ppapi/c/ppp.h"
#include "ppapi/proxy/broker_dispatcher.h"
#include "ppapi/shared_impl/ppp_flash_browser_operations_shared.h"

namespace content {

// Wrapper around a BrokerDispatcher that provides the necessary integration
// for plugin process management. This class is to avoid direct dependencies
// from the PPAPI proxy on the Chrome multiprocess infrastructure.
class BrokerProcessDispatcher
    : public ppapi::proxy::BrokerSideDispatcher,
      public base::SupportsWeakPtr<BrokerProcessDispatcher> {
 public:
  BrokerProcessDispatcher(PP_GetInterface_Func get_plugin_interface,
                          PP_ConnectInstance_Func connect_instance,
                          bool peer_is_browser);
  ~BrokerProcessDispatcher() override;

  // IPC::Listener overrides.
  bool OnMessageReceived(const IPC::Message& msg) override;

  void OnGetPermissionSettingsCompleted(
      uint32_t request_id,
      bool success,
      PP_Flash_BrowserOperations_Permission default_permission,
      const ppapi::FlashSiteSettings& sites);

 private:
  void OnGetSitesWithData(uint32_t request_id,
                          const base::FilePath& plugin_data_path);
  void OnClearSiteData(uint32_t request_id,
                       const base::FilePath& plugin_data_path,
                       const std::string& site,
                       uint64_t flags,
                       uint64_t max_age);
  void OnDeauthorizeContentLicenses(uint32_t request_id,
                                    const base::FilePath& plugin_data_path);
  void OnGetPermissionSettings(
      uint32_t request_id,
      const base::FilePath& plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type);
  void OnSetDefaultPermission(
      uint32_t request_id,
      const base::FilePath& plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      bool clear_site_specific);
  void OnSetSitePermission(uint32_t request_id,
                           const base::FilePath& plugin_data_path,
                           PP_Flash_BrowserOperations_SettingType setting_type,
                           const ppapi::FlashSiteSettings& sites);

  // Returns a list of sites that have data stored.
  void GetSitesWithData(const base::FilePath& plugin_data_path,
                        std::vector<std::string>* sites);

  // Requests that the plugin clear data, returning true on success.
  bool ClearSiteData(const base::FilePath& plugin_data_path,
                     const std::string& site,
                     uint64_t flags,
                     uint64_t max_age);

  bool DeauthorizeContentLicenses(const base::FilePath& plugin_data_path);
  bool SetDefaultPermission(const base::FilePath& plugin_data_path,
                            PP_Flash_BrowserOperations_SettingType setting_type,
                            PP_Flash_BrowserOperations_Permission permission,
                            bool clear_site_specific);
  bool SetSitePermission(const base::FilePath& plugin_data_path,
                         PP_Flash_BrowserOperations_SettingType setting_type,
                         const ppapi::FlashSiteSettings& sites);

  ScopedChildProcessReference process_ref_;

  PP_GetInterface_Func get_plugin_interface_;

  const PPP_Flash_BrowserOperations_1_3* flash_browser_operations_1_3_;
  const PPP_Flash_BrowserOperations_1_2* flash_browser_operations_1_2_;
  const PPP_Flash_BrowserOperations_1_0* flash_browser_operations_1_0_;

  bool peer_is_browser_;

  DISALLOW_COPY_AND_ASSIGN(BrokerProcessDispatcher);
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_BROKER_PROCESS_DISPATCHER_H_
