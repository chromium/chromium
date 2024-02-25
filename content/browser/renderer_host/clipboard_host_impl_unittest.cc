// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "content/browser/renderer_host/clipboard_host_impl.h"
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

namespace {

// A ClipboardHostImpl that mocks out the dependency on RenderFrameHost.
class FakeClipboardHostImpl : public ClipboardHostImpl {
 public:
  FakeClipboardHostImpl(
      RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
      : ClipboardHostImpl(render_frame_host, std::move(receiver)) {}

  void StartIsPasteAllowedRequest(
      const ui::ClipboardSequenceNumberToken& seqno,
      const ui::ClipboardFormatType& data_type,
      ui::ClipboardBuffer clipboard_buffer,
      ClipboardPasteData clipboard_paste_data) override {
    ++start_count_;
    data_type_ = data_type;
    clipboard_buffer_ = clipboard_buffer;
  }

  void CompleteRequest(const ui::ClipboardSequenceNumberToken& seqno,
                       ClipboardPasteData clipboard_paste_data) {
    ClipboardHostImpl::StartIsPasteAllowedRequest(
        seqno, data_type_, clipboard_buffer_, std::move(clipboard_paste_data));
  }

  size_t start_count() const { return start_count_; }

  using ClipboardHostImpl::CleanupObsoleteRequests;
  using ClipboardHostImpl::ClipboardPasteData;
  using ClipboardHostImpl::is_paste_allowed_requests_for_testing;
  using ClipboardHostImpl::kIsPasteAllowedRequestTooOld;
  using ClipboardHostImpl::PasteIfPolicyAllowed;

 private:
  // number of times StartIsPasteAllowedRequest() is called.
  size_t start_count_ = 0;

  ui::ClipboardFormatType data_type_;
  ui::ClipboardBuffer clipboard_buffer_ = ui::ClipboardBuffer::kCopyPaste;
};

}  // namespace

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

TEST_F(ClipboardHostImplTest, IsPasteAllowedRequest_AddCallback) {
  ClipboardHostImpl::IsPasteAllowedRequest request;
  int count = 0;

  // First call to AddCallback should return true, the next false.
  EXPECT_TRUE(request.AddCallback(base::BindLambdaForTesting(
      [&count](std::optional<ClipboardHostImpl::ClipboardPasteData>
                   clipboard_paste_data) { ++count; })));
  EXPECT_FALSE(request.AddCallback(base::BindLambdaForTesting(
      [&count](std::optional<ClipboardHostImpl::ClipboardPasteData>
                   clipboard_paste_data) { ++count; })));

  // In both cases, the callbacks should not be called since the request is
  // not complete.
  EXPECT_EQ(0, count);
}

TEST_F(ClipboardHostImplTest, IsPasteAllowedRequest_Complete) {
  ClipboardHostImpl::IsPasteAllowedRequest request;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data_1;
  clipboard_paste_data_1.text = u"text";
  clipboard_paste_data_1.png = {1, 2, 3, 4, 5};
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data_2;
  clipboard_paste_data_2.text = u"other text";
  clipboard_paste_data_2.png = {6, 7, 8, 9, 10};

  int count = 0;

  // Add a callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count, clipboard_paste_data_1](
          std::optional<ClipboardHostImpl::ClipboardPasteData>
              clipboard_paste_data) {
        ++count;
        ASSERT_EQ(clipboard_paste_data->text, clipboard_paste_data_1.text);
        ASSERT_EQ(clipboard_paste_data->png, clipboard_paste_data_1.png);
      }));
  EXPECT_EQ(0, count);

  // Complete the request.  Callback should fire.  Whether paste is allowed
  // or not is not important.
  request.Complete(clipboard_paste_data_1);
  EXPECT_EQ(1, count);

  // Add a second callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count, clipboard_paste_data_2](
          std::optional<ClipboardHostImpl::ClipboardPasteData>
              clipboard_paste_data) {
        ++count;
        ASSERT_EQ(clipboard_paste_data->text, clipboard_paste_data_2.text);
        ASSERT_EQ(clipboard_paste_data->png, clipboard_paste_data_2.png);
      }));
  EXPECT_EQ(1, count);

  // Calling `Complete()` again will call the second callback.
  request.Complete(clipboard_paste_data_2);
  EXPECT_EQ(2, count);
}

TEST_F(ClipboardHostImplTest, IsPasteAllowedRequest_IsObsolete) {
  ClipboardHostImpl::IsPasteAllowedRequest request;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"data";

  // A request is obsolete once it is too old and completed.
  // Whether paste is allowed or not is not important.
  request.Complete(clipboard_paste_data);
  EXPECT_TRUE(request.IsObsolete(
      request.completed_time() +
      ClipboardHostImpl::kIsPasteAllowedRequestTooOld + base::Microseconds(1)));
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

class ClipboardHostImplScanTest : public RenderViewHostTestHarness {
 protected:
  ClipboardHostImplScanTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        clipboard_(ui::TestClipboard::CreateForCurrentThread()) {}

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetContents(CreateTestWebContents());
    fake_clipboard_host_impl_ =
        new FakeClipboardHostImpl(*web_contents()->GetPrimaryMainFrame(),
                                  remote_.BindNewPipeAndPassReceiver());
  }

  ~ClipboardHostImplScanTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  FakeClipboardHostImpl* clipboard_host_impl() {
    return fake_clipboard_host_impl_;
  }

  mojo::Remote<blink::mojom::ClipboardHost>& mojo_clipboard() {
    return remote_;
  }

  ui::Clipboard* system_clipboard() { return clipboard_; }

  RenderFrameHost& rfh() { return clipboard_host_impl()->render_frame_host(); }

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  const raw_ptr<ui::Clipboard, DanglingUntriaged> clipboard_;
  // `FakeClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<FakeClipboardHostImpl, DanglingUntriaged> fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplScanTest, PerformPasteIfAllowed_SameHost_NotStarted) {
  const std::u16string kText = u"text";
  clipboard_host_impl()->WriteText(kText);
  clipboard_host_impl()->CommitWrite();

  std::u16string read_text;
  clipboard_host_impl()->ReadText(
      ui::ClipboardBuffer::kCopyPaste,
      base::BindLambdaForTesting(
          [&read_text](const std::u16string& value) { read_text = value; }));

  // When the same document writes and then reads from the clipboard, content
  // checks should be skipped.
  EXPECT_EQ(0u, clipboard_host_impl()->start_count());
  EXPECT_EQ(kText, read_text);
}

TEST_F(ClipboardHostImplScanTest, PerformPasteIfAllowed_External_Started) {
  const std::u16string kText = u"text";

  // Write directly to clipboard.
  {
    ui::ScopedClipboardWriter writer(ui::ClipboardBuffer::kCopyPaste);
    writer.WriteText(kText);
  }

  std::u16string read_text;
  clipboard_host_impl()->ReadText(
      ui::ClipboardBuffer::kCopyPaste,
      base::BindLambdaForTesting(
          [&read_text](const std::u16string& value) { read_text = value; }));

  // Completing the request invokes the callback.  The request will
  // remain pending until it is cleaned up.
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = kText;

  clipboard_host_impl()->CompleteRequest(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
      clipboard_paste_data);
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());

  // When a document reads from the clipboard, but the clipboard was written
  // from an unknown source, content checks should not be skipped.
  EXPECT_EQ(1u, clipboard_host_impl()->start_count());
  EXPECT_EQ(kText, read_text);
}

TEST_F(ClipboardHostImplScanTest, PasteIfPolicyAllowed_EmptyData) {
  int count = 0;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;

  // When data is empty, the callback is invoked right away.
  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [&count](std::optional<ClipboardHostImpl::ClipboardPasteData>
                       clipboard_paste_data) { ++count; }));

  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, PasteIfPolicyAllowed) {
  int count = 0;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"data";

  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data,
      base::BindLambdaForTesting(
          [&count](std::optional<ClipboardHostImpl::ClipboardPasteData>
                       clipboard_paste_data) {
            ++count;
            ASSERT_TRUE(clipboard_paste_data);
            ASSERT_EQ(clipboard_paste_data->text, u"data");
          }));

  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(0, count);

  // Completing the request invokes the callback.  The request will
  // remain pending until it is cleaned up.
  clipboard_host_impl()->CompleteRequest(
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste),
      clipboard_paste_data);

  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, CleanupObsoleteScanRequests) {
  ui::ClipboardSequenceNumberToken sequence_number;
  ClipboardHostImpl::ClipboardPasteData clipboard_paste_data;
  clipboard_paste_data.text = u"data";

  // Perform a request and complete it.
  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      clipboard_paste_data, base::DoNothing());
  clipboard_host_impl()->CompleteRequest(sequence_number, clipboard_paste_data);
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());

  // Make sure an appropriate amount of time passes to make the request old.
  // It should be cleaned up.
  task_environment()->FastForwardBy(
      FakeClipboardHostImpl::kIsPasteAllowedRequestTooOld +
      base::Microseconds(1));
  clipboard_host_impl()->CleanupObsoleteRequests();
  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
}

TEST_F(ClipboardHostImplScanTest, MainFrameURL) {
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
  // `FakeClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<FakeClipboardHostImpl> fake_clipboard_host_impl_grandchild =
      new FakeClipboardHostImpl(*grandchild_rfh,
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

  // A new request is created.
  EXPECT_EQ(1u, fake_clipboard_host_impl_grandchild
                    ->is_paste_allowed_requests_for_testing()
                    .size());
  // Count didn't change.
  EXPECT_FALSE(is_policy_callback_called);

  fake_clipboard_host_impl_grandchild->CompleteRequest(
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste),
      clipboard_paste_data);

  EXPECT_TRUE(is_policy_callback_called);
}

TEST_F(ClipboardHostImplScanTest, GetSourceEndpoint) {
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

}  // namespace content
