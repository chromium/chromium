// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/guest_view/mime_handler_view/test_mime_handler_view_guest.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"

using guest_view::GuestViewBase;

namespace extensions {

TestMimeHandlerViewGuest::TestMimeHandlerViewGuest(
    content::WebContents* owner_web_contents)
    : MimeHandlerViewGuest(owner_web_contents) {}

TestMimeHandlerViewGuest::~TestMimeHandlerViewGuest() {}

// static
GuestViewBase* TestMimeHandlerViewGuest::Create(
    content::WebContents* owner_web_contents) {
  return new TestMimeHandlerViewGuest(owner_web_contents);
}

// static
void TestMimeHandlerViewGuest::DelayNextCreateWebContents(int delay) {
  TestMimeHandlerViewGuest::delay_ = delay;
}

void TestMimeHandlerViewGuest::WaitForGuestAttached() {
  if (attached())
    return;
  created_message_loop_runner_ = new content::MessageLoopRunner;
  created_message_loop_runner_->Run();
}

void TestMimeHandlerViewGuest::CreateWebContents(
    const base::DictionaryValue& create_params,
    WebContentsCreatedCallback callback) {
  // Delay the creation of the guest's WebContents if |delay_| is set.
  if (delay_) {
    auto delta = base::TimeDelta::FromMilliseconds(delay_);
    base::PostDelayedTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&TestMimeHandlerViewGuest::CallBaseCreateWebContents,
                       weak_ptr_factory_.GetWeakPtr(),
                       create_params.CreateDeepCopy(), std::move(callback)),
        delta);

    // Reset the delay for the next creation.
    delay_ = 0;
    return;
  }

  MimeHandlerViewGuest::CreateWebContents(create_params, std::move(callback));
}

void TestMimeHandlerViewGuest::DidAttachToEmbedder() {
  MimeHandlerViewGuest::DidAttachToEmbedder();
  if (created_message_loop_runner_.get())
    created_message_loop_runner_->Quit();
}

void TestMimeHandlerViewGuest::CallBaseCreateWebContents(
    std::unique_ptr<base::DictionaryValue> create_params,
    WebContentsCreatedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MimeHandlerViewGuest::CreateWebContents(*create_params, std::move(callback));
}

// static
int TestMimeHandlerViewGuest::delay_ = 0;

}  // namespace extensions
