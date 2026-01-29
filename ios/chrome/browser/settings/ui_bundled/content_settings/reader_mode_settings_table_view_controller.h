// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/settings/ui_bundled/settings_root_table_view_controller.h"

namespace dom_distiller {
class DistilledPagePrefs;
}  // namespace dom_distiller

class PrefService;

// Controller for the UI that allows the user to change Reading Mode settings.
@interface ReaderModeSettingsTableViewController
    : SettingsRootTableViewController

// The designated initializer. `distilledPagePrefs` and `prefService` must not
// be null.
- (instancetype)initWithDistilledPagePrefs:
                    (dom_distiller::DistilledPagePrefs*)distilledPagePrefs
                               prefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_UI_BUNDLED_CONTENT_SETTINGS_READER_MODE_SETTINGS_TABLE_VIEW_CONTROLLER_H_
