// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/launch/file_handling_expiry_impl.h"

#include <memory>

#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom-blink.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/origin_trials/origin_trial_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/modules/launch/dom_window_launch_queue.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"

namespace blink {

void FileHandlingExpiryImpl::Create(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::FileHandlingExpiry>
        receiver) {
  DCHECK(frame);

  mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FileHandlingExpiryImpl>(*frame->DomWindow()),
      std::move(receiver));
}

FileHandlingExpiryImpl::FileHandlingExpiryImpl(LocalDOMWindow& window)
    : window_(window) {}

FileHandlingExpiryImpl::~FileHandlingExpiryImpl() = default;

void FileHandlingExpiryImpl::RequestOriginTrialExpiryTime(
    RequestOriginTrialExpiryTimeCallback callback) {
  if (!window_)
    return;

  auto* origin_trials = window_->GetExecutionContext()->GetOriginTrialContext();

  base::Time expiry_time =
      origin_trials->GetFeatureExpiry(OriginTrialFeature::kFileHandling);
  std::move(callback).Run(expiry_time);
}

}  // namespace blink
