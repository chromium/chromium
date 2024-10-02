// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/page_info/page_info_app_interface.h"

#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#import "components/optimization_guide/core/optimization_metadata.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "components/page_info/core/proto/about_this_site_metadata.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

@implementation PageInfoAppInterface

+ (void)addAboutThisSiteHintForURL:(NSString*)url
                       description:(NSString*)description
                  aboutThisSiteURL:(NSString*)aboutThisSiteURL {
  page_info::proto::SiteInfo siteInfo;
  if (description != nil) {
    page_info::proto::SiteDescription* siteInfoDescription =
        siteInfo.mutable_description();
    siteInfoDescription->set_description(base::SysNSStringToUTF8(description));
    siteInfoDescription->set_lang("en_US");
    siteInfoDescription->set_name("Example");
    siteInfoDescription->mutable_source()->set_url("https://example.com");
    siteInfoDescription->mutable_source()->set_label("Example source");
  }

  if (aboutThisSiteURL != nil) {
    siteInfo.mutable_more_about()->set_url(
        base::SysNSStringToUTF8(aboutThisSiteURL));
  }

  optimization_guide::OptimizationMetadata optimizationMetadata;
  page_info::proto::AboutThisSiteMetadata metadata;
  *metadata.mutable_site_info() = siteInfo;
  optimizationMetadata.SetAnyMetadataForTesting(metadata);

  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  service->AddHintForTesting(
      GURL(base::SysNSStringToUTF8(url)),
      optimization_guide::proto::OptimizationType::ABOUT_THIS_SITE,
      optimizationMetadata);
}

@end
