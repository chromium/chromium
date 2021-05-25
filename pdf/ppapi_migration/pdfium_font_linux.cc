// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/pdfium_font_linux.h"

#include "ppapi/cpp/instance.h"

namespace chrome_pdf {

namespace {

PP_Instance g_last_instance_id;

}  // namespace

void SetLastPepperInstance(pp::Instance* last_instance) {
  if (last_instance)
    g_last_instance_id = last_instance->pp_instance();
}

PP_Instance GetLastPepperInstance() {
  return g_last_instance_id;
}

}  // namespace chrome_pdf
