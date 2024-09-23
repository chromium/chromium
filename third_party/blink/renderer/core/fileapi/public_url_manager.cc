/*
 * Copyright (C) 2012 Motorola Mobility Inc.
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/fileapi/public_url_manager.h"

#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom-blink.h"
#include "third_party/blink/public/mojom/blob/blob_url_store.mojom-blink.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/core/workers/worklet_global_scope.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/blob/blob_url.h"
#include "third_party/blink/renderer/platform/blob/blob_url_null_origin_map.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/network/blink_schemeful_site.h"
#include "third_party/blink/renderer/platform/scheduler/main_thread/task_type_names.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

static void RemoveFromNullOriginMapIfNecessary(const KURL& blob_url) {
  DCHECK(blob_url.ProtocolIs("blob"));
  if (BlobURL::GetOrigin(blob_url) == "null")
    BlobURLNullOriginMap::GetInstance()->Remove(blob_url);
}

}  // namespace

PublicURLManager::PublicURLManager(ExecutionContext* execution_context)
    : ExecutionContextLifecycleObserver(execution_context),
      frame_url_store_(execution_context),
      worker_url_store_(execution_context) {
  if (auto* window = DynamicTo<LocalDOMWindow>(execution_context)) {
    LocalFrame* frame = window->GetFrame();
    if (!frame) {
      is_stopped_ = true;
      return;
    }

    frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
        frame_url_store_.BindNewEndpointAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kFileReading)));

  } else if (auto* worker_global_scope =
                 DynamicTo<WorkerGlobalScope>(execution_context)) {
    if (worker_global_scope->IsClosing()) {
      is_stopped_ = true;
      return;
    }

    worker_global_scope->GetBrowserInterfaceBroker().GetInterface(
        worker_url_store_.BindNewPipeAndPassReceiver(
            execution_context->GetTaskRunner(TaskType::kFileReading)));

  } else if (auto* worklet_global_scope =
                 DynamicTo<WorkletGlobalScope>(execution_context)) {
    if (worklet_global_scope->IsClosing()) {
      is_stopped_ = true;
      return;
    }

    if (worklet_global_scope->IsMainThreadWorkletGlobalScope()) {
      LocalFrame* frame = worklet_global_scope->GetFrame();
      if (!frame) {
        is_stopped_ = true;
        return;
      }

      frame->GetRemoteNavigationAssociatedInterfaces()->GetInterface(
          frame_url_store_.BindNewEndpointAndPassReceiver(
              execution_context->GetTaskRunner(TaskType::kFileReading)));
    } else {
      // For threaded worklets we don't have a frame accessible here, so
      // instead we'll use a PendingRemote provided by the frame that created
      // this worklet.
      mojo::PendingRemote<mojom::blink::BlobURLStore> pending_remote =
          worklet_global_scope->TakeBlobUrlStorePendingRemote();
      DCHECK(pending_remote.is_valid());
      worker_url_store_.Bind(
          std::move(pending_remote),
          execution_context->GetTaskRunner(TaskType::kFileReading));
    }
  } else {
    NOTREACHED_IN_MIGRATION();
  }
}

PublicURLManager::PublicURLManager(
    base::PassKey<GlobalStorageAccessHandle>,
    ExecutionContext* execution_context,
    mojo::PendingAssociatedRemote<mojom::blink::BlobURLStore>
        frame_url_store_remote)
    : ExecutionContextLifecycleObserver(execution_context),
      frame_url_store_(execution_context),
      worker_url_store_(execution_context) {
  frame_url_store_.Bind(
      std::move(frame_url_store_remote),
      execution_context->GetTaskRunner(TaskType::kFileReading));
}

mojom::blink::BlobURLStore& PublicURLManager::GetBlobURLStore() {
  DCHECK_NE(frame_url_store_.is_bound(), worker_url_store_.is_bound());
  if (frame_url_store_.is_bound()) {
    return *frame_url_store_.get();
  } else {
    return *worker_url_store_.get();
  }
}

String PublicURLManager::RegisterURL(URLRegistrable* registrable) {
  if (is_stopped_)
    return String();

  const KURL& url =
      BlobURL::CreatePublicURL(GetExecutionContext()->GetSecurityOrigin());
  DCHECK(!url.IsEmpty());
  const String& url_string = url.GetString();

  if (registrable->IsMojoBlob()) {
    mojo::PendingRemote<mojom::blink::Blob> blob_remote;
    mojo::PendingReceiver<mojom::blink::Blob> blob_receiver =
        blob_remote.InitWithNewPipeAndPassReceiver();

    // Determining the top-level site for workers is non-trivial. We assume
    // usage of blob URLs in workers is much lower than in windows, so we
    // should still get useful metrics even while ignoring workers.
    std::optional<BlinkSchemefulSite> top_level_site;
    if (GetExecutionContext()->IsWindow()) {
      auto* window = To<LocalDOMWindow>(GetExecutionContext());
      if (window->top() && window->top()->GetFrame()) {
        top_level_site = BlinkSchemefulSite(window->top()
                                                ->GetFrame()
                                                ->GetSecurityContext()
                                                ->GetSecurityOrigin());
      }
    }

    GetBlobURLStore().Register(std::move(blob_remote), url,
                               GetExecutionContext()->GetAgentClusterID(),
                               top_level_site);

    mojo_urls_.insert(url_string);
    registrable->CloneMojoBlob(std::move(blob_receiver));
  } else {
    URLRegistry* registry = &registrable->Registry();
    registry->RegisterURL(url, registrable);
    url_to_registry_.insert(url_string, registry);
  }

  SecurityOrigin* mutable_origin =
      GetExecutionContext()->GetMutableSecurityOrigin();
  if (mutable_origin->SerializesAsNull()) {
    BlobURLNullOriginMap::GetInstance()->Add(url, mutable_origin);
  }

  return url_string;
}

void PublicURLManager::Revoke(const KURL& url) {
  if (is_stopped_)
    return;
  // Don't bother trying to revoke URLs that can't have been registered anyway.
  if (!url.ProtocolIs("blob") || url.HasFragmentIdentifier())
    return;
  // Don't support revoking cross-origin blob URLs.
  if (!SecurityOrigin::Create(url)->IsSameOriginWith(
          GetExecutionContext()->GetSecurityOrigin()))
    return;

  GetBlobURLStore().Revoke(url);
  mojo_urls_.erase(url.GetString());

  RemoveFromNullOriginMapIfNecessary(url);
  auto it = url_to_registry_.find(url.GetString());
  if (it == url_to_registry_.end())
    return;
  it->value->UnregisterURL(url);
  url_to_registry_.erase(it);
}

void PublicURLManager::Resolve(
    const KURL& url,
    mojo::PendingReceiver<network::mojom::blink::URLLoaderFactory>
        factory_receiver) {
  if (is_stopped_)
    return;

  DCHECK(url.ProtocolIs("blob"));

  auto metrics_callback = [](ExecutionContext* execution_context,
                             const std::optional<base::UnguessableToken>&
                                 unsafe_agent_cluster_id,
                             const std::optional<BlinkSchemefulSite>&
                                 unsafe_top_level_site) {
    if (execution_context->GetAgentClusterID() != unsafe_agent_cluster_id) {
      execution_context->CountUse(
          WebFeature::
              kBlobStoreAccessAcrossAgentClustersInResolveAsURLLoaderFactory);
    }
    // Determining top-level site in a worker is non-trivial. Since this is only
    // used to calculate metrics it should be okay to not track top-level site
    // in that case, as long as the count for unknown top-level sites ends up
    // low enough compared to overall usage.
    std::optional<BlinkSchemefulSite> top_level_site;
    if (execution_context->IsWindow()) {
      auto* window = To<LocalDOMWindow>(execution_context);
      if (window->top() && window->top()->GetFrame()) {
        top_level_site = BlinkSchemefulSite(window->top()
                                                ->GetFrame()
                                                ->GetSecurityContext()
                                                ->GetSecurityOrigin());
      }
    }
    if ((!top_level_site || !unsafe_top_level_site) &&
        execution_context->GetAgentClusterID() != unsafe_agent_cluster_id) {
      // Either the registration or resolve happened in a context where it's not
      // easy to determine the top-level site, and agent cluster doesn't match
      // either (if agent cluster matches, by definition top-level site would
      // also match, so this only records page loads where there is a chance
      // that top-level site doesn't match).
      execution_context->CountUse(
          WebFeature::kBlobStoreAccessUnknownTopLevelSite);
    } else if (top_level_site != unsafe_top_level_site) {
      // Blob URL lookup happened with a different top-level site than Blob URL
      // registration.
      execution_context->CountUse(
          WebFeature::kBlobStoreAccessAcrossTopLevelSite);
    }
  };

  GetBlobURLStore().ResolveAsURLLoaderFactory(
      url, std::move(factory_receiver),
      WTF::BindOnce(metrics_callback, WrapPersistent(GetExecutionContext())));
}

void PublicURLManager::Resolve(
    const KURL& url,
    mojo::PendingReceiver<mojom::blink::BlobURLToken> token_receiver) {
  if (is_stopped_)
    return;

  DCHECK(url.ProtocolIs("blob"));

  auto metrics_callback = [](ExecutionContext* execution_context,
                             const std::optional<base::UnguessableToken>&
                                 unsafe_agent_cluster_id) {
    if (execution_context->GetAgentClusterID() != unsafe_agent_cluster_id) {
      execution_context->CountUse(
          WebFeature::
              kBlobStoreAccessAcrossAgentClustersInResolveForNavigation);
    }
  };

  GetBlobURLStore().ResolveForNavigation(
      url, std::move(token_receiver),
      WTF::BindOnce(metrics_callback, WrapPersistent(GetExecutionContext())));
}

void PublicURLManager::ContextDestroyed() {
  if (is_stopped_)
    return;

  is_stopped_ = true;
  for (auto& url_registry : url_to_registry_) {
    url_registry.value->UnregisterURL(KURL(url_registry.key));
    RemoveFromNullOriginMapIfNecessary(KURL(url_registry.key));
  }
  for (const auto& url : mojo_urls_)
    RemoveFromNullOriginMapIfNecessary(KURL(url));

  url_to_registry_.clear();
  mojo_urls_.clear();
}

void PublicURLManager::Trace(Visitor* visitor) const {
  visitor->Trace(frame_url_store_);
  visitor->Trace(worker_url_store_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
