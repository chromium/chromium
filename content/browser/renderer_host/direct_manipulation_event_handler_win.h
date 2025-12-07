// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_

#include <windows.h>

#include <directmanipulation.h>
#include <wrl.h>

#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/size.h"

namespace content {

class DirectManipulationHelper;
class DirectManipulationBrowserTestBase;
class DirectManipulationUnitTest;

// DirectManipulationEventHandler receives status update and gesture events from
// Direct Manipulation API.
class DirectManipulationEventHandler
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          Microsoft::WRL::Implements<
              Microsoft::WRL::RuntimeClassFlags<
                  Microsoft::WRL::RuntimeClassType::ClassicCom>,
              Microsoft::WRL::FtmBase,
              IDirectManipulationViewportEventHandler,
              IDirectManipulationInteractionEventHandler>> {
 public:
  explicit DirectManipulationEventHandler(
      base::WeakPtr<DirectManipulationHelper> helper);

  DirectManipulationEventHandler(const DirectManipulationEventHandler&) =
      delete;
  DirectManipulationEventHandler& operator=(
      const DirectManipulationEventHandler&) = delete;

  void SetViewportSizeInPixels(const gfx::Size& viewport_size_in_pixels);

  void SetDeviceScaleFactor(float device_scale_factor);

 private:
  friend class DirectManipulationBrowserTestBase;
  friend DirectManipulationUnitTest;

  // DirectManipulationEventHandler();
  ~DirectManipulationEventHandler() override;

  enum class GestureState { kNone, kScroll, kFling, kPinch };

  void TransitionToState(GestureState gesture);

  HRESULT STDMETHODCALLTYPE
  OnViewportStatusChanged(_In_ IDirectManipulationViewport* viewport,
                          _In_ DIRECTMANIPULATION_STATUS current,
                          _In_ DIRECTMANIPULATION_STATUS previous) override;

  HRESULT STDMETHODCALLTYPE
  OnViewportUpdated(_In_ IDirectManipulationViewport* viewport) override;

  HRESULT STDMETHODCALLTYPE
  OnContentUpdated(_In_ IDirectManipulationViewport* viewport,
                   _In_ IDirectManipulationContent* content) override;

  HRESULT STDMETHODCALLTYPE
  OnInteraction(_In_ IDirectManipulationViewport2* viewport,
                _In_ DIRECTMANIPULATION_INTERACTION_TYPE interaction) override;

  // Pointer to the DirectManipulationHelper that created this object. Since
  // this is a reference-counted COM object, it may outlive the
  // DirectManipulationHelper if other COM objects keep references to it.
  base::WeakPtr<DirectManipulationHelper> helper_;

  float device_scale_factor_ = 1.0f;
  float last_scale_ = 1.0f;
  int last_x_offset_ = 0;
  int last_y_offset_ = 0;
  bool should_send_scroll_begin_ = false;

  // Current recognized gesture from Direct Manipulation.
  GestureState gesture_state_ = GestureState::kNone;

  gfx::Size viewport_size_in_pixels_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DIRECT_MANIPULATION_EVENT_HANDLER_WIN_H_
