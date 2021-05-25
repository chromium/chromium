// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_
#define PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_

#include "ppapi/c/pp_instance.h"

namespace pp {
class Instance;
}

namespace chrome_pdf {

// Keeps track of the most recently used plugin instance. This is a no-op if
// `last_instance` is null.
void SetLastPepperInstance(pp::Instance* last_instance);

PP_Instance GetLastPepperInstance();

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_
