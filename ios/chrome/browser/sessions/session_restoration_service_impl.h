// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_

#include "ios/chrome/browser/sessions/session_restoration_service.h"

class SessionRestorationServiceImpl final : public SessionRestorationService {
 public:
  SessionRestorationServiceImpl();
  ~SessionRestorationServiceImpl() final;

  // KeyedService implementation.
  void Shutdown() final;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_
