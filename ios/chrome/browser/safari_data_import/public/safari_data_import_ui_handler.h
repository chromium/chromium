// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_UI_HANDLER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_UI_HANDLER_H_

@protocol SafariDataImportUIHandler

/// Alerts that the current Safari data import workflow was dismissed.
- (void)safariDataImportDidDismiss;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_PUBLIC_SAFARI_DATA_IMPORT_UI_HANDLER_H_
