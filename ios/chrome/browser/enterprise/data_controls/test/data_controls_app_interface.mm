// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/enterprise/data_controls/test/data_controls_app_interface.h"

#import "components/enterprise/data_controls/core/browser/prefs.h"
#import "components/enterprise/data_controls/core/browser/test_utils.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation DataControlsAppInterface

+ (void)setBlockCopyRule {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  data_controls::SetDataControls(prefs, {R"({
                                    "sources": {
                                      "urls": ["*"]
                                    },
                                    "restrictions": [
                                      {"class": "CLIPBOARD", "level": "BLOCK"}
                                    ]
                                  })"},
                                 /*machine_scope=*/false);
}

+ (void)setWarnCopyRule {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  data_controls::SetDataControls(prefs, {R"({
                                    "sources": {
                                      "urls": ["*"]
                                    },
                                    "restrictions": [
                                      {"class": "CLIPBOARD", "level": "WARN"}
                                    ]
                                  })"},
                                 /*machine_scope=*/false);
}

+ (void)clearDataControlRules {
  PrefService* prefs = chrome_test_util::GetOriginalProfile()->GetPrefs();
  prefs->ClearPref(data_controls::kDataControlsRulesPref);
}

@end
