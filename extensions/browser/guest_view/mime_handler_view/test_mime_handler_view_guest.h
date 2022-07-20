// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_TEST_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_TEST_MIME_HANDLER_VIEW_GUEST_H_

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

using guest_view::GuestViewBase;

namespace content {
class MessageLoopRunner;
}  // namespace content

namespace extensions {

// TestMimeHandlerViewGuest is used instead of its base class,
// MimeHandlerViewGuest, during MimeHandlerView tests. It allows for more
// control over the MimeHandlerViewGuest for the purposes of testing.
class TestMimeHandlerViewGuest : public MimeHandlerViewGuest {
 public:
  TestMimeHandlerViewGuest(const TestMimeHandlerViewGuest&) = delete;
  TestMimeHandlerViewGuest& operator=(const TestMimeHandlerViewGuest&) = delete;

  static GuestViewBase* Create(content::WebContents* owner_web_contents);

  // Set a delay in the next creation of a guest's WebContents by |delay|
  // milliseconds.
  // TODO(mcnee): The use of a timed delay makes for tests with fragile timing
  // dependencies. This should be implemented in a way that allows the test to
  // control when to resume creation based on a condition (e.g. QuitClosure,
  // OneShotEvent).
  static void DelayNextCreateWebContents(int delay);

  // Wait until the guest has attached to the embedder.
  void WaitForGuestAttached();

  // MimeHandlerViewGuest override:
  void CreateWebContents(const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) override;
  void DidAttachToEmbedder() override;

 private:
  explicit TestMimeHandlerViewGuest(content::WebContents* owner_web_contents);
  ~TestMimeHandlerViewGuest() override;

  // Used to call MimeHandlerViewGuest::CreateWebContents using a scoped_ptr for
  // |create_params|.
  void CallBaseCreateWebContents(base::Value::Dict create_params,
                                 WebContentsCreatedCallback callback);

  // A value in milliseconds that the next creation of a guest's WebContents
  // will be delayed. After this creation is delayed, |delay_| will be reset to
  // 0.
  static int delay_;

  scoped_refptr<content::MessageLoopRunner> created_message_loop_runner_;

  // This is used to ensure pending tasks will not fire after this object is
  // destroyed.
  base::WeakPtrFactory<TestMimeHandlerViewGuest> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_TEST_MIME_HANDLER_VIEW_GUEST_H_
