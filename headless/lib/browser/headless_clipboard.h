// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_
#define HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_data_endpoint.h"

namespace headless {

class HeadlessClipboard : public ui::Clipboard {
 public:
  HeadlessClipboard();
  ~HeadlessClipboard() override;

 private:
  // Clipboard overrides.
  void OnPreShutdown() override;
  uint64_t GetSequenceNumber(ui::ClipboardBuffer buffer) const override;
  bool IsFormatAvailable(
      const ui::ClipboardFormatType& format,
      ui::ClipboardBuffer buffer,
      const ui::ClipboardDataEndpoint* data_dst) const override;
  void Clear(ui::ClipboardBuffer buffer) override;
  void ReadAvailableTypes(ui::ClipboardBuffer buffer,
                          const ui::ClipboardDataEndpoint* data_dst,
                          std::vector<base::string16>* types) const override;
  std::vector<base::string16> ReadAvailablePlatformSpecificFormatNames(
      ui::ClipboardBuffer buffer,
      const ui::ClipboardDataEndpoint* data_dst) const override;
  void ReadText(ui::ClipboardBuffer buffer,
                const ui::ClipboardDataEndpoint* data_dst,
                base::string16* result) const override;
  void ReadAsciiText(ui::ClipboardBuffer buffer,
                     const ui::ClipboardDataEndpoint* data_dst,
                     std::string* result) const override;
  void ReadHTML(ui::ClipboardBuffer buffer,
                const ui::ClipboardDataEndpoint* data_dst,
                base::string16* markup,
                std::string* src_url,
                uint32_t* fragment_start,
                uint32_t* fragment_end) const override;
  void ReadSvg(ui::ClipboardBuffer buffer,
               const ui::ClipboardDataEndpoint* data_dst,
               base::string16* result) const override;
  void ReadRTF(ui::ClipboardBuffer buffer,
               const ui::ClipboardDataEndpoint* data_dst,
               std::string* result) const override;
  void ReadImage(ui::ClipboardBuffer buffer,
                 const ui::ClipboardDataEndpoint* data_dst,
                 ReadImageCallback callback) const override;
  void ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                      const base::string16& type,
                      const ui::ClipboardDataEndpoint* data_dst,
                      base::string16* result) const override;
  void ReadBookmark(const ui::ClipboardDataEndpoint* data_dst,
                    base::string16* title,
                    std::string* url) const override;
  void ReadData(const ui::ClipboardFormatType& format,
                const ui::ClipboardDataEndpoint* data_dst,
                std::string* result) const override;
#if defined(USE_OZONE)
  bool IsSelectionBufferAvailable() const override;
#endif  // defined(USE_OZONE)
  void WritePortableRepresentations(
      ui::ClipboardBuffer buffer,
      const ObjectMap& objects,
      std::unique_ptr<ui::ClipboardDataEndpoint> data_src) override;
  void WritePlatformRepresentations(
      ui::ClipboardBuffer buffer,
      std::vector<Clipboard::PlatformRepresentation> platform_representations,
      std::unique_ptr<ui::ClipboardDataEndpoint> data_src) override;
  void WriteText(const char* text_data, size_t text_len) override;
  void WriteHTML(const char* markup_data,
                 size_t markup_len,
                 const char* url_data,
                 size_t url_len) override;
  void WriteSvg(const char* markup_data, size_t markup_len) override;
  void WriteRTF(const char* rtf_data, size_t data_len) override;
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
    uint64_t sequence_number;
    std::map<ui::ClipboardFormatType, std::string> data;
    std::string url_title;
    std::string html_src_url;
    SkBitmap image;
  };

  // The non-const versions increment the sequence number as a side effect.
  const DataStore& GetStore(ui::ClipboardBuffer buffer) const;
  const DataStore& GetDefaultStore() const;
  DataStore& GetStore(ui::ClipboardBuffer buffer);
  DataStore& GetDefaultStore();

  ui::ClipboardBuffer default_store_buffer_;
  mutable std::map<ui::ClipboardBuffer, DataStore> stores_;

  DISALLOW_COPY_AND_ASSIGN(HeadlessClipboard);
};

}  // namespace headless

#endif  // HEADLESS_LIB_BROWSER_HEADLESS_CLIPBOARD_H_
