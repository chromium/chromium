// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_PRIVATE_PDF_H_
#define PPAPI_CPP_PRIVATE_PDF_H_

#include <stdint.h>

#include "ppapi/c/private/ppb_pdf.h"

namespace pp {

class InstanceHandle;

class PDF {
 public:
  // Returns true if the required interface is available.
  static bool IsAvailable();

  static void Print(const InstanceHandle& instance);
};

}  // namespace pp

#endif  // PPAPI_CPP_PRIVATE_PDF_H_
