// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
#define IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_

#import "components/keyed_service/core/keyed_service.h"

// Service that manages the "Level Up" feature, tracking user progress and
// stats.
class LevelUpService : public KeyedService {
 public:
  LevelUpService();
  ~LevelUpService() override;

  // Returns true if the user is opted in.
  bool IsOptedIn() const;

  // Opts the user in or out.
  void SetOptIn(bool opted_in);

  // Returns the current level of the user.
  int GetCurrentLevel() const;

  // KeyedService implementation.
  void Shutdown() override;

 private:
};

#endif  // IOS_CHROME_BROWSER_LEVEL_UP_MODEL_LEVEL_UP_SERVICE_H_
