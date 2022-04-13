// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_page_broadcast.h"

namespace content {

TestPageBroadcast::TestPageBroadcast(
    mojo::PendingAssociatedReceiver<blink::mojom::PageBroadcast> receiver)
    : receiver_(this, std::move(receiver)) {}

TestPageBroadcast::~TestPageBroadcast() = default;

// The user should add functionality as needed.

void TestPageBroadcast::SetPageLifecycleState(
    blink::mojom::PageLifecycleStatePtr state,
    blink::mojom::PageRestoreParamsPtr page_restore_params,
    SetPageLifecycleStateCallback callback) {
  std::move(callback).Run();
}

void TestPageBroadcast::AudioStateChanged(bool is_audio_playing) {}

void TestPageBroadcast::ActivatePrerenderedPage(
    blink::mojom::PrerenderPageActivationParamsPtr
        prerender_page_activation_params,
    ActivatePrerenderedPageCallback callback) {
  std::move(callback).Run();
}

void TestPageBroadcast::SetInsidePortal(bool is_inside_portal) {}

void TestPageBroadcast::UpdateWebPreferences(
    const blink::web_pref::WebPreferences& preferences) {}

void TestPageBroadcast::UpdateRendererPreferences(
    const blink::RendererPreferences& preferences) {}

void TestPageBroadcast::SetHistoryOffsetAndLength(int32_t history_offset,
                                                  int32_t history_length) {}

void TestPageBroadcast::SetPageBaseBackgroundColor(
    absl::optional<SkColor> color) {}

}  // namespace content
