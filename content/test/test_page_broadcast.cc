// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_page_broadcast.h"

#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_replication_state.mojom.h"
#include "third_party/blink/public/mojom/frame/remote_frame.mojom.h"

namespace content {

TestPageBroadcast::TestPageBroadcast(
    mojo::PendingAssociatedReceiver<blink::mojom::PageBroadcast> receiver)
    : receiver_(this, std::move(receiver)) {}

TestPageBroadcast::~TestPageBroadcast() = default;

void TestPageBroadcast::FlushForTesting() {
  receiver_.FlushForTesting();
}

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

void TestPageBroadcast::UpdateWebPreferences(
    const blink::web_pref::WebPreferences& preferences) {}

void TestPageBroadcast::UpdateRendererPreferences(
    const blink::RendererPreferences& preferences) {}

void TestPageBroadcast::SetHistoryOffsetAndLength(int32_t history_offset,
                                                  int32_t history_length) {}

void TestPageBroadcast::SetPageBaseBackgroundColor(
    std::optional<SkColor> color) {}

void TestPageBroadcast::CreateRemoteMainFrame(
    const blink::RemoteFrameToken& token,
    const std::optional<blink::FrameToken>& opener_frame_token,
    blink::mojom::FrameReplicationStatePtr replication_state,
    bool is_loading,
    const base::UnguessableToken& devtools_frame_token,
    blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
    blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces) {}

void TestPageBroadcast::UpdatePageBrowsingContextGroup(
    const blink::BrowsingContextGroupInfo& browsing_context_group_info) {}

void TestPageBroadcast::SetPageAttributionSupport(
    network::mojom::AttributionSupport support) {}

void TestPageBroadcast::UpdateColorProviders(
    const blink::ColorProviderColorMaps& color_provider_colors) {}

}  // namespace content
