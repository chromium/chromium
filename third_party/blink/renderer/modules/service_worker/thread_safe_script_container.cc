// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/thread_safe_script_container.h"

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"

namespace blink {

ThreadSafeScriptContainer::RawScriptData::RawScriptData(
    const String& encoding,
    Vector<uint8_t> script_text,
    Vector<uint8_t> meta_data)
    : encoding_(encoding),
      script_text_(std::move(script_text)),
      meta_data_(std::move(meta_data)),
      headers_(std::make_unique<CrossThreadHTTPHeaderMapData>()) {}

ThreadSafeScriptContainer::RawScriptData::~RawScriptData() = default;

void ThreadSafeScriptContainer::RawScriptData::AddHeader(const String& key,
                                                         const String& value) {
  headers_->emplace_back(key, value);
}

ThreadSafeScriptContainer::ThreadSafeScriptContainer()
    : waiting_cv_(&lock_), are_all_data_added_(false) {}

void ThreadSafeScriptContainer::AddOnIOThread(
    const KURL& url,
    std::unique_ptr<RawScriptData> data) {
  base::AutoLock locker(lock_);
  DCHECK(!base::Contains(script_data_, url));
  ScriptStatus status = data ? ScriptStatus::kReceived : ScriptStatus::kFailed;
  script_data_.Set(url, std::make_pair(status, std::move(data)));
  if (url == waiting_url_)
    waiting_cv_.Signal();
}

ThreadSafeScriptContainer::ScriptStatus
ThreadSafeScriptContainer::GetStatusOnWorkerThread(const KURL& url) {
  base::AutoLock locker(lock_);
  auto it = script_data_.find(url);
  if (it == script_data_.end())
    return ScriptStatus::kPending;
  return it->value.first;
}

void ThreadSafeScriptContainer::ResetOnWorkerThread(const KURL& url) {
  base::AutoLock locker(lock_);
  script_data_.erase(url);
}

bool ThreadSafeScriptContainer::WaitOnWorkerThread(const KURL& url) {
  base::AutoLock locker(lock_);
  DCHECK(!waiting_url_.IsValid())
      << "The script container is unexpectedly shared among worker threads.";
  waiting_url_ = url;
  while (!base::Contains(script_data_, url)) {
    // If waiting script hasn't been added yet though all data are received,
    // that means something went wrong.
    if (are_all_data_added_) {
      waiting_url_ = KURL();
      return false;
    }
    // This is possible to be waken up spuriously, so that it's necessary to
    // check if the entry is really added.
    waiting_cv_.Wait();
  }
  waiting_url_ = KURL();
  return true;
}

std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>
ThreadSafeScriptContainer::TakeOnWorkerThread(const KURL& url) {
  base::AutoLock locker(lock_);
  auto it = script_data_.find(url);
  CHECK(it != script_data_.end(), base::NotFatalUntil::M130)
      << "Script should have been received before calling Take";
  std::pair<ScriptStatus, std::unique_ptr<RawScriptData>>& pair = it->value;
  DCHECK_EQ(ScriptStatus::kReceived, pair.first);
  pair.first = ScriptStatus::kTaken;
  return std::move(pair.second);
}

void ThreadSafeScriptContainer::OnAllDataAddedOnIOThread() {
  base::AutoLock locker(lock_);
  are_all_data_added_ = true;
  waiting_cv_.Broadcast();
}

ThreadSafeScriptContainer::~ThreadSafeScriptContainer() = default;

}  // namespace blink
