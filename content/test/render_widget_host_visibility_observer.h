// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RENDER_WIDGET_HOST_VISIBILITY_OBSERVER_H_
#define CONTENT_TEST_RENDER_WIDGET_HOST_VISIBILITY_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_widget_host_observer.h"

#include "base/scoped_observation.h"
#include "content/browser/renderer_host/cross_process_frame_connector.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/test/test_utils.h"

namespace content {

using CrashVisibility = CrossProcessFrameConnector::CrashVisibility;

class RenderWidgetHostVisibilityObserver : public RenderWidgetHostObserver {
 public:
  explicit RenderWidgetHostVisibilityObserver(RenderWidgetHostImpl* rwhi,
                                              bool expected_visibility_state);
  ~RenderWidgetHostVisibilityObserver() override;

  RenderWidgetHostVisibilityObserver(
      const RenderWidgetHostVisibilityObserver&) = delete;
  RenderWidgetHostVisibilityObserver& operator=(
      const RenderWidgetHostVisibilityObserver&) = delete;

  bool WaitUntilSatisfied();

 private:
  // RenderWidgetHostObserver implementation.
  void RenderWidgetHostVisibilityChanged(RenderWidgetHost* widget_host,
                                         bool became_visible) override;
  void RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) override;

  bool expected_visibility_state_;
  base::RunLoop run_loop_;
  base::ScopedObservation<RenderWidgetHost, RenderWidgetHostObserver>
      observation_{this};
  bool was_observed_;
  bool did_fail_;
  raw_ptr<RenderWidgetHost, AcrossTasksDanglingUntriaged> render_widget_;
};

}  // namespace content

#endif  // CONTENT_TEST_RENDER_WIDGET_HOST_VISIBILITY_OBSERVER_H_
