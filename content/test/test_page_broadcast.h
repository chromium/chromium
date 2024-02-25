// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_PAGE_BROADCAST_H_
#define CONTENT_TEST_TEST_PAGE_BROADCAST_H_

#include "third_party/blink/public/mojom/page/page.mojom.h"

#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace content {

class TestPageBroadcast : public blink::mojom::PageBroadcast {
 public:
  explicit TestPageBroadcast(
      mojo::PendingAssociatedReceiver<blink::mojom::PageBroadcast> receiver);
  ~TestPageBroadcast() override;

  void FlushForTesting();

 private:
  void SetPageLifecycleState(
      blink::mojom::PageLifecycleStatePtr state,
      blink::mojom::PageRestoreParamsPtr page_restore_params,
      SetPageLifecycleStateCallback callback) override;
  void AudioStateChanged(bool is_audio_playing) override;
  void ActivatePrerenderedPage(blink::mojom::PrerenderPageActivationParamsPtr
                                   prerender_page_activation_params,
                               ActivatePrerenderedPageCallback) override;
  void UpdateWebPreferences(
      const blink::web_pref::WebPreferences& preferences) override;
  void UpdateRendererPreferences(
      const blink::RendererPreferences& preferences) override;
  void SetHistoryOffsetAndLength(int32_t history_offset,
                                 int32_t history_length) override;
  void SetPageBaseBackgroundColor(std::optional<SkColor> color) override;
  void CreateRemoteMainFrame(
      const blink::RemoteFrameToken& token,
      const std::optional<blink::FrameToken>& opener_frame_token,
      blink::mojom::FrameReplicationStatePtr replication_state,
      bool is_loading,
      const base::UnguessableToken& devtools_frame_token,
      blink::mojom::RemoteFrameInterfacesFromBrowserPtr remote_frame_interfaces,
      blink::mojom::RemoteMainFrameInterfacesPtr remote_main_frame_interfaces)
      override;
  void UpdatePageBrowsingContextGroup(const blink::BrowsingContextGroupInfo&
                                          browsing_context_group_info) override;
  void SetPageAttributionSupport(
      network::mojom::AttributionSupport support) override;
  void UpdateColorProviders(
      const blink::ColorProviderColorMaps& color_provider_colors) override;

  mojo::AssociatedReceiver<blink::mojom::PageBroadcast> receiver_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_PAGE_BROADCAST_H_
