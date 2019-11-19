// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_state.h"

#include <memory>
#include <utility>

#include "content/common/frame_messages.h"
#include "content/public/common/navigation_policy.h"
#include "content/renderer/internal_document_state_data.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"

namespace content {

NavigationState::~NavigationState() {
  RunCommitNavigationCallback(blink::mojom::CommitResult::Aborted);
  navigation_client_.reset();
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateBrowserInitiated(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    mojom::FrameNavigationControl::CommitNavigationCallback callback,
    mojom::NavigationClient::CommitNavigationCallback
        per_navigation_mojo_interface_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    bool was_initiated_in_this_frame) {
  return base::WrapUnique(new NavigationState(
      std::move(common_params), std::move(commit_params), false,
      std::move(callback), std::move(per_navigation_mojo_interface_callback),
      std::move(navigation_client), was_initiated_in_this_frame));
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateContentInitiated() {
  return base::WrapUnique(new NavigationState(
      CreateCommonNavigationParams(), CreateCommitNavigationParams(), true,
      content::mojom::FrameNavigationControl::CommitNavigationCallback(),
      content::mojom::NavigationClient::CommitNavigationCallback(), nullptr,
      true));
}

// static
NavigationState* NavigationState::FromDocumentLoader(
    blink::WebDocumentLoader* document_loader) {
  return InternalDocumentStateData::FromDocumentLoader(document_loader)
      ->navigation_state();
}

bool NavigationState::WasWithinSameDocument() {
  return was_within_same_document_;
}

bool NavigationState::IsContentInitiated() {
  return is_content_initiated_;
}

void NavigationState::RunCommitNavigationCallback(
    blink::mojom::CommitResult result) {
  if (commit_callback_)
    std::move(commit_callback_).Run(result);
}

void NavigationState::RunPerNavigationInterfaceCommitNavigationCallback(
    std::unique_ptr<::FrameHostMsg_DidCommitProvisionalLoad_Params> params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (per_navigation_mojo_interface_commit_callback_)
    std::move(per_navigation_mojo_interface_commit_callback_)
        .Run(std::move(params), std::move(interface_params));
  navigation_client_.reset();
}

NavigationState::NavigationState(
    mojom::CommonNavigationParamsPtr common_params,
    mojom::CommitNavigationParamsPtr commit_params,
    bool is_content_initiated,
    mojom::FrameNavigationControl::CommitNavigationCallback callback,
    mojom::NavigationClient::CommitNavigationCallback
        per_navigation_mojo_interface_commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    bool was_initiated_in_this_frame)
    : was_within_same_document_(false),
      was_initiated_in_this_frame_(was_initiated_in_this_frame),
      is_content_initiated_(is_content_initiated),
      common_params_(std::move(common_params)),
      commit_params_(std::move(commit_params)),
      navigation_client_(std::move(navigation_client)),
      commit_callback_(std::move(callback)),
      per_navigation_mojo_interface_commit_callback_(
          std::move(per_navigation_mojo_interface_commit_callback)) {}
}  // namespace content
