// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/strings/string_util.h"

namespace chrome_pdf {

namespace internal {

template <class StringType>
PDFiumAPIStringBufferAdapter<StringType>::PDFiumAPIStringBufferAdapter(
    StringType* str,
    size_t expected_size,
    bool check_expected_size)
    : str_(str),
      data_(base::WriteInto(str, expected_size + 1)),
      expected_size_(expected_size),
      check_expected_size_(check_expected_size),
      is_closed_(false) {}

template <class StringType>
PDFiumAPIStringBufferAdapter<StringType>::~PDFiumAPIStringBufferAdapter() {
  DCHECK(is_closed_);
}

template <class StringType>
void* PDFiumAPIStringBufferAdapter<StringType>::GetData() {
  DCHECK(!is_closed_);
  return data_;
}

template <class StringType>
void PDFiumAPIStringBufferAdapter<StringType>::Close(size_t actual_size) {
  DCHECK(!is_closed_);
  is_closed_ = true;

  if (check_expected_size_)
    DCHECK_EQ(expected_size_, actual_size);

  if (actual_size > 0) {
    DCHECK((*str_)[actual_size - 1] == 0);
    str_->resize(actual_size - 1);
  } else {
    str_->clear();
  }
}

PDFiumAPIStringBufferSizeInBytesAdapter::
    PDFiumAPIStringBufferSizeInBytesAdapter(std::u16string* str,
                                            size_t expected_size,
                                            bool check_expected_size)
    : adapter_(str, expected_size / sizeof(char16_t), check_expected_size) {
  DCHECK(expected_size % sizeof(char16_t) == 0);
}

PDFiumAPIStringBufferSizeInBytesAdapter::
    ~PDFiumAPIStringBufferSizeInBytesAdapter() = default;

void* PDFiumAPIStringBufferSizeInBytesAdapter::GetData() {
  return adapter_.GetData();
}

void PDFiumAPIStringBufferSizeInBytesAdapter::Close(size_t actual_size) {
  DCHECK(actual_size % sizeof(char16_t) == 0);
  adapter_.Close(actual_size / sizeof(char16_t));
}

// explicit instantiations
template class PDFiumAPIStringBufferAdapter<std::string>;
template class PDFiumAPIStringBufferAdapter<std::u16string>;

}  // namespace internal

}  // namespace chrome_pdf
