// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/stylus_handwriting_controller_win.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/win/windows_version.h"
#include "components/stylus_handwriting/win/features.h"
#include "content/browser/renderer_host/input/stylus_handwriting_callback_sink_win.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/display/win/screen_win.h"
#include "ui/events/win/stylus_handwriting_properties_win.h"

namespace content {

namespace {

StylusHandwritingControllerWin* g_instance = nullptr;
ITfThreadMgr* g_thread_manager_instance_for_testing = nullptr;
bool g_bind_interfaces_called_for_testing = false;

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
bool StylusHandwritingControllerWin::StylusHandwritingSupportedOnBuild() {
  const base::win::Version build = static_cast<base::win::Version>(
      base::win::OSInfo::GetInstance()->version_number().build);
  const uint32_t patch =
      base::win::OSInfo::GetInstance()->version_number().patch;
  // These range checks are helpful in determining safe builds for providing the
  // handwriting experience. We can't remove the version checks and solely rely
  // on the presence of handwriting APIs because there are currently builds that
  // have the handwriting APIs (which would cause the QI to pass), but lack some
  // necessary OS fixes. We don't want users to trigger handwriting on those
  // builds. Additionally, the Windows team can't backport fixes to all
  // handwriting supported builds due to code divergence.
  return (((build == base::win::Version::WIN11_22H2) ||
           (build == base::win::Version::WIN11_23H2)) &&
          patch >= 5126) ||
         ((build == base::win::Version::WIN11_24H2) && patch >= 3624) ||
         (build > base::win::Version::WIN11_24H2);
}

// static
void StylusHandwritingControllerWin::Initialize() {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);
  static bool handwriting_supported_on_winbuild =
      StylusHandwritingSupportedOnBuild();

  // We don't want to create a static instance with no destructor here because
  // the state can leak across test runs. In product builds, the controller is
  // not expected to leave until process shutdown.
  if (!stylus_handwriting::win::IsStylusHandwritingWinEnabled() ||
      !handwriting_supported_on_winbuild ||
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

int StylusHandwritingControllerWin::GetStylusHandwritingToleranceInDips(
    aura::Window& window) const {
  SIZE screen_size;

  // https://learn.microsoft.com/en-us/windows/win32/api/shellhandwriting/nf-shellhandwriting-itfhandwriting-gethandwritingdistancethreshold
  // TODO(crbug.com/355578906): Check with the Windows OS team if this value can
  // be 0.
  handwriting_->GetHandwritingDistanceThreshold(&screen_size);
  gfx::Size dip_size(display::win::GetScreenWin()->ScreenToDIPSize(
      window.GetHost()->GetAcceleratedWidget(),
      gfx::Size(screen_size.cx, screen_size.cy)));
  return std::max(dip_size.width(), dip_size.height());
}

void StylusHandwritingControllerWin::OnStartStylusWriting(
    OnFocusHandwritingTargetCallback callback,
    const ui::StylusHandwritingPropertiesWin& properties) {
  BOOL accepted;
  Microsoft::WRL::ComPtr<ITfHandwritingRequest> handwriting_request = nullptr;

  // https://learn.microsoft.com/en-us/windows/win32/api/shellhandwriting/nf-shellhandwriting-itfhandwriting-requesthandwritingforpointer
  HRESULT hr = handwriting_->RequestHandwritingForPointer(
      properties.handwriting_pointer_id, properties.handwriting_stroke_id,
      &accepted, &handwriting_request);

  if (SUCCEEDED(hr) && accepted && handwriting_request) {
    handwriting_callback_sink_->SetCallback(std::move(callback));
    handwriting_request->SetInputEvaluation(
        ::TfInputEvaluation::TF_IE_HANDWRITING);
  }

  base::UmaHistogramSparse("Stylus.Handwriting.RequestHandwritingForPointer",
                           hr);
}

void StylusHandwritingControllerWin::OnFocusHandled() {
  CHECK(handwriting_callback_sink_);
  handwriting_callback_sink_->OnFocusHandled();
}

void StylusHandwritingControllerWin::OnFocusFailed() {
  CHECK(handwriting_callback_sink_);
  handwriting_callback_sink_->OnFocusFailed();
}

// static
bool StylusHandwritingControllerWin::BindInterfacesCalledForTesting() {
  return g_bind_interfaces_called_for_testing;
}

void StylusHandwritingControllerWin::BindInterfaces() {
  // There can only be one StylusHandwritingControllerWin at any given time.
  CHECK(!g_instance);
  g_bind_interfaces_called_for_testing = true;
  if (const auto thread_manager = GetThreadManager()) {
    // https://learn.microsoft.com/en-us/windows/win32/api/shellhandwriting/nf-shellhandwriting-itfhandwriting-sethandwritingstate
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
