// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_UI_FILE_NAME_H_
#define PDF_UI_FILE_NAME_H_

#include <string>

namespace chrome_pdf {

// Creates a file name for saving a PDF file, given the source URL.
std::string GetFileNameForSaveFromUrl(const std::string& url);

}  // namespace chrome_pdf

#endif  // PDF_UI_FILE_NAME_H_
