// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom-blink.h"
#include "third_party/blink/public/platform/child_url_loader_factory_bundle.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_high_res_time_stamp.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_state_observer.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_set.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_priority.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace network {

struct ResourceRequest;

}  // namespace network

namespace blink {

class AbortSignal;
class ExceptionState;
class ExecutionContext;
class FetchRequestData;
class Response;
class ScriptState;
class FetchLaterResult;

// net::ERR_ABORTED is reported for page teardown, for script-initiated aborts
// and for internal loader cancellation alike, so the raw net error cannot tell
// a benign page exit apart from a real failure.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(FetchAbortedReason)
enum class FetchAbortedReason {
  // Script called abort() on the AbortSignal passed to fetch().
  kAbortSignal = 0,
  // The frame that issued the fetch was being detached: a tab close, a
  // navigation away or an iframe removal. Blink stops all loaders at the very
  // start of frame detach, while the document and the execution context still
  // look alive, so the frame lifecycle is the only state that identifies this.
  kFrameDetaching = 1,
  // The execution context itself was torn down before the loader was
  // cancelled. Frames are cancelled while their context is still alive, so
  // this mostly covers workers.
  kExecutionContextDestroyed = 2,
  // Cancellation from inside the loading stack with the frame still attached,
  // for example window.stop().
  kOther = 3,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/enums.xml:FetchAbortedReason)

class CORE_EXPORT FetchManager final
    : public GarbageCollected<FetchManager>,
      public ExecutionContextLifecycleObserver {
 public:
  static constexpr char kErrorCodesHistogramName[] = "Blink.Fetch.ErrorCodes";
  static constexpr char kErrorCodesBeforeResponseHeadersHistogramName[] =
      "Blink.Fetch.ErrorCodes.BeforeResponseHeaders";
  static constexpr char kErrorCodesAfterResponseHeadersHistogramName[] =
      "Blink.Fetch.ErrorCodes.AfterResponseHeaders";
  static constexpr char kAbortedReasonHistogramName[] =
      "Blink.Fetch.AbortedReason";

  explicit FetchManager(ExecutionContext*);

  ScriptPromise<Response> Fetch(ScriptState*,
                                FetchRequestData*,
                                AbortSignal*,
                                ExceptionState&);

  // ExecutionContextLifecycleObserver overrides:
  void ContextDestroyed() override;

  void Trace(Visitor*) const override;

 private:
  class Loader;

  // Removes loader from `loaders_`.
  void OnLoaderFinished(Loader*);

  HeapHashSet<Member<Loader>> loaders_;
};

class CORE_EXPORT FetchLaterManager final
    : public GarbageCollected<FetchLaterManager>,
      public ExecutionContextLifecycleObserver,
      public mojom::blink::PermissionObserver {
 public:
  explicit FetchLaterManager(ExecutionContext*);

  FetchLaterResult* FetchLater(ScriptState*,
                               FetchRequestData*,
                               AbortSignal*,
                               std::optional<DOMHighResTimeStamp>,
                               ExceptionState&);

  // Updates two types of quotas using the Step 9 of the following algorithm:
  // https://whatpr.org/fetch/1647.html#available-deferred-fetch-quota
  // 9. For each deferred fetch record deferredRecord of controlDocument's
  // fetch group’s deferred fetch records ...
  //
  // Specifically, the quotas are updated according to all the queued FetchLater
  // requests from `deferred_loaders_`.
  // Unlike spec, neither `quota_for_url_origin` nor `total_quota` can be
  // negative. They will be set to zero if they would have become negative.
  void UpdateDeferredBytesQuota(const KURL& url,
                                uint64_t& quota_for_url_origin,
                                uint64_t& total_quota) const;

  // ExecutionContextLifecycleObserver overrides:
  void ContextDestroyed() override;
  void ContextEnteredBackForwardCache() override;

  void Trace(Visitor*) const override;

  // For testing only:
  size_t NumLoadersForTesting() const;
  void RecreateTimerForTesting(scoped_refptr<base::SingleThreadTaskRunner>,
                               const base::TickClock*);

 private:
  class DeferredLoader;
  // TODO(crbug.com/1293679): Update to proper TaskType once spec is finalized.
  // Using the `TaskType::kNetworkingUnfreezable` as deferred requests need to
  // work when ExecutionContext is in BackForwardCache/frozen.
  static constexpr TaskType kTaskType = TaskType::kNetworkingUnfreezable;

  // Returns a pointer to the wrapper that provides FetchLaterLoaderFactory.
  // Returns nullptr if the context is detached.
  blink::ChildURLLoaderFactoryBundle* GetFactory();

  // Creates a network version of ResourceRequest using `request` and `options`.
  // Returns nullptr if any of the checks fail during the call.
  // Note that this method must only be used to generate FetchLater requests.
  std::unique_ptr<network::ResourceRequest> PrepareNetworkRequest(
      ResourceRequest request,
      const ResourceLoaderOptions& options) const;

  // mojom::blink::PermissionObserver overrides:
  void OnPermissionStatusChange(
      mojom::blink::PermissionStatusWithDetailsPtr status) override;

  // Returns true if BackgroundSync permission has been enabled for the
  // ExecutionContext of this.
  bool IsBackgroundSyncGranted() const;

  // Removes a loader from `deferred_loaders_`.
  void OnDeferredLoaderFinished(DeferredLoader*);

  // Every deferred loader represents a FetchLater request.
  HeapHashSet<Member<DeferredLoader>> deferred_loaders_;

  // Whether the ExecutionContext of `this` has permission to run deferred
  // requests after the context enters BackForwardCache.
  // Defaults to denied. It should be updated by
  // `permission_observer_receiver_` shortly after ctor.
  mojom::blink::PermissionStatus background_sync_permission_ =
      mojom::blink::PermissionStatus::DENIED;
  HeapMojoReceiver<mojom::blink::PermissionObserver, FetchLaterManager>
      permission_observer_receiver_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_FETCH_MANAGER_H_
