// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/contains.h"
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

  void StartIsPasteContentAllowedRequest(
      const ui::ClipboardSequenceNumberToken& seqno,
      const ui::ClipboardFormatType& data_type,
      std::string data) override {
    ++start_count_;
  }

  void CompleteRequest(const ui::ClipboardSequenceNumberToken& seqno,
                       const std::string& data) {
    FinishPasteIfContentAllowed(seqno, data);
  }

  size_t start_count() const { return start_count_; }

  using ClipboardHostImpl::CleanupObsoleteRequests;
  using ClipboardHostImpl::is_paste_allowed_requests_for_testing;
  using ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld;
  using ClipboardHostImpl::PasteIfPolicyAllowed;
  using ClipboardHostImpl::PerformPasteIfContentAllowed;

 private:
  // number of times StartIsPasteContentAllowedRequest() is called.
  size_t start_count_ = 0;
};

class PolicyControllerTest : public ui::DataTransferPolicyController {
 public:
  PolicyControllerTest() = default;
  ~PolicyControllerTest() override = default;

  MOCK_METHOD3(IsClipboardReadAllowed,
               bool(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst,
                    const absl::optional<size_t> size));

  MOCK_METHOD5(PasteIfAllowed,
               void(const ui::DataTransferEndpoint* const data_src,
                    const ui::DataTransferEndpoint* const data_dst,
                    const absl::optional<size_t> size,
                    content::RenderFrameHost* rfh,
                    base::OnceCallback<void(bool)> callback));

  MOCK_METHOD3(DropIfAllowed,
               void(const ui::OSExchangeData* drag_data,
                    const ui::DataTransferEndpoint* data_dst,
                    base::OnceClosure drop_cb));
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
  raw_ptr<ui::Clipboard> clipboard_;
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

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_AddCallback) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;
  int count = 0;

  // First call to AddCallback should return true, the next false.
  EXPECT_TRUE(request.AddCallback(base::BindLambdaForTesting(
      [&count](const absl::optional<std::string>& data) { ++count; })));
  EXPECT_FALSE(request.AddCallback(base::BindLambdaForTesting(
      [&count](const absl::optional<std::string>& data) { ++count; })));

  // In both cases, the callbacks should not be called since the request is
  // not complete.
  EXPECT_EQ(0, count);
}

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_Complete) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;
  int count = 0;

  // Add a callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count](const absl::optional<std::string>& data) {
        ++count;
        ASSERT_EQ(data.value(), "data");
      }));
  EXPECT_EQ(0, count);

  // Complete the request.  Callback should fire.  Whether paste is allowed
  // or not is not important.
  request.Complete("data");
  EXPECT_EQ(1, count);

  // Adding a new callback after completion invokes it immediately.
  request.AddCallback(base::BindLambdaForTesting(
      [&count](const absl::optional<std::string>& data) {
        ++count;
        ASSERT_EQ(data.value(), "data");
      }));
  EXPECT_EQ(2, count);
}

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_IsObsolete) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;

  // A request is obsolete once it is too old and completed.
  // Whether paste is allowed or not is not important.
  request.Complete("data");
  EXPECT_TRUE(request.IsObsolete(
      request.completed_time() +
      ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld +
      base::Microseconds(1)));
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

 private:
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  const raw_ptr<ui::Clipboard> clipboard_;
  // `FakeClipboardHostImpl` is a `DocumentService` and manages its own
  // lifetime.
  raw_ptr<FakeClipboardHostImpl> fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplScanTest,
       PerformPasteIfContentAllowed_SameHost_NotStarted) {
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

TEST_F(ClipboardHostImplScanTest,
       PerformPasteIfContentAllowed_External_Started) {
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
  clipboard_host_impl()->CompleteRequest(
      ui::Clipboard::GetForCurrentThread()->GetSequenceNumber(
          ui::ClipboardBuffer::kCopyPaste),
      base::UTF16ToUTF8(kText));
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

  // When data is empty, the callback is invoked right away.
  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      "",
      base::BindLambdaForTesting(
          [&count](const absl::optional<std::string>& data) { ++count; }));

  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, PerformPasteIfContentAllowed) {
  int count = 0;
  ui::ClipboardSequenceNumberToken sequence_number;
  clipboard_host_impl()->PerformPasteIfContentAllowed(
      sequence_number, ui::ClipboardFormatType::PlainTextType(), "data",
      base::BindLambdaForTesting(
          [&count](const absl::optional<std::string>& data) { ++count; }));

  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(0, count);

  // Completing the request invokes the callback.  The request will
  // remain pending until it is cleaned up.
  clipboard_host_impl()->CompleteRequest(sequence_number, "data");
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, CleanupObsoleteScanRequests) {
  ui::ClipboardSequenceNumberToken sequence_number;
  // Perform a request and complete it.
  clipboard_host_impl()->PerformPasteIfContentAllowed(
      sequence_number, ui::ClipboardFormatType::PlainTextType(), "data",
      base::DoNothing());
  clipboard_host_impl()->CompleteRequest(sequence_number, "data");
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());

  // Make sure an appropriate amount of time passes to make the request old.
  // It should be cleaned up.
  task_environment()->FastForwardBy(
      FakeClipboardHostImpl::kIsPasteContentAllowedRequestTooOld +
      base::Microseconds(1));
  clipboard_host_impl()->CleanupObsoleteRequests();
  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
}

TEST_F(ClipboardHostImplScanTest, IsPastePolicyAllowed_NoController) {
  bool is_policy_callback_called = false;

  // Policy controller doesn't exist.
  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      "data",
      base::BindLambdaForTesting([&is_policy_callback_called](
                                     const absl::optional<std::string>& data) {
        is_policy_callback_called = true;
      }));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_FALSE(is_policy_callback_called);

  clipboard_host_impl()->CompleteRequest(
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste),
      "data");

  EXPECT_TRUE(is_policy_callback_called);
}

TEST_F(ClipboardHostImplScanTest, IsPastePolicyAllowed_NotAllowed) {
  bool is_policy_callback_called = false;

  // Policy controller cancels the paste request.
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(testing::Invoke(
          [](const ui::DataTransferEndpoint* const data_src,
             const ui::DataTransferEndpoint* const data_dst,
             const absl::optional<size_t> size, content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(false);
          }));

  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      "data",
      base::BindLambdaForTesting([&is_policy_callback_called](
                                     const absl::optional<std::string>& data) {
        is_policy_callback_called = true;
      }));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&policy_controller);

  // No new request is created.
  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_TRUE(is_policy_callback_called);
}

TEST_F(ClipboardHostImplScanTest, IsPastePolicyAllowed_Allowed) {
  bool is_policy_callback_called = false;

  // Policy controller accepts the paste request.
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(testing::Invoke(
          [](const ui::DataTransferEndpoint* const data_src,
             const ui::DataTransferEndpoint* const data_dst,
             const absl::optional<size_t> size, content::RenderFrameHost* rfh,
             base::OnceCallback<void(bool)> callback) {
            std::move(callback).Run(true);
          }));

  clipboard_host_impl()->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      "data",
      base::BindLambdaForTesting([&is_policy_callback_called](
                                     const absl::optional<std::string>& data) {
        is_policy_callback_called = true;
      }));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&policy_controller);

  // A new request is created.
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  // Count didn't change.
  EXPECT_FALSE(is_policy_callback_called);

  clipboard_host_impl()->CompleteRequest(
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste),
      "data");

  EXPECT_TRUE(is_policy_callback_called);
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

  // Policy controller accepts the paste request.
  PolicyControllerTest policy_controller;
  EXPECT_CALL(policy_controller, PasteIfAllowed)
      .WillOnce(testing::Invoke(
          [&gurl1](const ui::DataTransferEndpoint* const data_src,
                   const ui::DataTransferEndpoint* const data_dst,
                   const absl::optional<size_t> size,
                   content::RenderFrameHost* rfh,
                   base::OnceCallback<void(bool)> callback) {
            ASSERT_TRUE(data_dst);
            EXPECT_EQ(*data_dst->GetURL(), gurl1);
            std::move(callback).Run(true);
          }));

  bool is_policy_callback_called = false;
  fake_clipboard_host_impl_grandchild->PasteIfPolicyAllowed(
      ui::ClipboardBuffer::kCopyPaste, ui::ClipboardFormatType::PlainTextType(),
      "data",
      base::BindLambdaForTesting([&is_policy_callback_called](
                                     const absl::optional<std::string>& data) {
        is_policy_callback_called = true;
      }));
  base::RunLoop().RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(&policy_controller);

  // A new request is created.
  EXPECT_EQ(1u, fake_clipboard_host_impl_grandchild
                    ->is_paste_allowed_requests_for_testing()
                    .size());
  // Count didn't change.
  EXPECT_FALSE(is_policy_callback_called);

  fake_clipboard_host_impl_grandchild->CompleteRequest(
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste),
      "data");

  EXPECT_TRUE(is_policy_callback_called);
}
}  // namespace content
