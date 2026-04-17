// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUT_H_
#define DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUT_H_

#include <GameInput.h>
#include <wrl.h>

#include "base/memory/raw_ptr.h"

namespace device {

class FakeIGameInput final
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IGameInput> {
 public:
  FakeIGameInput();
  FakeIGameInput(const FakeIGameInput&) = delete;
  FakeIGameInput& operator=(const FakeIGameInput&) = delete;

  // IGameInput fake implementation
  uint64_t STDMETHODCALLTYPE GetCurrentTimestamp() override;
  HRESULT STDMETHODCALLTYPE
  GetCurrentReading(GameInputKind inputKind,
                    IGameInputDevice* device,
                    IGameInputReading** reading) override;
  HRESULT STDMETHODCALLTYPE GetNextReading(IGameInputReading*,
                                           GameInputKind,
                                           IGameInputDevice*,
                                           IGameInputReading**) override;
  HRESULT STDMETHODCALLTYPE GetPreviousReading(IGameInputReading*,
                                               GameInputKind,
                                               IGameInputDevice*,
                                               IGameInputReading**) override;
  HRESULT STDMETHODCALLTYPE GetTemporalReading(uint64_t,
                                               IGameInputDevice*,
                                               IGameInputReading**) override;
  HRESULT STDMETHODCALLTYPE
  RegisterReadingCallback(IGameInputDevice*,
                          GameInputKind,
                          float,
                          void*,
                          GameInputReadingCallback,
                          GameInputCallbackToken*) override;
  HRESULT STDMETHODCALLTYPE
  RegisterDeviceCallback(IGameInputDevice* device,
                         GameInputKind inputKind,
                         GameInputDeviceStatus statusFilter,
                         GameInputEnumerationKind enumerationKind,
                         void* context,
                         GameInputDeviceCallback callbackFunc,
                         GameInputCallbackToken* callbackToken) override;
  HRESULT STDMETHODCALLTYPE
  RegisterSystemButtonCallback(IGameInputDevice*,
                               GameInputSystemButtons,
                               void*,
                               GameInputSystemButtonCallback,
                               GameInputCallbackToken*) override;
  HRESULT STDMETHODCALLTYPE
  RegisterKeyboardLayoutCallback(IGameInputDevice*,
                                 void*,
                                 GameInputKeyboardLayoutCallback,
                                 GameInputCallbackToken*) override;
  void STDMETHODCALLTYPE StopCallback(GameInputCallbackToken) override;
  bool STDMETHODCALLTYPE UnregisterCallback(GameInputCallbackToken,
                                            uint64_t) override;
  HRESULT STDMETHODCALLTYPE CreateDispatcher(IGameInputDispatcher**) override;
  HRESULT STDMETHODCALLTYPE CreateAggregateDevice(GameInputKind,
                                                  IGameInputDevice**) override;
  HRESULT STDMETHODCALLTYPE FindDeviceFromId(APP_LOCAL_DEVICE_ID const*,
                                             IGameInputDevice**) override;
  HRESULT STDMETHODCALLTYPE FindDeviceFromObject(IUnknown*,
                                                 IGameInputDevice**) override;
  HRESULT STDMETHODCALLTYPE
  FindDeviceFromPlatformHandle(HANDLE, IGameInputDevice**) override;
  HRESULT STDMETHODCALLTYPE
  FindDeviceFromPlatformString(LPCWSTR, IGameInputDevice**) override;
  HRESULT STDMETHODCALLTYPE EnableOemDeviceSupport(uint16_t,
                                                   uint16_t,
                                                   uint8_t,
                                                   uint8_t) override;
  void STDMETHODCALLTYPE SetFocusPolicy(GameInputFocusPolicy) override;

  // Test methods
  bool IsCallbackSet();
  void InvokeDeviceCallback(IGameInputDevice* device,
                            GameInputDeviceStatus status);
  void AssignMockReading(IGameInputReading* reading);
  bool IsSystemButtonCallbackSet();
  void InvokeSystemButtonCallback(IGameInputDevice* device, bool is_pressed);

 private:
  ~FakeIGameInput() override;

  GameInputDeviceCallback device_callback_ = nullptr;
  raw_ptr<void> device_callback_context_ = nullptr;
  GameInputSystemButtonCallback system_button_callback_ = nullptr;
  raw_ptr<void> system_button_callback_context_ = nullptr;
  bool is_system_button_pressed_ = false;
  Microsoft::WRL::ComPtr<IGameInputReading> mock_reading_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_TEST_SUPPORT_FAKE_IGAMEINPUT_H_
