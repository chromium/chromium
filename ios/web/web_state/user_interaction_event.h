// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_
#define IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_

#import <CoreFoundation/CFDate.h>

#include "url/gurl.h"

namespace web {

// Struct to capture data about a user interaction. Records the time of the
// interaction and the main document URL at that time.
struct UserInteractionEvent {
  explicit UserInteractionEvent(const GURL& url)
      : main_document_url(url), time(CFAbsoluteTimeGetCurrent()) {}
  // Main document URL at the time the interaction occurred.
  GURL main_document_url;
  // Time that the interaction occurred, measured in seconds since Jan 1 2001.
  CFAbsoluteTime time;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_
