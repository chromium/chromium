// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_
#define CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_

#include <dwrite.h>
#include <dwrite_2.h>
#include <dwrite_3.h>

#include <set>
#include <string>

namespace content {

// Custom error codes potentially emitted from FontFilePathAndTtcIndex and
// AddFilesForFont.
const HRESULT kErrorFontFileUtilTooManyFilesPerFace =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD001);
const HRESULT kErrorFontFileUtilEmptyFilePath =
    MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xD002);

HRESULT FontFilePathAndTtcIndex(IDWriteFontFace* font,
                                std::wstring& file_path,
                                uint32_t& ttc_index);
HRESULT FontFilePathAndTtcIndex(IDWriteFont* font,
                                std::wstring& file_path,
                                uint32_t& ttc_index);
HRESULT AddFilesForFont(IDWriteFont* font,
                        const std::u16string& windows_fonts_path,
                        std::set<std::wstring>* path_set);

std::u16string GetWindowsFontsPath();

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_DWRITE_FONT_FILE_UTIL_WIN_H_
