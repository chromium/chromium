// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/virtual_core_window_impl.h"

#include <mfapi.h>

#include "media/base/win/mf_helpers.h"

namespace media {

VirtualCoreWindowImpl::VirtualCoreWindowImpl() {
  DVLOG_FUNC(1);
}

VirtualCoreWindowImpl::~VirtualCoreWindowImpl() {
  DVLOG_FUNC(1);
  // The HWND is owned by the caller (typically the browser process); do not
  // destroy it here.
}

HRESULT VirtualCoreWindowImpl::RuntimeClassInitialize(HWND hwnd) {
  DVLOG_FUNC(1) << "hwnd=" << hwnd;
  hwnd_ = hwnd;
  return S_OK;
}

// ICoreWindow.
HRESULT VirtualCoreWindowImpl::get_AutomationHostProvider(
    IInspectable** value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_Bounds(
    ABI::Windows::Foundation::Rect* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_CustomProperties(
    ABI::Windows::Foundation::Collections::IPropertySet** value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_Dispatcher(

    ABI::Windows::UI::Core::ICoreDispatcher** value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_FlowDirection(

    ABI::Windows::UI::Core::CoreWindowFlowDirection* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::put_FlowDirection(
    ABI::Windows::UI::Core::CoreWindowFlowDirection value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_IsInputEnabled(boolean* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::put_IsInputEnabled(boolean value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_PointerCursor(

    ABI::Windows::UI::Core::ICoreCursor** value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::put_PointerCursor(
    ABI::Windows::UI::Core::ICoreCursor* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_PointerPosition(
    ABI::Windows::Foundation::Point* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::get_Visible(boolean* value) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::Activate(void) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::Close(void) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::GetAsyncKeyState(
    ABI::Windows::System::VirtualKey virtualKey,
    ABI::Windows::UI::Core::CoreVirtualKeyStates* KeyState) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::GetKeyState(
    ABI::Windows::System::VirtualKey virtualKey,
    ABI::Windows::UI::Core::CoreVirtualKeyStates* KeyState) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::ReleasePointerCapture(void) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::SetPointerCapture(void) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_Activated(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::WindowActivatedEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_Activated(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::add_AutomationProviderRequested(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::AutomationProviderRequestedEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::remove_AutomationProviderRequested(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_CharacterReceived(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::CharacterReceivedEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::remove_CharacterReceived(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_Closed(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::CoreWindowEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_Closed(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_InputEnabled(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::InputEnabledEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_InputEnabled(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_KeyDown(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::KeyEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_KeyDown(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_KeyUp(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::KeyEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_KeyUp(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerCaptureLost(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::remove_PointerCaptureLost(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerEntered(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_PointerEntered(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerExited(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_PointerExited(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerMoved(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_PointerMoved(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerPressed(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_PointerPressed(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerReleased(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_PointerReleased(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_TouchHitTesting(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::TouchHitTestingEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_TouchHitTesting(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_PointerWheelChanged(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::PointerEventArgs*>* handler,
    EventRegistrationToken* cookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::remove_PointerWheelChanged(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_SizeChanged(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::WindowSizeChangedEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::remove_SizeChanged(
    EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

HRESULT VirtualCoreWindowImpl::add_VisibilityChanged(
    ABI::Windows::Foundation::ITypedEventHandler<
        ABI::Windows::UI::Core::CoreWindow*,
        ABI::Windows::UI::Core::VisibilityChangedEventArgs*>* handler,
    EventRegistrationToken* pCookie) {
  return E_NOTIMPL;
}

HRESULT
VirtualCoreWindowImpl::remove_VisibilityChanged(EventRegistrationToken cookie) {
  return E_NOTIMPL;
}

// ICoreWindowInterop.
HRESULT VirtualCoreWindowImpl::get_WindowHandle(HWND* hwnd) {
  DVLOG_FUNC(1) << "hwnd=" << hwnd_;
  *hwnd = hwnd_;
  return S_OK;
}
HRESULT VirtualCoreWindowImpl::put_MessageHandled(boolean value) {
  return E_NOTIMPL;
}

}  // namespace media
