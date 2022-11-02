// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "build/build_config.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace headless {

// TODO(crbug.com/1213221): Add tests. This class is mostly untested.
class HeadlessClipboard : public ui::Clipboard {
 public:
  HeadlessClipboard();
  HeadlessClipboard(const HeadlessClipboard&) = delete;
  HeadlessClipboard& operator=(const HeadlessClipboard&) = delete;
  ~HeadlessClipboard() override;

 private:
  // Clipboard overrides.
  void OnPreShutdown() override;
  ui::DataTransferEndpoint* GetSource(
      ui::ClipboardBuffer buffer) const override;
  const ui::ClipboardSequenceNumberToken& GetSequenceNumber(
      ui::ClipboardBuffer buffer) const override;
  std::vector<std::u16string> GetStandardFormats(
      ui::ClipboardBuffer buffer,
      const ui::DataTransferEndpoint* data_dst) const override;
  bool IsFormatAvailable(
      const ui::ClipboardFormatType& format,
      ui::ClipboardBuffer buffer,
      const ui::DataTransferEndpoint* data_dst) const override;
  void Clear(ui::ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ui::ClipboardBuffer buffer,
                          const ui::DataTransferEndpoint* data_dst,
                          std::vector<std::u16string>* types) const override;
  void ReadText(ui::ClipboardBuffer buffer,
                const ui::DataTransferEndpoint* data_dst,
                std::u16string* result) const override;
  void ReadAsciiText(ui::ClipboardBuffer buffer,
                     const ui::DataTransferEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ui::ClipboardBuffer buffer,
                const ui::DataTransferEndpoint* data_dst,
                std::u16string* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ui::ClipboardBuffer buffer,
               const ui::DataTransferEndpoint* data_dst,
               std::u16string* result) const override;
  void ReadRTF(ui::ClipboardBuffer buffer,
               const ui::DataTransferEndpoint* data_dst,
               std::string* result) const override;
  void ReadPng(ui::ClipboardBuffer buffer,
               const ui::DataTransferEndpoint* data_dst,
               ReadPngCallback callback) const override;
  void ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                      const std::u16string& type,
                      const ui::DataTransferEndpoint* data_dst,
                      std::u16string* result) const override;
  void ReadFilenames(ui::ClipboardBuffer buffer,
                     const ui::DataTransferEndpoint* data_dst,
                     std::vector<ui::FileInfo>* result) const override;
  void ReadBookmark(const ui::DataTransferEndpoint* data_dst,
                    std::u16string* title,
                    std::string* url) const override;
  void ReadData(const ui::ClipboardFormatType& format,
                const ui::DataTransferEndpoint* data_dst,
                std::string* result) const override;
#if BUILDFLAG(IS_OZONE)
  bool IsSelectionBufferAvailable() const override;
#endif  // BUILDFLAG(IS_OZONE)
  void WritePortableAndPlatformRepresentations(
      ui::ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<ui::DataTransferEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
  void WriteFilenames(std::vector<ui::FileInfo> filenames) override;
  void WriteBookmark(const char* title_data,
                     size_t title_len,
                     const char* url_data,
                     size_t url_len) override;
  void WriteWebSmartPaste() override;
  void WriteBitmap(const SkBitmap& bitmap) override;
  void WriteData(const ui::ClipboardFormatType& format,
                 const char* data_data,
                 size_t data_len) override;

  struct DataStore {
    DataStore();
    DataStore(const DataStore& other);
    ~DataStore();
    void Clear();
    ui::ClipboardSequenceNumberToken sequence_number;
    std::map<ui::ClipboardFormatType, std::string> data;
    std::string url_title;
    std::string html_src_url;
    std::vector<uint8_t> png;
    std::vector<ui::FileInfo> filenames;
  };

  // The non-const versions increment the sequence number as a side effect.
  const DataStore& GetStore(ui::ClipboardBuffer buffer) const;
  const DataStore& GetDefaultStore() const;
  DataStore& GetStore(ui::ClipboardBuffer buffer);
  DataStore& GetDefaultStore();

  ui::ClipboardBuffer default_store_buffer_;
  mutable std::map<ui::ClipboardBuffer, DataStore> stores_;
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_
