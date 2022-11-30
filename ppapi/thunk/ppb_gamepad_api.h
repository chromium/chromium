// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_GAMEPAD_API_H_
#define PPAPI_THUNK_PPB_GAMEPAD_API_H_

#include "ppapi/shared_impl/singleton_resource_id.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

struct PP_GamepadsSampleData;

namespace ppapi {
namespace thunk {

class PPAPI_THUNK_EXPORT PPB_Gamepad_API {
 public:
  virtual ~PPB_Gamepad_API() {}

  virtual void Sample(PP_Instance instance,
                      PP_GamepadsSampleData* data) = 0;

  static const SingletonResourceID kSingletonResourceID = GAMEPAD_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_GAMEPAD_API_H_
