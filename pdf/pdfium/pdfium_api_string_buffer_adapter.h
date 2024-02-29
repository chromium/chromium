// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_PDFIUM_PDFIUM_API_STRING_BUFFER_ADAPTER_H_
#define PDF_PDFIUM_PDFIUM_API_STRING_BUFFER_ADAPTER_H_

#include <stddef.h>

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"

namespace chrome_pdf {

namespace internal {

// Helper to deal with the fact that many PDFium APIs write the null-terminator
// into string buffers that are passed to them, but the PDF code likes to use
// std::strings / std::u16strings, where one should not count on the internal
// string buffers to be null-terminated.
template <class StringType>
class PDFiumAPIStringBufferAdapter {
 public:
  // `str` is the string to write into.
  // `expected_size` is the number of characters the PDFium API will write,
  // including the null-terminator. It should be at least 1.
  // `check_expected_size` whether to check the actual number of characters
  // written into `str` against `expected_size` when calling Close().
  PDFiumAPIStringBufferAdapter(StringType* str,
                               size_t expected_size,
                               bool check_expected_size);
  PDFiumAPIStringBufferAdapter(const PDFiumAPIStringBufferAdapter&) = delete;
  PDFiumAPIStringBufferAdapter& operator=(const PDFiumAPIStringBufferAdapter&) =
      delete;
  ~PDFiumAPIStringBufferAdapter();

  // Returns a pointer to `str_`'s buffer. The buffer's size is large enough to
  // hold `expected_size_` + 1 characters, so the PDFium API that uses the
  // pointer has space to write a null-terminator.
  void* GetData();

  // Resizes `str_` to `actual_size` - 1 characters, thereby removing the extra
  // null-terminator. This must be called prior to the adapter's destruction.
  // The pointer returned by GetData() should be considered invalid.
  void Close(size_t actual_size);

  template <typename IntType>
  void Close(IntType actual_size) {
    Close(base::checked_cast<size_t>(actual_size));
  }

 private:
  const raw_ptr<StringType> str_;
  const raw_ptr<void> data_;
  const size_t expected_size_;
  const bool check_expected_size_;
  bool is_closed_;
};

// Helper to deal with the fact that many PDFium APIs write the null-terminator
// into string buffers that are passed to them, but the PDF code likes to use
// std::strings / std::u16strings, where one should not count on the internal
// string buffers to be null-terminated. This version is suitable for APIs that
// work in terms of number of bytes instead of the number of characters. Though
// for std::strings, PDFiumAPIStringBufferAdapter is equivalent.
class PDFiumAPIStringBufferSizeInBytesAdapter {
 public:
  // `str` is the string to write into.
  // `expected_size` is the number of bytes the PDFium API will write,
  // including the null-terminator. It should be at least the size of a
  // character in bytes.
  // `check_expected_size` whether to check the actual number of bytes
  // written into `str` against `expected_size` when calling Close().
  PDFiumAPIStringBufferSizeInBytesAdapter(std::u16string* str,
                                          size_t expected_size,
                                          bool check_expected_size);
  ~PDFiumAPIStringBufferSizeInBytesAdapter();

  // Returns a pointer to `str_`'s buffer. The buffer's size is large enough to
  // hold `expected_size_` + sizeof(char16_t) bytes, so the PDFium API that
  // uses the pointer has space to write a null-terminator.
  void* GetData();

  // Resizes `str_` to `actual_size` - sizeof(char16_t) bytes, thereby
  // removing the extra null-terminator. This must be called prior to the
  // adapter's destruction. The pointer returned by GetData() should be
  // considered invalid.
  void Close(size_t actual_size);

  template <typename IntType>
  void Close(IntType actual_size) {
    Close(base::checked_cast<size_t>(actual_size));
  }

 private:
  PDFiumAPIStringBufferAdapter<std::u16string> adapter_;
};

template <class AdapterType,
          class StringType,
          typename BufferType,
          typename ReturnType>
std::optional<StringType> CallPDFiumStringBufferApiAndReturnOptional(
    base::RepeatingCallback<ReturnType(BufferType*, ReturnType)> api,
    bool check_expected_size) {
  ReturnType expected_size = api.Run(nullptr, 0);
  if (expected_size == 0)
    return std::nullopt;

  StringType str;
  AdapterType api_string_adapter(&str, expected_size, check_expected_size);
  auto* data = reinterpret_cast<BufferType*>(api_string_adapter.GetData());
  api_string_adapter.Close(api.Run(data, expected_size));
  return str;
}

template <class AdapterType,
          class StringType,
          typename BufferType,
          typename ReturnType>
StringType CallPDFiumStringBufferApi(
    base::RepeatingCallback<ReturnType(BufferType*, ReturnType)> api,
    bool check_expected_size) {
  std::optional<StringType> result =
      CallPDFiumStringBufferApiAndReturnOptional<AdapterType, StringType>(
          api, check_expected_size);
  return result.value_or(StringType());
}

}  // namespace internal

// Helper function to call PDFium APIs where the output buffer is expected to
// hold UTF-16 data, and the buffer length is specified in bytes.
template <typename BufferType>
std::u16string CallPDFiumWideStringBufferApi(
    base::RepeatingCallback<unsigned long(BufferType*, unsigned long)> api,
    bool check_expected_size) {
  using adapter_type = internal::PDFiumAPIStringBufferSizeInBytesAdapter;
  return internal::CallPDFiumStringBufferApi<adapter_type, std::u16string>(
      api, check_expected_size);
}

// Variant of CallPDFiumWideStringBufferApi() that distinguishes between API
// call failures and empty string return values.
template <typename BufferType>
std::optional<std::u16string> CallPDFiumWideStringBufferApiAndReturnOptional(
    base::RepeatingCallback<unsigned long(BufferType*, unsigned long)> api,
    bool check_expected_size) {
  using adapter_type = internal::PDFiumAPIStringBufferSizeInBytesAdapter;
  return internal::CallPDFiumStringBufferApiAndReturnOptional<adapter_type,
                                                              std::u16string>(
      api, check_expected_size);
}

// Helper function to call PDFium APIs where the output buffer is expected to
// hold ASCII or UTF-8 data, and the buffer length is specified in bytes.
template <typename BufferType, typename ReturnType>
std::string CallPDFiumStringBufferApi(
    base::RepeatingCallback<ReturnType(BufferType*, ReturnType)> api,
    bool check_expected_size) {
  using adapter_type = internal::PDFiumAPIStringBufferAdapter<std::string>;
  return internal::CallPDFiumStringBufferApi<adapter_type, std::string>(
      api, check_expected_size);
}

// Expose internal::PDFiumAPIStringBufferAdapter for special cases that cannot
// use the CallPDFiumStringBuffer* functions above.
template <class StringType>
using PDFiumAPIStringBufferAdapter =
    internal::PDFiumAPIStringBufferAdapter<StringType>;

}  // namespace chrome_pdf

#endif  // PDF_PDFIUM_PDFIUM_API_STRING_BUFFER_ADAPTER_H_
