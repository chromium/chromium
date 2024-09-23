// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_
#define IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace web {

// Struct to capture data about a user interaction. Records the time of the
// interaction and the main document URL at that time.
struct UserInteractionEvent {
  explicit UserInteractionEvent(const GURL& url)
      : main_document_url(url), time(base::TimeTicks::Now()) {}

  // Main document URL at the time the interaction occurred.
  GURL main_document_url;
  // Time that the interaction occurred.
  base::TimeTicks time;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_USER_INTERACTION_EVENT_H_
