// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SCOPED_UI_BLOCKER_UI_BUNDLED_SCOPED_UI_BLOCKER_H_
#define IOS_CHROME_BROWSER_SCOPED_UI_BLOCKER_UI_BUNDLED_SCOPED_UI_BLOCKER_H_

#import <Foundation/Foundation.h>

#import <memory>

#import "base/types/pass_key.h"

@class SceneState;
@class AppState;
@protocol UIBlockerManager;
@protocol UIBlockerTarget;

// A helper object that increments AppState's or ProfileState's blocking UI
// counter for its entire lifetime.
class ScopedUIBlocker {
 public:
  // Used to make the constructor private.
  using PassKey = base::PassKey<ScopedUIBlocker>;

  // Block the UI with given `target` and `manager`.
  ScopedUIBlocker(PassKey,
                  id<UIBlockerTarget> target,
                  id<UIBlockerManager> manager);

  // ScopedUIBlocker is a non-copyable, non-moveable class.
  ScopedUIBlocker(const ScopedUIBlocker&) = delete;
  ScopedUIBlocker& operator=(const ScopedUIBlocker&) = delete;

  ~ScopedUIBlocker();

  // Constructs a ProfileState scoped blocking UI.
  static std::unique_ptr<ScopedUIBlocker> ProfileScoped(SceneState* scene);

  // Constructs an AppState scoped blocking UI.
  static std::unique_ptr<ScopedUIBlocker> AppScoped(SceneState* scene,
                                                    AppState* app_state);

 private:
  // The target showing the blocking UI.
  __weak id<UIBlockerTarget> target_;
  __weak id<UIBlockerManager> manager_;
};

#endif  // IOS_CHROME_BROWSER_SCOPED_UI_BLOCKER_UI_BUNDLED_SCOPED_UI_BLOCKER_H_
