// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_BROWSER_FONT_SINGLETON_API_H_
#define PPAPI_THUNK_PPB_BROWSER_FONT_SINGLETON_API_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

namespace ppapi {
namespace thunk {

class PPB_BrowserFont_Singleton_API {
 public:
  virtual ~PPB_BrowserFont_Singleton_API() {}

  virtual PP_Var GetFontFamilies(PP_Instance instance) = 0;

  static const SingletonResourceID kSingletonResourceID =
      BROWSER_FONT_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_BROWSER_FONT_SINGLETON_API_H_
