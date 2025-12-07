// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_RECOMMENDED_DETAILS_H_
#define IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_RECOMMENDED_DETAILS_H_

#import <string>

#import "url/gurl.h"

struct UpgradeRecommendedDetails {
  GURL upgrade_url;
  std::string next_version;
  bool is_up_to_date = false;
};

#endif  // IOS_CHROME_BROWSER_UPGRADE_MODEL_UPGRADE_RECOMMENDED_DETAILS_H_
