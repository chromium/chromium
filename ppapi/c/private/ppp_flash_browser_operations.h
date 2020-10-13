/* Copyright 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* From private/ppp_flash_browser_operations.idl,
 *   modified Wed Oct 25 09:44:49 2017.
 */

#ifndef PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_
#define PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_macros.h"
#include "ppapi/c/pp_stdint.h"

#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_0 \
    "PPP_Flash_BrowserOperations;1.0"
#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_2 \
    "PPP_Flash_BrowserOperations;1.2"
#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_3 \
    "PPP_Flash_BrowserOperations;1.3"
#define PPP_FLASH_BROWSEROPERATIONS_INTERFACE \
    PPP_FLASH_BROWSEROPERATIONS_INTERFACE_1_3

/**
 * @file
 * This file contains the <code>PPP_Flash_BrowserOperations</code> interface.
 */


/**
 * @addtogroup Enums
 * @{
 */
typedef enum {
  PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_CAMERAMIC = 0,
  PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_PEERNETWORKING = 1,
  PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_LAST =
    PP_FLASH_BROWSEROPERATIONS_SETTINGTYPE_PEERNETWORKING
} PP_Flash_BrowserOperations_SettingType;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Flash_BrowserOperations_SettingType, 4);

typedef enum {
  /* This value is only used with <code>SetSitePermission()</code>. */
  PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT = 0,
  PP_FLASH_BROWSEROPERATIONS_PERMISSION_ALLOW = 1,
  PP_FLASH_BROWSEROPERATIONS_PERMISSION_BLOCK = 2,
  PP_FLASH_BROWSEROPERATIONS_PERMISSION_ASK = 3,
  PP_FLASH_BROWSEROPERATIONS_PERMISSION_LAST =
    PP_FLASH_BROWSEROPERATIONS_PERMISSION_ASK
} PP_Flash_BrowserOperations_Permission;
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_Flash_BrowserOperations_Permission, 4);
/**
 * @}
 */

/**
 * @addtogroup Structs
 * @{
 */
struct PP_Flash_BrowserOperations_SiteSetting {
  const char* site;
  PP_Flash_BrowserOperations_Permission permission;
};
/**
 * @}
 */

/**
 * @addtogroup Typedefs
 * @{
 */
typedef void (*PPB_Flash_BrowserOperations_GetSettingsCallback)(
    void* user_data,
    PP_Bool success,
    PP_Flash_BrowserOperations_Permission default_permission,
    uint32_t site_count,
    const struct PP_Flash_BrowserOperations_SiteSetting sites[]);
/**
 * @}
 */

/**
 * @addtogroup Interfaces
 * @{
 */
/**
 * This interface allows the browser to request the plugin do things.
 */
struct PPP_Flash_BrowserOperations_1_3 {
  /**
   * This function allows the plugin to implement the "Clear site data" feature.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin data is
   * stored. On UTF16 systems (Windows), this will be encoded as UTF-8. It will
   * be an absolute path and will not have a directory separator (slash) at the
   * end.
   * @param[in] site String specifying which site to clear the data for. This
   * will be null to clear data for all sites.
   * @param[in] flags Currently always 0 in Chrome to clear all data. This may
   * be extended in the future to clear only specific types of data.
   * @param[in] max_age The maximum age in seconds to clear data for. This
   * allows the plugin to implement "clear past hour" and "clear past data",
   * etc.
   *
   * @return PP_TRUE on success, PP_FALSE on failure.
   *
   * See also the NPP_ClearSiteData function in NPAPI.
   * https://wiki.mozilla.org/NPAPI:ClearSiteData
   */
  PP_Bool (*ClearSiteData)(const char* plugin_data_path,
                           const char* site,
                           uint64_t flags,
                           uint64_t max_age);
  /**
   * Requests the plugin to deauthorize content licenses. It prevents Flash from
   * playing protected content, such as movies and music the user may have
   * rented or purchased.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin settings are stored.
   *
   * @return <code>PP_TRUE</code> on success, <code>PP_FALSE</code> on failure.
   */
  PP_Bool (*DeauthorizeContentLicenses)(const char* plugin_data_path);
  /**
   * Gets permission settings. <code>callback</code> will be called exactly once
   * to return the settings.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin settings are stored.
   * @param[in] setting_type What type of setting to retrieve.
   * @param[in] callback The callback to return retrieved data.
   * @param[inout] user_data An opaque pointer that will be passed to
   * <code>callback</code>.
   */
  void (*GetPermissionSettings)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PPB_Flash_BrowserOperations_GetSettingsCallback callback,
      void* user_data);
  /**
   * Sets default permission. It applies to all sites except those with
   * site-specific settings.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin settings are stored.
   * @param[in] setting_type What type of setting to set.
   * @param[in] permission The default permission.
   * @param[in] clear_site_specific Whether to remove all site-specific
   * settings.
   *
   * @return <code>PP_TRUE</code> on success, <code>PP_FALSE</code> on failure.
   */
  PP_Bool (*SetDefaultPermission)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      PP_Bool clear_site_specific);
  /**
   * Sets site-specific permission. If a site has already got site-specific
   * permission and it is not in <code>sites</code>, it won't be affected.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin settings are stored.
   * @param[in] setting_type What type of setting to set.
   * @param[in] site_count How many items are there in <code>sites</code>.
   * @param[in] sites The site-specific settings. If a site is specified with
   * <code>PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT</code> permission, it
   * will be removed from the site-specific list.
   *
   * @return <code>PP_TRUE</code> on success, <code>PP_FALSE</code> on failure.
   */
  PP_Bool (*SetSitePermission)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      uint32_t site_count,
      const struct PP_Flash_BrowserOperations_SiteSetting sites[]);
  /**
   * Returns a list of sites that have stored data, for use with the
   * "Clear site data" feature.
   *
   * @param[in] plugin_data_path String containing the directory where the
   * plugin data is stored.
   * @param[out] sites A NULL-terminated array of sites that have stored data.
   * Use FreeSiteList on the array when done.
   *
   * See also the NPP_GetSitesWithData function in NPAPI:
   * https://wiki.mozilla.org/NPAPI:ClearSiteData
   */
  void (*GetSitesWithData)(const char* plugin_data_path, char*** sites);
  /**
   * Frees the list of sites returned by GetSitesWithData.
   *
   * @param[in] sites A NULL-terminated array of strings.
   */
  void (*FreeSiteList)(char* sites[]);
};

typedef struct PPP_Flash_BrowserOperations_1_3 PPP_Flash_BrowserOperations;

struct PPP_Flash_BrowserOperations_1_0 {
  PP_Bool (*ClearSiteData)(const char* plugin_data_path,
                           const char* site,
                           uint64_t flags,
                           uint64_t max_age);
};

struct PPP_Flash_BrowserOperations_1_2 {
  PP_Bool (*ClearSiteData)(const char* plugin_data_path,
                           const char* site,
                           uint64_t flags,
                           uint64_t max_age);
  PP_Bool (*DeauthorizeContentLicenses)(const char* plugin_data_path);
  void (*GetPermissionSettings)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PPB_Flash_BrowserOperations_GetSettingsCallback callback,
      void* user_data);
  PP_Bool (*SetDefaultPermission)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      PP_Flash_BrowserOperations_Permission permission,
      PP_Bool clear_site_specific);
  PP_Bool (*SetSitePermission)(
      const char* plugin_data_path,
      PP_Flash_BrowserOperations_SettingType setting_type,
      uint32_t site_count,
      const struct PP_Flash_BrowserOperations_SiteSetting sites[]);
};
/**
 * @}
 */

#endif  /* PPAPI_C_PRIVATE_PPP_FLASH_BROWSER_OPERATIONS_H_ */

