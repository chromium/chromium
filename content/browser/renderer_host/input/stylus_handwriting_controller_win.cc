// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"

namespace content {

namespace {

StylusHandwritingControllerWin* g_instance = nullptr;
ITfThreadMgr* g_thread_manager_instance_for_testing = nullptr;

}  // namespace

// static
base::ScopedClosureRunner StylusHandwritingControllerWin::InitializeForTesting(
    ITfThreadMgr* thread_manager) {
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

void StylusHandwritingControllerWin::OnStartStylusWriting(
    OnFocusHandwritingTargetCallback callback,
    uint32_t pointer_id,
    uint64_t stroke_id,
    ui::TextInputClient& text_input_client) {
  // TODO(crbug.com/355578906): Implement
  // SetInputEvaluation(::TfInputEvaluation::TF_IE_HANDWRITING) logic.
}

void StylusHandwritingControllerWin::OnFocusHandled(
    ui::TextInputClient& text_input_client) {
  // TODO(crbug.com/355578906): Forward call to ::ITfHandwritingSink controller.
}

void StylusHandwritingControllerWin::OnFocusFailed(
    ui::TextInputClient& text_input_client) {
  // TODO(crbug.com/355578906): Forward call to ::ITfHandwritingSink controller.
}

void StylusHandwritingControllerWin::BindInterfaces() {
  // There can only be one StylusHandwritingControllerWin at any given time.
  CHECK(!g_instance);
  if (const auto thread_manager = GetThreadManager()) {
    const bool initialized_successfully =
        SUCCEEDED(thread_manager.As(&handwriting_)) &&
        SUCCEEDED(handwriting_->SetHandwritingState(
            ::TF_HANDWRITING_POINTERDELIVERY));
    if (initialized_successfully) {
      g_instance = this;
    }
  }
}

}  // namespace content
