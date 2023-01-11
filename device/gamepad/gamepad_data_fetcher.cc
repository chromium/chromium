// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/gamepad/gamepad_data_fetcher.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"

namespace device {

namespace {

void RunCallbackOnCallbackThread(
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    mojom::GamepadHapticsResult result) {
  std::move(callback).Run(result);
}

GamepadDataFetcher::HidManagerBinder& GetHidManagerBinder() {
  static base::NoDestructor<GamepadDataFetcher::HidManagerBinder> binder;
  return *binder;
}

}  // namespace

GamepadDataFetcher::GamepadDataFetcher() = default;

GamepadDataFetcher::~GamepadDataFetcher() = default;

// static
void GamepadDataFetcher::SetHidManagerBinder(HidManagerBinder binder) {
  GetHidManagerBinder() = std::move(binder);
}

void GamepadDataFetcher::InitializeProvider(GamepadPadStateProvider* provider) {
  DCHECK(provider);

  provider_ = provider;
  OnAddedToProvider();
}

void GamepadDataFetcher::BindHidManager(
    mojo::PendingReceiver<mojom::HidManager> receiver) {
  const auto& binder = GetHidManagerBinder();
  if (binder)
    binder.Run(std::move(receiver));
}

void GamepadDataFetcher::PlayEffect(
    int source_id,
    mojom::GamepadHapticEffectType type,
    mojom::GamepadEffectParametersPtr params,
    mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RunVibrationCallback(std::move(callback), std::move(callback_runner),
                       mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

void GamepadDataFetcher::ResetVibration(
    int source_id,
    mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner) {
  RunVibrationCallback(std::move(callback), std::move(callback_runner),
                       mojom::GamepadHapticsResult::GamepadHapticsResultError);
}

bool GamepadDataFetcher::DisconnectUnrecognizedGamepad(int source_id) {
  return false;
}

// static
int64_t GamepadDataFetcher::TimeInMicroseconds(base::TimeTicks update_time) {
  return update_time.since_origin().InMicroseconds();
}

// static
int64_t GamepadDataFetcher::CurrentTimeInMicroseconds() {
  return TimeInMicroseconds(base::TimeTicks::Now());
}

// static
void GamepadDataFetcher::UpdateGamepadStrings(const std::string& product_name,
                                              uint16_t vendor_id,
                                              uint16_t product_id,
                                              bool has_standard_mapping,
                                              Gamepad& pad) {
  // The ID contains the device name, vendor and product IDs,
  // and an indication of whether the standard mapping is in use.
  std::string id = base::StringPrintf(
      "%s (%sVendor: %04x Product: %04x)", product_name.c_str(),
      has_standard_mapping ? "STANDARD GAMEPAD " : "", vendor_id, product_id);
  pad.SetID(base::UTF8ToUTF16(id));

  // Set GamepadMapping::kStandard if the gamepad has a standard mapping, or
  // GamepadMapping::kNone otherwise.
  pad.mapping =
      has_standard_mapping ? GamepadMapping::kStandard : GamepadMapping::kNone;
}

// static
void GamepadDataFetcher::RunVibrationCallback(
    base::OnceCallback<void(mojom::GamepadHapticsResult)> callback,
    scoped_refptr<base::SequencedTaskRunner> callback_runner,
    mojom::GamepadHapticsResult result) {
  callback_runner->PostTask(FROM_HERE,
                            base::BindOnce(&RunCallbackOnCallbackThread,
                                           std::move(callback), result));
}

GamepadDataFetcherFactory::GamepadDataFetcherFactory() = default;

}  // namespace device
