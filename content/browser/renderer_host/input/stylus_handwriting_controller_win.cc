// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/events/win/stylus_handwriting_properties_win.h"

namespace content {

namespace {

StylusHandwritingControllerWin* g_instance = nullptr;
ITfThreadMgr* g_thread_manager_instance_for_testing = nullptr;

}  // namespace

// static
base::ScopedClosureRunner StylusHandwritingControllerWin::InitializeForTesting(
    ITfThreadMgr* thread_manager) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // A RAII class that automatically disposes of test-only global state, to
  // hide all implementation details from the caller.
  class StateForTesting {
   public:
    explicit StateForTesting(ITfThreadMgr* thread_manager) {
      CHECK(!g_thread_manager_instance_for_testing);
      CHECK(!g_instance);
      g_thread_manager_instance_for_testing = thread_manager;
      controller_ = base::WrapUnique(new StylusHandwritingControllerWin());
    }
    ~StateForTesting() { g_thread_manager_instance_for_testing = nullptr; }

   private:
    std::unique_ptr<StylusHandwritingControllerWin> controller_;
  };

  // The scoped closure runner guarantees that the closure will be executed and
  // the state, saved as a bound parameter, will be disposed of by unique_ptr.
  return base::ScopedClosureRunner(
      base::BindOnce([](std::unique_ptr<StateForTesting> state) {},
                     std::make_unique<StateForTesting>(thread_manager)));
}

// static
bool StylusHandwritingControllerWin::IsHandwritingAPIAvailable() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!stylus_handwriting::win::IsStylusHandwritingWinEnabled()) {
    return false;
  }

  return GetInstance();
}

// static
void StylusHandwritingControllerWin::Initialize() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // We don't want to create a static instance with no destructor here because
  // the state can leak across test runs. In product builds, the controller is
  // not expected to leave until process shutdown.
  if (!stylus_handwriting::win::IsStylusHandwritingWinEnabled() ||
      g_thread_manager_instance_for_testing) {
    return;
  }

  // See the constructor and BindInterfaces() for more initialization details
  // where g_instance is set.
  static base::NoDestructor<StylusHandwritingControllerWin> instance;
}

// static
StylusHandwritingControllerWin* StylusHandwritingControllerWin::GetInstance() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(stylus_handwriting::win::IsStylusHandwritingWinEnabled());
  return g_instance;
}

// static
Microsoft::WRL::ComPtr<ITfThreadMgr>
StylusHandwritingControllerWin::GetThreadManager() {
  if (g_thread_manager_instance_for_testing) {
    return g_thread_manager_instance_for_testing;
  }

  return ui::TSFBridge::GetInstance()
             ? ui::TSFBridge::GetInstance()->GetThreadManager()
             : nullptr;
}

StylusHandwritingControllerWin::StylusHandwritingControllerWin() {
  BindInterfaces();
}

StylusHandwritingControllerWin::~StylusHandwritingControllerWin() {
  g_instance = nullptr;
}

Microsoft::WRL::ComPtr<StylusHandwritingCallbackSinkWin>
StylusHandwritingControllerWin::GetCallbackSinkForTesting() const {
  return handwriting_callback_sink_;
}

void StylusHandwritingControllerWin::OnStartStylusWriting(
    OnFocusHandwritingTargetCallback callback,
    const ui::StylusHandwritingPropertiesWin& properties,
    ui::TextInputClient& text_input_client) {
  BOOL accepted;
  Microsoft::WRL::ComPtr<ITfHandwritingRequest> handwriting_request = nullptr;
  HRESULT hr = handwriting_->RequestHandwritingForPointer(
      properties.handwriting_pointer_id, properties.handwriting_stroke_id,
      &accepted, &handwriting_request);

  if (SUCCEEDED(hr) && accepted) {
    handwriting_callback_sink_->SetCallback(std::move(callback));
    handwriting_request->SetInputEvaluation(
        ::TfInputEvaluation::TF_IE_HANDWRITING);
  }

  // TODO(crbug.com/355578906): Record instances when
  // RequestHandwritingForPointer() failed.
}

void StylusHandwritingControllerWin::OnFocusHandled(
    ui::TextInputClient& text_input_client) {
  CHECK(handwriting_callback_sink_);
  handwriting_callback_sink_->OnFocusHandled();
}

void StylusHandwritingControllerWin::OnFocusFailed(
    ui::TextInputClient& text_input_client) {
  CHECK(handwriting_callback_sink_);
  handwriting_callback_sink_->OnFocusFailed();
}

void StylusHandwritingControllerWin::BindInterfaces() {
  // There can only be one StylusHandwritingControllerWin at any given time.
  CHECK(!g_instance);
  if (const auto thread_manager = GetThreadManager()) {
    const bool initialized_successfully =
        SUCCEEDED(thread_manager.As(&handwriting_)) &&
        SUCCEEDED(handwriting_->SetHandwritingState(
            ::TF_HANDWRITING_POINTERDELIVERY)) &&
        SUCCEEDED(
            Microsoft::WRL::Details::MakeAndInitialize<
                StylusHandwritingCallbackSinkWin>(&handwriting_callback_sink_));
    if (initialized_successfully) {
      g_instance = this;
    }
  }
}

}  // namespace content
