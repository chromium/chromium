// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_browsing/model/safe_browsing_app_interface.h"

#import "base/check.h"
#import "base/strings/strcat.h"
#import "base/strings/sys_string_conversions.h"
#import "components/safe_browsing/core/browser/verdict_cache_manager.h"
#import "components/safe_browsing/core/common/phishing_classifier/scorer.h"
#import "components/safe_browsing/ios/browser/client_side_detection_feature_cache.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_host_ios.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service.h"
#import "ios/chrome/browser/safe_browsing/model/client_side_detection/client_side_detection_service_factory.h"
#import "ios/chrome/browser/safe_browsing/model/verdict_cache_manager_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_tab_helper.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

namespace {
constexpr int kCacheDurationSec = 60;
}  // namespace

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

+ (void)cacheRealTimeVerdictForURL:(NSString*)url
                      forceRequest:(BOOL)forceRequest {
  const GURL gurl(base::SysNSStringToUTF8(url));
  safe_browsing::RTLookupResponse response;
  if (forceRequest) {
    response.set_client_side_detection_type(
        safe_browsing::ClientSideDetectionType::FORCE_REQUEST);
  }
  safe_browsing::RTLookupResponse::ThreatInfo* threat_info =
      response.add_threat_info();
  threat_info->set_verdict_type(
      safe_browsing::RTLookupResponse::ThreatInfo::SAFE);
  threat_info->set_cache_duration_sec(kCacheDurationSec);
  std::string cache_expression = base::StrCat({gurl.host(), gurl.path()});
  threat_info->set_cache_expression_using_match_type(cache_expression);
  threat_info->set_cache_expression_match_type(
      safe_browsing::RTLookupResponse::ThreatInfo::EXACT_MATCH);

  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  safe_browsing::VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(profile);
  if (cache_manager) {
    cache_manager->CacheRealTimeUrlVerdict(response, base::Time::Now());
  }
}

+ (void)triggerClassificationDoneWithURL:(NSString*)url
                            visualScores:(NSArray<NSNumber*>*)scores {
  web::WebState* web_state = chrome_test_util::GetCurrentWebState();
  if (!web_state) {
    return;
  }

  SafeBrowsingTabHelper* tab_helper =
      SafeBrowsingTabHelper::FromWebState(web_state);
  if (!tab_helper || !tab_helper->client_side_detection_host()) {
    return;
  }

  const GURL gurl(base::SysNSStringToUTF8(url));
  std::vector<double> visual_scores;
  visual_scores.reserve(scores.count);
  for (NSNumber* score in scores) {
    visual_scores.push_back(score.doubleValue);
  }

  static_cast<safe_browsing::ClientSideDetectionHostIOS*>(
      tab_helper->client_side_detection_host())
      ->OnVisualClassificationDoneForTesting(gurl, visual_scores);
}

+ (NSInteger)cachedRealTimeURLClientSideDetectionTypeForURL:(NSString*)url {
  ProfileIOS* profile = chrome_test_util::GetOriginalProfile();
  safe_browsing::VerdictCacheManager* cache_manager =
      VerdictCacheManagerFactory::GetForProfile(profile);
  if (!cache_manager) {
    return -1;
  }

  const GURL gurl(base::SysNSStringToUTF8(url));
  return static_cast<NSInteger>(
      cache_manager->GetCachedRealTimeUrlClientSideDetectionType(gurl));
}

+ (void)setBypassLocalResourceCheckForTesting:(BOOL)bypass {
  safe_browsing::ClientSideDetectionHostIOS::
      SetBypassLocalResourceCheckForTesting(bypass);
}

@end
