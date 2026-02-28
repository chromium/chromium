// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_DETAILS_H_
#define IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_DETAILS_H_

#import <map>
#import <string>

#import "url/gurl.h"

// Wraps information about an error.
struct ScriptErrorDetails {
  //  public:
  ScriptErrorDetails(bool is_main_frame);
  ~ScriptErrorDetails();
  ScriptErrorDetails(const ScriptErrorDetails& other);
  ScriptErrorDetails(ScriptErrorDetails&& other);
  ScriptErrorDetails& operator=(ScriptErrorDetails&& other);

  // The gCrWeb api name associated with this error.
  std::string api;

  // The line number at which the error occurred.
  int line_number = 0;

  // The error message.
  std::string message;

  // The error stack.
  std::string stack;

  // The url where the error occurred.
  GURL url;

  // Whether or not this error occurred in the main frame.
  bool is_main_frame;

  // The crash keys set in the JavaScript code before this error occurred.
  std::map<std::string, std::string> crash_keys;
};

#endif  // IOS_WEB_JS_FEATURES_WINDOW_ERROR_SCRIPT_ERROR_DETAILS_H_
