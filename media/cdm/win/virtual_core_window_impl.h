// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_WIN_VIRTUAL_CORE_WINDOW_IMPL_H_
#define MEDIA_CDM_WIN_VIRTUAL_CORE_WINDOW_IMPL_H_

#include <CoreWindow.h>
#include <mfmediaengine.h>
#include <windows.ui.core.h>
#include <wrl.h>

#include "media/base/media_export.h"

namespace media {

// A virtual/mock CoreWindow implementation that only supports
// `ICoreWindowInterop::get_WindowHandle()`. This is used to provide an
// `ICoreWindow` object that can be set as the `MF_MEDIA_ENGINE_COREWINDOW`
// attribute on Media Foundation's `IMFExtendedDRMTypeSupport` interface so
// that Media Foundation can pick a GPU adapter based on the HWND's monitor.
// Note: `VirtualCoreWindowImpl` does not own the wrapped HWND.
class MEDIA_EXPORT VirtualCoreWindowImpl final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::WinRtClassicComMix |
              Microsoft::WRL::InhibitRoOriginateError>,
          ABI::Windows::UI::Core::ICoreWindow,
          ICoreWindowInterop,
          Microsoft::WRL::FtmBase> {
 public:
  VirtualCoreWindowImpl();
  VirtualCoreWindowImpl(const VirtualCoreWindowImpl&) = delete;
  VirtualCoreWindowImpl& operator=(const VirtualCoreWindowImpl&) = delete;
  ~VirtualCoreWindowImpl() final;

  // May be `nullptr`, in which case `get_WindowHandle()` will report
  // `nullptr` and Media Foundation will fall back to the primary adapter.
  HRESULT RuntimeClassInitialize(HWND hwnd);

  // ICoreWindow.
  IFACEMETHODIMP get_AutomationHostProvider(
      __RPC__deref_out_opt IInspectable** value);
  IFACEMETHODIMP get_Bounds(__RPC__out ABI::Windows::Foundation::Rect* value);
  IFACEMETHODIMP get_CustomProperties(
      __RPC__deref_out_opt ABI::Windows::Foundation::Collections::IPropertySet**
          value);
  IFACEMETHODIMP get_Dispatcher(
      __RPC__deref_out_opt ABI::Windows::UI::Core::ICoreDispatcher** value);
  IFACEMETHODIMP get_FlowDirection(
      __RPC__out ABI::Windows::UI::Core::CoreWindowFlowDirection* value);
  IFACEMETHODIMP put_FlowDirection(
      ABI::Windows::UI::Core::CoreWindowFlowDirection value);
  IFACEMETHODIMP get_IsInputEnabled(__RPC__out boolean* value);
  IFACEMETHODIMP put_IsInputEnabled(boolean value);
  IFACEMETHODIMP get_PointerCursor(
      __RPC__deref_out_opt ABI::Windows::UI::Core::ICoreCursor** value);
  IFACEMETHODIMP put_PointerCursor(
      __RPC__in_opt ABI::Windows::UI::Core::ICoreCursor* value);
  IFACEMETHODIMP get_PointerPosition(
      __RPC__out ABI::Windows::Foundation::Point* value);
  IFACEMETHODIMP get_Visible(__RPC__out boolean* value);
  IFACEMETHODIMP Activate(void);
  IFACEMETHODIMP Close(void);
  IFACEMETHODIMP GetAsyncKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      __RPC__out ABI::Windows::UI::Core::CoreVirtualKeyStates* KeyState);
  IFACEMETHODIMP GetKeyState(
      ABI::Windows::System::VirtualKey virtualKey,
      __RPC__out ABI::Windows::UI::Core::CoreVirtualKeyStates* KeyState);
  IFACEMETHODIMP ReleasePointerCapture(void);
  IFACEMETHODIMP SetPointerCapture(void);
  IFACEMETHODIMP add_Activated(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::WindowActivatedEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_Activated(EventRegistrationToken cookie);
  IFACEMETHODIMP add_AutomationProviderRequested(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::AutomationProviderRequestedEventArgs*>*
          handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_AutomationProviderRequested(
      EventRegistrationToken cookie);
  IFACEMETHODIMP add_CharacterReceived(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::CharacterReceivedEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_CharacterReceived(EventRegistrationToken cookie);
  IFACEMETHODIMP add_Closed(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::CoreWindowEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_Closed(EventRegistrationToken cookie);
  IFACEMETHODIMP add_InputEnabled(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::InputEnabledEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_InputEnabled(EventRegistrationToken cookie);
  IFACEMETHODIMP add_KeyDown(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::KeyEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_KeyDown(EventRegistrationToken cookie);
  IFACEMETHODIMP add_KeyUp(ABI::Windows::Foundation::ITypedEventHandler<
                               ABI::Windows::UI::Core::CoreWindow*,
                               ABI::Windows::UI::Core::KeyEventArgs*>* handler,
                           EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_KeyUp(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerCaptureLost(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerCaptureLost(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerEntered(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerEntered(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerExited(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerExited(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerMoved(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerMoved(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerPressed(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerPressed(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerReleased(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerReleased(EventRegistrationToken cookie);
  IFACEMETHODIMP add_TouchHitTesting(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::TouchHitTestingEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_TouchHitTesting(EventRegistrationToken cookie);
  IFACEMETHODIMP add_PointerWheelChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::PointerEventArgs*>* handler,
      EventRegistrationToken* cookie);
  IFACEMETHODIMP remove_PointerWheelChanged(EventRegistrationToken cookie);
  IFACEMETHODIMP add_SizeChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::WindowSizeChangedEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_SizeChanged(EventRegistrationToken cookie);
  IFACEMETHODIMP add_VisibilityChanged(
      ABI::Windows::Foundation::ITypedEventHandler<
          ABI::Windows::UI::Core::CoreWindow*,
          ABI::Windows::UI::Core::VisibilityChangedEventArgs*>* handler,
      EventRegistrationToken* pCookie);
  IFACEMETHODIMP remove_VisibilityChanged(EventRegistrationToken cookie);

  // ICoreWindowInterop.
  IFACEMETHODIMP get_WindowHandle(__RPC__deref_out_opt HWND* hwnd);
  HRESULT STDMETHODCALLTYPE put_MessageHandled(boolean value) final;

 private:
  HWND hwnd_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_CDM_WIN_VIRTUAL_CORE_WINDOW_IMPL_H_
