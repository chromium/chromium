// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CALLBACK_SINK_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CALLBACK_SINK_WIN_H_

#include "base/functional/callback.h"
#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

namespace content {

// Handles the callback from Shell Handwriting service.
class CONTENT_EXPORT StylusHandwritingCallbackSinkWin
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          ::ITfHandwritingSink> {
 public:
  using OnFocusHandwritingTargetCallback =
      StylusHandwritingControllerWin::OnFocusHandwritingTargetCallback;

  // The constructor is public for `MakeAndInitialize` and should not be called
  // directly.
  StylusHandwritingCallbackSinkWin();
  StylusHandwritingCallbackSinkWin(const StylusHandwritingCallbackSinkWin&) =
      delete;
  StylusHandwritingCallbackSinkWin& operator=(
      const StylusHandwritingCallbackSinkWin&) = delete;
  ~StylusHandwritingCallbackSinkWin() override;

  // Called during `MakeAndInitialize`.
  HRESULT STDMETHODCALLTYPE RuntimeClassInitialize();

  // ITfHandwritingSink:
  HRESULT STDMETHODCALLTYPE DetermineProximateHandwritingTarget(
      ::ITfDetermineProximateHandwritingTargetArgs* args) override;

  // ITfHandwritingSink:
  HRESULT STDMETHODCALLTYPE
  FocusHandwritingTarget(::ITfFocusHandwritingTargetArgs* args) override;

  void SetCallback(OnFocusHandwritingTargetCallback callback);

  void OnFocusHandled();
  void OnFocusFailed();

  // Causes the next FocusHandwritingTarget call to short circuit and return
  // TF_NO_HANDWRITING_TARGET. Called when the view that initiated the session
  // is destroyed before TSF delivered FocusHandwritingTarget, so the Shell
  // Handwriting API is not left awaiting a response.
  void AbandonInFlightFocus() { decline_next_target_ = true; }

  bool IsFocusHandwritingTargetPending() const {
    return !!pending_target_args_;
  }

 private:
  // Stores the callback to request setting the renderer focus based on the
  // target area of the pointer input and the distance threshold.
  OnFocusHandwritingTargetCallback handwriting_callback_;
  // The ITfSource interface is implemented by the TSF manager obtained from the
  // ITfThreadMgr instance. Used to install and uninstall the ITfHandwritingSink
  // so Shell Handwriting can call this controller instance, in particular
  // FocusHandwritingTarget to set the target focus.
  Microsoft::WRL::ComPtr<ITfSource> source_;
  // Args passed during FocusHandwritingTarget() call by Shell Handwriting
  // which help to retrieve pointer input details and specify the response
  // about focus handling back to Shell Handwriting.
  // Called 'pending' because after we receive the args from Shell Handwriting
  // we must wait for an async round-trip callout to the renderer to know if
  // focus succeeded.
  Microsoft::WRL::ComPtr<::ITfFocusHandwritingTargetArgs> pending_target_args_;
  // Set when initiating view is destroyed before FocusHandwritingTarget is
  // called. When true, the next FocusHandwritingTarget immediately returns
  // TF_NO_HANDWRITING_TARGET.
  bool decline_next_target_ = false;
  // Stores the address of a DWORD valued used to correctly uninstall the advise
  // sink on the object destruction.
  DWORD sink_cookie_ = TF_INVALID_COOKIE;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_STYLUS_HANDWRITING_CALLBACK_SINK_WIN_H_
