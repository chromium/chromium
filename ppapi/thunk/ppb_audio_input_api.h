// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_AUDIO_INPUT_API_H_
#define PPAPI_THUNK_PPB_AUDIO_INPUT_API_H_

#include <stdint.h>

#include "base/memory/scoped_refptr.h"
#include "ppapi/c/dev/ppb_audio_input_dev.h"

namespace ppapi {

class TrackedCallback;

namespace thunk {

class PPB_AudioInput_API {
 public:
  virtual ~PPB_AudioInput_API() {}

  virtual int32_t EnumerateDevices(const PP_ArrayOutput& output,
                                   scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t MonitorDeviceChange(PP_MonitorDeviceChangeCallback callback,
                                      void* user_data) = 0;
  virtual int32_t Open0_3(PP_Resource device_ref,
                          PP_Resource config,
                          PPB_AudioInput_Callback_0_3 audio_input_callback_0_3,
                          void* user_data,
                          scoped_refptr<TrackedCallback> callback) = 0;
  virtual int32_t Open(PP_Resource device_ref,
                       PP_Resource config,
                       PPB_AudioInput_Callback audio_input_callback,
                       void* user_data,
                       scoped_refptr<TrackedCallback> callback) = 0;
  virtual PP_Resource GetCurrentConfig() = 0;
  virtual PP_Bool StartCapture() = 0;
  virtual PP_Bool StopCapture() = 0;
  virtual void Close() = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_AUDIO_INPUT_API_H_
