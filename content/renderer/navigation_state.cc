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
  // If `commit_same_document_callback_` was set but hasn't run yet, run it now.
  // This will only happen if the navigation is being aborted before commit,
  // either due to frame detach or a new navigation preempting this one.
  // If `commit_same_document_callback_` was already run, this will not do
  // anything.
  RunCommitSameDocumentNavigationCallback(blink::mojom::CommitResult::Aborted);
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateForCrossDocumentCommit(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    bool was_initiated_in_this_frame) {
  return base::WrapUnique(new NavigationState(
      std::move(common_params), std::move(commit_params),
      /*is_for_synchronous_commit=*/false, std::move(commit_callback),
      std::move(navigation_client),
      mojom::Frame::CommitSameDocumentNavigationCallback(),
      was_initiated_in_this_frame));
}

std::unique_ptr<NavigationState>
NavigationState::CreateForSameDocumentCommitFromBrowser(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    mojom::Frame::CommitSameDocumentNavigationCallback
        commit_same_document_callback) {
  // This is a same-document navigation coming from the browser process (as
  // opposed to a fragment link click, which would have gone through
  // CreateForSynchronousCommit()), therefore |was_initiated_in_this_frame| must
  // be false.
  return base::WrapUnique(
      new NavigationState(std::move(common_params), std::move(commit_params),
                          /*is_for_synchronous_commit=*/false,
                          mojom::NavigationClient::CommitNavigationCallback(),
                          nullptr, std::move(commit_same_document_callback),
                          false /* was_initiated_in_this_frame */));
}

// static
std::unique_ptr<NavigationState> NavigationState::CreateForSynchronousCommit() {
  return base::WrapUnique(new NavigationState(
      blink::CreateCommonNavigationParams(),
      blink::CreateCommitNavigationParams(),
      /*is_for_synchronous_commit=*/true,
      content::mojom::NavigationClient::CommitNavigationCallback(),
      /*navigation_client=*/nullptr,
      mojom::Frame::CommitSameDocumentNavigationCallback(),
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

void NavigationState::RunCommitSameDocumentNavigationCallback(
    blink::mojom::CommitResult commit_result) {
  if (commit_same_document_callback_) {
    std::move(commit_same_document_callback_).Run(commit_result);
  }
}

NavigationState::NavigationState(
    blink::mojom::CommonNavigationParamsPtr common_params,
    blink::mojom::CommitNavigationParamsPtr commit_params,
    bool is_for_synchronous_commit,
    mojom::NavigationClient::CommitNavigationCallback commit_callback,
    std::unique_ptr<NavigationClient> navigation_client,
    mojom::Frame::CommitSameDocumentNavigationCallback
        commit_same_document_callback,
    bool was_initiated_in_this_frame)
    : commit_start_time_(base::TimeTicks::Now()),
      was_within_same_document_(false),
      was_initiated_in_this_frame_(was_initiated_in_this_frame),
      is_for_synchronous_commit_(is_for_synchronous_commit),
      common_params_(std::move(common_params)),
      commit_params_(std::move(commit_params)),
      navigation_client_(std::move(navigation_client)),
      commit_callback_(std::move(commit_callback)),
      commit_same_document_callback_(std::move(commit_same_document_callback)) {
}
}  // namespace content
