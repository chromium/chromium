// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_
#define PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_

#include <string>

namespace blink {
struct WebFontDescription;
}

namespace pp {
class Instance;
}

namespace chrome_pdf {

// Returns a handle to the font mapped based on `desc`, `font_family`, and
// `charset`. The handle is for use as the `font_id` in `GetPepperFontData()`
// and `DeletePepperFont()` below.
void* MapPepperFont(const blink::WebFontDescription& desc,
                    const std::string& font_family,
                    int charset);

// Reads data from the `font_id` handle for `table` into a `buffer` of
// `buf_size`. Returns the amount of data read on success, or 0 on failure. If
// `buffer` is null, then just return the required size for the buffer.
unsigned long GetPepperFontData(void* font_id,
                                unsigned int table,
                                unsigned char* buffer,
                                unsigned long buf_size);

// Releases resources allocated by MapPepperFont().
void DeletePepperFont(void* font_id);

// Keeps track of the most recently used plugin instance. This is a no-op if
// `last_instance` is null.
void SetLastPepperInstance(pp::Instance* last_instance);

}  // namespace chrome_pdf

#endif  // PDF_PPAPI_MIGRATION_PDFIUM_FONT_LINUX_H_
