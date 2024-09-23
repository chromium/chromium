// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/stub_render_widget_host_owner_delegate.h"

#include "third_party/blink/public/common/web_preferences/web_preferences.h"

namespace content {

bool StubRenderWidgetHostOwnerDelegate::MayRenderWidgetForwardKeyboardEvent(
    const input::NativeWebKeyboardEvent& key_event) {
  return true;
}

bool StubRenderWidgetHostOwnerDelegate::ShouldContributePriorityToProcess() {
  return false;
}

bool StubRenderWidgetHostOwnerDelegate::IsMainFrameActive() {
  return true;
}

bool StubRenderWidgetHostOwnerDelegate::IsNeverComposited() {
  return false;
}

blink::web_pref::WebPreferences
StubRenderWidgetHostOwnerDelegate::GetWebkitPreferencesForWidget() {
  return {};
}

}  // namespace content
