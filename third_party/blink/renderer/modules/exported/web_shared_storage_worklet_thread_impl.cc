// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_shared_storage_worklet_thread_impl.h"

#include "third_party/blink/public/mojom/browser_interface_broker.mojom.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom.h"
#include "third_party/blink/public/mojom/origin_trial_feature/origin_trial_feature.mojom-shared.h"
#include "third_party/blink/public/mojom/shared_storage/shared_storage_worklet_service.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom-blink.h"
#include "third_party/blink/public/mojom/worker/worklet_global_scope_creation_params.mojom.h"
#include "third_party/blink/renderer/core/workers/threaded_worklet_object_proxy.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

namespace {

mojom::blink::WorkletGlobalScopeCreationParamsPtr ToBlinkMojomType(
    mojom::WorkletGlobalScopeCreationParamsPtr global_scope_creation_params) {
  return mojom::blink::WorkletGlobalScopeCreationParams::New(
      KURL(global_scope_creation_params->script_url),
      SecurityOrigin::CreateFromUrlOrigin(
          global_scope_creation_params->starter_origin),
      Vector<mojom::blink::OriginTrialFeature>(
          global_scope_creation_params->origin_trial_features),
      global_scope_creation_params->devtools_token,
      CrossVariantMojoRemote<mojom::WorkletDevToolsHostInterfaceBase>(
          std::move(global_scope_creation_params->devtools_host)),
      CrossVariantMojoRemote<mojom::CodeCacheHostInterfaceBase>(
          std::move(global_scope_creation_params->code_cache_host)),
      CrossVariantMojoRemote<mojom::BrowserInterfaceBrokerInterfaceBase>(
          std::move(global_scope_creation_params->browser_interface_broker)),
      global_scope_creation_params->wait_for_debugger);
}

}  // namespace

// static
void WebSharedStorageWorkletThread::Start(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    CrossVariantMojoReceiver<
        mojom::blink::SharedStorageWorkletServiceInterfaceBase> receiver,
    mojom::WorkletGlobalScopeCreationParamsPtr global_scope_creation_params) {
  MakeGarbageCollected<WebSharedStorageWorkletThreadImpl>(
      main_thread_runner, std::move(receiver),
      ToBlinkMojomType(std::move(global_scope_creation_params)));
}

WebSharedStorageWorkletThreadImpl::WebSharedStorageWorkletThreadImpl(
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_runner,
    mojo::PendingReceiver<mojom::blink::SharedStorageWorkletService> receiver,
    mojom::blink::WorkletGlobalScopeCreationParamsPtr
        global_scope_creation_params)
    : main_thread_runner_(std::move(main_thread_runner)) {
  DCHECK(main_thread_runner_->BelongsToCurrentThread());

  messaging_proxy_ = MakeGarbageCollected<SharedStorageWorkletMessagingProxy>(
      main_thread_runner_, std::move(receiver),
      std::move(global_scope_creation_params),
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
