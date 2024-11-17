// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_

#import <Foundation/Foundation.h>

#include "base/logging.h"

@protocol UIBlockerManager;
@protocol UIBlockerTarget;

enum class UIBlockerExtent {
  kProfile,
  kApplication,
};

// A helper object that increments AppState's or ProfileState's blocking UI
// counter for its entire lifetime.
class ScopedUIBlocker {
 public:
  // Set `extent` to UIBlockerExtent::kApplication if the entire app should be
  // block and not only profile related scenes.
  explicit ScopedUIBlocker(id<UIBlockerTarget> target,
                           UIBlockerExtent extent = UIBlockerExtent::kProfile);
  ~ScopedUIBlocker();

 private:
  // The target showing the blocking UI.
  __weak id<UIBlockerTarget> target_;
  __weak id<UIBlockerManager> uiBlockerManager_;

  ScopedUIBlocker(const ScopedUIBlocker&) = delete;
  ScopedUIBlocker& operator=(const ScopedUIBlocker&) = delete;
};

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_
