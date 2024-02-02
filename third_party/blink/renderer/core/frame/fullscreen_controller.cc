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
#include "third_party/blink/public/mojom/frame/fullscreen.mojom-blink.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fullscreen_options.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/page_scale_constraints_set.h"
#include "third_party/blink/renderer/core/frame/screen.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen_request_type.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/spatial_navigation.h"
#include "third_party/blink/renderer/core/page/spatial_navigation_controller.h"

namespace blink {

namespace {

mojom::blink::FullscreenOptionsPtr ToMojoOptions(
    LocalFrame* frame,
    const FullscreenOptions* options,
    FullscreenRequestType request_type) {
  auto fullscreen_options = mojom::blink::FullscreenOptions::New();
  fullscreen_options->prefers_navigation_bar =
      options->navigationUI() == "show";
  if (options->hasScreen() &&
      options->screen()->DisplayId() != Screen::kInvalidDisplayId) {
    fullscreen_options->display_id = options->screen()->DisplayId();
  }

  // Propagate the type of fullscreen request (prefixed or unprefixed) to
  // OOPIF ancestor frames so that they fire matching prefixed or unprefixed
  // fullscreen events.
  fullscreen_options->is_prefixed =
      request_type & FullscreenRequestType::kPrefixed;
  fullscreen_options->is_xr_overlay =
      request_type & FullscreenRequestType::kForXrOverlay;
  fullscreen_options->prefers_status_bar =
      request_type & FullscreenRequestType::kForXrArWithCamera;

  return fullscreen_options;
}

}  // namespace

FullscreenController::FullscreenController(WebViewImpl* web_view_base)
    : web_view_base_(web_view_base),
      pending_frames_(MakeGarbageCollected<PendingFullscreenSet>()) {}

void FullscreenController::DidEnterFullscreen() {
  // |Browser::EnterFullscreenModeForTab()| can enter fullscreen without going
  // through |Fullscreen::RequestFullscreen()|, in which case there will be no
  // fullscreen element. Do nothing.
  if (state_ != State::kEnteringFullscreen &&
      state_ != State::kChangingFullscreenDisplays) {
    return;
  }

  UpdatePageScaleConstraints(false);

  // Only reset the scale for the local main frame.
  if (web_view_base_->MainFrameImpl()) {
    web_view_base_->SetPageScaleFactor(1.0f);
    web_view_base_->SetVisualViewportOffset(gfx::PointF());
  }

  state_ = State::kFullscreen;

  NotifyFramesOfFullscreenEntry(true /* success */);

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

    auto* local_frame = To<LocalFrame>(frame);
    DCHECK(local_frame->IsLocalRoot());
    if (Document* document = local_frame->GetDocument())
      Fullscreen::DidExitFullscreen(*document);

    // Skip over all descendant frames.
    while (next_frame && next_frame->Tree().IsDescendantOf(frame))
      next_frame = next_frame->Tree().TraverseNext();
    frame = next_frame;
  }
}

void FullscreenController::EnterFullscreen(LocalFrame& frame,
                                           const FullscreenOptions* options,
                                           FullscreenRequestType request_type) {
  const auto& screen_info = frame.GetChromeClient().GetScreenInfo(frame);

  const bool requesting_other_screen =
      options->hasScreen() &&
      options->screen()->DisplayId() != Screen::kInvalidDisplayId &&
      options->screen()->DisplayId() != screen_info.display_id;
  bool requesting_fullscreen_screen_change =
      state_ == State::kFullscreen && requesting_other_screen;

  // TODO(dtapuska): If we are already in fullscreen. If the options are
  // different than the currently requested one we may wish to request
  // fullscreen mode again.
  // If already fullscreen or exiting fullscreen, synchronously call
  // |DidEnterFullscreen()|. When exiting, the coming |DidExitFullscreen()| call
  // will again notify all frames.
  if ((state_ == State::kFullscreen && !requesting_fullscreen_screen_change) ||
      state_ == State::kExitingFullscreen) {
    State old_state = state_;
    state_ = State::kEnteringFullscreen;
    DidEnterFullscreen();
    state_ = old_state;
    return;
  }

  pending_frames_->insert(&frame);

  // If already entering fullscreen, just wait until the first request settles.
  // TODO(enne): currently, if you request fullscreen with different display ids
  // (or one with and one without display ids), then only the first request will
  // be considered, and all others will be ignored and be settled when the first
  // is resolved.  One way to fix this might be to queue up requests in
  // blink::Fullscreen such that we never have simultaneous requests with
  // conflicting options.
  if (state_ == State::kEnteringFullscreen ||
      state_ == State::kChangingFullscreenDisplays) {
    return;
  }

  DCHECK(state_ == State::kInitial || requesting_fullscreen_screen_change);
  auto fullscreen_options = ToMojoOptions(&frame, options, request_type);

  // We want to disallow entering fullscreen with status and navigation bars
  // both visible, as this would translate into "no fullscreen at all".
  DCHECK(!(fullscreen_options->prefers_status_bar &&
           fullscreen_options->prefers_navigation_bar));

#if DCHECK_IS_ON()
  DVLOG(2) << __func__ << ": request_type="
           << FullscreenRequestTypeToDebugString(request_type)
           << " fullscreen_options={display_id="
           << fullscreen_options->display_id << ", prefers_navigation_bar="
           << fullscreen_options->prefers_navigation_bar
           << ", prefers_status_bar=" << fullscreen_options->prefers_status_bar
           << ", is_prefixed=" << fullscreen_options->is_prefixed
           << ", is_xr_overlay=" << fullscreen_options->is_xr_overlay << "}";
#endif

  // Don't send redundant EnterFullscreen message to the browser for the
  // ancestor frames if the subframe has already entered fullscreen.
  if (!(request_type & FullscreenRequestType::kForCrossProcessDescendant)) {
    frame.GetLocalFrameHostRemote().EnterFullscreen(
        std::move(fullscreen_options),
        WTF::BindOnce(&FullscreenController::EnterFullscreenCallback,
                      WTF::Unretained(this)));
  }

  if (state_ == State::kInitial)
    state_ = State::kEnteringFullscreen;
  else  // if state_ == State::kFullscreen
    state_ = State::kChangingFullscreenDisplays;
}

void FullscreenController::ExitFullscreen(LocalFrame& frame) {
  // If not in fullscreen, ignore any attempt to exit. In particular, when
  // entering fullscreen, allow the transition into fullscreen to complete. Note
  // that the browser process is ultimately in control and can still exit
  // fullscreen at any time.
  if (state_ != State::kFullscreen)
    return;

  frame.GetLocalFrameHostRemote().ExitFullscreen();

  state_ = State::kExitingFullscreen;
}

void FullscreenController::FullscreenElementChanged(
    Element* old_element,
    Element* new_element,
    const FullscreenOptions* options,
    FullscreenRequestType request_type) {
  DCHECK_NE(old_element, new_element);

  // We only override the WebView's background color for overlay fullscreen
  // video elements, so have to restore the override when the element changes.
  auto* old_video_element = DynamicTo<HTMLVideoElement>(old_element);
  if (old_video_element)
    RestoreBackgroundColorOverride();

  if (new_element) {
    DCHECK(Fullscreen::IsFullscreenElement(*new_element));

    if (auto* video_element = DynamicTo<HTMLVideoElement>(*new_element)) {
      video_element->DidEnterFullscreen();
    }
  }

  if (old_element) {
    DCHECK(!Fullscreen::IsFullscreenElement(*old_element));

    if (old_video_element)
      old_video_element->DidExitFullscreen();
  }

  // Tell the browser the fullscreen state has changed.
  if (Element* owner = new_element ? new_element : old_element) {
    Document& doc = owner->GetDocument();
    bool in_fullscreen = !!new_element;
    if (LocalFrame* frame = doc.GetFrame()) {
      mojom::blink::FullscreenOptionsPtr mojo_options;
      if (in_fullscreen)
        mojo_options = ToMojoOptions(frame, options, request_type);

      frame->GetLocalFrameHostRemote().FullscreenStateChanged(
          in_fullscreen, std::move(mojo_options));
    }
  }
}

void FullscreenController::RestoreBackgroundColorOverride() {
  web_view_base_->SetBackgroundColorOverrideForFullscreenController(
      std::nullopt);
}

void FullscreenController::NotifyFramesOfFullscreenEntry(bool granted) {
  // Notify all pending local frames in order whether or not we successfully
  // entered fullscreen.
  for (LocalFrame* frame : *pending_frames_) {
    if (frame) {
      if (Document* document = frame->GetDocument()) {
        Fullscreen::DidResolveEnterFullscreenRequest(*document, granted);
      }
    }
  }

  // Notify all local frames whether or not we successfully entered fullscreen.
  for (Frame* frame = web_view_base_->GetPage()->MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    auto* local_frame = DynamicTo<LocalFrame>(frame);
    if (!local_frame)
      continue;
    if (Document* document = local_frame->GetDocument()) {
      Fullscreen::DidResolveEnterFullscreenRequest(*document, granted);
    }
  }
  pending_frames_->clear();
}

void FullscreenController::EnterFullscreenCallback(bool granted) {
  if (granted) {
    // If the fullscreen is granted, then the VisualPropertiesUpdated message
    // will later be fired and the state will be updated then.
    //
    // TODO(enne): the visual property updates *must* call DidEnterFullscreen
    // in order for the requestFullscreen promise to be resolved.
    // There are early outs in FullscreenController::EnterFullscreenModeForTab
    // that may prevent this from happening, especially with stale display id
    // differences, where a renderer might think the display id is changing
    // but the browser thinks it is the same and early outs.  This communication
    // needs to be more explicit in those cases to avoid hanging promises.
  } else {
    state_ = State::kInitial;
    NotifyFramesOfFullscreenEntry(false /* granted */);
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
    fullscreen_constraints.layout_size = gfx::SizeF(web_view_base_->Size());
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
