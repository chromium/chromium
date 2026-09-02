// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/pickle.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_content_browser_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/tokens/tokens.mojom-forward.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/file_info.h"
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

// Custom ContentBrowserClient for testing clipboard paste permissions.
class ClipboardPasteAllowedBrowserClient : public TestContentBrowserClient {
 public:
  ClipboardPasteAllowedBrowserClient() = default;
  ~ClipboardPasteAllowedBrowserClient() override = default;

  void set_is_clipboard_paste_allowed(bool allowed) {
    is_clipboard_paste_allowed_ = allowed;
  }

  // ContentBrowserClient:
  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override {
    return is_clipboard_paste_allowed_;
  }

 private:
  bool is_clipboard_paste_allowed_ = true;
};

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

  void NavigateAndCreateClipboardHostImpl(const GURL& url) {
    if (remote_.is_bound()) {
      remote_.reset();
    }
    NavigateAndCommit(url);
    // Recreating this after navigation is necessary because we usually change
    // RFH on navigation and clipboard host is bound to RFH.
    ClipboardHostImpl::Create(web_contents()->GetPrimaryMainFrame(),
                              remote_.BindNewPipeAndPassReceiver());
  }

  bool IsFormatAvailable(ui::ClipboardFormatType type) {
    return ui::clipboard_test_util::IsFormatAvailable(
        system_clipboard(), type, ui::ClipboardBuffer::kCopyPaste,
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
  mojo_clipboard().FlushForTesting();

  EXPECT_NE(sequence_number, system_clipboard()->GetSequenceNumber(
                                 ui::ClipboardBuffer::kCopyPaste));
  EXPECT_FALSE(ui::clipboard_test_util::IsFormatAvailable(
      system_clipboard(), ui::ClipboardFormatType::PlainTextType(),
      ui::ClipboardBuffer::kCopyPaste,
      /* data_dst=*/nullptr));
  EXPECT_TRUE(ui::clipboard_test_util::IsFormatAvailable(
      system_clipboard(), ui::ClipboardFormatType::BitmapType(),
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));
  EXPECT_TRUE(ui::clipboard_test_util::IsFormatAvailable(
      system_clipboard(), ui::ClipboardFormatType::PngType(),
      ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  std::vector<uint8_t> png = ui::clipboard_test_util::ReadPng(
      system_clipboard(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  SkBitmap actual = gfx::PNGCodec::Decode(png);
  ASSERT_TRUE(!actual.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

TEST_F(ClipboardHostImplTest, DoesNotCacheClipboard) {
  absl::uint128 unused_sequence_number;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                      &unused_sequence_number);

  DeleteAndRecreateClipboard();

  // This shouldn't crash after the original ui::Clipboard is gone.
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste,
                                      &unused_sequence_number);
}

TEST_F(ClipboardHostImplTest, WriteFromInactiveDocumentIsIgnored) {
  const std::u16string kInitial = u"initial";
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(kInitial);
  }

  static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
      ->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  ASSERT_FALSE(web_contents()->GetPrimaryMainFrame()->IsActive());

  mojo_clipboard()->WriteText(u"from-inactive-document");
  mojo_clipboard()->CommitWrite();
  mojo_clipboard().FlushForTesting();

  base::test::TestFuture<std::u16string> future;
  system_clipboard()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                               /*data_dst=*/std::nullopt, future.GetCallback());
  EXPECT_EQ(kInitial, future.Take());
}

TEST_F(ClipboardHostImplTest, ReadFromInactiveDocumentIsIgnored) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"clipboard-text");
  }

  static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
      ->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  ASSERT_FALSE(web_contents()->GetPrimaryMainFrame()->IsActive());

  std::u16string result = u"non-empty";
  mojo_clipboard()->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);
  EXPECT_TRUE(result.empty());

  std::vector<std::u16string> types = {u"non-empty"};
  mojo_clipboard()->ReadAvailableTypes(ui::ClipboardBuffer::kCopyPaste, &types);
  EXPECT_TRUE(types.empty());
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
  EXPECT_TRUE(std::ranges::contains(types, u"text/plain"));
  EXPECT_TRUE(std::ranges::contains(types, u"text/uri-list"));
}

TEST_F(ClipboardHostImplTest, GetSequenceNumber) {
  NavigateAndCreateClipboardHostImpl(GURL("https://google.com/"));
  absl::uint128 id1;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste, &id1);

  // Writing to the clipboard should change the sequence number and thus the ID.
  const SkBitmap kBitmap = gfx::test::CreateBitmap(3, 2);
  mojo_clipboard()->WriteImage(kBitmap);
  mojo_clipboard()->CommitWrite();
  mojo_clipboard().FlushForTesting();

  absl::uint128 id2;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste, &id2);
  EXPECT_NE(id1, id2);

  // The ID should be stable if the clipboard hasn't changed.
  absl::uint128 id3;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste, &id3);
  EXPECT_EQ(id2, id3);

  NavigateAndCreateClipboardHostImpl(GURL("https://foobar.com/"));
  // Even though the clipboard contents haven't changed, a different origin
  // should not have the same sequence number.
  absl::uint128 id4;
  mojo_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste, &id4);
  EXPECT_NE(id3, id4);
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

  void ValidateClipboardSource() {
    base::test::TestFuture<ClipboardEndpoint> future;
    GetSourceClipboardEndpoint(nullptr, ui::ClipboardBuffer::kCopyPaste,
                               future.GetCallback());
    ClipboardEndpoint source_endpoint = future.Take();
    EXPECT_TRUE(source_endpoint.data_transfer_endpoint());
    EXPECT_TRUE(source_endpoint.data_transfer_endpoint()->IsUrlType());
    EXPECT_EQ(source_endpoint.web_contents(),
              WebContents::FromRenderFrameHost(&rfh()));
    EXPECT_EQ(source_endpoint.browser_context(), rfh().GetBrowserContext());
  }

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<ClipboardHostImpl> fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplWriteTest, NoSourceWithoutDataWrite) {
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());
  EXPECT_EQ(u"", future.Take());

  base::test::TestFuture<ClipboardEndpoint> source_future;
  GetSourceClipboardEndpoint(nullptr, ui::ClipboardBuffer::kCopyPaste,
                             source_future.GetCallback());
  ClipboardEndpoint source_endpoint = source_future.Take();
  EXPECT_FALSE(source_endpoint.data_transfer_endpoint());
  EXPECT_FALSE(source_endpoint.web_contents());
  EXPECT_FALSE(source_endpoint.browser_context());
}

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

  base::RunLoop run_loop;
  fake_clipboard_host_impl_grandchild->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [&is_policy_callback_called,
           &run_loop](std::optional<ClipboardHostImpl::ClipboardPasteData>
                          clipboard_paste_data) {
            is_policy_callback_called = true;
            EXPECT_TRUE(clipboard_paste_data);
            EXPECT_EQ(clipboard_paste_data->text, u"data");
            run_loop.Quit();
          }));
  run_loop.Run();

  EXPECT_TRUE(is_policy_callback_called);
}

TEST_F(ClipboardHostImplWriteTest, WriteText) {
  const std::u16string kText = u"text";
  clipboard_host_impl()->WriteText(kText);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());
  EXPECT_EQ(kText, future.Take());
  ValidateClipboardSource();
}

TEST_F(ClipboardHostImplWriteTest, WriteText_Empty) {
  clipboard_host_impl()->WriteText(u"");
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                                  future.GetCallback());
  EXPECT_TRUE(future.Take().empty());
  ValidateClipboardSource();
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
  ValidateClipboardSource();
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
  ValidateClipboardSource();
}

TEST_F(ClipboardHostImplWriteTest, WriteSvg) {
  const std::u16string kSvg = u"svg data";
  clipboard_host_impl()->WriteSvg(kSvg);
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                 future.GetCallback());

  EXPECT_EQ(kSvg, future.Take());
  ValidateClipboardSource();
}

TEST_F(ClipboardHostImplWriteTest, WriteSvg_Empty) {
  clipboard_host_impl()->WriteSvg(u"");
  clipboard_host_impl()->CommitWrite();

  base::test::TestFuture<const std::u16string&> future;
  clipboard_host_impl()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                                 future.GetCallback());

  EXPECT_TRUE(future.Take().empty());
  ValidateClipboardSource();
}

// Regression coverage for crbug.com/495504337: a valid URL still round-trips
// through WriteBookmark after the invalid-URL guard was added.
TEST_F(ClipboardHostImplWriteTest, WriteBookmark_ValidUrl) {
  const std::string kUrl = "https://example.com/page";
  const std::u16string kTitle = u"Example Page";
  ASSERT_TRUE(GURL(kUrl).is_valid());

  clipboard_host_impl()->WriteBookmark(kUrl, kTitle);
  clipboard_host_impl()->CommitWrite();

  std::u16string title;
  std::string url;
  ui::clipboard_test_util::ReadBookmark(system_clipboard(),
                                        /*data_dst=*/nullptr, &title, &url);
  EXPECT_EQ(kUrl, url);
#if !BUILDFLAG(IS_WIN)
  EXPECT_EQ(kTitle, title);
#else
  // ClipboardWin::ReadURL does not round-trip the title.
  EXPECT_TRUE(title.empty()) << "Got title='" << title << "'";
#endif
  // ValidateClipboardSource() is intentionally not called: WriteBookmark does
  // not go through IsClipboardCopyAllowedByPolicy, so no SourceRFHToken is
  // pickled into the clipboard.
}

// Regression test for crbug.com/495504337: "http://[" canonicalizes to
// is_valid_=false with a non-empty spec_, which is exactly what GURL::spec()
// CHECKs on. With the fix the IPC is silently dropped at the trust boundary.
TEST_F(ClipboardHostImplWriteTest, WriteBookmark_InvalidUrl_DoesNotCrash) {
  const std::string kInvalidUrl = "http://[";
  ASSERT_FALSE(GURL(kInvalidUrl).is_valid());
  ASSERT_FALSE(GURL(kInvalidUrl).possibly_invalid_spec().empty());

  clipboard_host_impl()->WriteBookmark(kInvalidUrl, u"some title");
  clipboard_host_impl()->CommitWrite();

  std::u16string title;
  std::string url;
  ui::clipboard_test_util::ReadBookmark(system_clipboard(),
                                        /*data_dst=*/nullptr, &title, &url);
  EXPECT_TRUE(url.empty()) << "Got url='" << url << "'";
  EXPECT_TRUE(title.empty()) << "Got title='" << title << "'";
}

// Pins the silent-drop behavior for empty URLs (via ShouldSkipBookmark).
TEST_F(ClipboardHostImplWriteTest, WriteBookmark_EmptyUrl) {
  clipboard_host_impl()->WriteBookmark(std::string(), u"some title");
  clipboard_host_impl()->CommitWrite();

  std::u16string title;
  std::string url;
  ui::clipboard_test_util::ReadBookmark(system_clipboard(),
                                        /*data_dst=*/nullptr, &title, &url);
  EXPECT_TRUE(url.empty());
  EXPECT_TRUE(title.empty());
}

// file:// URLs are dropped so a renderer cannot place a local path on the
// clipboard via WriteBookmark and read it back via ReadFiles.
TEST_F(ClipboardHostImplWriteTest, WriteBookmark_FileUrl) {
  clipboard_host_impl()->WriteBookmark("file:///etc/passwd", u"some title");
  clipboard_host_impl()->CommitWrite();

  std::u16string title;
  std::string url;
  ui::clipboard_test_util::ReadBookmark(system_clipboard(),
                                        /*data_dst=*/nullptr, &title, &url);
  EXPECT_TRUE(url.empty()) << "Got url='" << url << "'";
  EXPECT_TRUE(title.empty()) << "Got title='" << title << "'";

  std::vector<ui::FileInfo> files = ui::clipboard_test_util::ReadFilenames(
      system_clipboard(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  EXPECT_TRUE(files.empty());
}

TEST_F(ClipboardHostImplWriteTest, WriteBitmap) {
  const SkBitmap kBitmap = gfx::test::CreateBitmap(3, 2);
  clipboard_host_impl()->WriteImage(kBitmap);
  clipboard_host_impl()->CommitWrite();

  std::vector<uint8_t> png = ui::clipboard_test_util::ReadPng(
      system_clipboard(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  SkBitmap actual = gfx::PNGCodec::Decode(png);
  ASSERT_FALSE(actual.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, actual));
  ValidateClipboardSource();
}

TEST_F(ClipboardHostImplWriteTest, WriteBitmap_Empty) {
  const SkBitmap kBitmap;
  clipboard_host_impl()->WriteImage(SkBitmap());
  clipboard_host_impl()->CommitWrite();

  std::vector<uint8_t> png = ui::clipboard_test_util::ReadPng(
      system_clipboard(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr);
  SkBitmap actual = gfx::PNGCodec::Decode(png);
  EXPECT_TRUE(actual.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(kBitmap, actual));
  EXPECT_TRUE(png.empty());
  ValidateClipboardSource();
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
  ValidateClipboardSource();
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
  ValidateClipboardSource();
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

    void OnCopyCustomFormatAllowedResult(
        const std::u16string& format,
        mojo_base::BigBuffer data,
        const ui::ClipboardFormatType& data_type,
        const ClipboardPasteData& paste_data,
        std::optional<std::u16string> replacement_data) override {
      if (delay_) {
        // We push this to same queue as other delayed allowed results so
        // `CallOneDelayedResult()` continues to work.
        delayed_on_copy_allowed_results_.push(
            base::BindOnce(&ClipboardHostImpl::OnCopyCustomFormatAllowedResult,
                           base::Unretained(this), format, std::move(data),
                           data_type, paste_data, std::move(replacement_data)));
      } else {
        ClipboardHostImpl::OnCopyCustomFormatAllowedResult(
            format, std::move(data), data_type, paste_data,
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

  mojo::Remote<blink::mojom::ClipboardHost>& remote() { return remote_; }

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

TEST_F(ClipboardHostImplAsyncWriteTest, WriteUnsanitizedCustomFormat) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  std::string test_data = "test custom format data";
  // The 'web ' prefix is added by blink and stripped by the browser during
  // initial parsing.
  const std::u16string write_format = u"text/custom-format";
  const std::u16string read_format = u"web text/custom-format";
  remote()->WriteUnsanitizedCustomFormat(
      write_format, mojo_base::BigBuffer(base::as_byte_span(test_data)));
  remote()->CommitWrite();
  remote().FlushForTesting();

  // Initially empty before policy evaluates.
  base::test::TestFuture<mojo_base::BigBuffer> pre_policy_future;
  remote()->ReadUnsanitizedCustomFormat(read_format,
                                        pre_policy_future.GetCallback());
  EXPECT_EQ(0u, pre_policy_future.Get().size());

  // Wait for mojo messages to process, then for delayed policy result to
  // propagate through `CommitWrite()`.
  async_write_clipboard_host_impl()->CallOneDelayedResult();
  remote().FlushForTesting();

  base::test::TestFuture<mojo_base::BigBuffer> post_policy_future;
  remote()->ReadUnsanitizedCustomFormat(read_format,
                                        post_policy_future.GetCallback());
  const auto& actual_result = post_policy_future.Get();
  EXPECT_GT(actual_result.size(), 0u);

  std::string read_string(actual_result.begin(), actual_result.end());
  EXPECT_EQ(read_string, test_data);
}

class PolicyBlockBrowserClient : public TestContentBrowserClient {
 public:
  PolicyBlockBrowserClient() = default;
  ~PolicyBlockBrowserClient() override = default;

  void IsClipboardCopyAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ui::ClipboardMetadata& metadata,
      const ClipboardPasteData& data,
      IsClipboardCopyAllowedCallback callback) override {
    // Simulate a policy block returning a replacement string.
    std::optional<std::u16string> replacement_data = u"Policy Blocked";
    std::move(callback).Run(metadata.format_type, data,
                            std::move(replacement_data));
  }

  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ui::ClipboardMetadata& metadata,
      ClipboardPasteData data,
      IsClipboardPasteAllowedCallback callback) override {
    if (metadata.format_type == ui::ClipboardFormatType::WebCustomFormatMap()) {
      // Custom formats cannot be string-replaced.
      std::move(callback).Run(std::nullopt);
      return;
    }
    // Simulate a policy block returning a replacement string.
    std::optional<ClipboardPasteData> replacement_data(data);
    replacement_data->text = u"Paste Policy Blocked";
    std::move(callback).Run(std::move(replacement_data));
  }
};

TEST_F(ClipboardHostImplAsyncWriteTest,
       WriteUnsanitizedCustomFormat_PolicyBlocked) {
  PolicyBlockBrowserClient browser_client;
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  std::string test_data = "test custom format data";
  // The 'web ' prefix is added by blink and stripped by the browser during
  // initial parsing, so we use the un-prefixed format for writing to mock a
  // browser write, but the prefixed format for reading to mock a blink read.
  const std::u16string write_format = u"text/custom-format";
  const std::u16string read_format = u"web text/custom-format";
  remote()->WriteUnsanitizedCustomFormat(
      write_format, mojo_base::BigBuffer(base::as_byte_span(test_data)));
  remote()->CommitWrite();
  remote().FlushForTesting();

  // Wait for mojo messages to process, then for delayed policy result to
  // propagate through `CommitWrite()`.
  async_write_clipboard_host_impl()->CallOneDelayedResult();
  remote().FlushForTesting();

  // Custom format should not be available, and replacement text should be on
  // clipboard instead.
  base::test::TestFuture<mojo_base::BigBuffer> future;
  remote()->ReadUnsanitizedCustomFormat(read_format, future.GetCallback());
  EXPECT_EQ(0u, future.Get().size());

  base::test::TestFuture<std::u16string> clipboard_text_future;
  ui::Clipboard::GetForCurrentThread()->ReadText(
      ui::ClipboardBuffer::kCopyPaste, /*data_dst=*/std::nullopt,
      clipboard_text_future.GetCallback());
  EXPECT_EQ(u"Policy Blocked", clipboard_text_future.Get());
}

TEST_F(ClipboardHostImplAsyncWriteTest,
       ReadUnsanitizedCustomFormat_PolicyBlocked) {
  PolicyBlockBrowserClient browser_client;
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  std::string test_data = "test custom format data";
  const std::u16string format = u"text/custom-format";

  // Write directly to the OS clipboard, so we can set up a paste scenario
  // without triggering the copy mock.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteData(format,
                     mojo_base::BigBuffer(base::as_byte_span(test_data)));
  }

  // Reading the custom format should be blocked by policy mock. Since custom
  // formats cannot be string-replaced, the result should simply be an empty
  // buffer.
  base::test::TestFuture<mojo_base::BigBuffer> future;
  remote()->ReadUnsanitizedCustomFormat(format, future.GetCallback());

  EXPECT_EQ(0u, future.Get().size());
}

class ClipboardHostImplChangeTest : public RenderViewHostTestHarness {
 protected:
  ClipboardHostImplChangeTest()
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

  ~ClipboardHostImplChangeTest() override {
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

 protected:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;

 private:
  // `ClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<ClipboardHostImpl> fake_clipboard_host_impl_;
};

class MockClipboardListener : public blink::mojom::ClipboardListener {
 public:
  MockClipboardListener() = default;
  ~MockClipboardListener() override = default;

  // Implementation of blink::mojom::ClipboardListener
  MOCK_METHOD(void,
              OnClipboardDataChanged,
              (const std::vector<std::u16string>& types,
               const absl::uint128& change_id),
              (override));

  mojo::PendingRemote<blink::mojom::ClipboardListener> GetRemote() {
    mojo::PendingRemote<blink::mojom::ClipboardListener> remote;
    receiver_.Bind(remote.InitWithNewPipeAndPassReceiver());
    return remote;
  }

  void CloseConnection() { receiver_.reset(); }

  // Synchronously drains the listener pipe, so a caller can assert that no
  // OnClipboardDataChanged() message is in flight without spinning the loop.
  void FlushForTesting() { receiver_.FlushForTesting(); }

 private:
  mojo::Receiver<blink::mojom::ClipboardListener> receiver_{this};
};

// Defers the ReadAvailableTypes() callback so the test can change document
// state while the asynchronous read is in flight.
class DeferredReadAvailableTypesClipboard : public ui::TestClipboard {
 public:
  DeferredReadAvailableTypesClipboard() = default;
  ~DeferredReadAvailableTypesClipboard() override = default;

  void ReadAvailableTypes(
      ui::ClipboardBuffer buffer,
      const std::optional<ui::DataTransferEndpoint>& data_dst,
      ReadAvailableTypesCallback callback) const override {
    read_available_types_callback_ = std::move(callback);
  }

  bool HasPendingReadAvailableTypes() const {
    return !read_available_types_callback_.is_null();
  }

  void CompleteReadAvailableTypes(std::vector<std::u16string> types) {
    std::move(read_available_types_callback_).Run(std::move(types));
  }

 private:
  mutable ReadAvailableTypesCallback read_available_types_callback_;
};

TEST_F(ClipboardHostImplChangeTest, AddClipboardListener) {
  // Initially, the clipboard host should not be listening to clipboard changes
  EXPECT_FALSE(clipboard_host_impl()->listening_to_clipboard_);

  // Create the mock listener and bind it
  auto mock_listener = std::make_unique<MockClipboardListener>();

  // Set up the expectation that OnClipboardDataChanged will be called once,
  // and use it to end the wait below.
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_listener, OnClipboardDataChanged)
      .Times(1)
      .WillOnce([&run_loop](const std::vector<std::u16string>&,
                            const absl::uint128&) { run_loop.Quit(); });

  // Add the clipboard listener to the clipboard host
  clipboard_host_impl()->RegisterClipboardListener(mock_listener->GetRemote());

  // Verify that the class is now listening for clipboard changes
  EXPECT_TRUE(clipboard_host_impl()->listening_to_clipboard_);

  // Simulate clipboard data change - this should trigger OnClipboardDataChanged
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Wait for the notification to be delivered.
  run_loop.Run();
}

TEST_F(ClipboardHostImplChangeTest, NoNotificationToInactiveDocument) {
  auto mock_listener = std::make_unique<MockClipboardListener>();
  EXPECT_CALL(*mock_listener, OnClipboardDataChanged).Times(0);

  clipboard_host_impl()->RegisterClipboardListener(mock_listener->GetRemote());
  EXPECT_TRUE(clipboard_host_impl()->listening_to_clipboard_);

  static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
      ->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  ASSERT_FALSE(web_contents()->GetPrimaryMainFrame()->IsActive());

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  remote_.FlushForTesting();
}

// The document can become inactive while the asynchronous ReadAvailableTypes()
// call is in flight, so the state must be re-checked in the callback.
TEST_F(ClipboardHostImplChangeTest,
       NoNotificationWhenDocumentBecomesInactiveDuringRead) {
  ui::Clipboard::DestroyClipboardForCurrentThread();
  auto deferred_clipboard =
      std::make_unique<DeferredReadAvailableTypesClipboard>();
  auto* deferred_clipboard_ptr = deferred_clipboard.get();
  ui::Clipboard::SetClipboardForCurrentThread(std::move(deferred_clipboard));

  auto mock_listener = std::make_unique<MockClipboardListener>();
  EXPECT_CALL(*mock_listener, OnClipboardDataChanged).Times(0);

  clipboard_host_impl()->RegisterClipboardListener(mock_listener->GetRemote());
  remote_.FlushForTesting();

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_TRUE(deferred_clipboard_ptr->HasPendingReadAvailableTypes());

  // The document goes away before the clipboard read completes.
  static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame())
      ->SetLifecycleState(
          RenderFrameHostImpl::LifecycleStateImpl::kInBackForwardCache);
  deferred_clipboard_ptr->CompleteReadAvailableTypes({u"text/plain"});
  // Drain the listener pipe so that an unwanted OnClipboardDataChanged() would
  // actually be delivered, and therefore caught by the Times(0) expectation.
  mock_listener->FlushForTesting();
  remote_.FlushForTesting();
}

// The listener can also disconnect while the asynchronous ReadAvailableTypes()
// call is in flight. StopObservingClipboard() resets `clipboard_listener_`, so
// the callback must not dereference it.
TEST_F(ClipboardHostImplChangeTest,
       NoNotificationWhenListenerDisconnectsDuringRead) {
  ui::Clipboard::DestroyClipboardForCurrentThread();
  auto deferred_clipboard =
      std::make_unique<DeferredReadAvailableTypesClipboard>();
  auto* deferred_clipboard_ptr = deferred_clipboard.get();
  ui::Clipboard::SetClipboardForCurrentThread(std::move(deferred_clipboard));

  auto mock_listener = std::make_unique<MockClipboardListener>();
  EXPECT_CALL(*mock_listener, OnClipboardDataChanged).Times(0);

  clipboard_host_impl()->RegisterClipboardListener(mock_listener->GetRemote());
  remote_.FlushForTesting();

  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();
  ASSERT_TRUE(deferred_clipboard_ptr->HasPendingReadAvailableTypes());

  // The renderer goes away before the clipboard read completes. Round-trip the
  // host pipe so the queued disconnect notification is processed.
  // base::test::RunUntil() is avoided here because this fixture uses MOCK_TIME,
  // where an unmet condition hangs until the test launcher timeout instead of
  // failing.
  mock_listener->CloseConnection();
  remote_.FlushForTesting();
  ASSERT_FALSE(clipboard_host_impl()->clipboard_listener_);

  // An unwanted OnClipboardDataChanged() would dereference the now-unbound
  // `clipboard_listener_` synchronously inside this call.
  deferred_clipboard_ptr->CompleteReadAvailableTypes({u"text/plain"});
  remote_.FlushForTesting();
}

TEST_F(ClipboardHostImplChangeTest, ClipboardListenerDisconnect) {
  // Initially, the clipboard host should not be listening to clipboard changes
  EXPECT_FALSE(clipboard_host_impl()->listening_to_clipboard_);

  // Create the mock listener and bind it
  auto mock_listener = std::make_unique<MockClipboardListener>();

  // Set up the expectation that OnClipboardDataChanged will not be called
  EXPECT_CALL(*mock_listener, OnClipboardDataChanged).Times(0);

  // Add the clipboard listener to the clipboard host
  clipboard_host_impl()->RegisterClipboardListener(mock_listener->GetRemote());

  // Verify that the class is now listening for clipboard changes
  EXPECT_TRUE(clipboard_host_impl()->listening_to_clipboard_);

  // Close the connection from the client side
  mock_listener->CloseConnection();

  // Round-trip the host pipe so the queued disconnect notification is
  // processed, then verify the class stopped listening.
  remote_.FlushForTesting();
  EXPECT_FALSE(clipboard_host_impl()->listening_to_clipboard_);

  // Simulate clipboard data change - this should not trigger
  // OnClipboardDataChanged
  ui::ClipboardMonitor::GetInstance()->NotifyClipboardDataChanged();

  // Drain the host pipe so that any unwanted message would have been
  // dispatched, and therefore caught by the Times(0) expectation.
  remote_.FlushForTesting();
}

TEST_F(ClipboardHostImplTest,
       ReadUnsanitizedCustomFormat_WithoutUserActivation) {
  // Setup: Custom browser client that denies clipboard paste
  ClipboardPasteAllowedBrowserClient browser_client;
  browser_client.set_is_clipboard_paste_allowed(false);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  // Write custom format to clipboard
  std::string test_data = "confidential_custom_data";
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteData(u"web text/custom",
                     mojo_base::BigBuffer(base::as_byte_span(test_data)));
  }

  // Test: Try to read custom format without user activation
  base::test::TestFuture<mojo_base::BigBuffer> future;
  mojo_clipboard()->ReadUnsanitizedCustomFormat(u"web text/custom",
                                                future.GetCallback());

  // Verify: Should return empty buffer due to permission check failure
  EXPECT_EQ(0u, future.Get().size());
}

TEST_F(ClipboardHostImplTest,
       ReadAvailableCustomAndStandardFormats_WithUserActivation) {
  // Setup: Custom browser client that allows clipboard paste
  ClipboardPasteAllowedBrowserClient browser_client;
  browser_client.set_is_clipboard_paste_allowed(true);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  // Write some standard format data that TestClipboard can handle
  mojo_clipboard()->WriteText(u"test text");
  mojo_clipboard()->CommitWrite();
  mojo_clipboard().FlushForTesting();

  // Test: Read available formats with permission allowed
  base::test::TestFuture<const std::vector<std::u16string>&> future;
  mojo_clipboard()->ReadAvailableCustomAndStandardFormats(future.GetCallback());

  // Verify: With permission allowed, the call completes successfully.
  // TestClipboard should return standard formats like "text/plain".
  const auto& formats = future.Get();
  EXPECT_TRUE(std::ranges::contains(formats, u"text/plain"));
}

TEST_F(ClipboardHostImplTest, ReadUnsanitizedCustomFormat_WithUserActivation) {
  // Setup: Custom browser client that allows clipboard paste
  ClipboardPasteAllowedBrowserClient browser_client;
  browser_client.set_is_clipboard_paste_allowed(true);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  // Write custom format data using ScopedClipboardWriter which properly
  // handles web custom format metadata
  std::string test_data = "test_custom_data";
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteData(u"text/custom",
                     mojo_base::BigBuffer(base::as_byte_span(test_data)));
  }

  // Test: Read custom format with permission allowed
  // Note: Need to prepend "web " prefix to match how ExtractCustomPlatformNames
  // works
  base::test::TestFuture<mojo_base::BigBuffer> future;
  mojo_clipboard()->ReadUnsanitizedCustomFormat(u"web text/custom",
                                                future.GetCallback());

  // Verify: With permission allowed, the data should be successfully retrieved
  const auto& result = future.Get();
  EXPECT_GT(result.size(), 0u);

  // Verify the content matches what was written
  std::string retrieved_data(result.begin(), result.end());
  EXPECT_EQ(retrieved_data, test_data);
}

// ContentBrowserClient that lets a paste pass the renderer permission gate but
// blocks it at the data controls / DLP policy layer (full deny).
class PolicyDenyBrowserClient : public TestContentBrowserClient {
 public:
  PolicyDenyBrowserClient() = default;
  ~PolicyDenyBrowserClient() override = default;

  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override {
    return true;
  }

  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ui::ClipboardMetadata& metadata,
      ClipboardPasteData data,
      IsClipboardPasteAllowedCallback callback) override {
    // Block the paste entirely.
    std::move(callback).Run(std::nullopt);
  }
};

// Regression test for crbug.com/495455546: when the data controls / DLP policy
// blocks a file paste, ReadFiles() must not leave the renderer with any file
// read capability. Because granting now happens only for the policy-allowed
// subset (after the policy decision), a full deny grants nothing.
TEST_F(ClipboardHostImplTest, ReadFiles_PolicyDeny_GrantsNoFileAccess) {
  PolicyDenyBrowserClient browser_client;
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  // Seed the clipboard with a file. A real (absolute) path is used so the
  // uri-list round-trips cleanly on every platform; the file need not exist.
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath blocked_file =
      temp_dir.GetPath().AppendASCII("confidential.docx");
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames(
        ui::FileInfosToURIList({ui::FileInfo(blocked_file, base::FilePath())}));
  }

  RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  const ChildProcessId child_id = process->GetID();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  ASSERT_FALSE(policy->CanReadFile(child_id, blocked_file));

  base::test::TestFuture<blink::mojom::ClipboardFilesPtr> future;
  mojo_clipboard()->ReadFiles(ui::ClipboardBuffer::kCopyPaste,
                              future.GetCallback());
  blink::mojom::ClipboardFilesPtr result = future.Take();

  // No files and no isolated filesystem id are returned, and -- crucially --
  // the renderer was granted no read access to the blocked file.
  EXPECT_TRUE(result->files.empty());
  EXPECT_FALSE(result->file_system_id);
  EXPECT_FALSE(policy->CanReadFile(child_id, blocked_file));
}

// Custom TestClipboard that returns configured HTML markup, source URL, and
// fragment start/end offsets from ReadHTML().
class CustomFragmentTestClipboard : public ui::TestClipboard {
 public:
  CustomFragmentTestClipboard(std::u16string markup,
                              GURL src_url,
                              uint32_t fragment_start,
                              uint32_t fragment_end)
      : markup_(std::move(markup)),
        src_url_(std::move(src_url)),
        fragment_start_(fragment_start),
        fragment_end_(fragment_end) {}
  ~CustomFragmentTestClipboard() override = default;

  void ReadHTML(ui::ClipboardBuffer buffer,
                const std::optional<ui::DataTransferEndpoint>& data_dst,
                ReadHtmlCallback callback) const override {
    std::move(callback).Run(markup_, src_url_, fragment_start_, fragment_end_);
  }

 private:
  std::u16string markup_;
  GURL src_url_;
  uint32_t fragment_start_;
  uint32_t fragment_end_;
};

// When policies block an HTML paste, ReadHtml() must clear all data (markup,
// source URL, and fragment start/end) so that no information is leaked to the
// renderer.
TEST_F(ClipboardHostImplTest, ReadHtml_PolicyDeny_ClearsAllData) {
  PolicyDenyBrowserClient browser_client;
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  const GURL kUrl("https://example.com");
  const std::u16string kHtml = u"<html>foo</html>";
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(kHtml, kUrl.spec());
  }

  std::u16string markup;
  GURL url;
  uint32_t start = 123;
  uint32_t end = 456;
  mojo_clipboard()->ReadHtml(ui::ClipboardBuffer::kCopyPaste, &markup, &url,
                             &start, &end);

  EXPECT_TRUE(markup.empty());
  EXPECT_TRUE(url.is_empty());
  EXPECT_EQ(0u, start);
  EXPECT_EQ(0u, end);
}

// This test verifies that non-zero fragment start and end offsets returned by
// the clipboard are cleared to 0 when pasting is blocked by policy.
TEST_F(ClipboardHostImplTest,
       ReadHtml_PolicyDeny_ClearsNonZeroFragmentOffsets) {
  ui::Clipboard::DestroyClipboardForCurrentThread();
  ui::Clipboard::SetClipboardForCurrentThread(
      std::make_unique<CustomFragmentTestClipboard>(
          u"<html>foo</html>", GURL("https://example.com"),
          /*fragment_start=*/10, /*fragment_end=*/20));
  base::ScopedClosureRunner cleanup(
      base::BindLambdaForTesting([this]() { DeleteAndRecreateClipboard(); }));

  // When policy allows, the non-zero fragment offsets and metadata are passed
  // through.
  {
    std::u16string markup;
    GURL url;
    uint32_t start = 0;
    uint32_t end = 0;
    mojo_clipboard()->ReadHtml(ui::ClipboardBuffer::kCopyPaste, &markup, &url,
                               &start, &end);
    EXPECT_EQ(u"<html>foo</html>", markup);
    EXPECT_EQ(GURL("https://example.com"), url);
    EXPECT_EQ(10u, start);
    EXPECT_EQ(20u, end);
  }

  // When policy denies, all data including fragment offsets must be cleared.
  {
    PolicyDenyBrowserClient browser_client;
    ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

    std::u16string markup;
    GURL url;
    uint32_t start = 123;
    uint32_t end = 456;
    mojo_clipboard()->ReadHtml(ui::ClipboardBuffer::kCopyPaste, &markup, &url,
                               &start, &end);
    EXPECT_TRUE(markup.empty());
    EXPECT_TRUE(url.is_empty());
    EXPECT_EQ(0u, start);
    EXPECT_EQ(0u, end);
  }
}

// ContentBrowserClient that lets a paste pass the renderer permission gate but
// allows only a configured subset of files through the data controls / DLP
// policy layer (partial allow).
class FilesPolicyAllowSubsetBrowserClient : public TestContentBrowserClient {
 public:
  explicit FilesPolicyAllowSubsetBrowserClient(
      std::set<base::FilePath> allowed_paths)
      : allowed_paths_(std::move(allowed_paths)) {}
  ~FilesPolicyAllowSubsetBrowserClient() override = default;

  bool IsClipboardPasteAllowed(
      content::RenderFrameHost* render_frame_host) override {
    return true;
  }

  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ui::ClipboardMetadata& metadata,
      ClipboardPasteData data,
      IsClipboardPasteAllowedCallback callback) override {
    // Return a ClipboardPasteData containing only the allowed subset of paths.
    ClipboardPasteData allowed;
    for (const base::FilePath& path : data.file_paths) {
      if (allowed_paths_.contains(path)) {
        allowed.file_paths.push_back(path);
      }
    }
    std::move(callback).Run(std::move(allowed));
  }

 private:
  std::set<base::FilePath> allowed_paths_;
};

// Regression test for crbug.com/495455546: when the data controls / DLP policy
// allows only a subset of the pasted files, ReadFiles() must grant the renderer
// read access to exactly that subset -- the blocked files must never be
// registered with ChildProcessSecurityPolicy or exposed to the renderer.
TEST_F(ClipboardHostImplTest, ReadFiles_PolicyAllowSubset_GrantsOnlyAllowed) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath allowed_file =
      temp_dir.GetPath().AppendASCII("public.docx");
  const base::FilePath blocked_file =
      temp_dir.GetPath().AppendASCII("confidential.docx");

  FilesPolicyAllowSubsetBrowserClient browser_client({allowed_file});
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames(
        ui::FileInfosToURIList({ui::FileInfo(allowed_file, base::FilePath()),
                                ui::FileInfo(blocked_file, base::FilePath())}));
  }

  RenderProcessHost* process =
      web_contents()->GetPrimaryMainFrame()->GetProcess();
  const ChildProcessId child_id = process->GetID();
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  ASSERT_FALSE(policy->CanReadFile(child_id, allowed_file));
  ASSERT_FALSE(policy->CanReadFile(child_id, blocked_file));

  base::test::TestFuture<blink::mojom::ClipboardFilesPtr> future;
  mojo_clipboard()->ReadFiles(ui::ClipboardBuffer::kCopyPaste,
                              future.GetCallback());
  blink::mojom::ClipboardFilesPtr result = future.Take();

  // Exactly the allowed file is returned and granted; the blocked file is
  // neither returned nor readable by the renderer.
  EXPECT_EQ(1u, result->files.size());
  EXPECT_TRUE(result->file_system_id);
  EXPECT_TRUE(policy->CanReadFile(child_id, allowed_file));
  EXPECT_FALSE(policy->CanReadFile(child_id, blocked_file));
}

TEST_F(ClipboardHostImplTest,
       ReadAvailableCustomAndStandardFormats_TextWithoutUserActivation) {
  // Setup: Custom browser client that denies clipboard paste
  ClipboardPasteAllowedBrowserClient browser_client;
  browser_client.set_is_clipboard_paste_allowed(false);
  ScopedContentBrowserClientSetting browser_client_setting(&browser_client);

  // Write standard text format to clipboard
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"test text");
  }

  // Test: Try to read available formats without permission
  base::test::TestFuture<const std::vector<std::u16string>&> future;
  mojo_clipboard()->ReadAvailableCustomAndStandardFormats(future.GetCallback());

  // Verify: Should return empty vector due to permission check failure
  EXPECT_EQ(0u, future.Get().size());
}

class SequenceNumberInterceptBrowserClient : public TestContentBrowserClient {
 public:
  SequenceNumberInterceptBrowserClient() = default;
  ~SequenceNumberInterceptBrowserClient() override = default;

  void IsClipboardPasteAllowedByPolicy(
      const ClipboardEndpoint& source,
      const ClipboardEndpoint& destination,
      const ui::ClipboardMetadata& metadata,
      ClipboardPasteData data,
      IsClipboardPasteAllowedCallback callback) override {
    last_seqno_ = metadata.seqno;
    std::move(callback).Run(std::move(data));
  }

  ui::ClipboardSequenceNumberToken last_seqno() const { return last_seqno_; }

 private:
  ui::ClipboardSequenceNumberToken last_seqno_;
};

class RaceConditionTestClipboard : public ui::TestClipboard {
 public:
  RaceConditionTestClipboard() = default;
  ~RaceConditionTestClipboard() override = default;

  void SetCallbackOnRead(base::RepeatingClosure callback) {
    on_read_callback_ = std::move(callback);
  }

  void ReadText(ui::ClipboardBuffer buffer,
                const std::optional<ui::DataTransferEndpoint>& data_dst,
                ReadTextCallback callback) const override {
    ui::TestClipboard::ReadText(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadTextCallback callback, std::u16string result) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(result));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadHTML(ui::ClipboardBuffer buffer,
                const std::optional<ui::DataTransferEndpoint>& data_dst,
                ReadHtmlCallback callback) const override {
    ui::TestClipboard::ReadHTML(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadHtmlCallback callback, std::u16string markup, GURL src_url,
               uint32_t fragment_start, uint32_t fragment_end) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(markup), std::move(src_url),
                                      fragment_start, fragment_end);
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadSvg(ui::ClipboardBuffer buffer,
               const std::optional<ui::DataTransferEndpoint>& data_dst,
               ReadSvgCallback callback) const override {
    ui::TestClipboard::ReadSvg(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadSvgCallback callback, std::u16string svg) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(svg));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadRTF(ui::ClipboardBuffer buffer,
               const std::optional<ui::DataTransferEndpoint>& data_dst,
               ReadRTFCallback callback) const override {
    ui::TestClipboard::ReadRTF(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadRTFCallback callback, std::string rtf) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(rtf));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadPng(ui::ClipboardBuffer buffer,
               const std::optional<ui::DataTransferEndpoint>& data_dst,
               ReadPngCallback callback) const override {
    ui::TestClipboard::ReadPng(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadPngCallback callback, const std::vector<uint8_t>& data) {
              std::vector<uint8_t> png_copy = data;
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(png_copy));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadFilenames(ui::ClipboardBuffer buffer,
                     const std::optional<ui::DataTransferEndpoint>& data_dst,
                     ReadFilenamesCallback callback) const override {
    ui::TestClipboard::ReadFilenames(
        buffer, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadFilenamesCallback callback,
               std::vector<ui::FileInfo> filenames) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(filenames));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadDataTransferCustomData(
      ui::ClipboardBuffer buffer,
      const std::u16string& type,
      const std::optional<ui::DataTransferEndpoint>& data_dst,
      ReadDataTransferCustomDataCallback callback) const override {
    ui::TestClipboard::ReadDataTransferCustomData(
        buffer, type, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadDataTransferCustomDataCallback callback,
               std::u16string result) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(result));
            },
            on_read_callback_, std::move(callback)));
  }

  void ReadData(const ui::ClipboardFormatType& format,
                const std::optional<ui::DataTransferEndpoint>& data_dst,
                ReadDataCallback callback) const override {
    ui::TestClipboard::ReadData(
        format, data_dst,
        base::BindOnce(
            [](base::RepeatingClosure on_read_callback,
               ReadDataCallback callback, std::string result) {
              if (on_read_callback) {
                on_read_callback.Run();
              }
              std::move(callback).Run(std::move(result));
            },
            on_read_callback_, std::move(callback)));
  }

 private:
  base::RepeatingClosure on_read_callback_;
};

class ClipboardHostImplRaceConditionTest : public ClipboardHostImplTest {
 protected:
  void SetUp() override {
    ClipboardHostImplTest::SetUp();
    browser_client_setting_ =
        std::make_unique<ScopedContentBrowserClientSetting>(&browser_client_);

    ui::Clipboard::DestroyClipboardForCurrentThread();
    auto test_clipboard = std::make_unique<RaceConditionTestClipboard>();
    test_clipboard_ = test_clipboard.get();
    ui::Clipboard::SetClipboardForCurrentThread(std::move(test_clipboard));
  }

  void TearDown() override {
    test_clipboard_ = nullptr;
    DeleteAndRecreateClipboard();
    browser_client_setting_.reset();
    ClipboardHostImplTest::TearDown();
  }

  SequenceNumberInterceptBrowserClient& browser_client() {
    return browser_client_;
  }

  RaceConditionTestClipboard* test_clipboard() { return test_clipboard_; }

 private:
  SequenceNumberInterceptBrowserClient browser_client_;
  std::unique_ptr<ScopedContentBrowserClientSetting> browser_client_setting_;
  raw_ptr<RaceConditionTestClipboard> test_clipboard_ = nullptr;
};

TEST_F(ClipboardHostImplRaceConditionTest, ReadTextUsesCapturedSequenceNumber) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"initial text");
  }

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  std::u16string result;
  mojo_clipboard()->ReadText(ui::ClipboardBuffer::kCopyPaste, &result);

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_EQ(result, u"initial text");
}

TEST_F(ClipboardHostImplRaceConditionTest, ReadHtmlUsesCapturedSequenceNumber) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteHTML(u"<b>html</b>", "https://example.com");
  }

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  std::u16string markup;
  GURL url;
  uint32_t start = 0;
  uint32_t end = 0;
  mojo_clipboard()->ReadHtml(ui::ClipboardBuffer::kCopyPaste, &markup, &url,
                             &start, &end);

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_EQ(markup, u"<b>html</b>");
}

TEST_F(ClipboardHostImplRaceConditionTest, ReadSvgUsesCapturedSequenceNumber) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteSvg(u"<svg></svg>");
  }

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  base::test::TestFuture<const std::u16string&> future;
  mojo_clipboard()->ReadSvg(ui::ClipboardBuffer::kCopyPaste,
                            future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_EQ(future.Get(), u"<svg></svg>");
}

TEST_F(ClipboardHostImplRaceConditionTest, ReadRtfUsesCapturedSequenceNumber) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteRTF("{\\rtf1\\ansi}");
  }

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  std::string result;
  mojo_clipboard()->ReadRtf(ui::ClipboardBuffer::kCopyPaste, &result);

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_EQ(result, "{\\rtf1\\ansi}");
}

TEST_F(ClipboardHostImplRaceConditionTest, ReadPngUsesCapturedSequenceNumber) {
  SkBitmap bitmap = gfx::test::CreateBitmap(3, 2);
  mojo_clipboard()->WriteImage(bitmap);
  mojo_clipboard()->CommitWrite();
  mojo_clipboard().FlushForTesting();

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  base::test::TestFuture<mojo_base::BigBuffer> future;
  mojo_clipboard()->ReadPng(ui::ClipboardBuffer::kCopyPaste,
                            future.GetCallback());
  ASSERT_TRUE(future.Wait());

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  mojo_base::BigBuffer buffer = future.Take();
  EXPECT_FALSE(buffer.byte_span().empty());
  SkBitmap actual = gfx::PNGCodec::Decode(buffer.byte_span());
  ASSERT_TRUE(!actual.isNull());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

TEST_F(ClipboardHostImplRaceConditionTest,
       ReadFilesUsesCapturedSequenceNumber) {
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteFilenames("file:///test/file");
  }

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  blink::mojom::ClipboardFilesPtr result;
  mojo_clipboard()->ReadFiles(ui::ClipboardBuffer::kCopyPaste, &result);

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_TRUE(result);
  EXPECT_EQ(result->files.size(), 1u);
  EXPECT_EQ(result->files[0]->path,
#if BUILDFLAG(IS_WIN)
            base::FilePath(FILE_PATH_LITERAL("\\test\\file")));
#else
            base::FilePath(FILE_PATH_LITERAL("/test/file")));
#endif
}

TEST_F(ClipboardHostImplRaceConditionTest,
       ReadDataTransferCustomDataUsesCapturedSequenceNumber) {
  base::flat_map<std::u16string, std::u16string> custom_data;
  custom_data[u"custom/type"] = u"custom data";
  mojo_clipboard()->WriteDataTransferCustomData(custom_data);
  mojo_clipboard()->CommitWrite();
  mojo_clipboard().FlushForTesting();

  ui::ClipboardSequenceNumberToken expected_seqno =
      test_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);

  test_clipboard()->SetCallbackOnRead(base::BindLambdaForTesting([]() {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(u"Benign text to increment sequence number");
  }));

  std::u16string result;
  mojo_clipboard()->ReadDataTransferCustomData(ui::ClipboardBuffer::kCopyPaste,
                                               u"custom/type", &result);

  EXPECT_EQ(browser_client().last_seqno(), expected_seqno);
  EXPECT_EQ(result, u"custom data");
}

}  // namespace content
