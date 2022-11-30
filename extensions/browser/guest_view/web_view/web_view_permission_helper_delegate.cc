// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/web_view/web_view_permission_helper_delegate.h"

#include "extensions/browser/guest_view/web_view/web_view_guest.h"

namespace extensions {

WebViewPermissionHelperDelegate::WebViewPermissionHelperDelegate(
    WebViewPermissionHelper* web_view_permission_helper)
    : web_view_permission_helper_(web_view_permission_helper) {}

WebViewPermissionHelperDelegate::~WebViewPermissionHelperDelegate() {
}

}  // namespace extensions
