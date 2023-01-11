// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/mock_vendor_tag_ops.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"

namespace media {
namespace unittest_internal {

MockVendorTagOps::MockVendorTagOps()
    : mock_vendor_tag_ops_thread_("MockVendorTagOpsThread") {
  CHECK(mock_vendor_tag_ops_thread_.Start());
}

MockVendorTagOps::~MockVendorTagOps() {
  mock_vendor_tag_ops_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&MockVendorTagOps::CloseBindingOnThread,
                                base::Unretained(this)));
  mock_vendor_tag_ops_thread_.Stop();
}

void MockVendorTagOps::Bind(
    mojo::PendingReceiver<cros::mojom::VendorTagOps> receiver) {
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  mock_vendor_tag_ops_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&MockVendorTagOps::BindOnThread, base::Unretained(this),
                     base::Unretained(&done), std::move(receiver)));
  done.Wait();
}

void MockVendorTagOps::GetTagCount(GetTagCountCallback callback) {
  std::move(callback).Run(DoGetTagCount());
}

void MockVendorTagOps::GetAllTags(GetAllTagsCallback callback) {
  std::move(callback).Run(DoGetAllTags());
}

void MockVendorTagOps::GetSectionName(uint32_t tag,
                                      GetSectionNameCallback callback) {
  std::move(callback).Run(DoGetSectionName(tag));
}

void MockVendorTagOps::GetTagName(uint32_t tag, GetTagNameCallback callback) {
  std::move(callback).Run(DoGetTagName(tag));
}

void MockVendorTagOps::CloseBindingOnThread() {
  receiver_.reset();
}

void MockVendorTagOps::BindOnThread(
    base::WaitableEvent* done,
    mojo::PendingReceiver<cros::mojom::VendorTagOps> receiver) {
  receiver_.Bind(std::move(receiver));
  done->Signal();
}

}  // namespace unittest_internal
}  // namespace media
