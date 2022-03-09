// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_PDF_H_
#define PPAPI_C_PRIVATE_PPB_PDF_H_

#include "ppapi/c/pp_instance.h"

#define PPB_PDF_INTERFACE "PPB_PDF;1"

struct PPB_PDF {
  // Invoke Print dialog for plugin.
  void (*Print)(PP_Instance instance);
};

#endif  // PPAPI_C_PRIVATE_PPB_PDF_H_
