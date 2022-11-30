// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_FONT_LINUX_H_
#define PDF_PDFIUM_PDFIUM_FONT_LINUX_H_

namespace chrome_pdf {

// Initializes a Linux-specific font mapper.
// This is necessary because font loading does not work in the sandbox on Linux.
// Depending on how this process is initialized, it either:
// a) sends font requests to Blink.
// b) proxies font requests via PPAPI.
void InitializeLinuxFontMapper();

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_FONT_LINUX_H_
