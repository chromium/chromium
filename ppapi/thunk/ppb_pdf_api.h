// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_PDF_API_H_
#define PPAPI_THUNK_PPB_PDF_API_H_

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/shared_impl/singleton_resource_id.h"

namespace ppapi {
namespace thunk {

class PPB_PDF_API {
 public:
  virtual void Print() = 0;

  static const SingletonResourceID kSingletonResourceID = PDF_SINGLETON_ID;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_PDF_API_H_
