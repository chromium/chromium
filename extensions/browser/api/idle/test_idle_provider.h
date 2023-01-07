// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_IDLE_TEST_IDLE_PROVIDER_H_
#define EXTENSIONS_BROWSER_API_IDLE_TEST_IDLE_PROVIDER_H_

#include "extensions/browser/api/idle/idle_manager.h"

#include "ui/base/idle/idle.h"

namespace extensions {

class TestIdleProvider : public IdleManager::IdleTimeProvider {
 public:
  TestIdleProvider();
  ~TestIdleProvider() override;
  ui::IdleState CalculateIdleState(int idle_threshold) override;
  int CalculateIdleTime() override;
  bool CheckIdleStateIsLocked() override;

  void set_idle_time(int idle_time) { idle_time_ = idle_time; }
  void set_locked(bool locked) { locked_ = locked; }

 private:
  int idle_time_ = 0;
  bool locked_ = false;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_IDLE_TEST_IDLE_PROVIDER_H_
