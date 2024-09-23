// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/modules/storage/cached_storage_area.h"

#include <inttypes.h>

#include <algorithm>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/trace_event/memory_dump_manager.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/string_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/unicode.h"
#include "third_party/blink/renderer/platform/wtf/text/utf8.h"

namespace blink {

namespace {

// Don't change or reorder any of the values in this enum, as these values
// are serialized on disk.
enum class StorageFormat : uint8_t { UTF16 = 0, Latin1 = 1 };

// These methods are used to pack and unpack the page_url/storage_area_id into
// source strings to/from the browser.
String PackSource(const KURL& page_url, const String& storage_area_id) {
  return page_url.GetString() + "\n" + storage_area_id;
}

void UnpackSource(const String& source,
                  KURL* page_url,
                  String* storage_area_id) {
  Vector<String> result;
  source.Split("\n", true, result);
  DCHECK_EQ(result.size(), 2u);
  *page_url = KURL(result[0]);
  *storage_area_id = result[1];
}

// Makes a callback which ignores the |success| result of some async operation
// but which also holds onto a paused WebScopedVirtualTimePauser until invoked.
base::OnceCallback<void(bool)> MakeSuccessCallback(
    CachedStorageArea::Source* source) {
  WebScopedVirtualTimePauser virtual_time_pauser =
      source->CreateWebScopedVirtualTimePauser(
          "CachedStorageArea",
          WebScopedVirtualTimePauser::VirtualTaskDuration::kNonInstant);
  virtual_time_pauser.PauseVirtualTime();
  return WTF::BindOnce([](WebScopedVirtualTimePauser, bool) {},
                       std::move(virtual_time_pauser));
}

}  // namespace

unsigned CachedStorageArea::GetLength() {
  EnsureLoaded();
  return map_->GetLength();
}

String CachedStorageArea::GetKey(unsigned index) {
  EnsureLoaded();
  return map_->GetKey(index);
}

String CachedStorageArea::GetItem(const String& key) {
  EnsureLoaded();
  return map_->GetItem(key);
}

bool CachedStorageArea::SetItem(const String& key,
                                const String& value,
                                Source* source) {
  DCHECK(areas_->Contains(source));

  // A quick check to reject obviously overbudget items to avoid priming the
  // cache.
  if ((key.length() + value.length()) * 2 >
      mojom::blink::StorageArea::kPerStorageAreaQuota) {
    return false;
  }

  EnsureLoaded();
  String old_value;
  if (!map_->SetItem(key, value, &old_value))
    return false;

  const FormatOption value_format = GetValueFormat();
  std::optional<Vector<uint8_t>> optional_old_value;
  if (!old_value.IsNull() && should_send_old_value_on_mutations_)
    optional_old_value = StringToUint8Vector(old_value, value_format);
  KURL page_url = source->GetPageUrl();
  String source_id = areas_->at(source);
  String source_string = PackSource(page_url, source_id);

  if (!is_session_storage_for_prerendering_) {
    remote_area_->Put(StringToUint8Vector(key, GetKeyFormat()),
                      StringToUint8Vector(value, value_format),
                      optional_old_value, source_string,
                      MakeSuccessCallback(source));
    EnqueueCheckpointMicrotask(source);
  }
  if (!IsSessionStorage())
    EnqueuePendingMutation(key, value, old_value, source_string);
  else if (old_value != value)
    EnqueueStorageEvent(key, old_value, value, page_url, source_id);
  return true;
}

void CachedStorageArea::EnqueueCheckpointMicrotask(Source* source) {
  if (checkpoint_queued_) {
    return;
  }

  LocalDOMWindow* window = source->GetDOMWindow();
  if (!window) {
    return;
  }

  checkpoint_queued_ = true;
  window->GetAgent()->event_loop()->EnqueueMicrotask(WTF::BindOnce(
      &CachedStorageArea::NotifyCheckpoint, weak_factory_.GetWeakPtr()));
}

void CachedStorageArea::NotifyCheckpoint() {
  checkpoint_queued_ = false;
  if (remote_area_) {
    remote_area_->Checkpoint();
  }
}

void CachedStorageArea::RemoveItem(const String& key, Source* source) {
  DCHECK(areas_->Contains(source));

  EnsureLoaded();
  String old_value;
  if (!map_->RemoveItem(key, &old_value))
    return;

  std::optional<Vector<uint8_t>> optional_old_value;
  if (should_send_old_value_on_mutations_)
    optional_old_value = StringToUint8Vector(old_value, GetValueFormat());
  KURL page_url = source->GetPageUrl();
  String source_id = areas_->at(source);
  String source_string = PackSource(page_url, source_id);
  if (!is_session_storage_for_prerendering_) {
    remote_area_->Delete(StringToUint8Vector(key, GetKeyFormat()),
                         optional_old_value, source_string,
                         MakeSuccessCallback(source));
    EnqueueCheckpointMicrotask(source);
  }
  if (!IsSessionStorage())
    EnqueuePendingMutation(key, String(), old_value, source_string);
  else
    EnqueueStorageEvent(key, old_value, String(), page_url, source_id);
}

void CachedStorageArea::Clear(Source* source) {
  DCHECK(areas_->Contains(source));

  mojo::PendingRemote<mojom::blink::StorageAreaObserver> new_observer;
  bool already_empty = false;
  if (IsSessionStorage()) {
    EnsureLoaded();
    already_empty = map_->GetLength() == 0u;
  } else if (!map_) {
    // If this is our first operation on the StorageArea, |map_| is null and
    // |receiver_| is still bound to the initial StorageAreaObserver pipe
    // created upon CachedStorageArea construction. We rebind |receiver_| here
    // with a new pipe whose event sequence will be synchronized against the
    // backend from the exact point at which the impending |DeleteAll()|
    // operation takes place. The first event observed after this will always
    // be a corresponding |AllDeleted()| from |source|.
    DCHECK(pending_mutations_by_source_.empty());
    DCHECK(pending_mutations_by_key_.empty());
    receiver_.reset();
    new_observer = receiver_.BindNewPipeAndPassRemote();
  }

  map_ = std::make_unique<StorageAreaMap>(
      mojom::blink::StorageArea::kPerStorageAreaQuota);

  KURL page_url = source->GetPageUrl();
  String source_id = areas_->at(source);
  String source_string = PackSource(page_url, source_id);
  if (!is_session_storage_for_prerendering_) {
    remote_area_->DeleteAll(source_string, std::move(new_observer),
                            MakeSuccessCallback(source));
    EnqueueCheckpointMicrotask(source);
  }
  if (!IsSessionStorage())
    EnqueuePendingMutation(String(), String(), String(), source_string);
  else if (!already_empty)
    EnqueueStorageEvent(String(), String(), String(), page_url, source_id);
}

String CachedStorageArea::RegisterSource(Source* source) {
  String id = String::Number(base::RandUint64());
  areas_->insert(source, id);
  return id;
}

CachedStorageArea::CachedStorageArea(
    AreaType type,
    const BlinkStorageKey& storage_key,
    LocalDOMWindow* local_dom_window,
    StorageNamespace* storage_namespace,
    bool is_session_storage_for_prerendering,
    mojo::PendingRemote<mojom::blink::StorageArea> storage_area)
    : type_(type),
      storage_key_(storage_key),
      storage_namespace_(storage_namespace),
      is_session_storage_for_prerendering_(is_session_storage_for_prerendering),
      areas_(MakeGarbageCollected<HeapHashMap<WeakMember<Source>, String>>()) {
  BindStorageArea(std::move(storage_area), local_dom_window);
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "DOMStorage",
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted()));
}

CachedStorageArea::~CachedStorageArea() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

LocalDOMWindow* CachedStorageArea::GetBestCurrentDOMWindow() {
  for (auto key : areas_->Keys()) {
    if (!key->GetDOMWindow()) {
      continue;
    }
    return key->GetDOMWindow();
  }
  return nullptr;
}

void CachedStorageArea::BindStorageArea(
    mojo::PendingRemote<mojom::blink::StorageArea> new_area,
    LocalDOMWindow* local_dom_window) {
  // Some tests may not provide a StorageNamespace.
  DCHECK(!remote_area_);
  if (!local_dom_window)
    local_dom_window = GetBestCurrentDOMWindow();
  if (!local_dom_window) {
    // If there isn't a local_dom_window to bind to, clear out storage areas and
    // mutations. When EnsureLoaded is called it will attempt to re-bind.
    map_ = nullptr;
    pending_mutations_by_key_.clear();
    pending_mutations_by_source_.clear();
    return;
  }

  // Because the storage area is keyed by the BlinkStorageKey it could be
  // reused by other frames in the same agent cluster so we use the
  // associated AgentGroupScheduler's task runner.
  auto task_runner = local_dom_window->GetFrame()
                         ->GetFrameScheduler()
                         ->GetAgentGroupScheduler()
                         ->DefaultTaskRunner();
  if (new_area) {
    remote_area_.Bind(std::move(new_area), task_runner);
  } else if (storage_namespace_) {
    storage_namespace_->BindStorageArea(
        storage_key_, local_dom_window->GetLocalFrameToken(),
        remote_area_.BindNewPipeAndPassReceiver(task_runner));
  } else {
    return;
  }

  receiver_.reset();
  remote_area_->AddObserver(receiver_.BindNewPipeAndPassRemote(task_runner));
}

void CachedStorageArea::ResetConnection(
    mojo::PendingRemote<mojom::blink::StorageArea> new_area) {
  DCHECK(!is_session_storage_for_prerendering_);
  remote_area_.reset();
  BindStorageArea(std::move(new_area));

  // If we had not yet initialized a local cache, there's no synchronization to
  // be done.
  if (!map_)
    return;

  std::unique_ptr<StorageAreaMap> old_map;
  std::swap(map_, old_map);
  pending_mutations_by_key_.clear();
  pending_mutations_by_source_.clear();

  // Fully repopulate the cache from the loaded backend state.
  EnsureLoaded();

  // The data received from the backend may differ from what we had cached
  // previously. How we proceed depends on the type of storage. First we
  // compute the full diff between our old cache and the restored cache.
  struct ValueDelta {
    String previously_cached_value;
    String restored_value;
  };
  HashMap<String, ValueDelta> deltas;
  for (unsigned i = 0; i < map_->GetLength(); ++i) {
    const String key = map_->GetKey(i);
    const String previously_cached_value = old_map->GetItem(key);
    const String restored_value = map_->GetItem(key);
    if (previously_cached_value != restored_value)
      deltas.insert(key, ValueDelta{previously_cached_value, restored_value});
  }

  // Make sure we also get values for keys that weren't stored in the backend.
  for (unsigned i = 0; i < old_map->GetLength(); ++i) {
    const String key = old_map->GetKey(i);
    const String previously_cached_value = old_map->GetItem(key);
    // This key will already be covered if it's also present in the restored
    // cache.
    if (!map_->GetItem(key).IsNull())
      continue;
    deltas.insert(key, ValueDelta{previously_cached_value, String()});
  }

  if (!IsSessionStorage()) {
    // For Local Storage we have no way of knowing whether changes not reflected
    // by the backend were merely dropped, or if they were landed and
    // overwritten by another client. For simplicity we treat them as dropped,
    // use the restored cache without modification, and notify script about any
    // deltas from the previously cached state.
    for (const auto& delta : deltas) {
      EnqueueStorageEvent(delta.key, delta.value.previously_cached_value,
                          delta.value.restored_value, "", "");
    }
    return;
  }

  // For Session Storage, we're the source of truth for our own data, so we
  // can simply push any needed updates down to the backend and continue
  // using our previous cache.
  map_ = std::move(old_map);
  for (const auto& delta : deltas) {
    if (delta.value.previously_cached_value.IsNull()) {
      remote_area_->Delete(
          StringToUint8Vector(delta.key, GetKeyFormat()),
          StringToUint8Vector(delta.value.restored_value, GetValueFormat()),
          /*source=*/"\n", base::DoNothing());
    } else {
      const FormatOption value_format = GetValueFormat();
      remote_area_->Put(
          StringToUint8Vector(delta.key, GetKeyFormat()),
          StringToUint8Vector(delta.value.previously_cached_value,
                              value_format),
          StringToUint8Vector(delta.value.restored_value, value_format),
          /*source=*/"\n", base::DoNothing());
    }
  }
}

void CachedStorageArea::KeyChanged(
    const Vector<uint8_t>& key,
    const Vector<uint8_t>& new_value,
    const std::optional<Vector<uint8_t>>& old_value,
    const String& source) {
  DCHECK(!IsSessionStorage());

  String key_string =
      Uint8VectorToString(key, FormatOption::kLocalStorageDetectFormat);
  String new_value_string =
      Uint8VectorToString(new_value, FormatOption::kLocalStorageDetectFormat);
  String old_value_string;
  if (old_value) {
    old_value_string = Uint8VectorToString(
        *old_value, FormatOption::kLocalStorageDetectFormat);
  }

  std::unique_ptr<PendingMutation> local_mutation = PopPendingMutation(source);
  if (map_ && !local_mutation)
    MaybeApplyNonLocalMutationForKey(key_string, new_value_string);

  // If we did pop a mutation, it had better be for the same key this event
  // references.
  DCHECK(!local_mutation || local_mutation->key == key_string);

  KURL page_url;
  String storage_area_id;
  UnpackSource(source, &page_url, &storage_area_id);
  EnqueueStorageEvent(key_string, old_value_string, new_value_string, page_url,
                      storage_area_id);
}

void CachedStorageArea::KeyChangeFailed(const Vector<uint8_t>& key,
                                        const String& source) {
  DCHECK(!IsSessionStorage());

  String key_string =
      Uint8VectorToString(key, FormatOption::kLocalStorageDetectFormat);
  std::unique_ptr<PendingMutation> local_mutation = PopPendingMutation(source);

  // We don't care about failed changes from other clients.
  if (!local_mutation)
    return;

  // If we did pop a mutation, it had better be for the same key this event
  // references.
  DCHECK_EQ(local_mutation->key, key_string);

  // A pending local mutation was rejected by the backend.
  String old_value = local_mutation->old_value;
  auto key_queue_iter = pending_mutations_by_key_.find(key_string);
  if (key_queue_iter == pending_mutations_by_key_.end()) {
    // If this was the only pending local mutation for the key, we simply revert
    // the cache to the stored |old_value|. Note that this may not be the value
    // originally stored before the corresponding |Put()| which enqueued this
    // mutation: see logic below and in |MaybeApplyNonLocalMutationForKey()| for
    // potential updates to the stored |old_value|.
    DCHECK(map_);
    String invalid_cached_value = map_->GetItem(key_string);
    if (old_value.IsNull())
      map_->RemoveItem(key_string, nullptr);
    else
      map_->SetItemIgnoringQuota(key_string, old_value);

    KURL page_url;
    String storage_area_id;
    UnpackSource(source, &page_url, &storage_area_id);
    EnqueueStorageEvent(key_string, invalid_cached_value, old_value, page_url,
                        storage_area_id);
    return;
  }

  // This was NOT the only pending local mutation for the key, so its failure
  // is irrelevant to the current cache state (i.e. something has already
  // overwritten its local change.) In this case, there's no event to dispatch
  // and no cache update to make. We do however need to propagate the stored
  // |old_value| to the next queued PendingMutation so that *it* can restore the
  // correct value if it fails.
  key_queue_iter->value.front()->old_value = old_value;
}

void CachedStorageArea::KeyDeleted(
    const Vector<uint8_t>& key,
    const std::optional<Vector<uint8_t>>& old_value,
    const String& source) {
  DCHECK(!IsSessionStorage());

  String key_string =
      Uint8VectorToString(key, FormatOption::kLocalStorageDetectFormat);
  std::unique_ptr<PendingMutation> local_mutation = PopPendingMutation(source);

  if (map_ && !local_mutation)
    MaybeApplyNonLocalMutationForKey(key_string, String());

  // If we did pop a mutation, it had better be for the same key this event
  // references.
  DCHECK(!local_mutation || local_mutation->key == key_string);

  if (old_value) {
    KURL page_url;
    String storage_area_id;
    UnpackSource(source, &page_url, &storage_area_id);
    EnqueueStorageEvent(
        key_string,
        Uint8VectorToString(*old_value,
                            FormatOption::kLocalStorageDetectFormat),
        String(), page_url, storage_area_id);
  }
}

void CachedStorageArea::AllDeleted(bool was_nonempty, const String& source) {
  std::unique_ptr<PendingMutation> local_mutation = PopPendingMutation(source);

  // Note that if this event was from a local source, we've already cleared the
  // cache when |Clear()| was called so there's nothing to do other than
  // broadcast the StorageEvent (see below). Broadcast was deferred until now in
  // that case because we needed to know whether the StorageArea was actually
  // non-empty prior to the call.
  if (!local_mutation) {
    map_ = std::make_unique<StorageAreaMap>(
        mojom::blink::StorageArea::kPerStorageAreaQuota);

    // Re-apply the most recent local mutations for each key. These must have
    // occurred after the deletion, because we haven't observed events for them
    // yet. Since they would eventually need to be restored by those impending
    // events otherwise, we instead restore them now to avoid observable churn
    // in the storage state.
    for (const auto& key_mutations : pending_mutations_by_key_) {
      DCHECK(!key_mutations.value.empty());
      PendingMutation* const most_recent_mutation = key_mutations.value.back();
      if (!most_recent_mutation->new_value.IsNull()) {
        map_->SetItemIgnoringQuota(key_mutations.key,
                                   most_recent_mutation->new_value);
      }

      // And now the first unacked mutation should revert to an empty value on
      // failure since that's the state in the backend.
      key_mutations.value.front()->old_value = String();
    }
  }

  // If we did pop a mutation, it had better be from a corresponding |Clear()|,
  // i.e. with no key value.
  DCHECK(!local_mutation || local_mutation->key.IsNull());

  if (was_nonempty) {
    KURL page_url;
    String storage_area_id;
    UnpackSource(source, &page_url, &storage_area_id);
    EnqueueStorageEvent(String(), String(), String(), page_url,
                        storage_area_id);
  }
}

void CachedStorageArea::ShouldSendOldValueOnMutations(bool value) {
  DCHECK(!IsSessionStorage());
  should_send_old_value_on_mutations_ = value;
}

bool CachedStorageArea::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;

  WTF::String dump_name = WTF::String::Format(
      "site_storage/%s/0x%" PRIXPTR "/cache_size",
      IsSessionStorage() ? "session_storage" : "local_storage",
      reinterpret_cast<uintptr_t>(this));
  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name.Utf8());
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, memory_used());
  pmd->AddSuballocation(dump->guid(),
                        WTF::Partitions::kAllocatedObjectPoolName);
  return true;
}

void CachedStorageArea::EnqueuePendingMutation(const String& key,
                                               const String& new_value,
                                               const String& old_value,
                                               const String& source) {
  // Track this pending mutation until we observe a corresponding event on
  // our StorageAreaObserver interface. As long as this operation is pending,
  // we will effectively ignore other observed mutations on this key. Note that
  // |old_value| may be updated while this PendingMutation sits in queue, if
  // non-local mutations are observed while waiting for this local mutation to
  // be acknowledged. See logic in |MaybeApplyNonLocalMutationForKey()|.
  auto mutation = std::make_unique<PendingMutation>();
  mutation->key = key;
  mutation->new_value = new_value;
  mutation->old_value = old_value;

  // |key| is null for |Clear()| mutation events.
  if (!key.IsNull()) {
    pending_mutations_by_key_.insert(key, Deque<PendingMutation*>())
        .stored_value->value.push_back(mutation.get());
  }
  pending_mutations_by_source_.insert(source, OwnedPendingMutationQueue())
      .stored_value->value.push_back(std::move(mutation));
}

std::unique_ptr<CachedStorageArea::PendingMutation>
CachedStorageArea::PopPendingMutation(const String& source) {
  auto source_queue_iter = pending_mutations_by_source_.find(source);
  if (source_queue_iter == pending_mutations_by_source_.end())
    return nullptr;

  OwnedPendingMutationQueue& mutations_for_source = source_queue_iter->value;
  DCHECK(!mutations_for_source.empty());
  std::unique_ptr<PendingMutation> mutation =
      std::move(mutations_for_source.front());
  mutations_for_source.pop_front();
  if (mutations_for_source.empty())
    pending_mutations_by_source_.erase(source_queue_iter);

  // If |key| is non-null, the oldest mutation queued for that key MUST be this
  // mutation, and we remove it as well. If |key| is null, that means |mutation|
  // was a |DeleteAll()| request and there is no corresponding per-key queue
  // entry.
  const String key = mutation->key;
  if (!key.IsNull()) {
    auto key_queue_iter = pending_mutations_by_key_.find(key);
    CHECK(key_queue_iter != pending_mutations_by_key_.end(),
          base::NotFatalUntil::M130);
    DCHECK_EQ(key_queue_iter->value.front(), mutation.get());
    key_queue_iter->value.pop_front();
    if (key_queue_iter->value.empty())
      pending_mutations_by_key_.erase(key_queue_iter);
  }

  return mutation;
}

void CachedStorageArea::MaybeApplyNonLocalMutationForKey(
    const String& key,
    const String& new_value) {
  DCHECK(map_);
  auto key_queue_iter = pending_mutations_by_key_.find(key);
  if (key_queue_iter == pending_mutations_by_key_.end()) {
    // No pending local mutations, so we can apply this non-local mutation
    // directly to our cache and then we're done.
    if (new_value.IsNull()) {
      map_->RemoveItem(key, nullptr);
    } else {
      // We turn off quota checking here to accommodate the over budget
      // allowance that's provided in the browser process.
      map_->SetItemIgnoringQuota(key, new_value);
    }

    return;
  }

  // We don't want to apply this mutation yet, possibly ever. We need to wait
  // until one or more pending local mutations are acknowledged either
  // successfully or unsuccessfully. For now we stash this non-local mutation in
  // the |old_value| at the front of the key's queue so we don't lose it. If the
  // local mutation eventually fails, we may restore the key to this non-local
  // mutation value.
  key_queue_iter->value.front()->old_value = new_value;
}

void CachedStorageArea::EnsureLoaded() {
  if (map_)
    return;
  if (!remote_area_)
    BindStorageArea();

  // There might be something weird happening during the sync call that destroys
  // this object. Keep a reference to either fix or rule out that this is the
  // problem. See https://crbug.com/915577.
  scoped_refptr<CachedStorageArea> keep_alive(this);
  base::TimeTicks before = base::TimeTicks::Now();
  Vector<mojom::blink::KeyValuePtr> data;

  // We had no cached |map_|, which means |receiver_| was bound to the original
  // StorageAreaObserver pipe created upon CachedStorageArea construction. We
  // replace it with a new receiver whose event sequence is synchronized against
  // the result of |GetAll()| for consistency.
  receiver_.reset();
  remote_area_->GetAll(receiver_.BindNewPipeAndPassRemote(), &data);

  // Determine data formats.
  const FormatOption key_format = GetKeyFormat();
  const FormatOption value_format = GetValueFormat();

  map_ = std::make_unique<StorageAreaMap>(
      mojom::blink::StorageArea::kPerStorageAreaQuota);
  for (const auto& item : data) {
    map_->SetItemIgnoringQuota(Uint8VectorToString(item->key, key_format),
                               Uint8VectorToString(item->value, value_format));
  }

  base::TimeDelta time_to_prime = base::TimeTicks::Now() - before;
  UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrime", time_to_prime);

  size_t local_storage_size_kb = map_->quota_used() / 1024;
  // Track localStorage size, from 0-6MB. Note that the maximum size should be
  // 10MB, but we add some slop since we want to make sure the max size is
  // always above what we see in practice, since histograms can't change.
  UMA_HISTOGRAM_CUSTOM_COUNTS(
      "LocalStorage.MojoSizeInKB",
      base::saturated_cast<base::Histogram::Sample>(local_storage_size_kb), 1,
      6 * 1024, 50);
  if (local_storage_size_kb < 100) {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeForUnder100KB",
                        time_to_prime);
  } else if (local_storage_size_kb < 1000) {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeFor100KBTo1MB",
                        time_to_prime);
  } else {
    UMA_HISTOGRAM_TIMES("LocalStorage.MojoTimeToPrimeFor1MBTo5MB",
                        time_to_prime);
  }
}

CachedStorageArea::FormatOption CachedStorageArea::GetKeyFormat() const {
  return IsSessionStorage() ? FormatOption::kSessionStorageForceUTF8
                            : FormatOption::kLocalStorageDetectFormat;
}

CachedStorageArea::FormatOption CachedStorageArea::GetValueFormat() const {
  return IsSessionStorage() ? FormatOption::kSessionStorageForceUTF16
                            : FormatOption::kLocalStorageDetectFormat;
}

bool CachedStorageArea::IsSessionStorage() const {
  return type_ == AreaType::kSessionStorage;
}

void CachedStorageArea::EnqueueStorageEvent(const String& key,
                                            const String& old_value,
                                            const String& new_value,
                                            const String& url,
                                            const String& storage_area_id) {
  // Ignore key-change events which aren't actually changing the value.
  if (!key.IsNull() && new_value == old_value)
    return;

  HeapVector<Member<Source>, 1> areas_to_remove_;
  for (const auto& area : *areas_) {
    if (area.value != storage_area_id) {
      bool keep = area.key->EnqueueStorageEvent(key, old_value, new_value, url);
      if (!keep)
        areas_to_remove_.push_back(area.key);
    }
  }
  areas_->RemoveAll(areas_to_remove_);
  if (storage_namespace_) {
    storage_namespace_->DidDispatchStorageEvent(storage_key_, key, old_value,
                                                new_value);
  }
}

// static
String CachedStorageArea::Uint8VectorToString(const Vector<uint8_t>& input,
                                              FormatOption format_option) {
  if (input.empty())
    return g_empty_string;
  const wtf_size_t input_size = input.size();
  String result;
  bool corrupt = false;
  switch (format_option) {
    case FormatOption::kSessionStorageForceUTF16: {
      if (input_size % sizeof(UChar) != 0) {
        corrupt = true;
        break;
      }
      StringBuffer<UChar> buffer(input_size / sizeof(UChar));
      std::memcpy(buffer.Characters(), input.data(), input_size);
      result = String::Adopt(buffer);
      break;
    }
    case FormatOption::kSessionStorageForceUTF8: {
      // TODO(mek): When this lived in content it used to do a "lenient"
      // conversion, while this is a strict conversion. Figure out if that
      // difference actually matters in practice.
      result = String::FromUTF8(input.data(), input_size);
      if (result.IsNull()) {
        corrupt = true;
        break;
      }
      break;
    }
    case FormatOption::kLocalStorageDetectFormat: {
      StorageFormat format = static_cast<StorageFormat>(input[0]);
      const wtf_size_t payload_size = input_size - 1;
      switch (format) {
        case StorageFormat::UTF16: {
          if (payload_size % sizeof(UChar) != 0) {
            corrupt = true;
            break;
          }
          StringBuffer<UChar> buffer(payload_size / sizeof(UChar));
          std::memcpy(buffer.Characters(), input.data() + 1, payload_size);
          result = String::Adopt(buffer);
          break;
        }
        case StorageFormat::Latin1:
          result = String(reinterpret_cast<const char*>(input.data() + 1),
                          payload_size);
          break;
        default:
          corrupt = true;
      }
      break;
    }
  }
  if (corrupt) {
    // TODO(mek): Better error recovery when corrupt (or otherwise invalid) data
    // is detected.
    LOCAL_HISTOGRAM_BOOLEAN("LocalStorageCachedArea.CorruptData", true);
    LOG(ERROR) << "Corrupt data in domstorage";
    return g_empty_string;
  }
  return result;
}

// static
Vector<uint8_t> CachedStorageArea::StringToUint8Vector(
    const String& input,
    FormatOption format_option) {
  switch (format_option) {
    case FormatOption::kSessionStorageForceUTF16: {
      Vector<uint8_t> result(input.length() * sizeof(UChar));
      input.CopyTo(reinterpret_cast<UChar*>(result.data()), 0, input.length());
      return result;
    }
    case FormatOption::kSessionStorageForceUTF8: {
      unsigned length = input.length();
      if (input.Is8Bit() && input.ContainsOnlyASCIIOrEmpty()) {
        Vector<uint8_t> result(length);
        std::memcpy(result.data(), input.Characters8(), length);
        return result;
      }
      // Handle 8 bit case where it's not only ascii.
      if (input.Is8Bit()) {
        // This code is copied from WTF::String::Utf8(), except the vector
        // doesn't have a stack-allocated capacity.
        // We do this because there isn't a way to transform the std::string we
        // get from WTF::String::Utf8() to a Vector without an extra copy.
        if (length > std::numeric_limits<unsigned>::max() / 3)
          return Vector<uint8_t>();
        Vector<uint8_t> buffer_vector(length * 3);
        uint8_t* buffer = buffer_vector.data();
        const LChar* characters = input.Characters8();

        WTF::unicode::ConversionResult result =
            WTF::unicode::ConvertLatin1ToUTF8(
                &characters, characters + length,
                reinterpret_cast<char**>(&buffer),
                reinterpret_cast<char*>(buffer + buffer_vector.size()));
        // (length * 3) should be sufficient for any conversion
        DCHECK_NE(result, WTF::unicode::kTargetExhausted);
        buffer_vector.Shrink(
            static_cast<wtf_size_t>(buffer - buffer_vector.data()));
        return buffer_vector;
      }

      // TODO(dmurph): Figure out how to avoid a copy here.
      // TODO(dmurph): Handle invalid UTF16 better. https://crbug.com/873280.
      StringUTF8Adaptor utf8(
          input, WTF::kStrictUTF8ConversionReplacingUnpairedSurrogatesWithFFFD);
      Vector<uint8_t> result(utf8.size());
      std::memcpy(result.data(), utf8.data(), utf8.size());
      return result;
    }
    case FormatOption::kLocalStorageDetectFormat: {
      if (input.ContainsOnlyLatin1OrEmpty()) {
        Vector<uint8_t> result(input.length() + 1);
        result[0] = static_cast<uint8_t>(StorageFormat::Latin1);
        if (input.Is8Bit()) {
          std::memcpy(result.data() + 1, input.Characters8(), input.length());
        } else {
          for (unsigned i = 0; i < input.length(); ++i) {
            result[i + 1] = input[i];
          }
        }
        return result;
      }
      DCHECK(!input.Is8Bit());
      Vector<uint8_t> result(input.length() * sizeof(UChar) + 1);
      result[0] = static_cast<uint8_t>(StorageFormat::UTF16);
      std::memcpy(result.data() + 1, input.Characters16(),
                  input.length() * sizeof(UChar));
      return result;
    }
  }
  NOTREACHED_IN_MIGRATION();
}

void CachedStorageArea::EvictCachedData() {
  map_.reset();
}

}  // namespace blink
