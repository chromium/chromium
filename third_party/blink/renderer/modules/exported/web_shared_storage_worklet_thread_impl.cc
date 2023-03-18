// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_shared_storage_worklet_thread_impl.h"

#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
void WebSharedStorageWorkletThread::Start(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver) {
  MakeGarbageCollected<WebSharedStorageWorkletThreadImpl>(main_thread_runner,
                                                          std::move(receiver));
}

WebSharedStorageWorkletThreadImpl::WebSharedStorageWorkletThreadImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    mojo::PendingReceiver<mojom::SharedStorageWorkletService> receiver)
    : main_thread_runner_(std::move(main_thread_runner)) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  messaging_proxy_ = MakeGarbageCollected<SharedStorageWorkletMessagingProxy>(
      main_thread_runner_, std::move(receiver),
      /*worklet_terminated_callback=*/
      WTF::BindOnce(&WebSharedStorageWorkletThreadImpl::DeleteSelf,
                    WrapPersistent(this)));
}

WebSharedStorageWorkletThreadImpl::~WebSharedStorageWorkletThreadImpl() =
    default;

void WebSharedStorageWorkletThreadImpl::Trace(Visitor* visitor) const {
  visitor->Trace(messaging_proxy_);
}

void WebSharedStorageWorkletThreadImpl::DeleteSelf() {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  keep_alive_.Clear();
}

}  // namespace blink
