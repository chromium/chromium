// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/dwrite_font_file_util_win.h"

#include <shlobj.h>
#include <wrl.h>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/dwrite_font_uma_logging_win.h"

namespace content {

HRESULT FontFilePathAndTtcIndex(IDWriteFont* font,
                                base::string16& file_path,
                                uint32_t& ttc_index) {
  Microsoft::WRL::ComPtr<IDWriteFontFace> font_face;
  HRESULT hr;
  hr = font->CreateFontFace(&font_face);
  if (FAILED(hr)) {
    base::UmaHistogramSparse("DirectWrite.Fonts.Proxy.CreateFontFaceResult",
                             hr);
    LogMessageFilterError(
        MessageFilterError::ADD_FILES_FOR_FONT_CREATE_FACE_FAILED);
    return hr;
  }
  return FontFilePathAndTtcIndex(font_face.Get(), file_path, ttc_index);
}

HRESULT FontFilePathAndTtcIndex(IDWriteFontFace* font_face,
                                base::string16& file_path,
                                uint32_t& ttc_index) {
  TRACE_EVENT0("dwrite,fonts",
               "dwrite_font_file_util::FontFilePathAndTtcIndex");
  UINT32 file_count;
  HRESULT hr;
  hr = font_face->GetFiles(&file_count, nullptr);
  if (FAILED(hr)) {
    LogMessageFilterError(
        MessageFilterError::ADD_FILES_FOR_FONT_GET_FILE_COUNT_FAILED);
    return hr;
  }

  // We've learned from the DirectWrite team at MS that the number of font files
  // retrieved per IDWriteFontFile can only ever be 1. Other font formats such
  // as Type 1, which represent one font in multiple files, are currently not
  // supported in the API (as of December 2018, Windows 10). In Chrome we do not
  // plan to support Type 1 fonts, or generally other font formats different
  // from OpenType, hence no need to loop over file_count or retrieve multiple
  // files.
  DCHECK_EQ(file_count, 1u);
  if (file_count > 1) {
    LogMessageFilterError(
        MessageFilterError::GET_FILE_COUNT_INVALID_NUMBER_OF_FILES);
    return kErrorFontFileUtilTooManyFilesPerFace;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFile> font_file;
  hr = font_face->GetFiles(&file_count, &font_file);
  if (FAILED(hr)) {
    LogMessageFilterError(
        MessageFilterError::ADD_FILES_FOR_FONT_GET_FILES_FAILED);
    return hr;
  }

  Microsoft::WRL::ComPtr<IDWriteFontFileLoader> loader;
  hr = font_file->GetLoader(&loader);
  if (FAILED(hr)) {
    LogMessageFilterError(
        MessageFilterError::ADD_FILES_FOR_FONT_GET_LOADER_FAILED);
    return hr;
  }

  Microsoft::WRL::ComPtr<IDWriteLocalFontFileLoader> local_loader;
  hr = loader.As(&local_loader);

  if (hr == E_NOINTERFACE) {
    // We could get here if the system font collection contains fonts that
    // are backed by something other than files in the system fonts folder.
    // I don't think that is actually possible, so for now we'll just
    // ignore it (result will be that we'll be unable to match any styles
    // for this font, forcing blink/skia to fall back to whatever font is
    // next). If we get telemetry indicating that this case actually
    // happens, we can implement this by exposing the loader via ipc. That
    // will likely be by loading the font data into shared memory, although
    // we could proxy the stream reads directly instead.
    LogLoaderType(DirectWriteFontLoaderType::OTHER_LOADER);
    DCHECK(false);
    return hr;
  } else if (FAILED(hr)) {
    LogMessageFilterError(MessageFilterError::ADD_FILES_FOR_FONT_QI_FAILED);
    return hr;
  }

  const void* key;
  UINT32 key_size;
  hr = font_file->GetReferenceKey(&key, &key_size);
  if (FAILED(hr)) {
    LogMessageFilterError(
        MessageFilterError::ADD_LOCAL_FILE_GET_REFERENCE_KEY_FAILED);
    return hr;
  }

  UINT32 path_length = 0;
  hr = local_loader->GetFilePathLengthFromKey(key, key_size, &path_length);
  if (FAILED(hr)) {
    LogMessageFilterError(
        MessageFilterError::ADD_LOCAL_FILE_GET_PATH_LENGTH_FAILED);
    return hr;
  }
  base::string16 retrieve_file_path;
  retrieve_file_path.resize(
      ++path_length);  // Reserve space for the null terminator.
  hr = local_loader->GetFilePathFromKey(key, key_size, &retrieve_file_path[0],
                                        path_length);
  if (FAILED(hr)) {
    LogMessageFilterError(MessageFilterError::ADD_LOCAL_FILE_GET_PATH_FAILED);
    return hr;
  }
  // No need for the null-terminator in base::string16.
  retrieve_file_path.resize(--path_length);

  uint32_t retrieve_ttc_index = font_face->GetIndex();
  if (FAILED(hr)) {
    return hr;
  }

  file_path = retrieve_file_path;
  ttc_index = retrieve_ttc_index;

  return S_OK;
}

HRESULT AddFilesForFont(IDWriteFont* font,
                        const base::string16& windows_fonts_path,
                        std::set<base::string16>* path_set,
                        std::set<base::string16>* custom_font_path_set,
                        uint32_t* ttc_index) {
  base::string16 file_path;
  HRESULT hr = FontFilePathAndTtcIndex(font, file_path, *ttc_index);
  if (FAILED(hr)) {
    return hr;
  }

  base::string16 file_path_folded = base::i18n::FoldCase(file_path);

  if (!file_path_folded.size())
    return kErrorFontFileUtilEmptyFilePath;

  if (!base::StartsWith(file_path_folded, windows_fonts_path,
                        base::CompareCase::SENSITIVE)) {
    LogLoaderType(DirectWriteFontLoaderType::FILE_OUTSIDE_SANDBOX);
    custom_font_path_set->insert(file_path);
  } else {
    LogLoaderType(DirectWriteFontLoaderType::FILE_SYSTEM_FONT_DIR);
    path_set->insert(file_path);
  }
  return S_OK;
}

base::string16 GetWindowsFontsPath() {
  std::vector<base::char16> font_path_chars;
  // SHGetSpecialFolderPath requires at least MAX_PATH characters.
  font_path_chars.resize(MAX_PATH);
  BOOL result = SHGetSpecialFolderPath(nullptr /* hwndOwner - reserved */,
                                       font_path_chars.data(), CSIDL_FONTS,
                                       FALSE /* fCreate */);
  DCHECK(result);
  return base::i18n::FoldCase(font_path_chars.data());
}

}  // namespace content
