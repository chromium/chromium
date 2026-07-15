// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_shared_storage_worklet_thread_impl.h"

#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

// static
void WebSharedStorageWorkletThread::Start(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    CrossVariantMojoReceiver<
        mojom::blink::SharedStorageWorkletServiceInterfaceBase> receiver,
    mojom::WorkletGlobalScopeCreationParamsPtr global_scope_creation_params) {
  MakeGarbageCollected<WebSharedStorageWorkletThreadImpl>(
      main_thread_runner, std::move(receiver), nullptr);
}

WebSharedStorageWorkletThreadImpl::WebSharedStorageWorkletThreadImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    mojom::blink::WorkletGlobalScopeCreationParamsPtr
        global_scope_creation_params)
    : main_thread_runner_(std::move(main_thread_runner)) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());
}

WebSharedStorageWorkletThreadImpl::~WebSharedStorageWorkletThreadImpl() =
    default;

void WebSharedStorageWorkletThreadImpl::Trace(Visitor* visitor) const {}

}  // namespace blink
