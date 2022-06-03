// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBXR_ANDROID_XR_INSTALL_HELPER_DELEGATE_H_
#define COMPONENTS_WEBXR_ANDROID_XR_INSTALL_HELPER_DELEGATE_H_

namespace content {
class WebContents;
}

namespace infobars {
class InfoBarManager;
}

namespace webxr {

// Helper class for InstallHelpers to gain access to other components. Since the
// InstallHelpers are intended to be plumbed through content/public's
// XrIntegrationClient, they may not contain direct references to other
// component types. This provides a means for embedders to pass accessors for
// other component types to the InstallHelpers.
class XrInstallHelperDelegate {
 public:
  virtual ~XrInstallHelperDelegate() = default;

  XrInstallHelperDelegate(const XrInstallHelperDelegate&) = delete;
  XrInstallHelperDelegate& operator=(const XrInstallHelperDelegate&) = delete;

  // Gets the InfoBarManager for the provided web_contents, which can be used to
  // display the installation confirmation message. It is acceptable to return
  // a null InfoBarManager, but installation may fail after doing so.
  virtual infobars::InfoBarManager* GetInfoBarManager(
      content::WebContents* web_contents) = 0;

 protected:
  XrInstallHelperDelegate() = default;
};

}  // namespace webxr

#endif  // COMPONENTS_WEBXR_ANDROID_XR_INSTALL_HELPER_DELEGATE_H_
