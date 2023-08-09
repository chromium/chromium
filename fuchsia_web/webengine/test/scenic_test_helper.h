// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_TEST_SCENIC_TEST_HELPER_H_
#define FUCHSIA_WEB_WEBENGINE_TEST_SCENIC_TEST_HELPER_H_

#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "fuchsia_web/webengine/browser/frame_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

// Helpers for browsertests that need to create Scenic Views.
class ScenicTestHelper {
 public:
  ScenicTestHelper();
  ~ScenicTestHelper();

  ScenicTestHelper(const ScenicTestHelper&) = delete;
  ScenicTestHelper& operator=(const ScenicTestHelper&) = delete;

  // Simulate the creation of a Scenic View, bypassing the creation of a Scenic
  // PlatformWindow.
  void CreateScenicView(FrameImpl* frame_impl, fuchsia::web::FramePtr& frame);

  // Prepare the view for interaction by setting its focus state and size.
  void SetUpViewForInteraction(content::WebContents* web_contents);

  fuchsia::ui::views::ViewRef CloneViewRef();

 protected:
  FrameImpl* frame_impl_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_TEST_SCENIC_TEST_HELPER_H_
