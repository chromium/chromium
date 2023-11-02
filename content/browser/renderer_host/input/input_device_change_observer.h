// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace content {
class RenderViewHostImpl;

// This class monitors input changes on all platforms.
//
// It is responsible to instantiate the various platforms observers
// and it gets notified whenever the input capabilities change. Whenever
// a change is detected the WebKit preferences are getting updated so the
// interactions media-queries can be updated.
class InputDeviceChangeObserver : public ui::InputDeviceEventObserver {
 public:
  InputDeviceChangeObserver(RenderViewHostImpl* rvh);

  InputDeviceChangeObserver(const InputDeviceChangeObserver&) = delete;
  InputDeviceChangeObserver& operator=(const InputDeviceChangeObserver&) =
      delete;

  ~InputDeviceChangeObserver() override;

  // InputDeviceEventObserver public overrides.
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

 private:
  // TODO(crbug.com/1298696): Breaks extensions_unittests.
  raw_ptr<RenderViewHostImpl, DegradeToNoOpWhenMTE> render_view_host_impl_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_
