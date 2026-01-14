// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/web/model/print/print_tab_helper.h"

#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tabs/model/tab_title_util.h"
#import "ios/chrome/browser/web/model/print/print_handler.h"

PrintTabHelper::PrintTabHelper(web::WebState* web_state)
    : web_state_(web_state) {}

PrintTabHelper::~PrintTabHelper() = default;

void PrintTabHelper::set_printer(id<PrintHandler> printer) {
  printer_ = printer;
}

void PrintTabHelper::Print() {
  BOOL printingEnabled =
      ProfileIOS::FromBrowserState(web_state_->GetBrowserState())
          ->GetPrefs()
          ->GetBoolean(prefs::kPrintingEnabled);

  if (!printingEnabled) {
    // Ignore window.print() if the PrintingEnabled pref is set to NO.
    return;
  }

  [printer_ printView:web_state_->GetView()
            withTitle:tab_util::GetTabTitle(web_state_)];
}
