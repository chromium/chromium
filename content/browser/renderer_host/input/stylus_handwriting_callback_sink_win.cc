// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"

#include "components/stylus_handwriting/win/features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/display/win/screen_win.h"
#include "ui/gfx/geometry/rect.h"

namespace content {

StylusHandwritingCallbackSinkWin::StylusHandwritingCallbackSinkWin() = default;
StylusHandwritingCallbackSinkWin::~StylusHandwritingCallbackSinkWin() {
  if (sink_cookie_ == TF_INVALID_COOKIE) {
    return;
  }

  source_->UnadviseSink(sink_cookie_);
  sink_cookie_ = TF_INVALID_COOKIE;
}

HRESULT STDMETHODCALLTYPE
StylusHandwritingCallbackSinkWin::RuntimeClassInitialize() {
  const auto thread_manager =
      StylusHandwritingControllerWin::GetThreadManager();
  return (thread_manager && SUCCEEDED(thread_manager.As(&source_)) &&
          SUCCEEDED(source_->AdviseSink(__uuidof(::ITfHandwritingSink), this,
                                        &sink_cookie_)))
             ? S_OK
             : E_FAIL;
}

HRESULT STDMETHODCALLTYPE
StylusHandwritingCallbackSinkWin::DetermineProximateHandwritingTarget(
    ::ITfDetermineProximateHandwritingTargetArgs* args) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
StylusHandwritingCallbackSinkWin::FocusHandwritingTarget(
    ::ITfFocusHandwritingTargetArgs* args) {
  CHECK(handwriting_callback_);
  HWND window;
  RECT rect;
  SIZE distance_threshold;
  HRESULT hr = args->GetPointerTargetInfo(&window, &rect, &distance_threshold);
  if (FAILED(hr)) {
    return hr;
  }

  handwriting_callback_.Run(
      display::win::GetScreenWin()->ScreenToDIPRect(window, gfx::Rect(rect)),
      display::win::GetScreenWin()->ScreenToDIPSize(
          window, gfx::Size(distance_threshold.cx, distance_threshold.cy)));

  // Check that we have no pending callback.
  DCHECK(!pending_target_args_);
  pending_target_args_ = args;

  // The response is later be set via OnEditElementFocusedForStylusWriting
  return TF_S_ASYNC;
}

void StylusHandwritingCallbackSinkWin::OnFocusHandled() {
  if (pending_target_args_) {
    pending_target_args_->SetResponse(::TF_HANDWRITING_TARGET_FOCUSED);
    pending_target_args_.Reset();
  }
}

void StylusHandwritingCallbackSinkWin::OnFocusFailed() {
  if (pending_target_args_) {
    pending_target_args_->SetResponse(::TF_NO_HANDWRITING_TARGET);
    pending_target_args_.Reset();
  }
}

}  // namespace content
