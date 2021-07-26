// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_IDLE_TIME_PROVIDER_H_
#define CONTENT_PUBLIC_BROWSER_IDLE_TIME_PROVIDER_H_

#include "base/time/time.h"

namespace content {

// Provides an interface for querying a user's idle time and screen state.
class IdleTimeProvider {
 public:
  virtual ~IdleTimeProvider() = default;

  // See ui/base/idle/idle.h for the semantics of these methods.
  virtual base::TimeDelta CalculateIdleTime() = 0;
  virtual bool CheckIdleStateIsLocked() = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_IDLE_TIME_PROVIDER_H_
