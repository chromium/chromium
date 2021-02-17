// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/bind.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "skia/ext/skia_utils_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/test/clipboard_test_util.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/skia_util.h"

namespace content {

namespace {

// A ClipboardHostImpl that mocks out the dependency on RenderFrameHost.
class ClipboardHostImplNoRFH : public ClipboardHostImpl {
 public:
  ClipboardHostImplNoRFH(
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver)
      : ClipboardHostImpl(/*render_frame_host=*/nullptr),
        receiver_(this, std::move(receiver)) {}

  void StartIsPasteContentAllowedRequest(
      uint64_t seqno,
      const ui::ClipboardFormatType& data_type,
      std::string data) override {}

  void CompleteRequest(uint64_t seqno) {
    FinishPasteIfContentAllowed(
        seqno, ClipboardHostImpl::ClipboardPasteContentAllowed(true));
  }

  using ClipboardHostImpl::CleanupObsoleteRequests;
  using ClipboardHostImpl::is_paste_allowed_requests_for_testing;
  using ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld;
  using ClipboardHostImpl::PerformPasteIfContentAllowed;

  mojo::Receiver<blink::mojom::ClipboardHost> receiver_;
};

}  // namespace

class ClipboardHostImplTest : public ::testing::Test {
 protected:
  ClipboardHostImplTest()
      : clipboard_(ui::TestClipboard::CreateForCurrentThread()) {
    ClipboardHostImpl::Create(/*render_frame_host=*/nullptr,
                              remote_.BindNewPipeAndPassReceiver());
  }

  ~ClipboardHostImplTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  mojo::Remote<blink::mojom::ClipboardHost>& mojo_clipboard() {
    return remote_;
  }

  ui::Clipboard* system_clipboard() { return clipboard_; }

 private:
  const BrowserTaskEnvironment task_environment_;
  ui::Clipboard* const clipboard_;
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
};

// Test that it actually works.
TEST_F(ClipboardHostImplTest, SimpleImage) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(3, 2);
  bitmap.eraseARGB(255, 0, 255, 0);
  mojo_clipboard()->WriteImage(bitmap);
  uint64_t sequence_number =
      system_clipboard()->GetSequenceNumber(ui::ClipboardBuffer::kCopyPaste);
  mojo_clipboard()->CommitWrite();
  base::RunLoop().RunUntilIdle();

  EXPECT_NE(sequence_number, system_clipboard()->GetSequenceNumber(
                                 ui::ClipboardBuffer::kCopyPaste));
  EXPECT_FALSE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::GetPlainTextType(),
      ui::ClipboardBuffer::kCopyPaste, /* data_dst=*/nullptr));
  EXPECT_TRUE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::GetBitmapType(), ui::ClipboardBuffer::kCopyPaste,
      /*data_dst=*/nullptr));

  SkBitmap actual = ui::clipboard_test_util::ReadImage(system_clipboard());
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_AddCallback) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;
  int count = 0;

  // First call to AddCallback should return true, the next false.
  EXPECT_TRUE(request.AddCallback(base::BindLambdaForTesting(
      [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
        ++count;
      })));
  EXPECT_FALSE(request.AddCallback(base::BindLambdaForTesting(
      [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
        ++count;
      })));

  // In both cases, the callbacks should noy be called since the request is
  // not complete.
  EXPECT_EQ(0, count);
}

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_Complete) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;
  int count = 0;

  // Add a callback.  It should not fire right away.
  request.AddCallback(base::BindLambdaForTesting(
      [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
        ++count;
        ASSERT_EQ(ClipboardHostImpl::ClipboardPasteContentAllowed(true),
                  allowed);
      }));
  EXPECT_EQ(0, count);

  // Complete the request.  Callback should fire.  Whether paste is allowed
  // or not is not important.
  request.Complete(ClipboardHostImpl::ClipboardPasteContentAllowed(true));
  EXPECT_EQ(1, count);

  // Adding a new callback after completion invokes it immediately.
  request.AddCallback(base::BindLambdaForTesting(
      [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
        ++count;
        ASSERT_EQ(ClipboardHostImpl::ClipboardPasteContentAllowed(true),
                  allowed);
      }));
  EXPECT_EQ(2, count);
}

TEST_F(ClipboardHostImplTest, IsPasteContentAllowedRequest_IsObsolete) {
  ClipboardHostImpl::IsPasteContentAllowedRequest request;

  // A request that is not too old is not obsolete, even if it has no callbacks.
  EXPECT_FALSE(request.IsObsolete(
      request.time() +
      ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld / 2));

  // A request that still has callbacks is not obsolete, even if older than
  // "too old".
  request.AddCallback(base::DoNothing());
  EXPECT_FALSE(request.IsObsolete(
      request.time() + ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld +
      base::TimeDelta::FromMicroseconds(1)));

  // A request is obsolete once it is too old and has no callbacks.
  // Whether paste is allowed or not is not important.
  request.Complete(ClipboardHostImpl::ClipboardPasteContentAllowed(true));
  EXPECT_TRUE(request.IsObsolete(
      request.time() + ClipboardHostImpl::kIsPasteContentAllowedRequestTooOld +
      base::TimeDelta::FromMicroseconds(1)));
}

class ClipboardHostImplScanTest : public ::testing::Test {
 protected:
  ClipboardHostImplScanTest()
      : clipboard_(ui::TestClipboard::CreateForCurrentThread()),
        fake_clipboard_host_impl_(remote_.BindNewPipeAndPassReceiver()) {}

  ~ClipboardHostImplScanTest() override {
    ui::Clipboard::DestroyClipboardForCurrentThread();
  }

  ClipboardHostImplNoRFH* clipboard_host_impl() {
    return &fake_clipboard_host_impl_;
  }

  BrowserTaskEnvironment* task_environment() { return &task_environment_; }

 private:
  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  ui::Clipboard* const clipboard_;
  ClipboardHostImplNoRFH fake_clipboard_host_impl_;
};

TEST_F(ClipboardHostImplScanTest, PerformPasteIfContentAllowed_EmptyData) {
  int count = 0;

  // When data is empty, the callback is invoked right away.
  clipboard_host_impl()->PerformPasteIfContentAllowed(
      1, ui::ClipboardFormatType::GetPlainTextType(), "",
      base::BindLambdaForTesting(
          [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
            ++count;
          }));

  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, PerformPasteIfContentAllowed) {
  int count = 0;

  clipboard_host_impl()->PerformPasteIfContentAllowed(
      1, ui::ClipboardFormatType::GetPlainTextType(), "data",
      base::BindLambdaForTesting(
          [&count](ClipboardHostImpl::ClipboardPasteContentAllowed allowed) {
            ++count;
          }));

  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(0, count);

  // Completing the request invokes the callback.  The request will
  // remain pending until it is cleaned up.
  clipboard_host_impl()->CompleteRequest(1);
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
  EXPECT_EQ(1, count);
}

TEST_F(ClipboardHostImplScanTest, CleanupObsoleteScanRequests) {
  // Perform a request and complete it.
  clipboard_host_impl()->PerformPasteIfContentAllowed(
      1, ui::ClipboardFormatType::GetPlainTextType(), "data",
      base::DoNothing());
  clipboard_host_impl()->CompleteRequest(1);
  EXPECT_EQ(
      1u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());

  // Make sure an appropriate amount of time passes to make the request old.
  // It should be cleaned up.
  task_environment()->FastForwardBy(
      ClipboardHostImplNoRFH::kIsPasteContentAllowedRequestTooOld +
      base::TimeDelta::FromMicroseconds(1));
  clipboard_host_impl()->CleanupObsoleteRequests();
  EXPECT_EQ(
      0u,
      clipboard_host_impl()->is_paste_allowed_requests_for_testing().size());
}

}  // namespace content
