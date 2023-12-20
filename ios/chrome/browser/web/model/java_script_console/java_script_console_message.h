// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_
#define IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_

#include "url/gurl.h"

@class NSString;

// Wraps information from a received console message.
struct JavaScriptConsoleMessage {
 public:
  JavaScriptConsoleMessage();
  ~JavaScriptConsoleMessage();

  // The url of the frame which sent the message. May be set to an invalid URL
  // if the URL was not available to the JavaScript error listener.
  GURL url;

  // The log level associated with the message. (From console.js, i.e. "log",
  // "debug", "info", "warn", "error")
  NSString* level;

  // The message contents.
  NSString* message;
};

#endif  // IOS_CHROME_BROWSER_WEB_MODEL_JAVA_SCRIPT_CONSOLE_JAVA_SCRIPT_CONSOLE_MESSAGE_H_
