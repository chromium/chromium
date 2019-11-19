/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/frame/find_in_page.h"

#include <utility>

#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_plugin.h"
#include "third_party/blink/public/web/web_plugin_document.h"
#include "third_party/blink/public/web/web_widget_client.h"
#include "third_party/blink/renderer/core/editing/finder/text_finder.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

FindInPage::FindInPage(WebLocalFrameImpl& frame,
                       InterfaceRegistry* interface_registry)
    : frame_(&frame) {
  // TODO(rakina): Use InterfaceRegistry of |frame| directly rather than passing
  // both of them.
  if (!interface_registry)
    return;
  // TODO(crbug.com/800641): Use InterfaceValidator when it works for associated
  // interfaces.
  interface_registry->AddAssociatedInterface(WTF::BindRepeating(
      &FindInPage::BindToReceiver, WrapWeakPersistent(this)));
}

void FindInPage::Find(int request_id,
                      const String& search_text,
                      mojom::blink::FindOptionsPtr options) {
  DCHECK(!search_text.IsEmpty());
  blink::WebPlugin* plugin = GetWebPluginForFind();
  // Check if the plugin still exists in the document.
  if (plugin) {
    if (options->find_next) {
      // Just navigate back/forward.
      plugin->SelectFindResult(options->forward, request_id);
      LocalFrame* core_frame = frame_->GetFrame();
      core_frame->GetPage()->GetFocusController().SetFocusedFrame(core_frame);
    } else if (!plugin->StartFind(search_text, options->match_case,
                                  request_id)) {
      // Send "no results"
      ReportFindInPageMatchCount(request_id, 0 /* count */,
                                 true /* final_update */);
    }
    return;
  }

  // Send "no results" if this frame has no visible content.
  if (!frame_->HasVisibleContent() && !options->force) {
    ReportFindInPageMatchCount(request_id, 0 /* count */,
                               true /* final_update */);
    return;
  }

  WebRange current_selection = frame_->SelectionRange();
  bool result = false;
  bool active_now = false;

  // Search for an active match only if this frame is focused or if this is a
  // find next
  if (frame_->IsFocused() || options->find_next) {
    result = FindInternal(request_id, search_text, *options,
                          false /* wrap_within_frame */, &active_now);
  }

  if (result && !options->find_next) {
    // Indicate that at least one match has been found. 1 here means
    // possibly more matches could be coming.
    ReportFindInPageMatchCount(request_id, 1 /* count */,
                               false /* final_update */);
  }

  // There are three cases in which scoping is needed:
  //
  // (1) This is an initial find request (|options.findNext| is false). This
  // will be the first scoping effort for this find session.
  //
  // (2) Something has been selected since the last search. This means that we
  // cannot just increment the current match ordinal; we need to re-generate
  // it.
  //
  // (3) TextFinder::Find() found what should be the next match (|result| is
  // true), but was unable to activate it (|activeNow| is false). This means
  // that the text containing this match was dynamically added since the last
  // scope of the frame. The frame needs to be re-scoped so that any matches
  // in the new text can be highlighted and included in the reported number of
  // matches.
  //
  // If none of these cases are true, then we just report the current match
  // count without scoping.
  if (/* (1) */ options->find_next && /* (2) */ current_selection.IsNull() &&
      /* (3) */ !(result && !active_now)) {
    // Force report of the actual count.
    EnsureTextFinder().IncreaseMatchCount(request_id, 0);
    return;
  }

  // Start a new scoping  If the scoping function determines that it
  // needs to scope, it will defer until later.
  EnsureTextFinder().StartScopingStringMatches(request_id, search_text,
                                               *options);
}

bool WebLocalFrameImpl::FindForTesting(int identifier,
                                       const WebString& search_text,
                                       bool match_case,
                                       bool forward,
                                       bool find_next,
                                       bool force,
                                       bool wrap_within_frame) {
  auto options = mojom::blink::FindOptions::New();
  options->match_case = match_case;
  options->forward = forward;
  options->find_next = find_next;
  options->force = force;
  options->run_synchronously_for_testing = true;
  bool result = find_in_page_->FindInternal(identifier, search_text, *options,
                                            wrap_within_frame, nullptr);
  find_in_page_->StopFinding(
      mojom::blink::StopFindAction::kStopFindActionKeepSelection);
  return result;
}

bool FindInPage::FindInternal(int identifier,
                              const WebString& search_text,
                              const mojom::blink::FindOptions& options,
                              bool wrap_within_frame,
                              bool* active_now) {
  if (!frame_->GetFrame())
    return false;

  // Unlikely, but just in case we try to find-in-page on a detached frame.
  DCHECK(frame_->GetFrame()->GetPage());

  // Up-to-date, clean tree is required for finding text in page, since it
  // relies on TextIterator to look over the text.
  frame_->GetFrame()->GetDocument()->UpdateStyleAndLayout();

  return EnsureTextFinder().Find(identifier, search_text, options,
                                 wrap_within_frame, active_now);
}

void FindInPage::StopFinding(mojom::StopFindAction action) {
  WebPlugin* const plugin = GetWebPluginForFind();
  if (plugin) {
    plugin->StopFind();
    return;
  }

  const bool clear_selection =
      action == mojom::StopFindAction::kStopFindActionClearSelection;
  if (clear_selection)
    frame_->ExecuteCommand(WebString::FromUTF8("Unselect"));

  if (GetTextFinder()) {
    if (!clear_selection)
      GetTextFinder()->SetFindEndstateFocusAndSelection();
    GetTextFinder()->StopFindingAndClearSelection();
  }

  if (action == mojom::StopFindAction::kStopFindActionActivateSelection &&
      frame_->IsFocused()) {
    WebDocument doc = frame_->GetDocument();
    if (!doc.IsNull()) {
      WebElement element = doc.FocusedElement();
      if (!element.IsNull())
        element.SimulateClick();
    }
  }
}

int FindInPage::FindMatchMarkersVersion() const {
  if (GetTextFinder())
    return GetTextFinder()->FindMatchMarkersVersion();
  return 0;
}

WebFloatRect FindInPage::ActiveFindMatchRect() {
  if (GetTextFinder())
    return GetTextFinder()->ActiveFindMatchRect();
  return WebFloatRect();
}

void FindInPage::ActivateNearestFindResult(int request_id,
                                           const WebFloatPoint& point) {
  WebRect active_match_rect;
  const int ordinal =
      EnsureTextFinder().SelectNearestFindMatch(point, &active_match_rect);
  if (ordinal == -1) {
    // Something went wrong, so send a no-op reply (force the frame to report
    // the current match count) in case the host is waiting for a response due
    // to rate-limiting.
    EnsureTextFinder().IncreaseMatchCount(request_id, 0);
    return;
  }
  ReportFindInPageSelection(request_id, ordinal, active_match_rect,
                            true /* final_update */);
}

void FindInPage::SetClient(
    mojo::PendingRemote<mojom::blink::FindInPageClient> remote) {
  // TODO(crbug.com/984878): Having to call reset() to try to bind a remote that
  // might be bound is questionable behavior and suggests code may be buggy.
  client_.reset();
  client_.Bind(std::move(remote));
}

void FindInPage::GetNearestFindResult(const WebFloatPoint& point,
                                      GetNearestFindResultCallback callback) {
  float distance;
  EnsureTextFinder().NearestFindMatch(point, &distance);
  std::move(callback).Run(distance);
}

void FindInPage::FindMatchRects(int current_version,
                                FindMatchRectsCallback callback) {
  int rects_version = FindMatchMarkersVersion();
  Vector<WebFloatRect> rects;
  if (current_version != rects_version)
    rects = EnsureTextFinder().FindMatchRects();
  std::move(callback).Run(rects_version, rects, ActiveFindMatchRect());
}

void FindInPage::ClearActiveFindMatch() {
  // TODO(rakina): Do collapse selection as this currently does nothing.
  frame_->ExecuteCommand(WebString::FromUTF8("CollapseSelection"));
  EnsureTextFinder().ClearActiveFindMatch();
}

void WebLocalFrameImpl::SetTickmarks(const WebVector<WebRect>& tickmarks) {
  find_in_page_->SetTickmarks(tickmarks);
}

void FindInPage::SetTickmarks(const WebVector<WebRect>& tickmarks) {
  if (LayoutView* layout_view = frame_->GetFrame()->ContentLayoutObject()) {
    Vector<IntRect> tickmarks_converted(SafeCast<wtf_size_t>(tickmarks.size()));
    for (wtf_size_t i = 0; i < tickmarks.size(); ++i)
      tickmarks_converted[i] = tickmarks[i];
    layout_view->OverrideTickmarks(tickmarks_converted);
  }
}

TextFinder* WebLocalFrameImpl::GetTextFinder() const {
  return find_in_page_->GetTextFinder();
}

TextFinder* FindInPage::GetTextFinder() const {
  return text_finder_;
}

TextFinder& WebLocalFrameImpl::EnsureTextFinder() {
  return find_in_page_->EnsureTextFinder();
}

TextFinder& FindInPage::EnsureTextFinder() {
  if (!text_finder_)
    text_finder_ = MakeGarbageCollected<TextFinder>(*frame_);

  return *text_finder_;
}

void FindInPage::SetPluginFindHandler(WebPluginContainer* plugin) {
  plugin_find_handler_ = plugin;
}

WebPluginContainer* FindInPage::PluginFindHandler() const {
  return plugin_find_handler_;
}

WebPlugin* FindInPage::GetWebPluginForFind() {
  if (frame_->GetDocument().IsPluginDocument())
    return frame_->GetDocument().To<WebPluginDocument>().Plugin();
  if (plugin_find_handler_)
    return plugin_find_handler_->Plugin();
  return nullptr;
}

void FindInPage::BindToReceiver(
    mojo::PendingAssociatedReceiver<mojom::blink::FindInPage> receiver) {
  receiver_.Bind(std::move(receiver),
                 frame_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void FindInPage::Dispose() {
  receiver_.reset();
}

void FindInPage::ReportFindInPageMatchCount(int request_id,
                                            int count,
                                            bool final_update) {
  // In tests, |client_| might not be set.
  if (!client_)
    return;
  client_->SetNumberOfMatches(
      request_id, count,
      final_update ? mojom::blink::FindMatchUpdateType::kFinalUpdate
                   : mojom::blink::FindMatchUpdateType::kMoreUpdatesComing);
}

void FindInPage::ReportFindInPageSelection(int request_id,
                                           int active_match_ordinal,
                                           const blink::WebRect& selection_rect,
                                           bool final_update) {
  // In tests, |client_| might not be set.
  if (!client_)
    return;
  client_->SetActiveMatch(
      request_id, selection_rect, active_match_ordinal,
      final_update ? mojom::blink::FindMatchUpdateType::kFinalUpdate
                   : mojom::blink::FindMatchUpdateType::kMoreUpdatesComing);
}

}  // namespace blink
