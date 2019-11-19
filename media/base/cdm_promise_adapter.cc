// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_promise_adapter.h"

#include <utility>

namespace media {

CdmPromiseAdapter::CdmPromiseAdapter()
    : next_promise_id_(kInvalidPromiseId + 1) {}

CdmPromiseAdapter::~CdmPromiseAdapter() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DLOG_IF(WARNING, !promises_.empty()) << "There are unfulfilled promises";
  Clear();
}

uint32_t CdmPromiseAdapter::SavePromise(std::unique_ptr<CdmPromise> promise) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(kInvalidPromiseId, next_promise_id_);

  uint32_t promise_id = next_promise_id_++;
  if (next_promise_id_ == kInvalidPromiseId)
    next_promise_id_++;

  promises_[promise_id] = std::move(promise);
  return promise_id;
}

template <typename... T>
void CdmPromiseAdapter::ResolvePromise(uint32_t promise_id,
                                       const T&... result) {
  std::unique_ptr<CdmPromise> promise = TakePromise(promise_id);
  if (!promise) {
    LOG(ERROR) << "Promise not found for " << promise_id;
    return;
  }

  // Sanity check the type before we do static_cast.
  CdmPromise::ResolveParameterType type = promise->GetResolveParameterType();
  CdmPromise::ResolveParameterType expected = CdmPromiseTraits<T...>::kType;
  if (type != expected) {
    LOG(ERROR) << "Promise type mismatch: " << type << " vs " << expected;
    return;
  }

  static_cast<CdmPromiseTemplate<T...>*>(promise.get())->resolve(result...);
}

void CdmPromiseAdapter::RejectPromise(uint32_t promise_id,
                                      CdmPromise::Exception exception_code,
                                      uint32_t system_code,
                                      const std::string& error_message) {
  std::unique_ptr<CdmPromise> promise = TakePromise(promise_id);
  if (!promise) {
    LOG(ERROR) << "Promise not found for " << promise_id;
    return;
  }

  promise->reject(exception_code, system_code, error_message);
}

void CdmPromiseAdapter::Clear() {
  // Reject all outstanding promises.
  DCHECK(thread_checker_.CalledOnValidThread());
  for (auto& promise : promises_) {
    promise.second->reject(CdmPromise::Exception::INVALID_STATE_ERROR,
                           CdmPromise::SystemCode::kAborted,
                           "Operation aborted.");
  }
  promises_.clear();
}

std::unique_ptr<CdmPromise> CdmPromiseAdapter::TakePromise(
    uint32_t promise_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto it = promises_.find(promise_id);
  if (it == promises_.end())
    return nullptr;

  std::unique_ptr<CdmPromise> result = std::move(it->second);
  promises_.erase(it);
  return result;
}

// Explicit instantiation of function templates.
template MEDIA_EXPORT void CdmPromiseAdapter::ResolvePromise(uint32_t);
template MEDIA_EXPORT void CdmPromiseAdapter::ResolvePromise(
    uint32_t,
    const CdmKeyInformation::KeyStatus&);
template MEDIA_EXPORT void CdmPromiseAdapter::ResolvePromise(
    uint32_t,
    const std::string&);

}  // namespace media
