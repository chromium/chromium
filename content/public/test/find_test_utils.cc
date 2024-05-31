// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/find_test_utils.h"
#include "base/memory/raw_ptr.h"

#include "build/build_config.h"
#include "content/browser/find_request_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const int kInvalidId = -1;

}  // namespace

FindResults::FindResults(int request_id,
                         int number_of_matches,
                         int active_match_ordinal)
    : request_id(request_id),
      number_of_matches(number_of_matches),
      active_match_ordinal(active_match_ordinal) {}

FindResults::FindResults() : FindResults(kInvalidId, 0, 0) {}

FindTestWebContentsDelegate::FindTestWebContentsDelegate()
    : last_request_id_(kInvalidId),
      last_finished_request_id_(kInvalidId),
      next_reply_received_(false),
      record_replies_(false),
      waiting_for_(NOTHING) {}

FindTestWebContentsDelegate::~FindTestWebContentsDelegate() {}

const FindResults& FindTestWebContentsDelegate::GetFindResults() const {
  return current_results_;
}

void FindTestWebContentsDelegate::WaitForFinalReply() {
  if (last_finished_request_id_ >= last_request_id_)
    return;

  WaitFor(FINAL_REPLY);
}

void FindTestWebContentsDelegate::WaitForNextReply() {
  if (next_reply_received_)
    return;

  WaitFor(NEXT_REPLY);
}

void FindTestWebContentsDelegate::MarkNextReply() {
  next_reply_received_ = false;
}

void FindTestWebContentsDelegate::UpdateLastRequest(int request_id) {
  last_request_id_ = request_id;
}

void FindTestWebContentsDelegate::StartReplyRecord() {
  reply_record_.clear();
  record_replies_ = true;
}

const std::vector<FindResults>& FindTestWebContentsDelegate::GetReplyRecord() {
  record_replies_ = false;
  return reply_record_;
}

#if BUILDFLAG(IS_ANDROID)
void FindTestWebContentsDelegate::WaitForMatchRects() {
  WaitFor(MATCH_RECTS);
}
#endif

void FindTestWebContentsDelegate::FindReply(WebContents* web_contents,
                                            int request_id,
                                            int number_of_matches,
                                            const gfx::Rect& selection_rect,
                                            int active_match_ordinal,
                                            bool final_update) {
  if (record_replies_) {
    reply_record_.emplace_back(
        request_id, number_of_matches, active_match_ordinal);
  }

  // Update the current results.
  if (request_id > current_results_.request_id)
    current_results_.request_id = request_id;
  if (number_of_matches != -1)
    current_results_.number_of_matches = number_of_matches;
  if (active_match_ordinal != -1)
    current_results_.active_match_ordinal = active_match_ordinal;

  if (!final_update)
    return;

  if (request_id > last_finished_request_id_)
    last_finished_request_id_ = request_id;
  next_reply_received_ = true;

  // If we are waiting for this find reply, stop waiting.
  if (waiting_for_ == NEXT_REPLY ||
      (waiting_for_ == FINAL_REPLY &&
       last_finished_request_id_ >= last_request_id_)) {
    StopWaiting();
  }
}

bool FindTestWebContentsDelegate::IsBackForwardCacheSupported(
    WebContents& web_contents) {
  return true;
}

void FindTestWebContentsDelegate::WaitFor(WaitingFor wait_for) {
  ASSERT_EQ(NOTHING, waiting_for_);
  ASSERT_NE(NOTHING, wait_for);

  // Wait for |wait_for|.
  waiting_for_ = wait_for;
  message_loop_runner_ = new content::MessageLoopRunner;
  message_loop_runner_->Run();

  // Done waiting.
  waiting_for_ = NOTHING;
  message_loop_runner_ = nullptr;
}

void FindTestWebContentsDelegate::StopWaiting() {
  if (!message_loop_runner_.get())
    return;

  ASSERT_NE(NOTHING, waiting_for_);
  message_loop_runner_->Quit();
}

#if BUILDFLAG(IS_ANDROID)
void FindTestWebContentsDelegate::FindMatchRectsReply(
    WebContents* web_contents,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  // Update the current rects.
  find_match_rects_ = rects;
  active_match_rect_ = active_rect;

  // If we are waiting for match rects, stop waiting.
  if (waiting_for_ == MATCH_RECTS)
    StopWaiting();
}
#endif

std::unordered_set<raw_ptr<RenderFrameHost, CtnExperimental>>
GetRenderFrameHostsWithPendingFindResults(WebContents* web_contents) {
  if (auto* frm = static_cast<WebContentsImpl*>(web_contents)
                      ->GetFindRequestManagerForTesting()) {
    return frm->render_frame_hosts_pending_initial_reply_for_testing();
  }
  return std::unordered_set<raw_ptr<RenderFrameHost, CtnExperimental>>();
}

}  // namespace content
