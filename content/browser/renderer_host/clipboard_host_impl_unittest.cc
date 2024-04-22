// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/tokens/tokens.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/base/data_transfer_policy/data_transfer_policy_controller.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gfx/skia_util.h"
#include "url/gurl.h"

namespace ui {
class DataTransferEndpoint;
}
namespace content {

class ClipboardHostImplTest : public RenderViewHostTestHarness {
 protected:
  ClipboardHostImplTest()
      : clipboard_(ui::TestClipboard::CreateForCurrentThread()) {
  }

  ~ClipboardHostImplTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    ClipboardHostImpl::Create(web_contents()->GetPrimaryMainFrame(),
                              remote_.BindNewPipeAndPassReceiver());
  }

  bool IsFormatAvailable(ui::ClipboardFormatType type) {
    return system_clipboard()->IsFormatAvailable(
        type, ui::ClipboardBuffer::kCopyPaste,
        /* data_dst=*/nullptr);
  }

  mojo::Remote<blink::mojom::ClipboardHost>& mojo_clipboard() {
    return remote_;
  }

  // Re-creates the system clipboard and returns the previous clipboard.
  std::unique_ptr<ui::Clipboard> DeleteAndRecreateClipboard() {
    auto original_clipboard = ui::Clipboard::TakeForCurrentThread();
    clipboard_ = ui::TestClipboard::CreateForCurrentThread();
    return original_clipboard;
  }

  ui::Clipboard* system_clipboard() { return clipboard_; }

 private:
  raw_ptr<ui::Clipboard, DanglingUntriaged> clipboard_;
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
};

TEST_F(ClipboardHostImplTest, SimpleImage_ReadPng) {
  SkBitmap bitmap = gfx::test::CreateBitmap(3, 2);
  mojo_clipboard()->WriteImage(bitmap);
  ui::ClipboardSequenceNumberToken sequence_number =
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
  mojo_clipboard()->CommitWrite();
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(sequence_number, system_clipboard()->GetSequenceNumber(
                                 ui::ClipboardBuffer::kCopyPaste));
  EXPECT_FALSE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::PlainTextType(), ui::ClipboardBuffer::kCopyPaste,
      /* data_dst=*/nullptr));
  EXPECT_TRUE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::BitmapType(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
  EXPECT_TRUE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::PngType(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  std::vector<uint8_t> png =
      ui::clipboard_test_util::ReadPng(system_clipboard());
  SkBitmap actual;
  gfx::PNGCodec::Decode(png.data(), png.size(), &actual);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

TEST_F(ClipboardHostImplTest, DoesNotCacheClipboard) {
  ui::ClipboardSequenceNumberToken unused_sequence_number;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                      &unused_sequence_number);

  DeleteAndRecreateClipboard();

  // This shouldn't crash after the original ui::Clipboard is gone.
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                      &unused_sequence_number);
}

TEST_F(ClipboardHostImplTest, ReadAvailableTypes_TextUriList) {
  std::vector<std::u16string> types;

  // If clipboard contains files, only 'text/uri-list' should be available.
  // We exclude others like 'text/plain' which contin the full file path on some
  // platforms (http://crbug.com/1214108).
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames("file:///test/file");
    writer.WriteText(u"text");
  }
  EXPECT_TRUE(IsFormatAvailable(ui::ClipboardFormatType::FilenamesType()));
  EXPECT_TRUE(IsFormatAvailable(ui::ClipboardFormatType::PlainTextType()));
  mojo_clipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &types);
  EXPECT_EQ(std::vector<std::u16string>({u"text/uri-list"}), types);

  // If clipboard doesn't contain files, but custom data contains
  // 'text/uri-list', all other types should still be available since CrOS
  // FilesApp in particular sets types such as 'fs/sources' in addition to
  // 'text/uri-list' as custom types (http://crbug.com/1241671).
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"text");
    base::flat_map<std::u16string, std::u16string> custom_data;
    custom_data[u"text/uri-list"] = u"data";
    base::Pickle pickle;
    ui::WriteCustomDataToPickle(custom_data, &pickle);
    writer.WritePickledData(pickle,
                            ui::ClipboardFormatType::WebCustomDataType());
  }
  EXPECT_FALSE(IsFormatAvailable(ui::ClipboardFormatType::FilenamesType()));
  EXPECT_TRUE(IsFormatAvailable(ui::ClipboardFormatType::WebCustomDataType()));
  EXPECT_TRUE(IsFormatAvailable(ui::ClipboardFormatType::PlainTextType()));
  mojo_clipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &types);
  EXPECT_TRUE(base::Contains(types, u"text/plain"));
  EXPECT_TRUE(base::Contains(types, u"text/uri-list"));
}

class ClipboardHostImplWriteTest : public RenderViewHostTestHarness {
 protected:
  ClipboardHostImplWriteTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        clipboard_(ui::TestClipboard::CreateForCurrentThread()) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    fake_clipboard_host_impl_ =
        new ClipboardHostImpl(*web_contents()->GetPrimaryMainFrame(),
                              remote_.BindNewPipeAndPassReceiver());
  }

  ~ClipboardHostImplWriteTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  ClipboardHostImpl* clipboard_host_impl() { return fake_clipboard_host_impl_; }

  mojo::Remote<blink::mojom::ClipboardHost>& mojo_clipboard() {
    return remote_;
  }

  ui::Clipboard* system_clipboard() { return clipboard_; }

  RenderFrameHost& rfh() { return clipboard_host_impl()->render_frame_host(); }

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  const raw_ptr<ui::Clipboard, DanglingUntriaged> clipboard_;
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<ClipboardHostImpl, DanglingUntriaged> fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplWriteTest, MainFrameURL) {
  GURL gurl1("https://example.com");
  GURL gurl2("http://test.org");
  GURL gurl3("http://google.com");

  NavigateAndCommit(gurl1);
  content::RenderFrameHost* child_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          gurl2, content::RenderFrameHostTester::For(main_rfh())
                     ->AppendChild("child"));
  content::RenderFrameHost* grandchild_rfh =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          gurl3, content::RenderFrameHostTester::For(child_rfh)->AppendChild(
                     "grandchild"));

  mojo::Remote<blink::mojom::ClipboardHost> remote_grandchild;
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<ClipboardHostImpl> fake_clipboard_host_impl_grandchild =
      new ClipboardHostImpl(*grandchild_rfh,
                            remote_grandchild.BindNewPipeAndPassReceiver());

  bool is_policy_callback_called = false;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"data";
  fake_clipboard_host_impl_grandchild->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [&is_policy_callback_called](
              std::optional<ClipboardHostImpl::ClipboardPasteData>
                  clipboard_paste_data) {
            is_policy_callback_called = true;
            ASSERT_TRUE(clipboard_paste_data);
            ASSERT_EQ(clipboard_paste_data->text, u"data");
          }));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(is_policy_callback_called);
}

TEST_F(ClipboardHostImplWriteTest, GetSourceEndpoint) {
  const std::u16string kText = u"text";
  clipboard_host_impl()->WriteText(kText);
  clipboard_host_impl()->CommitWrite();

  // After writing the text to the clipboard with `clipboard_host_impl()`, the
  // source clipboard endpoint should match the current RFH.
  ClipboardEndpoint source_endpoint = GetSourceClipboardEndpoint(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
      ui::ClipboardBuffer::kCopyPaste);
  EXPECT_TRUE(source_endpoint.data_transfer_endpoint());
  EXPECT_TRUE(source_endpoint.data_transfer_endpoint()->IsUrlType());
  EXPECT_EQ(source_endpoint.web_contents(),
            WebContents::FromRenderFrameHost(&rfh()));
  EXPECT_EQ(source_endpoint.browser_context(), rfh().GetBrowserContext());

  // Calling `GetSourceClipboardEndpoint` with a different seqno will
  // return the same DTE, but no WebContents or BrowserContext.
  ui::ClipboardSequenceNumberToken other_seqno;
  ClipboardEndpoint empty_endpoint =
      GetSourceClipboardEndpoint(other_seqno, ui::ClipboardBuffer::kCopyPaste);
  EXPECT_TRUE(source_endpoint.data_transfer_endpoint());
  EXPECT_TRUE(source_endpoint.data_transfer_endpoint()->IsUrlType());
  EXPECT_FALSE(empty_endpoint.web_contents());
  EXPECT_FALSE(empty_endpoint.browser_context());
}

TEST_F(ClipboardHostImplWriteTest, WriteText) {
  const std::u16string kText = u"text";
  clipboard_host_impl()->WriteText(kText);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());
  EXPECT_EQ(kText, future.Take());
}

TEST_F(ClipboardHostImplWriteTest, WriteText_Empty) {
  clipboard_host_impl()->WriteText(u"");
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());
  EXPECT_TRUE(future.Take().empty());
}

TEST_F(ClipboardHostImplWriteTest, WriteHtml) {
  const GURL kUrl("https://example.com");
  const std::u16string kHtml = u"<html>foo</html>";
  clipboard_host_impl()->WriteHtml(kHtml, kUrl);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      future;
  clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());

  EXPECT_EQ(kHtml, future.Get<std::u16string>());
  EXPECT_EQ(kUrl, future.Get<GURL>());
  EXPECT_EQ(0u, future.Get<2>());
  EXPECT_EQ(kHtml.size(), future.Get<3>());
}

TEST_F(ClipboardHostImplWriteTest, WriteHtml_Empty) {
  clipboard_host_impl()->WriteHtml(u"", GURL());
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      future;
  clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());

  EXPECT_TRUE(future.Get<std::u16string>().empty());
  EXPECT_TRUE(future.Get<GURL>().is_empty());
  EXPECT_EQ(0u, future.Get<2>());
  EXPECT_EQ(0u, future.Get<3>());
}

TEST_F(ClipboardHostImplWriteTest, WriteSvg) {
  const std::u16string kSvg = u"svg data";
  clipboard_host_impl()->WriteSvg(kSvg);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                 future.GetCallback());

  EXPECT_EQ(kSvg, future.Take());
}

TEST_F(ClipboardHostImplWriteTest, WriteSvg_Empty) {
  clipboard_host_impl()->WriteSvg(u"");
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                 future.GetCallback());

  EXPECT_TRUE(future.Take().empty());
}

TEST_F(ClipboardHostImplWriteTest, WriteBitmap) {
  const SkBitmap kBitmap = gfx::test::CreateBitmap(3, 2);
  clipboard_host_impl()->WriteImage(kBitmap);
  clipboard_host_impl()->CommitWrite();

  std::vector<uint8_t> png =
      ui::clipboard_test_util::ReadPng(system_clipboard());
  SkBitmap actual;
  gfx::PNGCodec::Decode(png.data(), png.size(), &actual);
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, actual));
}

TEST_F(ClipboardHostImplWriteTest, WriteBitmap_Empty) {
  const SkBitmap kBitmap;
  clipboard_host_impl()->WriteImage(SkBitmap());
  clipboard_host_impl()->CommitWrite();

  std::vector<uint8_t> png =
      ui::clipboard_test_util::ReadPng(system_clipboard());
  SkBitmap actual;
  gfx::PNGCodec::Decode(png.data(), png.size(), &actual);
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, actual));
  EXPECT_TRUE(png.empty());
}

TEST_F(ClipboardHostImplWriteTest, WriteCustomData) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/type1"] = u"data1";
  custom_data[u"text/type2"] = u"data2";
  custom_data[u"text/type3"] = u"data3";

  clipboard_host_impl()->WriteCustomData(custom_data);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future_1;
  base::test::TestFuture<const std::u16string&> future_2;
  base::test::TestFuture<const std::u16string&> future_3;

  clipboard_host_impl()->ReadCustomData(ui::ClipboardBuffer::kCopyPaste,
                                        u"text/type1", future_1.GetCallback());
  clipboard_host_impl()->ReadCustomData(ui::ClipboardBuffer::kCopyPaste,
                                        u"text/type2", future_2.GetCallback());
  clipboard_host_impl()->ReadCustomData(ui::ClipboardBuffer::kCopyPaste,
                                        u"text/type3", future_3.GetCallback());

  EXPECT_EQ(custom_data[u"text/type1"], future_1.Take());
  EXPECT_EQ(custom_data[u"text/type2"], future_2.Take());
  EXPECT_EQ(custom_data[u"text/type3"], future_3.Take());
}

TEST_F(ClipboardHostImplWriteTest, WriteCustomData_Empty) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/type1"] = u"";

  clipboard_host_impl()->WriteCustomData(custom_data);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future_1;
  base::test::TestFuture<const std::u16string&> future_2;

  clipboard_host_impl()->ReadCustomData(ui::ClipboardBuffer::kCopyPaste,
                                        u"text/type1", future_1.GetCallback());
  clipboard_host_impl()->ReadCustomData(ui::ClipboardBuffer::kCopyPaste,
                                        u"text/type2", future_2.GetCallback());

  EXPECT_TRUE(future_1.Take().empty());
  EXPECT_TRUE(future_2.Take().empty());
}

}  // namespace content
