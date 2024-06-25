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
  ClipboardHostImplTest() { ui::TestClipboard::CreateForCurrentThread(); }

  ~ClipboardHostImplTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("https://google.com/"));
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

  // Re-creates the system clipboard.
  void DeleteAndRecreateClipboard() {
    ui::Clipboard::DestroyClipboardForCurrentThread();
    ui::TestClipboard::CreateForCurrentThread();
  }

  static ui::Clipboard* system_clipboard() {
    return ui::Clipboard::GetForCurrentThread();
  }

 private:
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
                            ui::ClipboardFormatType::DataTransferCustomType());
  }
  EXPECT_FALSE(IsFormatAvailable(ui::ClipboardFormatType::FilenamesType()));
  EXPECT_TRUE(
      IsFormatAvailable(ui::ClipboardFormatType::DataTransferCustomType()));
  EXPECT_TRUE(IsFormatAvailable(ui::ClipboardFormatType::PlainTextType()));
  mojo_clipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &types);
  EXPECT_TRUE(base::Contains(types, u"text/plain"));
  EXPECT_TRUE(base::Contains(types, u"text/uri-list"));
}

class ClipboardHostImplWriteTest : public RenderViewHostTestHarness {
 protected:
  ClipboardHostImplWriteTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    ui::TestClipboard::CreateForCurrentThread();
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("https://foobar.com/"));
  }

  void TearDown() override {
    fake_clipboard_host_impl_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  ~ClipboardHostImplWriteTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  // Creates a fake clipboard host if it doesn't exist, or returns the already
  // created pointer.
  ClipboardHostImpl* clipboard_host_impl() {
    if (!fake_clipboard_host_impl_) {
      fake_clipboard_host_impl_ =
          new ClipboardHostImpl(*web_contents()->GetPrimaryMainFrame(),
                                remote_.BindNewPipeAndPassReceiver());
    }
    return fake_clipboard_host_impl_;
  }

  mojo::Remote<blink::mojom::ClipboardHost>& mojo_clipboard() {
    return remote_;
  }

  static ui::Clipboard* system_clipboard() {
    return ui::Clipboard::GetForCurrentThread();
  }

  RenderFrameHost& rfh() { return *web_contents()->GetPrimaryMainFrame(); }

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<ClipboardHostImpl> fake_clipboard_host_impl_;
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

TEST_F(ClipboardHostImplWriteTest, WriteDataTransferCustomData) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/type1"] = u"data1";
  custom_data[u"text/type2"] = u"data2";
  custom_data[u"text/type3"] = u"data3";

  clipboard_host_impl()->WriteDataTransferCustomData(custom_data);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future_1;
  base::test::TestFuture<const std::u16string&> future_2;
  base::test::TestFuture<const std::u16string&> future_3;

  clipboard_host_impl()->ReadDataTransferCustomData(
      ui::ClipboardBuffer::kCopyPaste, u"text/type1", future_1.GetCallback());
  clipboard_host_impl()->ReadDataTransferCustomData(
      ui::ClipboardBuffer::kCopyPaste, u"text/type2", future_2.GetCallback());
  clipboard_host_impl()->ReadDataTransferCustomData(
      ui::ClipboardBuffer::kCopyPaste, u"text/type3", future_3.GetCallback());

  EXPECT_EQ(custom_data[u"text/type1"], future_1.Take());
  EXPECT_EQ(custom_data[u"text/type2"], future_2.Take());
  EXPECT_EQ(custom_data[u"text/type3"], future_3.Take());
}

TEST_F(ClipboardHostImplWriteTest, WriteDataTransferCustomData_Empty) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"text/type1"] = u"";

  clipboard_host_impl()->WriteDataTransferCustomData(custom_data);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future_1;
  base::test::TestFuture<const std::u16string&> future_2;

  clipboard_host_impl()->ReadDataTransferCustomData(
      ui::ClipboardBuffer::kCopyPaste, u"text/type1", future_1.GetCallback());
  clipboard_host_impl()->ReadDataTransferCustomData(
      ui::ClipboardBuffer::kCopyPaste, u"text/type2", future_2.GetCallback());

  EXPECT_TRUE(future_1.Take().empty());
  EXPECT_TRUE(future_2.Take().empty());
}

class ClipboardHostImplAsyncWriteTest : public RenderViewHostTestHarness {
 protected:
  class AsyncWriteClipboardHostImpl : public ClipboardHostImpl {
   public:
    AsyncWriteClipboardHostImpl(
        RenderFrameHost& render_frame_host,
        mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
        : ClipboardHostImpl(render_frame_host, std::move(receiver)) {}

    void OnCopyAllowedResult(
        const ui::ClipboardFormatType& data_type,
        const ClipboardPasteData& data,
        std::optional<std::u16string> replacement_data) override {
      if (delay_) {
        delayed_on_copy_allowed_results_.push(base::BindOnce(
            &ClipboardHostImpl::OnCopyAllowedResult, base::Unretained(this),
            data_type, data, std::move(replacement_data)));
      } else {
        ClipboardHostImpl::OnCopyAllowedResult(data_type, data,
                                               std::move(replacement_data));
      }
    }

    void OnCopyHtmlAllowedResult(
        const GURL& source_url,
        const ui::ClipboardFormatType& data_type,
        const ClipboardPasteData& data,
        std::optional<std::u16string> replacement_data) override {
      if (delay_) {
        delayed_on_copy_allowed_results_.push(base::BindOnce(
            &ClipboardHostImpl::OnCopyHtmlAllowedResult, base::Unretained(this),
            source_url, data_type, data, std::move(replacement_data)));
      } else {
        ClipboardHostImpl::OnCopyHtmlAllowedResult(source_url, data_type, data,
                                                   std::move(replacement_data));
      }
    }

    void CallOneDelayedResult() {
      delay_ = false;
      auto& front = delayed_on_copy_allowed_results_.front();
      std::move(front).Run();
      delayed_on_copy_allowed_results_.pop();
    }

    void DelayWrites() { delay_ = true; }

   private:
    bool delay_ = true;
    std::queue<base::OnceClosure> delayed_on_copy_allowed_results_;
  };

  ClipboardHostImplAsyncWriteTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    ui::TestClipboard::CreateForCurrentThread();
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    NavigateAndCommit(GURL("https://google.com/"));
    fake_clipboard_host_impl_ =
        new AsyncWriteClipboardHostImpl(*web_contents()->GetPrimaryMainFrame(),
                                        remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    fake_clipboard_host_impl_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  ~ClipboardHostImplAsyncWriteTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  AsyncWriteClipboardHostImpl* async_write_clipboard_host_impl() {
    return fake_clipboard_host_impl_;
  }

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<AsyncWriteClipboardHostImpl> fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplAsyncWriteTest, WriteText) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  const std::u16string kText = u"text";
  async_write_clipboard_host_impl()->WriteText(kText);
  async_write_clipboard_host_impl()->CommitWrite();

  // Even after calling `CommitWrite()`, reading from the clipboard shouldn't
  // return `kText` as we don't know yet if it's allowed or not.
  base::test::TestFuture<const std::u16string&> first_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              first_future.GetCallback());
  EXPECT_TRUE(first_future.Take().empty());

  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&> second_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              second_future.GetCallback());
  EXPECT_EQ(second_future.Take(), kText);
}

TEST_F(ClipboardHostImplAsyncWriteTest, WriteHtml) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  const GURL kUrl("https://example.com");
  const std::u16string kHtml = u"<html>foo</html>";
  async_write_clipboard_host_impl()->WriteHtml(kHtml, kUrl);
  async_write_clipboard_host_impl()->CommitWrite();

  // Even after calling `CommitWrite()`, reading from the clipboard shouldn't
  // return `kHtml` as we don't know yet if it's allowed or not.
  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      first_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              first_future.GetCallback());
  EXPECT_TRUE(first_future.Get<std::u16string>().empty());
  EXPECT_TRUE(first_future.Get<GURL>().is_empty());
  EXPECT_EQ(first_future.Get<2>(), 0u);
  EXPECT_EQ(first_future.Get<3>(), 0u);

  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      second_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              second_future.GetCallback());
  EXPECT_EQ(second_future.Get<std::u16string>(), kHtml);
  EXPECT_EQ(second_future.Get<GURL>(), kUrl);
  EXPECT_EQ(second_future.Get<2>(), 0u);
  EXPECT_EQ(second_future.Get<3>(), 16u);
}

TEST_F(ClipboardHostImplAsyncWriteTest, WriteTextAndHtml) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  const std::u16string kText = u"text";
  const GURL kUrl("https://example.com");
  const std::u16string kHtml = u"<html>foo</html>";
  async_write_clipboard_host_impl()->WriteText(kText);
  async_write_clipboard_host_impl()->WriteHtml(kHtml, kUrl);
  async_write_clipboard_host_impl()->CommitWrite();

  // Even after calling `CommitWrite()`, reading from the clipboard shouldn't
  // return anything as we don't know yet if the data is allowed or not.
  base::test::TestFuture<const std::u16string&> first_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              first_text_future.GetCallback());
  EXPECT_TRUE(first_text_future.Take().empty());

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      first_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              first_html_future.GetCallback());
  EXPECT_TRUE(first_html_future.Get<std::u16string>().empty());
  EXPECT_TRUE(first_html_future.Get<GURL>().is_empty());
  EXPECT_EQ(first_html_future.Get<2>(), 0u);
  EXPECT_EQ(first_html_future.Get<3>(), 0u);

  // After only one delayed result has been propagated, the clipboard still
  // shouldn't have data as it isn't committed until the last result is
  // resolved.
  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&> second_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              second_text_future.GetCallback());
  EXPECT_TRUE(second_text_future.Take().empty());

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      second_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              second_html_future.GetCallback());
  EXPECT_TRUE(second_html_future.Get<std::u16string>().empty());
  EXPECT_TRUE(second_html_future.Get<GURL>().is_empty());
  EXPECT_EQ(second_html_future.Get<2>(), 0u);
  EXPECT_EQ(second_html_future.Get<3>(), 0u);

  // After calling the last delayed callback, the data should be in the
  // clipboard.
  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&> third_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              third_text_future.GetCallback());
  EXPECT_EQ(third_text_future.Take(), kText);

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      third_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              third_html_future.GetCallback());
  EXPECT_EQ(third_html_future.Get<std::u16string>(), kHtml);
  EXPECT_EQ(third_html_future.Get<GURL>(), kUrl);
  EXPECT_EQ(third_html_future.Get<2>(), 0u);
  EXPECT_EQ(third_html_future.Get<3>(), 16u);
}

TEST_F(ClipboardHostImplAsyncWriteTest, ConcurrentWrites) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  const std::u16string kFirstText = u"first text";
  const GURL kFirstUrl("https://first.example.com");
  const std::u16string kFirstHtml = u"<html>first foo</html>";

  async_write_clipboard_host_impl()->WriteText(kFirstText);
  async_write_clipboard_host_impl()->WriteHtml(kFirstHtml, kFirstUrl);
  async_write_clipboard_host_impl()->CommitWrite();

  // Even after calling `CommitWrite()`, reading from the clipboard shouldn't
  // return anything as we don't know yet if the data is allowed or not.
  base::test::TestFuture<const std::u16string&> first_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              first_text_future.GetCallback());
  EXPECT_TRUE(first_text_future.Take().empty());

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      first_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              first_html_future.GetCallback());
  EXPECT_TRUE(first_html_future.Get<std::u16string>().empty());
  EXPECT_TRUE(first_html_future.Get<GURL>().is_empty());
  EXPECT_EQ(first_html_future.Get<2>(), 0u);
  EXPECT_EQ(first_html_future.Get<3>(), 0u);

  // After only one delayed result has been propagated, the clipboard still
  // shouldn't have data as it isn't committed until the last result is
  // resolved.
  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&> second_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              second_text_future.GetCallback());
  EXPECT_TRUE(second_text_future.Take().empty());

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      second_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              second_html_future.GetCallback());
  EXPECT_TRUE(second_html_future.Get<std::u16string>().empty());
  EXPECT_TRUE(second_html_future.Get<GURL>().is_empty());
  EXPECT_EQ(second_html_future.Get<2>(), 0u);
  EXPECT_EQ(second_html_future.Get<3>(), 0u);

  // Making more `Write*` calls the first set hasn't completed should simply
  // queue the new values while still only committing when the last result has
  // been processed.
  const std::u16string kSecondText = u"second text";
  const GURL kSecondUrl("https://second.example.com");
  const std::u16string kSecondHtml = u"<html>second foo</html>";
  const std::u16string kSvg = u"svg";

  async_write_clipboard_host_impl()->DelayWrites();
  async_write_clipboard_host_impl()->WriteText(kSecondText);
  async_write_clipboard_host_impl()->WriteHtml(kSecondHtml, kSecondUrl);
  async_write_clipboard_host_impl()->WriteSvg(kSvg);
  async_write_clipboard_host_impl()->CommitWrite();

  // At this point we still have the first HTML write, second text write, second
  // HTML write and SVG write queued, so we should be able to make three more
  // `CallOneDelayedResult()` calls without getting all the data committed.
  for (int i = 0; i < 3; ++i) {
    async_write_clipboard_host_impl()->CallOneDelayedResult();

    base::test::TestFuture<const std::u16string&> empty_text_future;
    async_write_clipboard_host_impl()->ReadText(
        ui::ClipboardBuffer::kCopyPaste, empty_text_future.GetCallback());
    EXPECT_TRUE(empty_text_future.Take().empty());

    base::test::TestFuture<const std::u16string&, const GURL&, uint32_t,
                           uint32_t>
        empty_html_future;
    async_write_clipboard_host_impl()->ReadHtml(
        ui::ClipboardBuffer::kCopyPaste, empty_html_future.GetCallback());

    EXPECT_TRUE(empty_html_future.Get<std::u16string>().empty());
    EXPECT_TRUE(empty_html_future.Get<GURL>().is_empty());
    EXPECT_EQ(empty_html_future.Get<2>(), 0u);
    EXPECT_EQ(empty_html_future.Get<3>(), 0u);

    base::test::TestFuture<const std::u16string&> empty_svg_future;
    async_write_clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                               empty_svg_future.GetCallback());
    EXPECT_TRUE(empty_svg_future.Take().empty());
  }

  // After calling the last delayed callback, the data should be in the
  // clipboard.
  async_write_clipboard_host_impl()->CallOneDelayedResult();

  base::test::TestFuture<const std::u16string&> last_text_future;
  async_write_clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                              last_text_future.GetCallback());
  EXPECT_EQ(last_text_future.Take(), kSecondText);

  base::test::TestFuture<const std::u16string&, const GURL&, uint32_t, uint32_t>
      last_html_future;
  async_write_clipboard_host_impl()->ReadHtml(ui::ClipboardBuffer::kCopyPaste,
                                              last_html_future.GetCallback());
  EXPECT_EQ(last_html_future.Get<std::u16string>(), kSecondHtml);
  EXPECT_EQ(last_html_future.Get<GURL>(), kSecondUrl);
  EXPECT_EQ(last_html_future.Get<2>(), 0u);
  EXPECT_EQ(last_html_future.Get<3>(), 23u);

  base::test::TestFuture<const std::u16string&> last_svg_future;
  async_write_clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                             last_svg_future.GetCallback());
  EXPECT_EQ(last_svg_future.Take(), kSvg);
}

}  // namespace content
