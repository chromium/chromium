// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/testing/earl_grey/app_launch_argument_generator.h"

#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"

NSArray<NSString*>* ArgumentsFromConfiguration(
    AppLaunchConfiguration configuration) {
  NSMutableArray<NSString*>* namesToEnable = [NSMutableArray array];
  NSMutableArray<NSString*>* namesToDisable = [NSMutableArray array];
  NSMutableArray<NSString*>* variations = [NSMutableArray array];

  for (const auto& feature : configuration.features_enabled) {
    [namesToEnable addObject:base::SysUTF8ToNSString(feature->name)];
  }

  for (const auto& feature : configuration.features_disabled) {
    [namesToDisable addObject:base::SysUTF8ToNSString(feature->name)];
  }

  for (const variations::VariationID& variation :
       configuration.variations_enabled) {
    [variations addObject:[NSString stringWithFormat:@"%d", variation]];
  }

  for (const variations::VariationID& variation :
       configuration.trigger_variations_enabled) {
    [variations addObject:[NSString stringWithFormat:@"t%d", variation]];
  }

  NSMutableArray<NSString*>* arguments = [[NSMutableArray alloc] init];

  if (configuration.iph_feature_enabled.has_value()) {
    std::string iph_enable_argument = base::StringPrintf(
        "--enable-iph=%s", configuration.iph_feature_enabled.value().c_str());
    [arguments addObject:base::SysUTF8ToNSString(iph_enable_argument)];
  }

  std::string enableKey = "--enable-features=";
  std::string disableKey = "--disable-features=";
  for (const std::string& arg : configuration.additional_args) {
    // Extract any args enabling or disabling features and combine all, plus
    // anything already provided directly in the configuration.
    if (arg.starts_with(enableKey)) {
      [namesToEnable
          addObject:base::SysUTF8ToNSString(arg.substr(enableKey.length()))];
      continue;
    }
    if (arg.starts_with(disableKey)) {
      [namesToDisable
          addObject:base::SysUTF8ToNSString(arg.substr(disableKey.length()))];
      continue;
    }
    [arguments addObject:base::SysUTF8ToNSString(arg)];
  }

  NSString* enabledString = @"";
  NSString* disabledString = @"";
  NSString* variationString = @"";
  if ([namesToEnable count] > 0) {
    enabledString = [NSString
        stringWithFormat:@"--enable-features=%@",
                         [namesToEnable componentsJoinedByString:@","]];
  }
  if ([namesToDisable count] > 0) {
    disabledString = [NSString
        stringWithFormat:@"--disable-features=%@",
                         [namesToDisable componentsJoinedByString:@","]];
  }
  if (variations.count > 0) {
    variationString =
        [NSString stringWithFormat:@"--force-variation-ids=%@",
                                   [variations componentsJoinedByString:@","]];
  }

  [arguments insertObject:enabledString atIndex:0];
  [arguments insertObject:disabledString atIndex:1];
  [arguments insertObject:variationString atIndex:2];

  // For the app to use the same language as the test module. This workaround
  // the fact that EarlGrey tests depends on localizable strings (unfortunate)
  // even though the test module does not have access to the system locale. To
  // make this work, we force the app to use the same locale as used by the
  // test module (which is the first item in -preferredLocalizations).
  NSArray<NSString*>* languages = [NSBundle mainBundle].preferredLocalizations;
  if (languages.count != 0) {
    NSString* language = [languages firstObject];
    [arguments addObject:@"-AppleLanguages"];
    [arguments addObject:[NSString stringWithFormat:@"(%@)", language]];
  }
  return arguments;
}
