// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_TEST_MIME_HANDLER_VIEW_GUEST_H_
#define EXTENSIONS_BROWSER_GUEST_VIEW_MIME_HANDLER_VIEW_TEST_MIME_HANDLER_VIEW_GUEST_H_

#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"

using guest_view::GuestViewBase;

namespace content {
class MessageLoopRunner;
}  // namespace content

namespace guest_view {
class TestGuestViewManager;
}  // namespace guest_view

namespace extensions {

// TestMimeHandlerViewGuest is used instead of its base class,
// MimeHandlerViewGuest, during MimeHandlerView tests. It allows for more
// control over the MimeHandlerViewGuest for the purposes of testing.
class TestMimeHandlerViewGuest : public MimeHandlerViewGuest {
 public:
  ~TestMimeHandlerViewGuest() override;
  TestMimeHandlerViewGuest(const TestMimeHandlerViewGuest&) = delete;
  TestMimeHandlerViewGuest& operator=(const TestMimeHandlerViewGuest&) = delete;

  // Have `manager` create TestMimeHandlerViewGuests in place of
  // MimeHandlerViewGuests.
  static void RegisterTestGuestViewType(
      guest_view::TestGuestViewManager* manager);

  static std::unique_ptr<GuestViewBase> Create(
      content::RenderFrameHost* owner_rfh);

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
  void CreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                         const base::Value::Dict& create_params,
                         WebContentsCreatedCallback callback) override;
  void DidAttachToEmbedder() override;

  // In preparation for the migration of guest view from inner WebContents to
  // MPArch (crbug/1261928), individual tests should avoid accessing the guest's
  // inner WebContents. The direct access is centralized in this helper function
  // for easier migration.
  //
  // TODO(crbug.com/40202416): Update this implementation for MPArch, and
  // consider relocate it to `content/public/test/browser_test_utils.h`.
  static void WaitForGuestLoadStartThenStop(GuestViewBase* guest_view);

 private:
  explicit TestMimeHandlerViewGuest(content::RenderFrameHost* owner_rfh);

  // Used to call MimeHandlerViewGuest::CreateWebContents using a scoped_ptr for
  // |create_params|.
  void CallBaseCreateWebContents(std::unique_ptr<GuestViewBase> owned_this,
                                 base::Value::Dict create_params,
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
