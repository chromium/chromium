// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
#define IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_

#include "base/metrics/field_trial.h"
#include "ios/web/public/init/web_main_parts.h"

namespace ios_web_view {

// WebView implementation of WebMainParts.
class WebViewWebMainParts : public web::WebMainParts {
 public:
  WebViewWebMainParts();

  WebViewWebMainParts(const WebViewWebMainParts&) = delete;
  WebViewWebMainParts& operator=(const WebViewWebMainParts&) = delete;

  ~WebViewWebMainParts() override;

 private:
  // web::WebMainParts implementation.
  void PreCreateMainMessageLoop() override;
  void PreCreateThreads() override;
  void PostCreateThreads() override;
  void PreMainMessageLoopRun() override;
  void PostMainMessageLoopRun() override;
  void PostDestroyThreads() override;

  // Loads resources that are not scaled. f.e. javascript files.
  void LoadNonScalableResources();
  // Loads resources that can be scaled. f.e. png images for @1x, @2x, and @3x.
  void LoadScalableResources();

  // Dummy FieldTrialList instance for code that consumes variations data,
  // although ios WebView does not support variations.
  base::FieldTrialList field_trial_list_;
};

}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_WEB_VIEW_WEB_MAIN_PARTS_H_
