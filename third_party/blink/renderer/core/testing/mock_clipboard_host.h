// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_

#include <map>

#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-blink.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class MockClipboardHost : public mojom::blink::ClipboardHost {
 public:
  MockClipboardHost();
  ~MockClipboardHost() override;

  void Bind(mojo::PendingReceiver<mojom::blink::ClipboardHost> receiver);
  // Clears all clipboard data.
  void Reset();

 private:
  // mojom::ClipboardHost
  void GetSequenceNumber(mojom::ClipboardBuffer clipboard_buffer,
                         GetSequenceNumberCallback callback) override;
  void IsFormatAvailable(mojom::ClipboardFormat format,
                         mojom::ClipboardBuffer clipboard_buffer,
                         IsFormatAvailableCallback callback) override;
  void ReadAvailableTypes(mojom::ClipboardBuffer clipboard_buffer,
                          ReadAvailableTypesCallback callback) override;
  void ReadText(mojom::ClipboardBuffer clipboard_buffer,
                ReadTextCallback callback) override;
  void ReadHtml(mojom::ClipboardBuffer clipboard_buffer,
                ReadHtmlCallback callback) override;
  void ReadSvg(mojom::ClipboardBuffer clipboard_buffer,
               ReadSvgCallback callback) override;
  void ReadRtf(mojom::ClipboardBuffer clipboard_buffer,
               ReadRtfCallback callback) override;
  void ReadImage(mojom::ClipboardBuffer clipboard_buffer,
                 ReadImageCallback callback) override;
  void ReadCustomData(mojom::ClipboardBuffer clipboard_buffer,
                      const String& type,
                      ReadCustomDataCallback callback) override;
  void WriteText(const String& text) override;
  void WriteHtml(const String& markup, const KURL& url) override;
  void WriteSvg(const String& markup) override;
  void WriteSmartPasteMarker() override;
  void WriteCustomData(const HashMap<String, String>& data) override;
  void WriteBookmark(const String& url, const String& title) override;
  void WriteImage(const SkBitmap& bitmap) override;
  void CommitWrite() override;
#if defined(OS_MAC)
  void WriteStringToFindPboard(const String& text) override;
#endif

  mojo::ReceiverSet<mojom::blink::ClipboardHost> receivers_;
  uint64_t sequence_number_ = 0;
  String plain_text_ = g_empty_string;
  String html_text_ = g_empty_string;
  String svg_text_ = g_empty_string;
  KURL url_;
  SkBitmap image_;
  HashMap<String, String> custom_data_;
  bool write_smart_paste_ = false;
  bool needs_reset_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockClipboardHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TESTING_MOCK_CLIPBOARD_HOST_H_
