// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_SAVE_DATA_BUFFER_HANDLER_FOR_DRIVE_H_
#define PDF_SAVE_DATA_BUFFER_HANDLER_FOR_DRIVE_H_

#include <stdint.h>

#include <vector>

#include "base/memory/raw_ptr.h"
#include "pdf/mojom/pdf.mojom.h"

namespace chrome_pdf {

class PdfViewWebPlugin;

// Buffer for saving data to Google Drive.
class SaveDataBufferHandlerForDrive : public pdf::mojom::SaveDataBufferHandler {
 public:
  explicit SaveDataBufferHandlerForDrive(PdfViewWebPlugin* pdf_view_web_plugin);
  SaveDataBufferHandlerForDrive(const SaveDataBufferHandlerForDrive&) = delete;
  SaveDataBufferHandlerForDrive& operator=(
      const SaveDataBufferHandlerForDrive&) = delete;
  ~SaveDataBufferHandlerForDrive() override;

  // Gets the total size of the data to be saved.
  virtual uint32_t GetFileSize() = 0;

 protected:
  PdfViewWebPlugin* pdf_view_web_plugin() { return pdf_view_web_plugin_; }

 private:
  raw_ptr<PdfViewWebPlugin> pdf_view_web_plugin_;
};

// Handler for reading the original data of a PDF file.
class OriginalDataHandlerForDrive final : public SaveDataBufferHandlerForDrive {
 public:
  explicit OriginalDataHandlerForDrive(PdfViewWebPlugin* pdf_view_web_plugin);
  ~OriginalDataHandlerForDrive() override;

  // SaveDataBufferHandlerForDrive:
  uint32_t GetFileSize() override;
  // pdf::mojom::SaveDataBufferHandler:
  void Read(uint32_t offset,
            uint32_t block_size,
            ReadCallback callback) override;
};

// Handler for reading the modified data of a PDF file.
class ModifiedDataBufferHandlerForDrive final
    : public SaveDataBufferHandlerForDrive {
 public:
  explicit ModifiedDataBufferHandlerForDrive(
      PdfViewWebPlugin* pdf_view_web_plugin);
  ~ModifiedDataBufferHandlerForDrive() override;

  // SaveDataBufferHandlerForDrive:
  uint32_t GetFileSize() override;
  // pdf::mojom::SaveDataBufferHandler:
  void Read(uint32_t offset,
            uint32_t block_size,
            ReadCallback callback) override;

 private:
  std::vector<uint8_t> buffer_;
};

}  // namespace chrome_pdf

#endif  // PDF_SAVE_DATA_BUFFER_HANDLER_FOR_DRIVE_H_
