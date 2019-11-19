// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/geolocation/CLLocation+XGeoHeader.h"

#include <stdint.h>

#include "base/base64url.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Returns the LocationDescriptor as an ASCII proto encoded in web safe base64.
std::string LocationDescriptorAsWebSafeBase64(int64_t timestamp,
                                              long radius,
                                              double latitude,
                                              double longitude) {
  const std::string location_descriptor =
      base::StringPrintf("role: CURRENT_LOCATION\n"
                         "producer: DEVICE_LOCATION\n"
                         "timestamp: %lld\n"
                         "radius: %ld\n"
                         "latlng <\n"
                         "  latitude_e7: %.f\n"
                         "  longitude_e7: %.f\n"
                         ">",
                         timestamp, radius, latitude, longitude);

  std::string encoded_descriptor;
  base::Base64UrlEncode(location_descriptor,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_descriptor);
  return encoded_descriptor;
}
}  // namespace

@implementation CLLocation (XGeoHeader)

// Returns the timestamp of this location in microseconds since the UNIX epoch.
// Returns 0 if the timestamp is unavailable or invalid.
- (int64_t)cr_timestampInMicroseconds {
  NSTimeInterval seconds = [self.timestamp timeIntervalSince1970];
  if (seconds > 0) {
    const int64_t kSecondsToMicroseconds = 1000000;
    return (int64_t)(seconds * kSecondsToMicroseconds);
  }
  return 0;
}

// Returns the horizontal accuracy radius of |location|. The smaller the value,
// the more accurate the location. A value -1 is returned if accuracy is
// unavailable.
- (long)cr_accuracyInMillimeters {
  const long kMetersToMillimeters = 1000;
  if (self.horizontalAccuracy > 0) {
    return (long)(self.horizontalAccuracy * kMetersToMillimeters);
  }
  return -1L;
}

- (NSString*)cr_xGeoString {
  const std::string encoded_location_descriptor =
      LocationDescriptorAsWebSafeBase64([self cr_timestampInMicroseconds],
                                        [self cr_accuracyInMillimeters],
                                        floor(self.coordinate.latitude * 1e7),
                                        floor(self.coordinate.longitude * 1e7));

  // The "a" indicates that it is an ASCII proto.
  return base::SysUTF8ToNSString(
      base::StringPrintf("a %s", encoded_location_descriptor.c_str()));
}

@end
