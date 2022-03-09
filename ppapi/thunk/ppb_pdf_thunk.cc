// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/private/ppb_pdf.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_pdf_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace thunk {

namespace {

void Print(PP_Instance instance) {
  EnterInstanceAPI<PPB_PDF_API> enter(instance);
  if (enter.succeeded())
    enter.functions()->Print();
}

const PPB_PDF g_ppb_pdf_thunk = {
    &Print,
};

}  // namespace

const PPB_PDF* GetPPB_PDF_Thunk() {
  return &g_ppb_pdf_thunk;
}

}  // namespace thunk
}  // namespace ppapi
