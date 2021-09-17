// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_DISPLAY_FEATURE_H_
#define CONTENT_TEST_MOCK_DISPLAY_FEATURE_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"

#if defined(OS_ANDROID)
#include <memory>
#endif

namespace content {

struct DisplayFeature;
class RenderWidgetHostViewBase;

#if defined(OS_ANDROID)
class TestViewAndroidDelegate;
#endif  // defined(OS_ANDROID)

class MockDisplayFeature {
 public:
  explicit MockDisplayFeature(RenderWidgetHostViewBase* rwhv);
  ~MockDisplayFeature();

  void SetDisplayFeature(const DisplayFeature* display_feature);

 private:
  raw_ptr<RenderWidgetHostViewBase> render_widget_host_view_;
#if defined(OS_ANDROID)
  std::unique_ptr<TestViewAndroidDelegate> test_view_android_delegate_;
#endif  // defined(OS_ANDROID)
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_DISPLAY_FEATURE_H_
