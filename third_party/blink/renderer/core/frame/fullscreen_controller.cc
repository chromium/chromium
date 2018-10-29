/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/frame/fullscreen_controller.h"

#include "base/memory/ptr_util.h"
#include "third_party/blink/public/platform/web_layer_tree_view.h"
#include "third_party/blink/public/web/web_fullscreen_options.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen_options.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/page/page.h"

namespace blink {

namespace {

WebLocalFrameClient& GetWebFrameClient(LocalFrame& frame) {
  WebLocalFrameImpl* web_frame = WebLocalFrameImpl::FromFrame(frame);
  DCHECK(web_frame);
  DCHECK(web_frame->Client());
  return *web_frame->Client();
}

}  // anonymous namespace

std::unique_ptr<FullscreenController> FullscreenController::Create(
    WebViewImpl* web_view_base) {
  return base::WrapUnique(new FullscreenController(web_view_base));
}

FullscreenController::FullscreenController(WebViewImpl* web_view_base)
    : web_view_base_(web_view_base),
      pending_frames_(new PendingFullscreenSet) {}

void FullscreenController::DidEnterFullscreen() {
  // |Browser::EnterFullscreenModeForTab()| can enter fullscreen without going
  // through |Fullscreen::RequestFullscreen()|, in which case there will be no
  // fullscreen element. Do nothing.
  if (state_ != State::kEnteringFullscreen)
    return;

  UpdatePageScaleConstraints(false);
  web_view_base_->SetPageScaleFactor(1.0f);
  web_view_base_->SetVisualViewportOffset(FloatPoint());

  state_ = State::kFullscreen;

  // Notify all pending local frames in order that we have entered fullscreen.
  for (LocalFrame* frame : *pending_frames_) {
    if (frame) {
      if (Document* document = frame->GetDocument())
        Fullscreen::DidEnterFullscreen(*document);
    }
  }

  // Notify all local frames that we have entered fullscreen.
  for (Frame* frame = web_view_base_->GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    if (Document* document = ToLocalFrame(frame)->GetDocument())
      Fullscreen::DidEnterFullscreen(*document);
  }
  pending_frames_->clear();

  // TODO(foolip): If the top level browsing context (main frame) ends up with
  // no fullscreen element, exit fullscreen again to recover.
}

void FullscreenController::DidExitFullscreen() {
  // The browser process can exit fullscreen at any time, e.g. if the user
  // presses Esc. After |Browser::EnterFullscreenModeForTab()|,
  // |Browser::ExitFullscreenModeForTab()| will make it seem like we exit when
  // not even in fullscreen. Do nothing.
  if (state_ == State::kInitial)
    return;

  UpdatePageScaleConstraints(true);

  state_ = State::kInitial;

  // Notify the topmost local frames that we have exited fullscreen.
  // |Fullscreen::DidExitFullscreen()| will take care of descendant frames.
  for (Frame* frame = web_view_base_->GetPage()->MainFrame(); frame;) {
    Frame* next_frame = frame->Tree().TraverseNext();

    if (frame->IsRemoteFrame()) {
      frame = next_frame;
      continue;
    }

    DCHECK(frame->IsLocalFrame() && ToLocalFrame(frame)->IsLocalRoot());
    if (Document* document = ToLocalFrame(frame)->GetDocument())
      Fullscreen::DidExitFullscreen(*document);

    // Skip over all descendant frames.
    while (next_frame && next_frame->Tree().IsDescendantOf(frame))
      next_frame = next_frame->Tree().TraverseNext();
    frame = next_frame;
  }
}

void FullscreenController::EnterFullscreen(LocalFrame& frame,
                                           const FullscreenOptions& options) {
  // TODO(dtapuska): If we are already in fullscreen. If the options are
  // different than the currently requested one we may wish to request
  // fullscreen mode again.
  // If already fullscreen or exiting fullscreen, synchronously call
  // |DidEnterFullscreen()|. When exiting, the coming |DidExitFullscreen()| call
  // will again notify all frames.
  if (state_ == State::kFullscreen || state_ == State::kExitingFullscreen) {
    State old_state = state_;
    state_ = State::kEnteringFullscreen;
    DidEnterFullscreen();
    state_ = old_state;
    return;
  }

  // We need to store these values here rather than in |DidEnterFullscreen()|
  // since by the time the latter is called, a Resize has already occured,
  // clamping the scroll offset. Don't save values if we're still waiting to
  // restore a previous set. This can happen if we exit and quickly reenter
  // fullscreen without performing a layout.
  if (state_ == State::kInitial) {
    initial_background_color_override_enabled_ =
        web_view_base_->BackgroundColorOverrideEnabled();
    initial_background_color_override_ =
        web_view_base_->BackgroundColorOverride();
  }

  pending_frames_->insert(&frame);

  // If already entering fullscreen, just wait.
  if (state_ == State::kEnteringFullscreen)
    return;

  DCHECK(state_ == State::kInitial);
  blink::WebFullscreenOptions blink_options;
  // Only clone options if the feature is enabled.
  if (RuntimeEnabledFeatures::FullscreenOptionsEnabled())
    blink_options.prefers_navigation_bar = options.navigationUI() != "hide";
  GetWebFrameClient(frame).EnterFullscreen(blink_options);

  state_ = State::kEnteringFullscreen;
}

void FullscreenController::ExitFullscreen(LocalFrame& frame) {
  // If not in fullscreen, ignore any attempt to exit. In particular, when
  // entering fullscreen, allow the transition into fullscreen to complete. Note
  // that the browser process is ultimately in control and can still exit
  // fullscreen at any time.
  if (state_ != State::kFullscreen)
    return;

  GetWebFrameClient(frame).ExitFullscreen();

  state_ = State::kExitingFullscreen;
}

void FullscreenController::FullscreenElementChanged(Element* old_element,
                                                    Element* new_element) {
  DCHECK_NE(old_element, new_element);

  // We only override the WebView's background color for overlay fullscreen
  // video elements, so have to restore the override when the element changes.
  RestoreBackgroundColorOverride();

  if (new_element) {
    DCHECK(Fullscreen::IsFullscreenElement(*new_element));

    if (auto* video_element = ToHTMLVideoElementOrNull(*new_element)) {
      video_element->DidEnterFullscreen();

      // If the video uses overlay fullscreen mode, make the background
      // transparent.
      if (video_element->UsesOverlayFullscreenVideo())
        web_view_base_->SetBackgroundColorOverride(Color::kTransparent);
    }
  }

  if (old_element) {
    DCHECK(!Fullscreen::IsFullscreenElement(*old_element));

    if (auto* video_element = ToHTMLVideoElementOrNull(*old_element))
      video_element->DidExitFullscreen();
  }

  // Tell the browser the fullscreen state has changed.
  if (Element* owner = new_element ? new_element : old_element) {
    if (LocalFrame* frame = owner->GetDocument().GetFrame())
      GetWebFrameClient(*frame).FullscreenStateChanged(!!new_element);
  }
}

void FullscreenController::RestoreBackgroundColorOverride() {
  if (web_view_base_->BackgroundColorOverrideEnabled() !=
          initial_background_color_override_enabled_ ||
      web_view_base_->BackgroundColorOverride() !=
          initial_background_color_override_) {
    if (initial_background_color_override_enabled_) {
      web_view_base_->SetBackgroundColorOverride(
          initial_background_color_override_);
    } else {
      web_view_base_->ClearBackgroundColorOverride();
    }
  }
}

void FullscreenController::UpdateSize() {
  DCHECK(web_view_base_->GetPage());

  if (state_ != State::kFullscreen && state_ != State::kExitingFullscreen)
    return;

  UpdatePageScaleConstraints(false);
}

void FullscreenController::UpdatePageScaleConstraints(bool reset_constraints) {
  PageScaleConstraints fullscreen_constraints;
  if (reset_constraints) {
    web_view_base_->GetPageScaleConstraintsSet().SetNeedsReset(true);
  } else {
    fullscreen_constraints = PageScaleConstraints(1.0, 1.0, 1.0);
    fullscreen_constraints.layout_size = FloatSize(web_view_base_->Size());
  }
  web_view_base_->GetPageScaleConstraintsSet().SetFullscreenConstraints(
      fullscreen_constraints);
  web_view_base_->GetPageScaleConstraintsSet().ComputeFinalConstraints();

  // Although we called |ComputeFinalConstraints()| above, the "final"
  // constraints are not actually final. They are still subject to scale factor
  // clamping by contents size. Normally they should be dirtied due to contents
  // size mutation after layout, however the contents size is not guaranteed to
  // mutate, and the scale factor may remain unclamped. Just fire the event
  // again to ensure the final constraints pick up the latest contents size.
  web_view_base_->DidChangeContentsSize();
  if (web_view_base_->MainFrameImpl() &&
      web_view_base_->MainFrameImpl()->GetFrameView())
    web_view_base_->MainFrameImpl()->GetFrameView()->SetNeedsLayout();

  web_view_base_->UpdateMainFrameLayoutSize();
}

}  // namespace blink
