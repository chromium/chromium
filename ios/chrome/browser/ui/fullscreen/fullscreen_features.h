// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_

#include "components/flags_ui/feature_entry.h"

namespace fullscreen {
namespace features {

// The name of the command line switch used to control the method by which the
// viewport of the content area is updated by scrolling events.
extern const char kViewportAdjustmentExperimentCommandLineSwitch[];

// The available viewport adjustment experiments.  The choices in this array
// correspond with the ViewportAdjustmentExperiment values.
extern const flags_ui::FeatureEntry::Choice
    kViewportAdjustmentExperimentChoices[6];

// Enum type describing viewport adjustment experiments.
enum class ViewportAdjustmentExperiment : short {
  FRAME = 0,      // Adjust the viewport by resizing the entire WKWebView.
  CONTENT_INSET,  // Adjust the viewport by updating the WKWebView's scroll view
                  // contentInset.
  SAFE_AREA,  // Adjust the viewport by updating the safe area of the browser
              // container view.
  HYBRID,  // Translates the web view up and down and updates the viewport using
           // safe area insets.
  SMOOTH_SCROLLING,  // Adjusts the viewport using the smooth scrolling
                     // workaround.
};

// Convenience method for retrieving the active viewport adjustment experiment
// from the command line.
ViewportAdjustmentExperiment GetActiveViewportExperiment();

}  // namespace features
}  // namespace fullscreen

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_FEATURES_H_
