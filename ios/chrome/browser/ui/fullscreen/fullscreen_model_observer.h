// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_OBSERVER_H_

#include <CoreGraphics/CoreGraphics.h>

#include "base/observer_list_types.h"

class FullscreenModel;

// Interface for listening to FullscreenModel changes.
class FullscreenModelObserver : public base::CheckedObserver {
 public:
  FullscreenModelObserver() = default;

  FullscreenModelObserver(const FullscreenModelObserver&) = delete;
  FullscreenModelObserver& operator=(const FullscreenModelObserver&) = delete;

  ~FullscreenModelObserver() override;

  // Invoked when `model`'s toolbar heights have been updated.
  virtual void FullscreenModelToolbarHeightsUpdated(FullscreenModel* model) {}

  // Invoked when `model`'s calculated progress() value is updated.
  virtual void FullscreenModelProgressUpdated(FullscreenModel* model) {}

  // Invoked when `model` is enabled or disabled.
  virtual void FullscreenModelEnabledStateChanged(FullscreenModel* model) {}

  // Invoked when a scroll event being tracked by `model` has started.
  virtual void FullscreenModelScrollEventStarted(FullscreenModel* model) {}

  // Invoked when a scroll event being tracked by `model` has ended.
  virtual void FullscreenModelScrollEventEnded(FullscreenModel* model) {}

  // Invoked when the model is reset.
  virtual void FullscreenModelWasReset(FullscreenModel* model) {}
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_MODEL_OBSERVER_H_
