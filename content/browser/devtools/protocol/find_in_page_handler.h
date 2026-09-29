// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FIND_IN_PAGE_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FIND_IN_PAGE_HANDLER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/find_in_page.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class RenderFrameHostImpl;
class WebContentsImpl;

namespace protocol {

// Implements the "FindInPage" DevTools protocol domain by driving
// content::FindRequestManager (via WebContents::Find()/StopFinding()), which
// searches across the whole frame tree, including out-of-process iframes.
class FindInPageHandler : public DevToolsDomainHandler,
                          public FindInPage::Backend,
                          public WebContentsObserver {
 public:
  FindInPageHandler();
  FindInPageHandler(const FindInPageHandler&) = delete;
  FindInPageHandler& operator=(const FindInPageHandler&) = delete;
  ~FindInPageHandler() override;

 private:
  // DevToolsDomainHandler overrides.
  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderer(int process_host_id,
                   RenderFrameHostImpl* frame_host) override;
  Response Disable() override;

  // FindInPage::Backend overrides.
  // FindFirst() starts a new session via FindRequestManager::Find(), which does
  // not necessarily restart the search at the beginning of the page: if a
  // match is currently active, the renderer continues forward from that
  // position instead.
  void FindFirst(const std::string& query,
                 std::unique_ptr<FindFirstCallback> callback) override;
  void FindNext(std::unique_ptr<FindNextCallback> callback) override;
  void FindPrev(std::unique_ptr<FindPrevCallback> callback) override;
  Response Stop() override;

  // WebContentsObserver override.
  void DidReceiveFindReply(int request_id,
                           int number_of_matches,
                           const gfx::Rect& selection_rect,
                           int active_match_ordinal,
                           bool final_update) override;

  // Shared internal implementation of FindNext()/FindPrev(). Continues the find
  // session started by the most recent FindFirst() call.
  template <typename CallbackT>
  void FindNextOrPrevInternal(bool forward,
                              std::unique_ptr<CallbackT> callback);

  // Does the actual work of Stop().
  void StopFindingInternal();

  // Resolves the pending callback, if any, with `response`, and clears it.
  void ResolvePendingCallback(Response response);

  WebContentsImpl* GetWebContents();

  raw_ptr<RenderFrameHostImpl> frame_host_ = nullptr;

  // The query passed to the most recent FindFirst() call, used by
  // FindNext()/FindPrev() to continue that find session. Empty iff no find
  // session is in progress.
  std::u16string query_;

  // The request ID and callback of the in-flight FindFirst()/FindNext()/
  // FindPrev() call, if any. Only meaningful when `pending_callback_` is not
  // null.
  int pending_request_id_ = 0;
  base::OnceCallback<void(Response)> pending_callback_;
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_FIND_IN_PAGE_HANDLER_H_
