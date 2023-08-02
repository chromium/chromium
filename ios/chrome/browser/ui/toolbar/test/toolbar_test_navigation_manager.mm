// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/test/toolbar_test_navigation_manager.h"

ToolbarTestNavigationManager::ToolbarTestNavigationManager()
    : can_go_back_(false), can_go_forward_(false) {}

bool ToolbarTestNavigationManager::CanGoBack() const {
  return can_go_back_;
}

bool ToolbarTestNavigationManager::CanGoForward() const {
  return can_go_forward_;
}

void ToolbarTestNavigationManager::set_can_go_back(bool can_go_back) {
  can_go_back_ = can_go_back;
}

void ToolbarTestNavigationManager::set_can_go_forward(bool can_go_forward) {
  can_go_forward_ = can_go_forward;
}
