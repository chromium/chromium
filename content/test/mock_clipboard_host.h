// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_
#define CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

class MockClipboardHost : public blink::mojom::ClipboardHost {
 public:
  MockClipboardHost();
  ~MockClipboardHost() override;

  void Bind(mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);
  void Reset();

 private:
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
  void ReadRtf(ui::ClipboardBuffer clipboard_buffer,
               ReadRtfCallback callback) override;
  void ReadImage(ui::ClipboardBuffer clipboard_buffer,
                 ReadImageCallback callback) override;
  void ReadCustomData(ui::ClipboardBuffer clipboard_buffer,
                      const base::string16& type,
                      ReadCustomDataCallback callback) override;
  void WriteText(const base::string16& text) override;
  void WriteHtml(const base::string16& markup, const GURL& url) override;
  void WriteSmartPasteMarker() override;
  void WriteCustomData(
      const base::flat_map<base::string16, base::string16>& data) override;
  void WriteRawData(const base::string16&, mojo_base::BigBuffer) override;
  void WriteBookmark(const std::string& url,
                     const base::string16& title) override;
  void WriteImage(const SkBitmap& bitmap) override;
  void CommitWrite() override;
#if defined(OS_MACOSX)
  void WriteStringToFindPboard(const base::string16& text) override;
#endif

  mojo::ReceiverSet<blink::mojom::ClipboardHost> receivers_;
  uint64_t sequence_number_ = 0;
  base::string16 plain_text_;
  base::string16 html_text_;
  GURL url_;
  SkBitmap image_;
  std::map<base::string16, base::string16> custom_data_;
  std::map<base::string16, mojo_base::BigBuffer> raw_data_;
  bool write_smart_paste_ = false;
  bool needs_reset_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockClipboardHost);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_CLIPBOARD_HOST_H_
