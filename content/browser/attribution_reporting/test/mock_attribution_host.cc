// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/test/mock_attribution_host.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/attribution_reporting/attribution_host.h"
#include "content/public/browser/web_contents.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"
#endif

namespace content {

// static
MockAttributionHost* MockAttributionHost::Override(WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  auto* old_host = AttributionHost::FromWebContents(web_contents);
  if (auto* input_event_tracker = old_host->input_event_tracker()) {
    input_event_tracker->RemoveObserverForTesting(web_contents);
  }
#endif
  auto host = base::WrapUnique(new MockAttributionHost(web_contents));
  auto* raw = host.get();
  web_contents->SetUserData(AttributionHost::UserDataKey(), std::move(host));
  return raw;
}

MockAttributionHost::MockAttributionHost(WebContents* web_contents)
    : AttributionHost(web_contents) {}

MockAttributionHost::~MockAttributionHost() = default;

}  // namespace content
