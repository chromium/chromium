// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/win/virtual_core_window_impl.h"

#include <wrl.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace media {

using Microsoft::WRL::ComPtr;
using Microsoft::WRL::MakeAndInitialize;

namespace {

// Arbitrary non-null HWND value for testing. No real window is needed
// because VirtualCoreWindowImpl only stores and returns the handle.
static const HWND kFakeHwnd =
    reinterpret_cast<HWND>(static_cast<uintptr_t>(0x1234));

}  // namespace

TEST(VirtualCoreWindowImplTest, GetWindowHandleReturnsInitializedHwnd) {
  ComPtr<VirtualCoreWindowImpl> window;
  ASSERT_HRESULT_SUCCEEDED(
      MakeAndInitialize<VirtualCoreWindowImpl>(&window, kFakeHwnd));

  HWND result = nullptr;
  ASSERT_HRESULT_SUCCEEDED(window->get_WindowHandle(&result));
  EXPECT_EQ(result, kFakeHwnd);
}

TEST(VirtualCoreWindowImplTest, GetWindowHandleReturnsNullWhenInitializedNull) {
  ComPtr<VirtualCoreWindowImpl> window;
  ASSERT_HRESULT_SUCCEEDED(
      MakeAndInitialize<VirtualCoreWindowImpl>(&window, nullptr));

  HWND result = kFakeHwnd;
  ASSERT_HRESULT_SUCCEEDED(window->get_WindowHandle(&result));
  EXPECT_EQ(result, nullptr);
}

TEST(VirtualCoreWindowImplTest, QueryInterfaceForICoreWindow) {
  ComPtr<VirtualCoreWindowImpl> window;
  ASSERT_HRESULT_SUCCEEDED(
      MakeAndInitialize<VirtualCoreWindowImpl>(&window, kFakeHwnd));

  ComPtr<ABI::Windows::UI::Core::ICoreWindow> core_window;
  EXPECT_HRESULT_SUCCEEDED(window.As(&core_window));
}

TEST(VirtualCoreWindowImplTest, QueryInterfaceForICoreWindowInterop) {
  ComPtr<VirtualCoreWindowImpl> window;
  ASSERT_HRESULT_SUCCEEDED(
      MakeAndInitialize<VirtualCoreWindowImpl>(&window, kFakeHwnd));

  ComPtr<ICoreWindowInterop> interop;
  EXPECT_HRESULT_SUCCEEDED(window.As(&interop));
}

TEST(VirtualCoreWindowImplTest, UnimplementedMethodsReturnNotImpl) {
  ComPtr<VirtualCoreWindowImpl> window;
  ASSERT_HRESULT_SUCCEEDED(
      MakeAndInitialize<VirtualCoreWindowImpl>(&window, kFakeHwnd));

  // ICoreWindow property getters/setters.
  IInspectable* inspectable = nullptr;
  EXPECT_EQ(E_NOTIMPL, window->get_AutomationHostProvider(&inspectable));

  ABI::Windows::Foundation::Rect rect = {};
  EXPECT_EQ(E_NOTIMPL, window->get_Bounds(&rect));

  ABI::Windows::Foundation::Collections::IPropertySet* props = nullptr;
  EXPECT_EQ(E_NOTIMPL, window->get_CustomProperties(&props));

  ABI::Windows::UI::Core::ICoreDispatcher* dispatcher = nullptr;
  EXPECT_EQ(E_NOTIMPL, window->get_Dispatcher(&dispatcher));

  ABI::Windows::UI::Core::CoreWindowFlowDirection flow_dir;
  EXPECT_EQ(E_NOTIMPL, window->get_FlowDirection(&flow_dir));
  EXPECT_EQ(E_NOTIMPL,
            window->put_FlowDirection(
                ABI::Windows::UI::Core::CoreWindowFlowDirection_LeftToRight));

  boolean bool_val = false;
  EXPECT_EQ(E_NOTIMPL, window->get_IsInputEnabled(&bool_val));
  EXPECT_EQ(E_NOTIMPL, window->put_IsInputEnabled(false));

  ABI::Windows::UI::Core::ICoreCursor* cursor = nullptr;
  EXPECT_EQ(E_NOTIMPL, window->get_PointerCursor(&cursor));
  EXPECT_EQ(E_NOTIMPL, window->put_PointerCursor(nullptr));

  ABI::Windows::Foundation::Point point = {};
  EXPECT_EQ(E_NOTIMPL, window->get_PointerPosition(&point));

  EXPECT_EQ(E_NOTIMPL, window->get_Visible(&bool_val));

  // ICoreWindow methods.
  EXPECT_EQ(E_NOTIMPL, window->Activate());
  EXPECT_EQ(E_NOTIMPL, window->Close());

  ABI::Windows::UI::Core::CoreVirtualKeyStates key_state;
  EXPECT_EQ(E_NOTIMPL, window->GetAsyncKeyState(
                           ABI::Windows::System::VirtualKey_None, &key_state));
  EXPECT_EQ(E_NOTIMPL, window->GetKeyState(
                           ABI::Windows::System::VirtualKey_None, &key_state));

  EXPECT_EQ(E_NOTIMPL, window->ReleasePointerCapture());
  EXPECT_EQ(E_NOTIMPL, window->SetPointerCapture());

  // ICoreWindow event add/remove (all return E_NOTIMPL).
  EventRegistrationToken token = {};
  EXPECT_EQ(E_NOTIMPL, window->add_Activated(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_Activated(token));
  EXPECT_EQ(E_NOTIMPL,
            window->add_AutomationProviderRequested(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_AutomationProviderRequested(token));
  EXPECT_EQ(E_NOTIMPL, window->add_CharacterReceived(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_CharacterReceived(token));
  EXPECT_EQ(E_NOTIMPL, window->add_Closed(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_Closed(token));
  EXPECT_EQ(E_NOTIMPL, window->add_InputEnabled(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_InputEnabled(token));
  EXPECT_EQ(E_NOTIMPL, window->add_KeyDown(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_KeyDown(token));
  EXPECT_EQ(E_NOTIMPL, window->add_KeyUp(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_KeyUp(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerCaptureLost(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerCaptureLost(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerEntered(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerEntered(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerExited(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerExited(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerMoved(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerMoved(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerPressed(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerPressed(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerReleased(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerReleased(token));
  EXPECT_EQ(E_NOTIMPL, window->add_TouchHitTesting(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_TouchHitTesting(token));
  EXPECT_EQ(E_NOTIMPL, window->add_PointerWheelChanged(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_PointerWheelChanged(token));
  EXPECT_EQ(E_NOTIMPL, window->add_SizeChanged(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_SizeChanged(token));
  EXPECT_EQ(E_NOTIMPL, window->add_VisibilityChanged(nullptr, &token));
  EXPECT_EQ(E_NOTIMPL, window->remove_VisibilityChanged(token));

  // ICoreWindowInterop.
  EXPECT_EQ(E_NOTIMPL, window->put_MessageHandled(false));
}

}  // namespace media
