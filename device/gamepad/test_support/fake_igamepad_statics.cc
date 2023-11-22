// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igamepad_statics.h"

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "device/gamepad/test_support/fake_igamepad.h"
#include "device/gamepad/test_support/fake_winrt_wgi_environment.h"

namespace device {

FakeIGamepadStatics::FakeIGamepadStatics() = default;

FakeIGamepadStatics::~FakeIGamepadStatics() = default;

// static
FakeIGamepadStatics* FakeIGamepadStatics::GetInstance() {
  static FakeIGamepadStatics* instance;
  if (!instance)
    instance = Microsoft::WRL::Make<FakeIGamepadStatics>().Detach();
  return instance;
}

HRESULT WINAPI FakeIGamepadStatics::add_GamepadAdded(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::Gamepad*>* event_handler,
    EventRegistrationToken* token) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kGamepadAddGamepadAddedFailed) {
    return E_FAIL;
  }

  token->value = next_event_registration_token_++;

  auto ret = gamepad_added_event_handler_map_.insert(
      {token->value,
       Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
           ABI::Windows::Gaming::Input::Gamepad*>>{event_handler}});
  if (ret.second)
    return S_OK;
  return E_FAIL;
}

HRESULT WINAPI FakeIGamepadStatics::add_GamepadRemoved(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::Gamepad*>* event_handler,
    EventRegistrationToken* token) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kGamepadAddGamepadRemovedFailed) {
    return E_FAIL;
  }

  token->value = next_event_registration_token_++;

  auto ret = gamepad_removed_event_handler_map_.insert(
      {token->value,
       Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IEventHandler<
           ABI::Windows::Gaming::Input::Gamepad*>>{event_handler}});
  if (ret.second)
    return S_OK;
  return E_FAIL;
}

HRESULT WINAPI
FakeIGamepadStatics::remove_GamepadAdded(EventRegistrationToken token) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kGamepadRemoveGamepadAddedFailed) {
    return E_FAIL;
  }
  size_t items_removed = base::EraseIf(
      gamepad_added_event_handler_map_,
      [=](const auto& entry) { return entry.first == token.value; });
  if (items_removed == 0)
    return E_FAIL;
  return S_OK;
}

HRESULT WINAPI
FakeIGamepadStatics::remove_GamepadRemoved(EventRegistrationToken token) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kGamepadRemoveGamepadRemovedFailed) {
    return E_FAIL;
  }
  size_t items_removed = base::EraseIf(
      gamepad_removed_event_handler_map_,
      [=](const auto& entry) { return entry.first == token.value; });
  if (items_removed == 0)
    return E_FAIL;
  return S_OK;
}

HRESULT WINAPI FakeIGamepadStatics::get_Gamepads(
    ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Gaming::Input::Gamepad*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::add_RawGameControllerAdded(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::RawGameController*>* value,
    EventRegistrationToken* token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::remove_RawGameControllerAdded(
    EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::add_RawGameControllerRemoved(
    ABI::Windows::Foundation::IEventHandler<
        ABI::Windows::Gaming::Input::RawGameController*>* value,
    EventRegistrationToken* token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::remove_RawGameControllerRemoved(
    EventRegistrationToken token) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::get_RawGameControllers(
    ABI::Windows::Foundation::Collections::IVectorView<
        ABI::Windows::Gaming::Input::RawGameController*>** value) {
  NOTIMPLEMENTED();
  return E_NOTIMPL;
}

HRESULT FakeIGamepadStatics::FromGameController(
    ABI::Windows::Gaming::Input::IGameController* gameController,
    ABI::Windows::Gaming::Input::IRawGameController** value) {
  if (FakeWinrtWgiEnvironment::GetError() ==
      WgiTestErrorCode::kErrorWgiRawGameControllerFromGameControllerFailed) {
    return E_FAIL;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad> gamepad;
  gameController->QueryInterface(IID_PPV_ARGS(&gamepad));
  Microsoft::WRL::ComPtr<device::FakeIGamepad> fake_gamepad;
  fake_gamepad = static_cast<FakeIGamepad*>(gamepad.Get());
  fake_raw_game_controller_map_[fake_gamepad->GetId()].CopyTo(value);
  return S_OK;
}
void FakeIGamepadStatics::SimulateGamepadEvent(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad,
    GamepadEventTriggerCallback callback) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  base::RunLoop run_loop;
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(callback, base::Unretained(this), gamepad));
  task_runner->PostTask(FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

void FakeIGamepadStatics::SimulateGamepadAdded(
    const Microsoft::WRL::ComPtr<FakeIGamepad>& gamepad_to_add,
    uint16_t hardware_product_id,
    uint16_t hardware_vendor_id,
    std::string_view display_name) {
  CacheGamepad(gamepad_to_add, hardware_product_id, hardware_vendor_id,
               display_name);
  SimulateGamepadEvent(
      gamepad_to_add,
      &FakeIGamepadStatics::TriggerGamepadAddedCallbackOnRandomThread);
}

void FakeIGamepadStatics::SimulateGamepadRemoved(
    const Microsoft::WRL::ComPtr<FakeIGamepad>& gamepad_to_remove) {
  SimulateGamepadEvent(
      gamepad_to_remove,
      &FakeIGamepadStatics::TriggerGamepadRemovedCallbackOnRandomThread);
}

size_t FakeIGamepadStatics::GetGamepadAddedEventHandlerCount() const {
  return gamepad_added_event_handler_map_.size();
}

size_t FakeIGamepadStatics::GetGamepadRemovedEventHandlerCount() const {
  return gamepad_removed_event_handler_map_.size();
}

void FakeIGamepadStatics::TriggerGamepadAddedCallbackOnRandomThread(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
        gamepad_to_add) {
  for (const auto& it : gamepad_added_event_handler_map_) {
    // Invokes the callback on a random thread.
    it.second->Invoke(
        static_cast<ABI::Windows::Gaming::Input::IGamepadStatics*>(this),
        gamepad_to_add.Get());
  }
}

void FakeIGamepadStatics::TriggerGamepadRemovedCallbackOnRandomThread(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>
        gamepad_to_remove) {
  for (auto& it : gamepad_removed_event_handler_map_) {
    // Invokes the callback on a random thread.
    it.second->Invoke(
        static_cast<ABI::Windows::Gaming::Input::IGamepadStatics*>(this),
        gamepad_to_remove.Get());
  }
}

void FakeIGamepadStatics::CacheGamepad(
    Microsoft::WRL::ComPtr<FakeIGamepad> fake_gamepad_to_add,
    uint16_t hardware_product_id,
    uint16_t hardware_vendor_id,
    std::string_view display_name) {
  uint64_t gamepad_id = next_gamepad_id_++;

  fake_gamepad_to_add->SetId(gamepad_id);
  fake_gamepad_map_[gamepad_id] = fake_gamepad_to_add;

  Microsoft::WRL::ComPtr<FakeIRawGameController>
      fake_raw_game_controller_to_add =
          Microsoft::WRL::Make<FakeIRawGameController>(
              gamepad_id, hardware_product_id, hardware_vendor_id,
              display_name);
  fake_raw_game_controller_to_add->set_id(gamepad_id);
  fake_raw_game_controller_map_[gamepad_id] = fake_raw_game_controller_to_add;
}

void FakeIGamepadStatics::RemoveCachedGamepad(
    const Microsoft::WRL::ComPtr<FakeIGamepad>& fake_gamepad_to_remove) {
  uint64_t gamepad_id = fake_gamepad_to_remove->GetId();
  fake_gamepad_map_.erase(gamepad_id);
  fake_raw_game_controller_map_.erase(gamepad_id);
}

}  // namespace device
