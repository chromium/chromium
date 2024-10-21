// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "components/stylus_handwriting/win/features.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"

namespace content {

namespace {

StylusHandwritingControllerWin* g_instance = nullptr;
StylusHandwritingControllerWin* g_instance_for_testing = nullptr;

}  // namespace

// static
base::AutoReset<StylusHandwritingControllerWin*>
StylusHandwritingControllerWin::SetInstanceForTesting(
    StylusHandwritingControllerWin* instance) {
  return {&g_instance_for_testing, instance};
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
  if (!stylus_handwriting::win::IsStylusHandwritingWinEnabled() ||
      g_instance_for_testing) {
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
  return g_instance_for_testing ? g_instance_for_testing : g_instance;
}

// static
Microsoft::WRL::ComPtr<ITfThreadMgr>
StylusHandwritingControllerWin::GetThreadManager() {
  return ui::TSFBridge::GetInstance()
             ? ui::TSFBridge::GetInstance()->GetThreadManager()
             : nullptr;
}

StylusHandwritingControllerWin::StylusHandwritingControllerWin() {
  BindInterfaces();
}

StylusHandwritingControllerWin::~StylusHandwritingControllerWin() = default;

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
  if (auto thread_manager = GetThreadManager()) {
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
