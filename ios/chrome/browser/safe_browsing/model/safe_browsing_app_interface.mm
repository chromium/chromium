// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_app_interface.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/ios/browser/client_side_detection_feature_cache.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@implementation SafeBrowsingAppInterface

+ (void)setMockScorer {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  safe_browsing::ClientSideDetectionService* service =
      ClientSideDetectionServiceFactory::GetForProfile(profile);
  DCHECK(service);
  auto scorer = std::make_unique<safe_browsing::Scorer>();
  service->SetScorerForTesting(std::move(scorer));
}

+ (BOOL)isScorerSet {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  safe_browsing::ClientSideDetectionService* service =
      ClientSideDetectionServiceFactory::GetForProfile(profile);
  return service && service->GetScorer() != nullptr;
}

+ (void)clearScorer {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  safe_browsing::ClientSideDetectionService* service =
      ClientSideDetectionServiceFactory::GetForProfile(profile);
  if (service) {
    service->SetScorerForTesting(nullptr);
  }
}

+ (BOOL)hasCachedVerdictForURL:(NSString*)url {
  GURL gurl(base::SysNSStringToUTF8(url));
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  if (!web_state) {
    return NO;
  }
  safe_browsing::ClientSideDetectionFeatureCache* feature_cache =
      safe_browsing::ClientSideDetectionFeatureCache::FromWebState(web_state);
  return feature_cache && feature_cache->GetVerdictForURL(gurl) != nullptr;
}

@end
