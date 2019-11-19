// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_FONT_LINUX_H_
#define PDF_PDFIUM_PDFIUM_FONT_LINUX_H_

namespace pp {
class Instance;
}

namespace chrome_pdf {

// Initializes a Linux-specific font mapper that proxies font requests via
// PPAPI. This is necessary because font loading does not work in the sandbox on
// Linux.
void InitializeLinuxFontMapper();

// Keeps track of the most recently used plugin instance. This is a no-op of
// |last_instance| is null.
void SetLastInstance(pp::Instance* last_instance);

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_FONT_LINUX_H_
