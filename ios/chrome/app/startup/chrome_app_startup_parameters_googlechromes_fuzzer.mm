// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string str = std::string(reinterpret_cast<const char*>(data), size);

  NSString* formatted_string =
      [NSString stringWithFormat:@"googlechromes://%s", str.c_str()];
  NSURL* url = [NSURL URLWithString:formatted_string];
  [ChromeAppStartupParameters startupParametersWithURL:url
                                     sourceApplication:nil];

  return 0;
}
