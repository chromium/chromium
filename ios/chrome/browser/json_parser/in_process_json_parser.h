// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_JSON_PARSER_IN_PROCESS_JSON_PARSER_H_
#define IOS_CHROME_BROWSER_JSON_PARSER_IN_PROCESS_JSON_PARSER_H_

#include <memory>
#include <string>

#include "base/callback.h"

namespace base {
class Value;
}

// Mimics SafeJsonParser but parses in-process using base::JSONReader. This
// is potentially unsafe (if there are bug in the JSON parsing logic), but
// there is no way to parse in a separate process like on other platforms.
// This class should be used when code depends on SafeJsonParser-like API.
//
// If iOS ever partially adaopt Swift, pre-sanitization in that language,
// like Android version does), would be a better option. The support would
// have to be added to SafeJsonParser and this class removed.
class InProcessJsonParser {
 public:
  using SuccessCallback = base::OnceCallback<void(base::Value)>;
  using ErrorCallback = base::OnceCallback<void(const std::string&)>;

  // As with SafeJsonParser, runs either |success_callback| or |error_callback|
  // on the calling thread, but not before the call returns.
  static void Parse(const std::string& unsafe_json,
                    SuccessCallback success_callback,
                    ErrorCallback error_callback);

  InProcessJsonParser() = delete;
};

#endif  // IOS_CHROME_BROWSER_JSON_PARSER_IN_PROCESS_JSON_PARSER_H_
