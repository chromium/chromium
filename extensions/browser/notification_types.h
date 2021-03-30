// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
#define EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

#include <string>

#include "content/public/browser/notification_types.h"
#include "extensions/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_EXTENSIONS)
#error "Extensions must be enabled"
#endif

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

namespace extensions {

// Only notifications fired by the extensions module should be here. The
// extensions module should not listen to notifications fired by the
// embedder.
enum NotificationType {
  // WARNING: This need to match chrome/browser/chrome_notification_types.h.
  NOTIFICATION_EXTENSIONS_START = content::NOTIFICATION_CONTENT_END,

  // Sent when a CrxInstaller finishes. Source is the CrxInstaller that
  // finished. The details are the extension which was installed.
  // DEPRECATED: Use extensions::InstallObserver::OnFinishCrxInstall()
  // TODO(https://crbug.com/1174728): Remove.
  NOTIFICATION_CRX_INSTALLER_DONE = NOTIFICATION_EXTENSIONS_START,

  // Sent when attempting to load a new extension, but they are disabled. The
  // details are an Extension, and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174732): Remove.
  NOTIFICATION_EXTENSION_UPDATE_DISABLED,

  // Sent when an extension's permissions change. The details are an
  // UpdatedExtensionPermissionsInfo, and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174733): Remove.
  NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,

  // An error occurred during extension install. The details are a string with
  // details about why the install failed.
  // TODO(https://crbug.com/1174734): Remove.
  NOTIFICATION_EXTENSION_INSTALL_ERROR,

  // Sent when an Extension object is removed from ExtensionService. This
  // can happen when an extension is uninstalled, upgraded, or blocklisted,
  // including all cases when the Extension is deleted. The details are an
  // Extension, and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174736): Remove.
  NOTIFICATION_EXTENSION_REMOVED,

  // Sent after a new ExtensionHost* is created. The details are
  // an ExtensionHost* and the source is a BrowserContext*.
  // TODO(https://crbug.com/1174737): Remove.
  NOTIFICATION_EXTENSION_HOST_CREATED,

  // Sent before an ExtensionHost* is destroyed. The details are
  // an ExtensionHost* and the source is a BrowserContext*.
  //
  // DEPRECATED: Use
  // extensions::ExtensionHostObserver::OnExtensionHostDestroyed()
  // TODO(https://crbug.com/1174738): Remove.
  NOTIFICATION_EXTENSION_HOST_DESTROYED,

  // Sent by an ExtensionHost* when it has finished its initial page load,
  // including any external resources.
  // The details are an ExtensionHost* and the source is a BrowserContext*.
  //
  // DEPRECATED: Use extensions::DeferredStartRenderHostObserver::
  // OnDeferredStartRenderHostDidStopFirstLoad()
  // TODO(https://crbug.com/1174741): Remove.
  NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,

  // Sent when extension render process ends (whether it crashes or closes). The
  // details are an ExtensionHost* and the source is a BrowserContext*. Not sent
  // during browser shutdown.
  // TODO(https://crbug.com/1174743): Remove.
  NOTIFICATION_EXTENSION_PROCESS_TERMINATED,

  // Sent when a background page is ready so other components can load.
  // TODO(https://crbug.com/1174744): Remove.
  NOTIFICATION_EXTENSION_BACKGROUND_PAGE_READY,

  // Sent by an extension to notify the browser about the results of a unit
  // test.
  // TODO(https://crbug.com/1174745): Remove.
  NOTIFICATION_EXTENSION_TEST_PASSED,
  NOTIFICATION_EXTENSION_TEST_FAILED,

  // Sent by extension test javascript code, typically in a browser test. The
  // sender is a std::string representing the extension id, and the details
  // are a std::string with some message. This is particularly useful when you
  // want to have C++ code wait for javascript code to do something.
  // TODO(https://crbug.com/1174746): Remove.
  NOTIFICATION_EXTENSION_TEST_MESSAGE,

  // Sent when an bookmarks extensions API function was successfully invoked.
  // The source is the id of the extension that invoked the function, and the
  // details are a pointer to the const BookmarksFunction in question.
  // TODO(https://crbug.com/1174748): Remove.
  NOTIFICATION_EXTENSION_BOOKMARKS_API_INVOKED,

  // Sent when an omnibox extension has sent back omnibox suggestions. The
  // source is the BrowserContext*, and the details are an
  // extensions::api::omnibox::SendSuggestions::Params object.
  // TODO(https://crbug.com/1174750): Remove.
  NOTIFICATION_EXTENSION_OMNIBOX_SUGGESTIONS_READY,

  // Sent when the user accepts the input in an extension omnibox keyword
  // session. The source is the BrowserContext*.
  // TODO(https://crbug.com/1174751): Remove.
  NOTIFICATION_EXTENSION_OMNIBOX_INPUT_ENTERED,

  // Sent when an omnibox extension has updated the default suggestion. The
  // source is the BrowserContext*.
  // TODO(https://crbug.com/1174752): Remove.
  NOTIFICATION_EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,

  // Sent when the extension updater starts checking for updates to installed
  // extensions. The source is a BrowserContext*, and there are no details.
  // TODO(https://crbug.com/1174753): Remove.
  NOTIFICATION_EXTENSION_UPDATING_STARTED,

  // The extension updater found an update and will attempt to download and
  // install it. The source is a BrowserContext*, and the details are an
  // extensions::UpdateDetails object with the extension id and version of the
  // found update.
  // TODO(https://crbug.com/1174754): Remove.
  NOTIFICATION_EXTENSION_UPDATE_FOUND,

  NOTIFICATION_EXTENSIONS_END
};

// **
// ** NOTICE
// **
// ** The notification system is deprecated, obsolete, and is slowly being
// ** removed. See https://crbug.com/268984 and https://crbug.com/411569.
// **
// ** Please don't add any new notification types, and please help migrate
// ** existing uses of the notification types below to use the Observer and
// ** Callback patterns.
// **

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
