// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_WEB_CONTENTS_OBSERVER_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_WEB_CONTENTS_OBSERVER_H_

#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/extension_web_contents_observer.h"

namespace extensions {

// The app_shell version of ExtensionWebContentsObserver.
class ShellExtensionWebContentsObserver
    : public ExtensionWebContentsObserver,
      public content::WebContentsUserData<ShellExtensionWebContentsObserver> {
 public:
  ShellExtensionWebContentsObserver(const ShellExtensionWebContentsObserver&) =
      delete;
  ShellExtensionWebContentsObserver& operator=(
      const ShellExtensionWebContentsObserver&) = delete;

  ~ShellExtensionWebContentsObserver() override;

  // Creates and initializes an instance of this class for the given
  // |web_contents|, if it doesn't already exist.
  static void CreateForWebContents(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<ShellExtensionWebContentsObserver>;

  explicit ShellExtensionWebContentsObserver(
      content::WebContents* web_contents);

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_EXTENSION_WEB_CONTENTS_OBSERVER_H_
