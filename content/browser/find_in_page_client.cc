// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/find_in_page_client.h"

#include <utility>

#include "content/browser/find_request_manager.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {

FindInPageClient::FindInPageClient(FindRequestManager* find_request_manager,
                                   RenderFrameHostImpl* rfh)
    : frame_(rfh), find_request_manager_(find_request_manager) {
  frame_->GetFindInPage()->SetClient(receiver_.BindNewPipeAndPassRemote());
}

FindInPageClient::~FindInPageClient() {}

void FindInPageClient::SetNumberOfMatches(
    int request_id,
    unsigned int number_of_matches,
    blink::mojom::FindMatchUpdateType update_type) {
  if (find_request_manager_->ShouldIgnoreReply(frame_, request_id))
    return;
  const int old_matches = number_of_matches_;
  number_of_matches_ = number_of_matches;
  find_request_manager_->UpdatedFrameNumberOfMatches(frame_, old_matches,
                                                     number_of_matches);
  HandleUpdateType(request_id, update_type);
}

void FindInPageClient::SetActiveMatch(
    int request_id,
    const gfx::Rect& active_match_rect,
    int active_match_ordinal,
    blink::mojom::FindMatchUpdateType update_type) {
  if (find_request_manager_->ShouldIgnoreReply(frame_, request_id))
    return;
  find_request_manager_->SetActiveMatchRect(active_match_rect);
  find_request_manager_->SetActiveMatchOrdinal(frame_, request_id,
                                               active_match_ordinal);
  HandleUpdateType(request_id, update_type);
}

#if BUILDFLAG(IS_ANDROID)
void FindInPageClient::ActivateNearestFindResult(int request_id,
                                                 const gfx::PointF& point) {
  frame_->GetFindInPage()->ActivateNearestFindResult(request_id, point);
}
#endif

void FindInPageClient::HandleUpdateType(
    int request_id,
    blink::mojom::FindMatchUpdateType update_type) {
  // If this is the final update for this frame, it might be the final update
  // for the find request out of all the frames, so we need to handle it.
  // Otherwise just notify directly while saying this is not the final update
  // for the request.
  if (update_type == blink::mojom::FindMatchUpdateType::kFinalUpdate)
    find_request_manager_->HandleFinalUpdateForFrame(frame_, request_id);
  else
    find_request_manager_->NotifyFindReply(request_id,
                                           false /* final_update */);
}

}  // namespace content
