// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_
#define CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_

#include <string>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

class MockClipboardHost : public blink::mojom::ClipboardHost {
 public:
  MockClipboardHost();

  MockClipboardHost(const MockClipboardHost&) = delete;
  MockClipboardHost& operator=(const MockClipboardHost&) = delete;

  ~MockClipboardHost() override;

  void Bind(mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);
  // Clears all clipboard data.
  void Reset();

  // Call tracking for tests.
  bool read_text_called() const { return read_text_called_; }
  bool read_html_called() const { return read_html_called_; }
  bool read_unsanitized_custom_format_called() const {
    return read_unsanitized_custom_format_called_;
  }
  bool read_available_custom_and_standard_formats_called() const {
    return read_available_custom_and_standard_formats_called_;
  }
  void ResetReadTracking();

  // blink::mojom::ClipboardHost
  void GetSequenceNumber(ui::ClipboardBuffer clipboard_buffer,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(blink::mojom::ClipboardFormat format,
                         ui::ClipboardBuffer clipboard_buffer,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(ui::ClipboardBuffer clipboard_buffer,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(ui::ClipboardBuffer clipboard_buffer,
                ReadTextCallback callback) override;
  void ReadHtml(ui::ClipboardBuffer clipboard_buffer,
                ReadHtmlCallback callback) override;
  void ReadSvg(ui::ClipboardBuffer clipboard_buffer,
               ReadSvgCallback callback) override;
  void ReadRtf(ui::ClipboardBuffer clipboard_buffer,
               ReadRtfCallback callback) override;
  void ReadPng(ui::ClipboardBuffer clipboard_buffer,
               ReadPngCallback callback) override;
  void ReadFiles(ui::ClipboardBuffer clipboard_buffer,
                 ReadFilesCallback callback) override;
  void ReadDataTransferCustomData(
      ui::ClipboardBuffer clipboard_buffer,
      const std::u16string& type,
      ReadDataTransferCustomDataCallback callback) override;
  void WriteText(const std::u16string& text) override;
  void WriteHtml(const std::u16string& markup, const GURL& url) override;
  void WriteSvg(const std::u16string& markup) override;
  void WriteSmartPasteMarker() override;
  void WriteDataTransferCustomData(
      const base::flat_map<std::u16string, std::u16string>& data) override;
  void WriteBookmark(const std::string& url,
                     const std::u16string& title) override;
  void WriteImage(const SkBitmap& bitmap) override;
  void CommitWrite() override;
  void ReadAvailableCustomAndStandardFormats(
      ReadAvailableCustomAndStandardFormatsCallback callback) override;
  void ReadUnsanitizedCustomFormat(
      const std::u16string& format,
      ReadUnsanitizedCustomFormatCallback callback) override;
  void WriteUnsanitizedCustomFormat(const std::u16string& format,
                                    mojo_base::BigBuffer data) override;
  void RegisterClipboardListener(
      mojo::PendingRemote<blink::mojom::ClipboardListener> listener) override;
#if BUILDFLAG(IS_MAC)
  void WriteStringToFindPboard(const std::u16string& text) override;
  void GetPlatformPermissionState(
      GetPlatformPermissionStateCallback callback) override;
#endif
  enum class StandardFormat {
    kPlainText,
    kHtml,
    kSvg,
    kPng,
  };

  bool HasStandardFormat(StandardFormat format) const;

 private:
  std::vector<std::u16string> ReadStandardFormatNames();
  void OnClipboardDataChanged();

  mojo::ReceiverSet<blink::mojom::ClipboardHost> receivers_;
  mojo::Remote<blink::mojom::ClipboardListener> clipboard_listener_;
  ui::ClipboardSequenceNumberToken sequence_number_;
  std::u16string plain_text_;
  std::u16string html_text_;
  std::u16string svg_text_;
  absl::flat_hash_set<StandardFormat> standard_formats_present_;
  GURL url_;
  std::vector<uint8_t> png_;
  std::map<std::u16string, std::u16string> custom_data_;
  bool write_smart_paste_ = false;
  bool needs_reset_ = false;
  std::map<std::u16string, std::vector<uint8_t>> unsanitized_custom_data_map_;

  // Call tracking.
  bool read_text_called_ = false;
  bool read_html_called_ = false;
  bool read_unsanitized_custom_format_called_ = false;
  bool read_available_custom_and_standard_formats_called_ = false;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_
