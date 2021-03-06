// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_RAW_CLIPBOARD_HOST_H_
#define CONTENT_TEST_MOCK_RAW_CLIPBOARD_HOST_H_

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom.h"

namespace content {

class MockClipboardHost;

// Emulates platform-specific clipboard behavior, such as type conversions
// (ex. CF_TEXT on Windows MockRawClipboardHost to normal text on the
// associated MockClipboardHost).
class MockRawClipboardHost : public blink::mojom::RawClipboardHost {
 public:
  // |mock_clipboard_host| must outlive the newly created MockRawClipboardHost
  // instance.
  explicit MockRawClipboardHost(MockClipboardHost* mock_clipboard_host);
  MockRawClipboardHost(const MockRawClipboardHost&) = delete;
  MockRawClipboardHost& operator=(const MockRawClipboardHost&) = delete;
  ~MockRawClipboardHost() override;

  void Bind(mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver);
  // Clears all clipboard data.
  void Reset();

  // blink::mojom::RawClipboardHost
  void ReadAvailableFormatNames(
      ReadAvailableFormatNamesCallback callback) override;
  void Read(const base::string16& format, ReadCallback callback) override;
  void Write(const base::string16& format, mojo_base::BigBuffer data) override;
  void CommitWrite() override;

 private:
  mojo::ReceiverSet<blink::mojom::RawClipboardHost> receivers_;
  // The associated sanitized clipboard, for emulating platform-specific
  // clipboard type conversions. Owned by WebTestContentBrowserClient.
  MockClipboardHost* const mock_clipboard_host_;
  std::map<base::string16, std::vector<uint8_t>> raw_data_map_;
  // Tracks whether a commit has happened since the last write. After a
  // sequence of writes are committed, future writes should clear the clipboard
  // before continuing to write.
  bool needs_reset_ = false;
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_RAW_CLIPBOARD_HOST_H_
