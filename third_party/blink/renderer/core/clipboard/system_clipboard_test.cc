// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/clipboard/system_clipboard.h"

#include <memory>

#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/clipboard/clipboard_mime_types.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/dummy_page_holder.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {

mojom::blink::ClipboardFilesPtr CreateFiles(int count) {
  WTF::Vector<mojom::blink::DataTransferFilePtr> vec;
  for (int i = 0; i < count; ++i) {
    vec.emplace_back(mojom::blink::DataTransferFile::New(
        base::FilePath(FILE_PATH_LITERAL("path")),
        base::FilePath(FILE_PATH_LITERAL("name")),
        mojo::PendingRemote<
            mojom::blink::FileSystemAccessDataTransferToken>()));
  }

  return mojom::blink::ClipboardFiles::New(std::move(vec), "file_system_id");
}

}  // namespace

class SystemClipboardTest : public testing::Test {
 public:
  SystemClipboardTest() {
    page_holder_ = std::make_unique<DummyPageHolder>(gfx::Size(1, 1));
    clipboard_provider_ =
        std::make_unique<PageTestBase::MockClipboardHostProvider>(
            page_holder_.get()->GetFrame().GetBrowserInterfaceBroker());
  }

 protected:
  MockClipboardHost* mock_clipboard_host() {
    return clipboard_provider_->clipboard_host();
  }
  mojom::blink::ClipboardHost* clipboard_host() {
    return clipboard_provider_->clipboard_host();
  }
  SystemClipboard& system_clipboard() {
    return *(page_holder_.get()->GetFrame().GetSystemClipboard());
  }
  void reset_remote_and_validate_buffer() {
    // Reset mojo remote to unbound.
    system_clipboard().clipboard_.reset();
    EXPECT_FALSE(system_clipboard().clipboard_.is_bound());
    // Check if the buffer is valid to make sure the read APIs return null
    // string because of unbound mojo remote and not because of invalid buffer.
    EXPECT_TRUE(
        system_clipboard().IsValidBufferType(system_clipboard().buffer_));
  }

  void RunUntilIdle() { test::RunPendingTasks(); }

 private:
  test::TaskEnvironment task_environment;

  std::unique_ptr<DummyPageHolder> page_holder_;
  std::unique_ptr<PageTestBase::MockClipboardHostProvider> clipboard_provider_;
};

TEST_F(SystemClipboardTest, Text) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadPlainText(), "");

  // Setting text in the host is visible in system.
  clipboard_host()->WriteText("first");
  EXPECT_EQ(system_clipboard().ReadPlainText(), "first");

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    clipboard_host()->WriteText("second");
    EXPECT_EQ(system_clipboard().ReadPlainText(), "second");

    clipboard_host()->WriteText("third");
    EXPECT_EQ(system_clipboard().ReadPlainText(), "second");
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadPlainText(), "third");
}

TEST_F(SystemClipboardTest, Html) {
  KURL url;
  unsigned fragment_start;
  unsigned fragment_end;

  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end), "");

  // Setting text in the host is visible in system.
  clipboard_host()->WriteHtml("first", KURL("http://first.com"));
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            "first");
  EXPECT_EQ(url, KURL("http://first.com"));

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    clipboard_host()->WriteHtml("second", KURL("http://second.com"));
    EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
              "second");
    EXPECT_EQ(url, KURL("http://second.com"));

    clipboard_host()->WriteHtml("third", KURL("http://third.com"));
    EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
              "second");
    EXPECT_EQ(url, KURL("http://second.com"));
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            "third");
  EXPECT_EQ(url, KURL("http://third.com"));
}

TEST_F(SystemClipboardTest, ReadHtml_SameFragmentArgs) {
  KURL url;
  unsigned fragment_start;
  unsigned fragment_end;
  const String kHtmlText = "first";

  // Setting text in the host is visible in system.
  clipboard_host()->WriteHtml(kHtmlText, KURL("http://first.com"));

  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            kHtmlText);
  EXPECT_EQ(fragment_start, 0u);
  EXPECT_EQ(fragment_end, kHtmlText.length());

  ScopedSystemClipboardSnapshot snapshot(system_clipboard());

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result.  Specify the same variable for start and end
  // to make sure this does not mess up the values stored in the
  // snapshot.
  unsigned ignore;
  EXPECT_EQ(system_clipboard().ReadHTML(url, ignore, ignore), kHtmlText);

  // Now perform a ReadHTML() with different variable for start and end.
  // This will read from the snapshot and should return expected values.
  unsigned fragment_start2;
  unsigned fragment_end2;
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start2, fragment_end2),
            kHtmlText);
  EXPECT_EQ(fragment_start2, 0u);
  EXPECT_EQ(fragment_end2, kHtmlText.length());
}

TEST_F(SystemClipboardTest, Rtf) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadRTF(), "");

  // Setting text in the host is visible in system.
  mock_clipboard_host()->WriteRtf("first");
  EXPECT_EQ(system_clipboard().ReadRTF(), "first");

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    mock_clipboard_host()->WriteRtf("second");
    EXPECT_EQ(system_clipboard().ReadRTF(), "second");

    mock_clipboard_host()->WriteRtf("third");
    EXPECT_EQ(system_clipboard().ReadRTF(), "second");
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadRTF(), "third");
}

TEST_F(SystemClipboardTest, Png) {
  auto buf = mojom::blink::ClipboardBuffer::kStandard;

  // Clipboard starts empty.
  mojo_base::BigBuffer png = system_clipboard().ReadPng(buf);
  EXPECT_EQ(png.size(), 0u);

  // Create test bitmaps to put into the clipboard.
  SkBitmap bitmap1;
  SkBitmap bitmap2;
  SkBitmap bitmap3;
  ASSERT_TRUE(bitmap1.tryAllocPixelsFlags(
      SkImageInfo::Make(4, 3, kN32_SkColorType, kOpaque_SkAlphaType), 0));
  ASSERT_TRUE(bitmap2.tryAllocPixelsFlags(
      SkImageInfo::Make(40, 30, kN32_SkColorType, kOpaque_SkAlphaType), 0));
  ASSERT_TRUE(bitmap3.tryAllocPixelsFlags(
      SkImageInfo::Make(400, 300, kN32_SkColorType, kOpaque_SkAlphaType), 0));

  // Setting image in the host is visible in system.
  clipboard_host()->WriteImage(bitmap1);
  clipboard_host()->CommitWrite();

  SkBitmap bitmap;
  png = system_clipboard().ReadPng(buf);
  ASSERT_TRUE(gfx::PNGCodec::Decode(png.data(), png.size(), &bitmap));
  EXPECT_EQ(bitmap.width(), 4);
  EXPECT_EQ(bitmap.height(), 3);

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    clipboard_host()->WriteImage(bitmap2);
    clipboard_host()->CommitWrite();
    png = system_clipboard().ReadPng(buf);
    ASSERT_TRUE(gfx::PNGCodec::Decode(png.data(), png.size(), &bitmap));
    EXPECT_EQ(bitmap.width(), 40);
    EXPECT_EQ(bitmap.height(), 30);

    clipboard_host()->WriteImage(bitmap3);
    clipboard_host()->CommitWrite();
    png = system_clipboard().ReadPng(buf);
    ASSERT_TRUE(gfx::PNGCodec::Decode(png.data(), png.size(), &bitmap));
    EXPECT_EQ(bitmap.width(), 40);
    EXPECT_EQ(bitmap.height(), 30);
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  png = system_clipboard().ReadPng(buf);
  ASSERT_TRUE(gfx::PNGCodec::Decode(png.data(), png.size(), &bitmap));
  EXPECT_EQ(bitmap.width(), 400);
  EXPECT_EQ(bitmap.height(), 300);
}

TEST_F(SystemClipboardTest, Files) {
  // Clipboard starts empty.
  auto files = system_clipboard().ReadFiles();
  EXPECT_EQ(files->files.size(), 0u);

  // Setting file in the host is visible in system.
  mock_clipboard_host()->WriteFiles(CreateFiles(1));
  EXPECT_EQ(system_clipboard().ReadFiles()->files.size(), 1u);

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    mock_clipboard_host()->WriteFiles(CreateFiles(2));
    EXPECT_EQ(system_clipboard().ReadFiles()->files.size(), 2u);

    mock_clipboard_host()->WriteFiles(CreateFiles(3));
    EXPECT_EQ(system_clipboard().ReadFiles()->files.size(), 2u);
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadFiles()->files.size(), 3u);
}

TEST_F(SystemClipboardTest, CustomData) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("a"), "");

  // Setting text in the host is visible in system.
  clipboard_host()->WriteDataTransferCustomData({{"a", "first"}});
  EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("a"), "first");

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    clipboard_host()->WriteDataTransferCustomData({{"a", "second"}});
    EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("a"), "second");

    clipboard_host()->WriteDataTransferCustomData(
        {{"a", "third"}, {"b", "fourth"}});
    EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("a"), "second");
    EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("b"), "fourth");
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadDataTransferCustomData("a"), "third");
}

TEST_F(SystemClipboardTest, SnapshotNesting) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadPlainText(), "");

  // Setting text in the host is visible in system.
  clipboard_host()->WriteText("first");
  EXPECT_EQ(system_clipboard().ReadPlainText(), "first");

  // Inside a snapshot scope, the first read from the system clipboard
  // remembers the result, even if the underlying clipboard host changes.
  {
    ScopedSystemClipboardSnapshot snapshot(system_clipboard());

    clipboard_host()->WriteText("second");
    EXPECT_EQ(system_clipboard().ReadPlainText(), "second");

    clipboard_host()->WriteText("third");
    EXPECT_EQ(system_clipboard().ReadPlainText(), "second");

    // Nest another snapshot.  Things should remain stable.
    {
      ScopedSystemClipboardSnapshot snapshot2(system_clipboard());

      clipboard_host()->WriteText("fourth");
      EXPECT_EQ(system_clipboard().ReadPlainText(), "second");
    }

    // When second snapshot closes, the original one should still be be in
    // effect.
    clipboard_host()->WriteText("fifth");
    EXPECT_EQ(system_clipboard().ReadPlainText(), "second");
  }

  // Now that the snapshot is out of scope, reads from the system clipboard
  // reflect the final state of the clipboard host.
  EXPECT_EQ(system_clipboard().ReadPlainText(), "fifth");
}

TEST_F(SystemClipboardTest, ReadTextWithUnboundClipboardHost) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadPlainText(), "");

  // Setting text in the host is visible in system clipboard.
  clipboard_host()->WriteText("first");
  EXPECT_EQ(system_clipboard().ReadPlainText(), "first");

  reset_remote_and_validate_buffer();

  // Now the Reads should return null string.
  EXPECT_EQ(system_clipboard().ReadPlainText(), String());
  // Writes will fail since the mojo remote is unbound.
  clipboard_host()->WriteText("second");
  EXPECT_EQ(system_clipboard().ReadPlainText(), String());
}

TEST_F(SystemClipboardTest, ReadHtmlWithUnboundClipboardHost) {
  KURL url;
  unsigned fragment_start;
  unsigned fragment_end;

  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end), "");

  // Setting text in the host is visible in system clipboard.
  clipboard_host()->WriteHtml("first", KURL("http://first.com"));
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            "first");
  EXPECT_EQ(url, KURL("http://first.com"));

  reset_remote_and_validate_buffer();

  // Now the Reads should return null string.
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            String());
  // Writes will fail since the mojo remote is unbound.
  clipboard_host()->WriteHtml("first", KURL("http://first.com"));
  EXPECT_EQ(system_clipboard().ReadHTML(url, fragment_start, fragment_end),
            String());
  EXPECT_EQ(url, KURL(String()));
}

TEST_F(SystemClipboardTest, ReadRtfWithUnboundClipboardHost) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadRTF(), "");

  // Setting text in the host is visible in system clipboard.
  mock_clipboard_host()->WriteRtf("first");
  EXPECT_EQ(system_clipboard().ReadRTF(), "first");

  reset_remote_and_validate_buffer();

  // Now the Reads should return null string.
  EXPECT_EQ(system_clipboard().ReadRTF(), String());
  // Writes will fail since the mojo remote is unbound.
  mock_clipboard_host()->WriteRtf("second");
  EXPECT_EQ(system_clipboard().ReadRTF(), String());
}

TEST_F(SystemClipboardTest, ReadPngWithUnboundClipboardHost) {
  auto buf = mojom::blink::ClipboardBuffer::kStandard;

  // Clipboard starts empty.
  mojo_base::BigBuffer png = system_clipboard().ReadPng(buf);
  EXPECT_EQ(png.size(), 0u);

  // Create test bitmaps to put into the clipboard.
  SkBitmap bitmapIn;
  ASSERT_TRUE(bitmapIn.tryAllocPixelsFlags(
      SkImageInfo::Make(4, 3, kN32_SkColorType, kOpaque_SkAlphaType), 0));

  // Setting image in the host is visible in system clipboard.
  clipboard_host()->WriteImage(bitmapIn);
  clipboard_host()->CommitWrite();

  SkBitmap bitmapOut;
  png = system_clipboard().ReadPng(buf);
  ASSERT_TRUE(gfx::PNGCodec::Decode(png.data(), png.size(), &bitmapOut));
  EXPECT_EQ(bitmapOut.width(), 4);
  EXPECT_EQ(bitmapOut.height(), 3);

  reset_remote_and_validate_buffer();

  // Now the Reads should return zero sized png.
  png = system_clipboard().ReadPng(buf);
  EXPECT_EQ(png.size(), 0u);

  // Setting image in the host will fail since the mojo remote is unbound.
  clipboard_host()->WriteImage(bitmapIn);
  clipboard_host()->CommitWrite();
  png = system_clipboard().ReadPng(buf);
  EXPECT_EQ(png.size(), 0u);
}

TEST_F(SystemClipboardTest, ReadFilesWithUnboundClipboardHost) {
  // Clipboard starts empty.
  auto files = system_clipboard().ReadFiles();
  EXPECT_EQ(files->files.size(), 0u);

  // Setting file in the host is visible in system clipboard.
  mock_clipboard_host()->WriteFiles(CreateFiles(1));
  EXPECT_EQ(system_clipboard().ReadFiles()->files.size(), 1u);

  reset_remote_and_validate_buffer();

  // Now the Reads should return null pointer to files.
  EXPECT_TRUE(system_clipboard().ReadFiles().is_null());
  // Writes will fail since the mojo remote is unbound.
  mock_clipboard_host()->WriteFiles(CreateFiles(1));
  EXPECT_TRUE(system_clipboard().ReadFiles().is_null());
}

TEST_F(SystemClipboardTest, IsFormatAvailableWithUnboundClipboardHost) {
  // Clipboard starts empty.
  EXPECT_FALSE(system_clipboard().IsFormatAvailable(
      blink::mojom::ClipboardFormat::kPlaintext));

  // Setting text in the host is visible in system clipboard.
  clipboard_host()->WriteText("first");
  EXPECT_TRUE(system_clipboard().IsFormatAvailable(
      blink::mojom::ClipboardFormat::kPlaintext));

  reset_remote_and_validate_buffer();

  // Now the Reads should return false.
  EXPECT_FALSE(system_clipboard().IsFormatAvailable(
      blink::mojom::ClipboardFormat::kPlaintext));
  // Writes will fail since the mojo remote is unbound.
  clipboard_host()->WriteText("second");
  EXPECT_FALSE(system_clipboard().IsFormatAvailable(
      blink::mojom::ClipboardFormat::kPlaintext));
}

TEST_F(SystemClipboardTest, ReadAvailableTypesWithUnboundClipboardHost) {
  // Clipboard starts empty.
  EXPECT_EQ(system_clipboard().ReadAvailableTypes().size(), 0u);

  // Setting text in the host is visible in system clipboard.
  clipboard_host()->WriteText("first");
  EXPECT_EQ(system_clipboard().ReadAvailableTypes().size(), 1u);

  reset_remote_and_validate_buffer();

  // Now the Reads should return no available types.
  EXPECT_EQ(system_clipboard().ReadAvailableTypes().size(), 0u);
  // Writes will fail since the mojo remote is unbound.
  clipboard_host()->WriteText("second");
  EXPECT_EQ(system_clipboard().ReadAvailableTypes().size(), 0u);
}

TEST_F(SystemClipboardTest, SequenceNumberWithUnboundClipboardHost) {
  // Clipboard starts empty.
  auto sequence_number = system_clipboard().SequenceNumber();
  // Setting text in the host is visible in system clipboard.
  clipboard_host()->WriteText("first");
  clipboard_host()->CommitWrite();
  auto sequence_number_after_write = system_clipboard().SequenceNumber();
  EXPECT_NE(sequence_number, sequence_number_after_write);

  reset_remote_and_validate_buffer();

  // After clipboard reset, sequenceNumber will be random.
  auto sequence_number_after_reset = system_clipboard().SequenceNumber();
  EXPECT_NE(sequence_number_after_write, sequence_number_after_reset);
}
}  // namespace blink
