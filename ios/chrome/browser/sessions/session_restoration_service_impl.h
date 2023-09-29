// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_

#include "base/observer_list.h"
#include "ios/chrome/browser/sessions/session_restoration_observer.h"
#include "ios/chrome/browser/sessions/session_restoration_service.h"

class SessionRestorationServiceImpl final : public SessionRestorationService {
 public:
  SessionRestorationServiceImpl();
  ~SessionRestorationServiceImpl() final;

  // KeyedService implementation.
  void Shutdown() final;

  // SessionRestorationService implementation.
  void AddObserver(SessionRestorationObserver* observer) final;
  void RemoveObserver(SessionRestorationObserver* observer) final;

 private:
  // Observer list.
  base::ObserverList<SessionRestorationObserver, true> observers_;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_IMPL_H_
