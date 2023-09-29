// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_H_
#define IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

class SessionRestorationObserver;

// Service responsible for session saving and restoration.
//
// This service is only used when the optimized session restoration
// feature (web::features::kEnableSessionSerializationOptimizations)
// is enabled.
//
// TODO(crbug.com/1383087): Update this comment once launched.
class SessionRestorationService : public KeyedService {
 public:
  SessionRestorationService() = default;
  ~SessionRestorationService() override = default;

  // Registers `observer` to be notified about session restoration events.
  virtual void AddObserver(SessionRestorationObserver* observer) = 0;

  // Unregisters `observer` from the list of observers to notify about
  // session restoration events.
  virtual void RemoveObserver(SessionRestorationObserver* observer) = 0;
};

#endif  // IOS_CHROME_BROWSER_SESSIONS_SESSION_RESTORATION_SERVICE_H_
