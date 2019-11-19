// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/clipboard_host_impl.h"

#include <stddef.h>
#include <stdint.h>

#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/bind_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/test/test_clipboard.h"
#include "ui/gfx/skia_util.h"

namespace content {

class ClipboardHostImplTest : public ::testing::Test {
 protected:
  ClipboardHostImplTest()
      : clipboard_(ui::TestClipboard::CreateForCurrentThread()) {
    ClipboardHostImpl::Create(remote_.BindNewPipeAndPassReceiver());
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
  mojo::Remote<blink::mojom::ClipboardHost> remote_;
  ui::Clipboard* const clipboard_;
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
      ui::ClipboardBuffer::kCopyPaste));
  EXPECT_TRUE(system_clipboard()->IsFormatAvailable(
      ui::ClipboardFormatType::GetBitmapType(),
      ui::ClipboardBuffer::kCopyPaste));

  SkBitmap actual =
      system_clipboard()->ReadImage(ui::ClipboardBuffer::kCopyPaste);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap, actual));
}

TEST_F(ClipboardHostImplTest, ReentrancyInSyncCall) {
  // Due to the nature of this test, it's somewhat racy. On some platforms
  // (currently Linux), reading the clipboard requires running a nested message
  // loop. During that time, it's possible to send a bad message that causes the
  // message pipe to be closed. Make sure ClipboardHostImpl doesn't UaF |this|
  // after exiting the nested message loop.

  // ReadText() is a sync method, so normally, one wouldn't call this method
  // directly. These are not normal times though...
  mojo_clipboard()->ReadText(ui::ClipboardBuffer::kCopyPaste,
                             base::DoNothing());

  // Now purposely write a raw message which (hopefully) won't deserialize to
  // anything valid. The receiver side should still be in the midst of
  // dispatching ReadText() when Mojo attempts to deserialize this message,
  // which should cause a validation failure that signals a connection error.
  base::RunLoop run_loop;
  mojo::WriteMessageRaw(mojo_clipboard().internal_state()->handle(), "moo", 3,
                        nullptr, 0, MOJO_WRITE_MESSAGE_FLAG_NONE);
  mojo_clipboard().set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(mojo_clipboard().is_connected());
}

}  // namespace content
