// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/common/channel_info.h"

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

#import "base/apple/bundle_locations.h"
#import "base/strings/sys_string_conversions.h"
#import "build/branding_buildflags.h"
#import "components/version_info/version_info.h"
#import "components/version_info/version_string.h"

namespace {

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// Channel of the running application, initialized by the first call to
// GetChannel() and cached for the whole application lifetime.
version_info::Channel g_channel = version_info::Channel::UNKNOWN;
#endif

}  // namespace

std::string GetVersionString() {
  return version_info::GetVersionStringWithModifier(GetChannelString());
}

std::string GetChannelString() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Only ever return one of "" (for STABLE channel), "unknown", "beta", "dev"
  // or "canary" in branded build.
  switch (GetChannel()) {
    case version_info::Channel::STABLE:
      return std::string();

    case version_info::Channel::BETA:
      return "beta";

    case version_info::Channel::DEV:
      return "dev";

    case version_info::Channel::CANARY:
      return "canary";

    case version_info::Channel::UNKNOWN:
      return "unknown";
  }
#else
  // Always return empty string for non-branded builds.
  return std::string();
#endif
}

version_info::Channel GetChannel() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  static dispatch_once_t channel_dispatch_token;
  dispatch_once(&channel_dispatch_token, ^{
    NSBundle* bundle = base::apple::OuterBundle();

    // Only Keystone-enabled build can have a channel.
    if (![bundle objectForInfoDictionaryKey:@"KSProductID"])
      return;

    NSString* channel = [bundle objectForInfoDictionaryKey:@"KSChannelID"];
    if (!channel) {
      // KSChannelID is unset for the stable channel.
      g_channel = version_info::Channel::STABLE;
    } else if ([channel isEqualToString:@"beta"]) {
      g_channel = version_info::Channel::BETA;
    } else if ([channel isEqualToString:@"dev"]) {
      g_channel = version_info::Channel::DEV;
    } else if ([channel isEqualToString:@"canary"]) {
      g_channel = version_info::Channel::CANARY;
    }
  });

  return g_channel;
#else
  return version_info::Channel::UNKNOWN;
#endif
}
