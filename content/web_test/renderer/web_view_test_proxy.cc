// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_view_test_proxy.h"

#include <stddef.h>
#include <stdint.h>

#include "content/web_test/common/web_test_string_util.h"
#include "content/web_test/renderer/test_runner.h"
#include "content/web_test/renderer/web_frame_test_proxy.h"
#include "third_party/blink/public/mojom/v8_cache_options.mojom.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

WebViewTestProxy::WebViewTestProxy(AgentSchedulingGroup& agent_scheduling_group,
                                   CompositorDependencies* compositor_deps,
                                   const mojom::CreateViewParams& params)
    : RenderViewImpl(agent_scheduling_group, compositor_deps, params) {}

WebViewTestProxy::~WebViewTestProxy() = default;

}  // namespace content
