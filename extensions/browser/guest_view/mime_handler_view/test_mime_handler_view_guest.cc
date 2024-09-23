// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using guest_view::GuestViewBase;

namespace extensions {

TestMimeHandlerViewGuest::TestMimeHandlerViewGuest(
    content::RenderFrameHost* owner_rfh)
    : MimeHandlerViewGuest(owner_rfh) {}

TestMimeHandlerViewGuest::~TestMimeHandlerViewGuest() = default;

// static
void TestMimeHandlerViewGuest::RegisterTestGuestViewType(
    guest_view::TestGuestViewManager* manager) {
  manager->RegisterGuestViewType(
      TestMimeHandlerViewGuest::Type,
      base::BindRepeating(&TestMimeHandlerViewGuest::Create),
      base::NullCallback());
}

// static
std::unique_ptr<GuestViewBase> TestMimeHandlerViewGuest::Create(
    content::RenderFrameHost* owner_rfh) {
  return base::WrapUnique(new TestMimeHandlerViewGuest(owner_rfh));
}

// static
void TestMimeHandlerViewGuest::DelayNextCreateWebContents(int delay) {
  TestMimeHandlerViewGuest::delay_ = delay;
}

void TestMimeHandlerViewGuest::WaitForGuestAttached() {
  if (attached()) {
    return;
  }
  created_message_loop_runner_ = new content::MessageLoopRunner;
  created_message_loop_runner_->Run();
}

void TestMimeHandlerViewGuest::CreateWebContents(
    std::unique_ptr<GuestViewBase> owned_this,
    const base::Value::Dict& create_params,
    WebContentsCreatedCallback callback) {
  // Delay the creation of the guest's WebContents if |delay_| is set.
  if (delay_) {
    auto delta = base::Milliseconds(delay_);
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TestMimeHandlerViewGuest::CallBaseCreateWebContents,
                       weak_ptr_factory_.GetWeakPtr(), std::move(owned_this),
                       create_params.Clone(), std::move(callback)),
        delta);

    // Reset the delay for the next creation.
    delay_ = 0;
    return;
  }

  MimeHandlerViewGuest::CreateWebContents(std::move(owned_this), create_params,
                                          std::move(callback));
}

void TestMimeHandlerViewGuest::DidAttachToEmbedder() {
  MimeHandlerViewGuest::DidAttachToEmbedder();
  if (created_message_loop_runner_.get())
    created_message_loop_runner_->Quit();
}

void TestMimeHandlerViewGuest::WaitForGuestLoadStartThenStop(
    GuestViewBase* guest_view) {
  auto* guest_contents = guest_view->web_contents();
  // Wait for loading to start.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return guest_contents->IsLoading() ||
           guest_view->GetController().GetLastCommittedEntry();
  }));
  ASSERT_TRUE(content::WaitForLoadStop(guest_contents));
}

void TestMimeHandlerViewGuest::CallBaseCreateWebContents(
    std::unique_ptr<GuestViewBase> owned_this,
    base::Value::Dict create_params,
    WebContentsCreatedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MimeHandlerViewGuest::CreateWebContents(std::move(owned_this), create_params,
                                          std::move(callback));
}

// static
int TestMimeHandlerViewGuest::delay_ = 0;

}  // namespace extensions
