// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_APP_SORTING_H_
#define EXTENSIONS_BROWSER_APP_SORTING_H_

#include <stddef.h>

#include <string>

#include "components/sync/model/string_ordinal.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"

namespace extensions {

// An interface that provides a fixed ordering for a set of apps.
class AppSorting {
 public:
  AppSorting() {}

  AppSorting(const AppSorting&) = delete;
  AppSorting& operator=(const AppSorting&) = delete;

  virtual ~AppSorting() {}

  // Signals that ordinals for the WebAppProvider system should (or can) be
  // loaded now. Calls to the WebAppProvider system should not be done before
  // this is called. Called from WebAppUiManagerImpl::Start().
  virtual void InitializePageOrdinalMapFromWebApps() = 0;

  // Resolves any conflicts the might be created as a result of syncing that
  // results in two icons having the same page and app launch ordinal. After
  // this is called it is guaranteed that there are no collisions of NTP icons.
  virtual void FixNTPOrdinalCollisions() = 0;

  // This ensures that the extension has valid ordinals, and if it doesn't then
  // properly initialize them. |suggested_page| will be used if it is valid and
  // the extension has no valid user-set page ordinal.
  virtual void EnsureValidOrdinals(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& suggested_page) = 0;

  // Gets the default ordinals for |extension_id|. Returns false if no default
  // ordinals for |extension_id| is defined. Otherwise, returns true and
  // ordinals is updated with corresponding ordinals.
  virtual bool GetDefaultOrdinals(
      const ExtensionId& extension_id,
      syncer::StringOrdinal* page_ordinal,
      syncer::StringOrdinal* app_launch_ordinal) = 0;

  // Updates the app launcher value for the moved extension so that it is now
  // located after the given predecessor and before the successor.
  // Empty strings are used to indicate no successor or predecessor.
  virtual void OnExtensionMoved(const std::string& moved_extension_id,
                                const std::string& predecessor_extension_id,
                                const std::string& successor_extension_id) = 0;

  // Get the application launch ordinal for an app with |extension_id|. This
  // determines the order in which the app appears on the page it's on in the
  // New Tab Page (Note that you can compare app launch ordinals only if the
  // apps are on the same page). A string value close to |a*| generally
  // indicates top left. If the extension has no launch ordinal, an invalid
  // StringOrdinal is returned.
  virtual syncer::StringOrdinal GetAppLaunchOrdinal(
      const ExtensionId& extension_id) const = 0;

  // Sets a specific launch ordinal for an app with |extension_id|.
  virtual void SetAppLaunchOrdinal(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& new_app_launch_ordinal) = 0;

  // Returns a StringOrdinal that is lower than any app launch ordinal for the
  // given page.
  virtual syncer::StringOrdinal CreateFirstAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const = 0;

  // Returns a StringOrdinal that is higher than any app launch ordinal for the
  // given page.
  virtual syncer::StringOrdinal CreateNextAppLaunchOrdinal(
      const syncer::StringOrdinal& page_ordinal) const = 0;

  // Returns a StringOrdinal that is lower than any existing page ordinal.
  virtual syncer::StringOrdinal CreateFirstAppPageOrdinal() const = 0;

  // Gets the page a new app should install to, which is the earliest non-full
  // page.  The returned ordinal may correspond to a page that doesn't yet exist
  // if all pages are full.
  virtual syncer::StringOrdinal GetNaturalAppPageOrdinal() const = 0;

  // Get the page ordinal for an app with |extension_id|. This determines
  // which page an app will appear on in page-based NTPs.  If the app has no
  // page specified, an invalid StringOrdinal is returned.
  virtual syncer::StringOrdinal GetPageOrdinal(
      const ExtensionId& extension_id) const = 0;

  // Sets a specific page ordinal for an app with |extension_id|.
  virtual void SetPageOrdinal(
      const ExtensionId& extension_id,
      const syncer::StringOrdinal& new_page_ordinal) = 0;

  // Removes the ordinal values for an app.
  virtual void ClearOrdinals(const ExtensionId& extension_id) = 0;

  // Convert the page StringOrdinal value to its integer equivalent. This takes
  // O(# of apps) worst-case.
  virtual int PageStringOrdinalAsInteger(
      const syncer::StringOrdinal& page_ordinal) const = 0;

  // Converts the page index integer to its StringOrdinal equivalent. This takes
  // O(# of apps) worst-case.
  virtual syncer::StringOrdinal PageIntegerAsStringOrdinal(
      size_t page_index) = 0;

  // Hides an extension from the new tab page, or makes a previously hidden
  // extension visible.
  virtual void SetExtensionVisible(const ExtensionId& extension_id,
                                   bool visible) = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_APP_SORTING_H_
