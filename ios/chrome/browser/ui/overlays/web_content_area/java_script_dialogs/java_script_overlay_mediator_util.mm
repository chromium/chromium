// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/web_content_area/java_script_dialogs/java_script_overlay_mediator_util.h"

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/overlays/public/web_content_area/java_script_dialog_source.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* GetJavaScriptDialogTitle(const JavaScriptDialogSource& source,
                                   const std::string& config_message) {
  return source.is_main_frame()
             ? base::SysUTF8ToNSString(config_message)
             : l10n_util::GetNSString(
                   IDS_JAVASCRIPT_MESSAGEBOX_TITLE_NONSTANDARD_URL_IFRAME);
}

NSString* GetJavaScriptDialogMessage(const JavaScriptDialogSource& source,
                                     const std::string& config_message) {
  return source.is_main_frame() ? nil : base::SysUTF8ToNSString(config_message);
}
