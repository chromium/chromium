// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FIND_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_FIND_TEST_UTILS_H_

#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/test_utils.h"

namespace content {

// The results of a find request.
struct FindResults {
  FindResults(int request_id,
              int number_of_matches,
              int active_match_ordinal);
  FindResults();

  int request_id;
  int number_of_matches;
  int active_match_ordinal;
};

// This test delegate is used during find-in-page tests, in order to directly
// access find replies going through the WebContentsDelegate. Tests functions in
// this delegate allow for waiting on specific or all find replies to come in,
// and observe find results within them.
class FindTestWebContentsDelegate : public WebContentsDelegate {
 public:
  FindTestWebContentsDelegate();

  FindTestWebContentsDelegate(const FindTestWebContentsDelegate&) = delete;
  FindTestWebContentsDelegate& operator=(const FindTestWebContentsDelegate&) =
      delete;

  ~FindTestWebContentsDelegate() override;

  // Returns the current find results.
  const FindResults& GetFindResults() const;

  // Waits for all pending replies to be received.
  void WaitForFinalReply();

  // Waits for the next find reply. This is useful for waiting for a single
  // match to be activated, or for a new frame to be searched.
  void WaitForNextReply();

  // Indicates that the next find reply from this point will be the one to wait
  // for when WaitForNextReply() is called. It may be the case that the reply
  // comes before the call to WaitForNextReply(), in which case it will return
  // immediately.
  void MarkNextReply();

  // Called when a new find request is issued, so the delegate knows the last
  // request ID.
  void UpdateLastRequest(int request_id);

  // From when this function is called, all replies coming in via FindReply()
  // will be recorded. These replies can be retrieved via GetReplyRecord().
  void StartReplyRecord();

  // Retreives the results from the find replies recorded since the last call to
  // StartReplyRecord(). Calling this function also stops the recording new find
  // replies.
  const std::vector<FindResults>& GetReplyRecord();

#if BUILDFLAG(IS_ANDROID)
  // Waits for all of the find match rects to be received.
  void WaitForMatchRects();

  const std::vector<gfx::RectF>& find_match_rects() const {
    return find_match_rects_;
  }

  const gfx::RectF& active_match_rect() const {
    return active_match_rect_;
  }
#endif

 private:
  enum WaitingFor {
    NOTHING,
    FINAL_REPLY,
    NEXT_REPLY,
#if BUILDFLAG(IS_ANDROID)
    MATCH_RECTS
#endif
  };

  // WebContentsDelegate override.
  void FindReply(WebContents* web_contents,
                 int request_id,
                 int number_of_matches,
                 const gfx::Rect& selection_rect,
                 int active_match_ordinal,
                 bool final_update) override;
  bool IsBackForwardCacheSupported(WebContents& web_contents) override;

  // Uses |message_loop_runner_| to wait for various things.
  void WaitFor(WaitingFor wait_for);

  // Stop waiting for |waiting_for_|.
  void StopWaiting();

#if BUILDFLAG(IS_ANDROID)
  // WebContentsDelegate override.
  void FindMatchRectsReply(WebContents* web_contents,
                           int version,
                           const std::vector<gfx::RectF>& rects,
                           const gfx::RectF& active_rect) override;

  std::vector<gfx::RectF> find_match_rects_;

  gfx::RectF active_match_rect_;
#endif

  // The latest known results from the current find request.
  FindResults current_results_;

  // The ID of the last find request issued.
  int last_request_id_;

  // The ID of the last find request to finish (all replies received).
  int last_finished_request_id_;

  // Indicates whether the next reply after MarkNextReply() has been received.
  bool next_reply_received_;

  // Indicates whether the find results from incoming find replies are currently
  // being recorded.
  bool record_replies_;

  // A record of all find replies that have come in via FindReply() since
  // StartReplyRecor() was last called.
  std::vector<FindResults> reply_record_;

  // Indicates what |message_loop_runner_| is waiting for, if anything.
  WaitingFor waiting_for_;

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
};

// Finds the set of all RenderFrameHosts that the FindRequestManager associated
// with |web_contents| has an ongoing find request. Note that the
// FindRequestManager could be owned by an outer WebContents of |web_contents|
// and the returned RenderFrameHosts are not necessarily part of |web_contents|
// frame tree.
std::unordered_set<raw_ptr<RenderFrameHost, CtnExperimental>>
GetRenderFrameHostsWithPendingFindResults(WebContents* web_contents);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FIND_TEST_UTILS_H_
