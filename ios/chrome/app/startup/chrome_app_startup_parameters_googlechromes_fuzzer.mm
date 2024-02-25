// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/startup/chrome_app_startup_parameters.h"

#import "base/at_exit.h"
#import "base/i18n/icu_util.h"

struct Environment {
  Environment() { CHECK(base::i18n::InitializeICU()); }
  base::AtExitManager at_exit;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Environment env;

  std::string str = std::string(reinterpret_cast<const char*>(data), size);

  NSString* formatted_string =
      [NSString stringWithFormat:@"googlechromes://%s", str.c_str()];
  NSURL* url = [NSURL URLWithString:formatted_string];
  [ChromeAppStartupParameters startupParametersWithURL:url
                                     sourceApplication:nil];

  return 0;
}
