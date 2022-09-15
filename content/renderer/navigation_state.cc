// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/navigation_state.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "content/common/frame_messages.mojom.h"
#include "third_party/blink/public/common/navigation/navigation_params.h"
#include "third_party/blink/public/mojom/commit_result/commit_result.mojom.h"

namespace content {

NavigationState::~NavigationState() {
  navigation_client_.reset();
}

// static
std::unique_ptr<NavigationState> NavigationState::Create(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    bool was_initiated_in_this_frame) {
  return base::WrapUnique(new NavigationState(
      std::move(common_params), std::move(commit_params),
      /*is_for_synchronous_commit=*/false, std::move(commit_callback),
      std::move(navigation_client), was_initiated_in_this_frame));
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateForSynchronousCommit() {
  return base::WrapUnique(new NavigationState(
      blink::CreateCommonNavigationParams(),
      blink::CreateCommitNavigationParams(),
      /*is_for_synchronous_commit=*/true,
      content::mojom::NavigationClient::CommitNavigationCallback(),
      /*navigation_client=*/nullptr,
      /*was_initiated_in_this_frame=*/true));
}

bool NavigationState::WasWithinSameDocument() {
  return was_within_same_document_;
}

bool NavigationState::IsForSynchronousCommit() {
  return is_for_synchronous_commit_;
}

void NavigationState::RunCommitNavigationCallback(
    mojom::DidCommitProvisionalLoadParamsPtr params,
    mojom::DidCommitProvisionalLoadInterfaceParamsPtr interface_params) {
  if (commit_callback_) {
    std::move(commit_callback_)
        .Run(std::move(params), std::move(interface_params));
  }
  navigation_client_.reset();
}

NavigationState::NavigationState(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool is_for_synchronous_commit,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    bool was_initiated_in_this_frame)
    : was_within_same_document_(false),
      was_initiated_in_this_frame_(was_initiated_in_this_frame),
      is_for_synchronous_commit_(is_for_synchronous_commit),
      common_params_(std::move(common_params)),
      commit_params_(std::move(commit_params)),
      navigation_client_(std::move(navigation_client)),
      commit_callback_(std::move(commit_callback)) {}
}  // namespace content
