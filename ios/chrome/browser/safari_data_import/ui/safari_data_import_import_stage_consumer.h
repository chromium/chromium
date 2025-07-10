// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_CONSUMER_H_
#define IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_CONSUMER_H_

enum class SafariDataImportStage;

/// Consumer that updates the UI to reflect import stage transition.
@protocol SafariDataImportImportStageConsumer

/// Method for the consumer to reflect the UI for `stage`.
- (void)transitionToImportStage:(SafariDataImportStage)stage;

@end

#endif  // IOS_CHROME_BROWSER_SAFARI_DATA_IMPORT_UI_SAFARI_DATA_IMPORT_IMPORT_STAGE_CONSUMER_H_
