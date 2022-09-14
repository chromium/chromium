// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_
#define IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_

#import <Foundation/Foundation.h>

#include "base/logging.h"

@protocol UIBlockerManager;
@protocol UIBlockerTarget;

// A helper object that increments AppState's blocking UI counter for
// its entire lifetime.
class ScopedUIBlocker {
 public:
  explicit ScopedUIBlocker(id<UIBlockerTarget> target);
  ~ScopedUIBlocker();

 private:
  // The target showing the blocking UI.
  __weak id<UIBlockerTarget> target_;

  ScopedUIBlocker(const ScopedUIBlocker&) = delete;
  ScopedUIBlocker& operator=(const ScopedUIBlocker&) = delete;
};

#endif  // IOS_CHROME_BROWSER_UI_SCOPED_UI_BLOCKER_SCOPED_UI_BLOCKER_H_
