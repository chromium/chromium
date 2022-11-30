// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_
#define EXTENSIONS_BROWSER_NOTIFICATION_TYPES_H_

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

  // An error occurred during extension install. The details are a string with
  // details about why the install failed.
  // TODO(https://crbug.com/1174734): Remove.
  NOTIFICATION_EXTENSION_INSTALL_ERROR,

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
