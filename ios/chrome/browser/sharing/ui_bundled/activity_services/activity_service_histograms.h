// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_
#define IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_

#import "ios/chrome/browser/sharing/ui_bundled/activity_services/activity_type_util.h"
#import "ios/chrome/browser/sharing/ui_bundled/sharing_scenario.h"

// Enum representing an aggregation of the `ActivityType` enum values in a way
// that is relevant for metric collection. Current values should not
// be renumbered. Please keep in sync with "IOSShareAction" in
// src/tools/metrics/histograms/enums.xml.
enum class ShareActionType {
  Unknown = 0,
  Cancel = 1,
  Bookmark = 2,
  Copy = 3,
  SaveImage = 4,
  FindInPage = 5,
  Print = 6,
  ReadingList = 7,
  Mail = 8,
  RequestDesktopMobileSite = 9,
  SendTabToSelf = 10,
  CreateQRCode = 11,
  NativeMessage = 12,
  UnknownGoogleApp = 13,
  NativeSocialApp = 14,
  ThirdPartyMessagingApp = 15,
  ThirdPartyContentApp = 16,
  SaveFile = 17,
  Markup = 18,
  NativeAddToHome = 19,
  kMaxValue = NativeAddToHome
};

// Records a histogram metric for the current scenario.
void RecordScenarioInitiated(SharingScenario scenario);

// Records the given activity `type` for a `scenario`.
void RecordActivityForScenario(activity_type_util::ActivityType type,
                               SharingScenario scenario);

// Records the given activity `type` for a `scenario`.
void RecordCancelledScenario(SharingScenario scenario);

#endif  // IOS_CHROME_BROWSER_SHARING_UI_BUNDLED_ACTIVITY_SERVICES_ACTIVITY_SERVICE_HISTOGRAMS_H_
