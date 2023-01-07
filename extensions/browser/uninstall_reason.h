// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_UNINSTALL_REASON_H_
#define EXTENSIONS_BROWSER_UNINSTALL_REASON_H_

namespace extensions {

// Do not remove/reorder these, as they are used in uninstall ping data and we
// depend on their values being stable.
enum UninstallReason {
  UNINSTALL_REASON_FOR_TESTING,         // Used for testing code only
  UNINSTALL_REASON_USER_INITIATED,      // User performed some UI gesture
  UNINSTALL_REASON_EXTENSION_DISABLED,  // Extension disabled due to error
  UNINSTALL_REASON_STORAGE_THRESHOLD_EXCEEDED,
  UNINSTALL_REASON_INSTALL_CANCELED,
  UNINSTALL_REASON_MANAGEMENT_API,
  UNINSTALL_REASON_SYNC,
  UNINSTALL_REASON_ORPHANED_THEME,
  UNINSTALL_REASON_ORPHANED_EPHEMERAL_EXTENSION,
  // The entries below imply bypassing checking user has permission to
  // uninstall the corresponding extension id.
  UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
  UNINSTALL_REASON_ORPHANED_SHARED_MODULE,
  UNINSTALL_REASON_INTERNAL_MANAGEMENT,  // Internal extensions (see usages)
  UNINSTALL_REASON_REINSTALL,
  UNINSTALL_REASON_COMPONENT_REMOVED,
  UNINSTALL_REASON_MIGRATED,  // Migrated to component extensions

  UNINSTALL_REASON_CHROME_WEBSTORE,

  UNINSTALL_REASON_ARC,  // Web app that was uninstalled via ARC

  UNINSTALL_REASON_MAX,  // Should always be the last value
};

// The source of an uninstall. Do *NOT* reorder or delete any of the named
// values, as they are used in UMA. Put all new values above
// NUM_UNINSTALL_SOURCES.
enum UninstallSource {
  UNINSTALL_SOURCE_FOR_TESTING,
  UNINSTALL_SOURCE_TOOLBAR_CONTEXT_MENU,
  UNINSTALL_SOURCE_PERMISSIONS_INCREASE,
  UNINSTALL_SOURCE_STORAGE_THRESHOLD_EXCEEDED,
  UNINSTALL_SOURCE_APP_LIST,
  UNINSTALL_SOURCE_APP_INFO_DIALOG,
  UNINSTALL_SOURCE_CHROME_APPS_PAGE,
  UNINSTALL_SOURCE_CHROME_EXTENSIONS_PAGE,
  UNINSTALL_SOURCE_EXTENSION,
  UNINSTALL_SOURCE_CHROME_WEBSTORE,
  UNINSTALL_SOURCE_HOSTED_APP_MENU,
  NUM_UNINSTALL_SOURCES,
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_UNINSTALL_REASON_H_
