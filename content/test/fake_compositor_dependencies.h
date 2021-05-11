// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "content/renderer/compositor/compositor_dependencies.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

class FakeCompositorDependencies : public CompositorDependencies {
 public:
  FakeCompositorDependencies();
  ~FakeCompositorDependencies() override;

  // CompositorDependencies implementation.
  bool IsUseZoomForDSFEnabled() override;

  void set_use_zoom_for_dsf_enabled(bool enabled) {
    use_zoom_for_dsf_ = enabled;
  }

 private:
  bool use_zoom_for_dsf_ = false;

  DISALLOW_COPY_AND_ASSIGN(FakeCompositorDependencies);
};

}  // namespace content

#endif  // CONTENT_TEST_FAKE_COMPOSITOR_DEPENDENCIES_H_
