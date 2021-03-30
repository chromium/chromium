// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/mock_raw_clipboard_host.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/test/mock_clipboard_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/clipboard/raw_clipboard.mojom.h"

namespace content {

MockRawClipboardHost::~MockRawClipboardHost() = default;

MockRawClipboardHost::MockRawClipboardHost(
    MockClipboardHost* mock_clipboard_host)
    : mock_clipboard_host_(mock_clipboard_host) {
  EXPECT_TRUE(mock_clipboard_host_);
}

void MockRawClipboardHost::Bind(
    mojo::PendingReceiver<blink::mojom::RawClipboardHost> receiver) {
  if (!base::FeatureList::IsEnabled(blink::features::kRawClipboard))
    return;

  receivers_.Add(this, std::move(receiver));
}

void MockRawClipboardHost::Reset() {
  EXPECT_TRUE(mock_clipboard_host_);
  mock_clipboard_host_->Reset();
  raw_data_map_.clear();
  needs_reset_ = false;
}

void MockRawClipboardHost::ReadAvailableFormatNames(
    ReadAvailableFormatNamesCallback callback) {
  std::vector<std::u16string> format_names;
  for (const auto& item : raw_data_map_)
    format_names.emplace_back(item.first);
  std::move(callback).Run(format_names);
}

void MockRawClipboardHost::Read(const std::u16string& format,
                                ReadCallback callback) {
  const auto it = raw_data_map_.find(format);
  if (it == raw_data_map_.end())
    return;

  mojo_base::BigBuffer buffer = mojo_base::BigBuffer(
      base::make_span(it->second.data(), it->second.size()));
  std::move(callback).Run(std::move(buffer));
}

void MockRawClipboardHost::Write(const std::u16string& format,
                                 mojo_base::BigBuffer data) {
  if (needs_reset_)
    Reset();
  // Simulate the underlying platform copying this data.
  std::vector<uint8_t> data_copy(data.data(), data.data() + data.size());

  // Provide one commonly-used format on some platforms, where the platforms
  // automatically convert between certain format names, for use in testing.
  // Platforms often provide many converted formats, so not all converted-to
  // formats are provided.
  static constexpr char kPlatformTextFormat[] =
#if defined(OS_WIN)
      "CF_TEXT";
#elif defined(USE_X11)
      "text/plain";
#else
      "";
#endif

  if (format == base::ASCIIToUTF16(kPlatformTextFormat)) {
    EXPECT_TRUE(mock_clipboard_host_);
    std::u16string text = base::UTF8ToUTF16(base::StringPiece(
        reinterpret_cast<const char*>(data_copy.data()), data_copy.size()));
    mock_clipboard_host_->WriteText(text);
  }
  raw_data_map_[format] = data_copy;
}

void MockRawClipboardHost::CommitWrite() {
  // As the RawClipboardHost is an extension of ClipboardHost,
  // RawClipboardHost will make ClipboardHost commit, but not vice versa.
  EXPECT_TRUE(mock_clipboard_host_);
  mock_clipboard_host_->CommitWrite();
  needs_reset_ = true;
}

}  // namespace content
