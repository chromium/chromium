// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/save_data_buffer_handler_for_drive.h"

#include "pdf/pdf_view_web_plugin.h"

namespace chrome_pdf {

SaveDataBufferHandlerForDrive::SaveDataBufferHandlerForDrive(
    PdfViewWebPlugin* pdf_view_web_plugin)
    : pdf_view_web_plugin_(pdf_view_web_plugin) {}

SaveDataBufferHandlerForDrive::~SaveDataBufferHandlerForDrive() = default;

OriginalDataHandlerForDrive::OriginalDataHandlerForDrive(
    PdfViewWebPlugin* pdf_view_web_plugin)
    : SaveDataBufferHandlerForDrive(pdf_view_web_plugin) {}

OriginalDataHandlerForDrive::~OriginalDataHandlerForDrive() = default;

uint32_t OriginalDataHandlerForDrive::GetFileSize() {
  return pdf_view_web_plugin()->GetOriginalFileSize();
}

void OriginalDataHandlerForDrive::Read(uint32_t offset,
                                       uint32_t block_size,
                                       ReadCallback callback) {
  std::move(callback).Run(mojo_base::BigBuffer(
      pdf_view_web_plugin()->GetOriginalFileData(offset, block_size)));
}

ModifiedDataBufferHandlerForDrive::ModifiedDataBufferHandlerForDrive(
    PdfViewWebPlugin* pdf_view_web_plugin)
    : SaveDataBufferHandlerForDrive(pdf_view_web_plugin) {}

ModifiedDataBufferHandlerForDrive::~ModifiedDataBufferHandlerForDrive() =
    default;

uint32_t ModifiedDataBufferHandlerForDrive::GetFileSize() {
  pdf_view_web_plugin()->PopulateBufferWithModifiedFileData(buffer_);
  return static_cast<uint32_t>(buffer_.size());
}

void ModifiedDataBufferHandlerForDrive::Read(uint32_t offset,
                                             uint32_t block_size,
                                             ReadCallback callback) {
  std::move(callback).Run(
      mojo_base::BigBuffer(pdf_view_web_plugin()->GetModifiedFileDataFromBuffer(
          buffer_, offset, block_size)));
}

}  // namespace chrome_pdf
