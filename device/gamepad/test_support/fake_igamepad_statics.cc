// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/test_support/fake_igamepad_statics.h"

#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

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
  if (add_gamepad_added_status_ != S_OK)
    return add_gamepad_added_status_;

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
  if (add_gamepad_removed_status_ != S_OK)
    return add_gamepad_removed_status_;

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
  size_t items_removed = base::EraseIf(
      gamepad_added_event_handler_map_,
      [=](const auto& entry) { return entry.first == token.value; });
  if (items_removed == 0)
    return E_FAIL;
  return S_OK;
}

HRESULT WINAPI
FakeIGamepadStatics::remove_GamepadRemoved(EventRegistrationToken token) {
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
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad_to_add) {
  SimulateGamepadEvent(
      gamepad_to_add,
      &FakeIGamepadStatics::TriggerGamepadAddedCallbackOnRandomThread);
}

void FakeIGamepadStatics::SimulateGamepadRemoved(
    const Microsoft::WRL::ComPtr<ABI::Windows::Gaming::Input::IGamepad>&
        gamepad_to_remove) {
  SimulateGamepadEvent(
      gamepad_to_remove,
      &FakeIGamepadStatics::TriggerGamepadRemovedCallbackOnRandomThread);
}

void FakeIGamepadStatics::SetAddGamepadAddedStatus(HRESULT value) {
  add_gamepad_added_status_ = value;
}

void FakeIGamepadStatics::SetAddGamepadRemovedStatus(HRESULT value) {
  add_gamepad_removed_status_ = value;
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

}  // namespace device
