// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/optimization_guide/model/optimization_guide_test_app_interface.h"

#import <vector>

#import "base/command_line.h"
#import "base/logging.h"
#import "base/strings/sys_string_conversions.h"
#import "components/optimization_guide/core/hints_component_info.h"
#import "components/optimization_guide/core/hints_component_util.h"
#import "components/optimization_guide/core/hints_fetcher_factory.h"
#import "components/optimization_guide/core/hints_manager.h"
#import "components/optimization_guide/core/optimization_guide_switches.h"
#import "components/optimization_guide/core/optimization_hints_component_update_listener.h"
#import "components/optimization_guide/core/test_hints_component_creator.h"
#import "components/optimization_guide/proto/hints.pb.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service.h"
#import "ios/chrome/browser/optimization_guide/model/optimization_guide_service_factory.h"
#import "ios/chrome/test/app/chrome_test_util.h"

void OptimizationGuideTestAppInterfaceWrapper::SetOptimizationGuideServiceUrl(
    NSString* url) {
  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  GURL gurl(base::SysNSStringToUTF8(url));
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      optimization_guide::switches::kOptimizationGuideServiceGetHintsURL,
      gurl.spec());
  service->GetHintsManager()
      ->GetHintsFetcherFactory()
      ->OverrideOptimizationGuideServiceUrlForTesting(gurl);
}

optimization_guide::testing::TestHintsComponentCreator
    test_hints_component_creator;

@implementation OptimizationGuideTestAppInterface

+ (void)setGetHintsURL:(NSString*)url {
  OptimizationGuideTestAppInterfaceWrapper::SetOptimizationGuideServiceUrl(url);
}

+ (void)setComponentUpdateHints:(NSString*)url {
  const optimization_guide::HintsComponentInfo& component_info =
      test_hints_component_creator.CreateHintsComponentInfoWithPageHints(
          optimization_guide::proto::NOSCRIPT, {base::SysNSStringToUTF8(url)},
          "*");

  // Use the max int as version in components info, so that it will pick up and
  // update hints from test component info.
  optimization_guide::HintsComponentInfo new_component_info(
      base::Version({UINT32_MAX}), component_info.path);
  optimization_guide::OptimizationHintsComponentUpdateListener::GetInstance()
      ->MaybeUpdateHintsComponent(new_component_info);
}

+ (void)registerOptimizationType:
    (optimization_guide::proto::OptimizationType)type {
  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  service->RegisterOptimizationTypes({type});
}

+ (void)canApplyOptimization:(NSString*)url
                        type:(optimization_guide::proto::OptimizationType)type
                    metadata:
                        (optimization_guide::OptimizationMetadata*)metadata {
  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  service->CanApplyOptimization(GURL(base::SysNSStringToUTF8(url)), type,
                                metadata);
}

+ (void)addHintForTesting:(NSString*)url
                     type:(optimization_guide::proto::OptimizationType)type
           serialized_any:(NSData*)serialized_any
                 type_url:(NSString*)type_url

{
  std::string serialized = std::string(
      static_cast<const char*>(serialized_any.bytes), serialized_any.length);

  optimization_guide::proto::Any any_metadata;
  any_metadata.set_type_url(base::SysNSStringToUTF8(type_url).c_str());
  any_metadata.set_value(serialized);
  optimization_guide::OptimizationMetadata metadata;
  metadata.set_any_metadata(any_metadata);

  OptimizationGuideService* service =
      OptimizationGuideServiceFactory::GetForProfile(
          chrome_test_util::GetOriginalProfile());
  DCHECK(service);
  service->AddHintForTesting(GURL(base::SysNSStringToUTF8(url)), type,
                             metadata);
}

@end
