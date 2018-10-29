// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/dom_storage/dom_storage_cached_area.h"

#include <limits>

#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/dom_storage/dom_storage_map.h"
#include "content/renderer/dom_storage/dom_storage_proxy.h"
#include "third_party/blink/public/platform/scheduler/web_thread_scheduler.h"

namespace content {

DOMStorageCachedArea::DOMStorageCachedArea(
    const std::string& namespace_id,
    const GURL& origin,
    DOMStorageProxy* proxy,
    blink::scheduler::WebThreadScheduler* main_thread_scheduler)
    : ignore_all_mutations_(false),
      namespace_id_(namespace_id),
      origin_(origin),
      proxy_(proxy),
      main_thread_scheduler_(main_thread_scheduler),
      weak_factory_(this) {}

DOMStorageCachedArea::~DOMStorageCachedArea() {}

unsigned DOMStorageCachedArea::GetLength(int connection_id) {
  PrimeIfNeeded(connection_id);
  return map_->Length();
}

base::NullableString16 DOMStorageCachedArea::GetKey(
    int connection_id,
    unsigned index,
    bool* did_decrease_iterator) {
  PrimeIfNeeded(connection_id);
  return map_->Key(index, did_decrease_iterator);
}

base::NullableString16 DOMStorageCachedArea::GetItem(
    int connection_id,
    const base::string16& key) {
  PrimeIfNeeded(connection_id);
  return map_->GetItem(key);
}

bool DOMStorageCachedArea::SetItem(int connection_id,
                                   const base::string16& key,
                                   const base::string16& value,
                                   const GURL& page_url) {
  // A quick check to reject obviously overbudget items to avoid
  // the priming the cache.
  if ((key.length() + value.length()) * sizeof(base::char16) >
      kPerStorageAreaQuota)
    return false;

  PrimeIfNeeded(connection_id);
  base::NullableString16 old_value;
#if !defined(OS_ANDROID)
  if (!map_->SetItem(key, value, nullptr))
    return false;
#else
  // The old value is only used on Android when the cache stores only the keys.
  // Do not send old value on other platforms.
  // TODO(ssid): Clear this value when values are stored in the browser cache on
  // Android, crbug.com/743187.
  if (!map_->SetItem(key, value, &old_value))
    return false;
#endif  // !defined(OS_ANDROID)

  // Ignore mutations to 'key' until OnSetItemComplete.
  blink::WebScopedVirtualTimePauser virtual_time_pauser =
      main_thread_scheduler_->CreateWebScopedVirtualTimePauser(
          "DOMStorageCachedArea");
  virtual_time_pauser.PauseVirtualTime();
  ignore_key_mutations_[key]++;
  proxy_->SetItem(connection_id, key, value, old_value, page_url,
                  base::BindOnce(&DOMStorageCachedArea::OnSetItemComplete,
                                 weak_factory_.GetWeakPtr(), key,
                                 std::move(virtual_time_pauser)));
  return true;
}

void DOMStorageCachedArea::RemoveItem(int connection_id,
                                      const base::string16& key,
                                      const GURL& page_url) {
  PrimeIfNeeded(connection_id);
  base::string16 old_value;
#if !defined(OS_ANDROID)
  if (!map_->RemoveItem(key, nullptr))
    return;
#else
  // The old value is only used on Android when the cache stores only the keys.
  // Do not send old value on other platforms.
  if (!map_->RemoveItem(key, &old_value))
    return;
#endif

  // Ignore mutations to 'key' until OnRemoveItemComplete.
  blink::WebScopedVirtualTimePauser virtual_time_pauser =
      main_thread_scheduler_->CreateWebScopedVirtualTimePauser(
          "DOMStorageCachedArea");
  virtual_time_pauser.PauseVirtualTime();
  ignore_key_mutations_[key]++;
  proxy_->RemoveItem(connection_id, key,
                     base::NullableString16(old_value, false), page_url,
                     base::BindOnce(&DOMStorageCachedArea::OnRemoveItemComplete,
                                    weak_factory_.GetWeakPtr(), key,
                                    std::move(virtual_time_pauser)));
}

void DOMStorageCachedArea::Clear(int connection_id, const GURL& page_url) {
  // No need to prime the cache in this case.
  Reset();
  map_ = new DOMStorageMap(kPerStorageAreaQuota);

  // Ignore all mutations until OnClearComplete time.
  blink::WebScopedVirtualTimePauser virtual_time_pauser =
      main_thread_scheduler_->CreateWebScopedVirtualTimePauser(
          "DOMStorageCachedArea");
  virtual_time_pauser.PauseVirtualTime();
  ignore_all_mutations_ = true;
  proxy_->ClearArea(connection_id, page_url,
                    base::BindOnce(&DOMStorageCachedArea::OnClearComplete,
                                   weak_factory_.GetWeakPtr(),
                                   std::move(virtual_time_pauser)));
}

void DOMStorageCachedArea::ApplyMutation(
    const base::NullableString16& key,
    const base::NullableString16& new_value) {
  if (!map_.get() || ignore_all_mutations_)
    return;

  if (key.is_null()) {
    // It's a clear event.
    scoped_refptr<DOMStorageMap> old = map_;
    map_ = new DOMStorageMap(kPerStorageAreaQuota);

    // We have to retain local additions which happened after this
    // clear operation from another process.
    auto iter = ignore_key_mutations_.begin();
    while (iter != ignore_key_mutations_.end()) {
      base::NullableString16 value = old->GetItem(iter->first);
      if (!value.is_null()) {
        map_->SetItem(iter->first, value.string(), nullptr);
      }
      ++iter;
    }
    return;
  }

  // We have to retain local changes.
  if (should_ignore_key_mutation(key.string()))
    return;

  if (new_value.is_null()) {
    // It's a remove item event.
    map_->RemoveItem(key.string(), nullptr);
    return;
  }

  // It's a set item event.
  // We turn off quota checking here to accomodate the over budget
  // allowance that's provided in the browser process.
  map_->set_quota(std::numeric_limits<int32_t>::max());
  map_->SetItem(key.string(), new_value.string(), nullptr);
  map_->set_quota(kPerStorageAreaQuota);
}

void DOMStorageCachedArea::Prime(int connection_id) {
  DCHECK(!map_.get());

  // The LoadArea method is actually synchronous, but we have to
  // wait for an asyncly delivered message to know when incoming
  // mutation events should be applied. Our valuemap is plucked
  // from ipc stream out of order, mutations in front if it need
  // to be ignored.

  // Ignore all mutations until OnLoadComplete time.
  ignore_all_mutations_ = true;
  DOMStorageValuesMap values;
  base::TimeTicks before = base::TimeTicks::Now();
  proxy_->LoadArea(connection_id, &values,
                   base::BindOnce(&DOMStorageCachedArea::OnLoadComplete,
                                  weak_factory_.GetWeakPtr()));
  base::TimeDelta time_to_prime = base::TimeTicks::Now() - before;
  // Keeping this histogram named the same (without the ForRenderer suffix)
  // to maintain histogram continuity.
  UMA_HISTOGRAM_TIMES("LocalStorage.TimeToPrimeLocalStorage",
                      time_to_prime);
  map_ = new DOMStorageMap(kPerStorageAreaQuota);
  map_->SwapValues(&values);

  size_t local_storage_size_kb = map_->storage_used() / 1024;
  // Track localStorage size, from 0-6MB. Note that the maximum size should be
  // 5MB, but we add some slop since we want to make sure the max size is always
  // above what we see in practice, since histograms can't change.
  UMA_HISTOGRAM_CUSTOM_COUNTS("LocalStorage.RendererLocalStorageSizeInKB",
                              local_storage_size_kb,
                              1, 6 * 1024, 50);
  if (local_storage_size_kb < 100) {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.RendererTimeToPrimeLocalStorageUnder100KB",
        time_to_prime);
  } else if (local_storage_size_kb < 1000) {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.RendererTimeToPrimeLocalStorage100KBTo1MB",
        time_to_prime);
  } else {
    UMA_HISTOGRAM_TIMES(
        "LocalStorage.RendererTimeToPrimeLocalStorage1MBTo5MB",
        time_to_prime);
  }
}

void DOMStorageCachedArea::Reset() {
  map_ = nullptr;
  weak_factory_.InvalidateWeakPtrs();
  ignore_key_mutations_.clear();
  ignore_all_mutations_ = false;
}

void DOMStorageCachedArea::OnLoadComplete(bool success) {
  DCHECK(success);
  DCHECK(ignore_all_mutations_);
  ignore_all_mutations_ = false;
}

void DOMStorageCachedArea::OnSetItemComplete(const base::string16& key,
                                             blink::WebScopedVirtualTimePauser,
                                             bool success) {
  if (!success) {
    Reset();
    return;
  }
  auto found = ignore_key_mutations_.find(key);
  DCHECK(found != ignore_key_mutations_.end());
  if (--found->second == 0)
    ignore_key_mutations_.erase(found);
}

void DOMStorageCachedArea::OnRemoveItemComplete(
    const base::string16& key,
    blink::WebScopedVirtualTimePauser,
    bool success) {
  DCHECK(success);
  auto found = ignore_key_mutations_.find(key);
  DCHECK(found != ignore_key_mutations_.end());
  if (--found->second == 0)
    ignore_key_mutations_.erase(found);
}

void DOMStorageCachedArea::OnClearComplete(blink::WebScopedVirtualTimePauser,
                                           bool success) {
  DCHECK(success);
  DCHECK(ignore_all_mutations_);
  ignore_all_mutations_ = false;
}

}  // namespace content
