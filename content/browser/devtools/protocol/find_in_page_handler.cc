// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/find_in_page_handler.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/blink/public/mojom/frame/find_in_page.mojom.h"

namespace content::protocol {

namespace {

template <typename CallbackT>
base::OnceCallback<void(Response)> MakeResponder(
    std::unique_ptr<CallbackT> callback) {
  return base::BindOnce(
      [](std::unique_ptr<CallbackT> callback, Response response) {
        if (response.IsSuccess()) {
          callback->sendSuccess();
        } else {
          callback->sendFailure(std::move(response));
        }
      },
      std::move(callback));
}

}  // namespace

FindInPageHandler::FindInPageHandler()
    : DevToolsDomainHandler(FindInPage::Metainfo::domainName) {}

FindInPageHandler::~FindInPageHandler() {
  if (pending_callback_) {
    std::move(pending_callback_)
        .Run(Response::ServerError("FindInPageHandler destroyed"));
  }
}

void FindInPageHandler::Wire(UberDispatcher* dispatcher) {
  FindInPage::Dispatcher::wire(dispatcher, this);
}

void FindInPageHandler::SetRenderer(int process_host_id,
                                    RenderFrameHostImpl* frame_host) {
  if (!frame_host) {
    Disable();
  }
  frame_host_ = frame_host;
  Observe(frame_host_ ? WebContents::FromRenderFrameHost(frame_host_)
                      : nullptr);
}

Response FindInPageHandler::Disable() {
  ResolvePendingCallback(Response::ServerError("Target is being disposed"));
  StopFindingInternal();
  return Response::Success();
}

void FindInPageHandler::FindFirst(const std::string& query,
                                  std::unique_ptr<FindFirstCallback> callback) {
  if (query.empty()) {
    callback->sendFailure(Response::InvalidParams("query must not be empty"));
    return;
  }
  if (!frame_host_) {
    callback->sendFailure(
        Response::ServerError("No renderer associated with this target"));
    return;
  }
  if (pending_callback_) {
    callback->sendFailure(Response::ServerError(
        "A find request is already in flight for this target"));
    return;
  }

  query_ = base::UTF8ToUTF16(query);

  WebContentsImpl* const web_contents = GetWebContents();
  pending_callback_ = MakeResponder(std::move(callback));

  web_contents->Find(
      query_, blink::mojom::FindOptions::New(), /*skip_delay=*/true,
      [this](int request_id) { pending_request_id_ = request_id; });
}

void FindInPageHandler::FindNext(std::unique_ptr<FindNextCallback> callback) {
  FindNextOrPrevInternal(/*forward=*/true, std::move(callback));
}

void FindInPageHandler::FindPrev(std::unique_ptr<FindPrevCallback> callback) {
  FindNextOrPrevInternal(/*forward=*/false, std::move(callback));
}

Response FindInPageHandler::Stop() {
  if (!frame_host_) {
    return Response::ServerError("No renderer associated with this target");
  }

  ResolvePendingCallback(Response::ServerError("Find session was stopped"));
  StopFindingInternal();
  return Response::Success();
}

void FindInPageHandler::DidReceiveFindReply(int request_id,
                                            int number_of_matches,
                                            const gfx::Rect& selection_rect,
                                            int active_match_ordinal,
                                            bool final_update) {
  if (!pending_callback_ || request_id != pending_request_id_ ||
      !final_update) {
    return;
  }

  ResolvePendingCallback(Response::Success());
}

template <typename CallbackT>
void FindInPageHandler::FindNextOrPrevInternal(
    bool forward,
    std::unique_ptr<CallbackT> callback) {
  if (query_.empty()) {
    callback->sendFailure(Response::ServerError(
        "No active find session; call FindFirst() first"));
    return;
  }
  if (!frame_host_) {
    callback->sendFailure(
        Response::ServerError("No renderer associated with this target"));
    return;
  }
  if (pending_callback_) {
    callback->sendFailure(Response::ServerError(
        "A find request is already in flight for this target"));
    return;
  }

  WebContentsImpl* const web_contents = GetWebContents();
  pending_callback_ = MakeResponder(std::move(callback));

  auto options = blink::mojom::FindOptions::New();
  options->new_session = false;
  options->forward = forward;
  web_contents->Find(
      query_, std::move(options), /*skip_delay=*/true,
      [this](int request_id) { pending_request_id_ = request_id; });
}

void FindInPageHandler::StopFindingInternal() {
  query_.clear();

  if (!frame_host_) {
    return;
  }

  if (frame_host_->IsRenderFrameLive()) {
    GetWebContents()->StopFinding(
        content::StopFindAction::STOP_FIND_ACTION_CLEAR_SELECTION);
  }
}

void FindInPageHandler::ResolvePendingCallback(Response response) {
  if (!pending_callback_) {
    return;
  }
  std::move(pending_callback_).Run(std::move(response));
}

WebContentsImpl* FindInPageHandler::GetWebContents() {
  CHECK(frame_host_);
  return static_cast<WebContentsImpl*>(
      WebContents::FromRenderFrameHost(frame_host_));
}

}  // namespace content::protocol
