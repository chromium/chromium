// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ui/events/devices/input_device_event_observer.h"

namespace content {
class RenderViewHostImpl;

// This class monitors input changes on all platforms.
//
// It is responsible to instantiate the various platforms observers
// and it gets notified whenever the input capabilities change. Whenever
// a change is detected the WebKit preferences are getting updated so the
// interactions media-queries can be updated.
class CONTENT_EXPORT InputDeviceChangeObserver
    : public ui::InputDeviceEventObserver {
 public:
  InputDeviceChangeObserver(RenderViewHostImpl* rvh);
  ~InputDeviceChangeObserver() override;

  // InputDeviceEventObserver public overrides.
  void OnInputDeviceConfigurationChanged(uint8_t input_device_types) override;

 private:
  RenderViewHostImpl* render_view_host_impl_;
  DISALLOW_COPY_AND_ASSIGN(InputDeviceChangeObserver);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_INPUT_DEVICE_CHANGE_OBSERVER_H_
