// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_
#define DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_

#include <windows.perception.h>
#include <windows.ui.input.spatial.h>
#include <wrl.h>

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/macros.h"

namespace device {
class WMRInputSourceState;
class WMRInputSourceEventArgs {
 public:
  virtual ~WMRInputSourceEventArgs() = default;

  virtual ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind
  PressKind() const = 0;
  virtual std::unique_ptr<WMRInputSourceState> State() const = 0;
};

class WMRInputSourceEventArgsImpl : public WMRInputSourceEventArgs {
 public:
  explicit WMRInputSourceEventArgsImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs>
          args);
  ~WMRInputSourceEventArgsImpl() override;

  ABI::Windows::UI::Input::Spatial::SpatialInteractionPressKind PressKind()
      const override;
  std::unique_ptr<WMRInputSourceState> State() const override;

 private:
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs>
      args_;
  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs2>
      args2_;
};

class WMRInputManager {
 public:
  using InputEventCallbackList =
      base::CallbackList<void(const WMRInputSourceEventArgs&)>;
  using InputEventCallback =
      base::RepeatingCallback<void(const WMRInputSourceEventArgs&)>;

  virtual ~WMRInputManager() = default;

  virtual std::vector<std::unique_ptr<WMRInputSourceState>>
  GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) = 0;

  virtual std::unique_ptr<InputEventCallbackList::Subscription>
  AddPressedCallback(const InputEventCallback& cb) = 0;

  virtual std::unique_ptr<InputEventCallbackList::Subscription>
  AddReleasedCallback(const InputEventCallback& cb) = 0;
};

class WMRInputManagerImpl : public WMRInputManager {
 public:
  using InputEventCallbackList =
      base::CallbackList<void(const WMRInputSourceEventArgs&)>;
  using InputEventCallback =
      base::RepeatingCallback<void(const WMRInputSourceEventArgs&)>;

  explicit WMRInputManagerImpl(
      Microsoft::WRL::ComPtr<
          ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
          manager);
  ~WMRInputManagerImpl() override;

  std::vector<std::unique_ptr<WMRInputSourceState>>
  GetDetectedSourcesAtTimestamp(
      Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp>
          timestamp) override;

  std::unique_ptr<InputEventCallbackList::Subscription> AddPressedCallback(
      const InputEventCallback& cb) override;

  std::unique_ptr<InputEventCallbackList::Subscription> AddReleasedCallback(
      const InputEventCallback& cb) override;

 private:
  void SubscribeEvents();
  void UnsubscribeEvents();

  HRESULT OnSourcePressed(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);
  HRESULT OnSourceReleased(
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager* sender,
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionSourceEventArgs*
          args);

  Microsoft::WRL::ComPtr<
      ABI::Windows::UI::Input::Spatial::ISpatialInteractionManager>
      manager_;

  EventRegistrationToken pressed_token_;
  EventRegistrationToken released_token_;
  InputEventCallbackList pressed_callback_list_;
  InputEventCallbackList released_callback_list_;

  DISALLOW_COPY_AND_ASSIGN(WMRInputManagerImpl);
};
}  // namespace device

#endif  // DEVICE_VR_WINDOWS_MIXED_REALITY_WRAPPERS_WMR_INPUT_MANAGER_H_
