/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/quota/deprecated_storage_info.h"

#include "base/location.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/quota/deprecated_storage_quota.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

DeprecatedStorageInfo::DeprecatedStorageInfo() = default;

void DeprecatedStorageInfo::queryUsageAndQuota(
    ScriptState* script_state,
    int storage_type,
    V8StorageUsageCallback* success_callback,
    V8StorageErrorCallback* error_callback) {
  // The BlinkIDL definition for queryUsageAndQuota() already has a [Measure]
  // attribute, so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kQuotaRead);
  // Dispatching the request to DeprecatedStorageQuota, as this interface is
  // deprecated in favor of DeprecatedStorageQuota.
  DeprecatedStorageQuota* storage_quota = GetStorageQuota(storage_type);
  if (!storage_quota) {
    // Unknown storage type is requested.
    DeprecatedStorageQuota::EnqueueStorageErrorCallback(
        script_state, error_callback, DOMExceptionCode::kNotSupportedError);
    return;
  }
  storage_quota->queryUsageAndQuota(script_state, success_callback,
                                    error_callback);
}

void DeprecatedStorageInfo::requestQuota(
    ScriptState* script_state,
    int storage_type,
    uint64_t new_quota_in_bytes,
    V8StorageQuotaCallback* success_callback,
    V8StorageErrorCallback* error_callback) {
  // The BlinkIDL definition for requestQuota() already has a [Measure]
  // attribute, so the kQuotaRead use counter must be explicitly updated.
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kQuotaRead);
  // Dispatching the request to DeprecatedStorageQuota, as this interface is
  // deprecated in favor of DeprecatedStorageQuota.
  DeprecatedStorageQuota* storage_quota = GetStorageQuota(storage_type);
  if (!storage_quota) {
    // Unknown storage type is requested.
    DeprecatedStorageQuota::EnqueueStorageErrorCallback(
        script_state, error_callback, DOMExceptionCode::kNotSupportedError);
    return;
  }
  storage_quota->requestQuota(script_state, new_quota_in_bytes,
                              success_callback, error_callback);
}

DeprecatedStorageQuota* DeprecatedStorageInfo::GetStorageQuota(
    int storage_type) {
  switch (storage_type) {
    case kTemporary:
      if (!temporary_storage_) {
        temporary_storage_ = MakeGarbageCollected<DeprecatedStorageQuota>(
            DeprecatedStorageQuota::kTemporary);
      }
      return temporary_storage_.Get();
    case kPersistent:
      if (!persistent_storage_) {
        persistent_storage_ = MakeGarbageCollected<DeprecatedStorageQuota>(
            DeprecatedStorageQuota::kPersistent);
      }
      return persistent_storage_.Get();
  }
  return nullptr;
}

void DeprecatedStorageInfo::Trace(blink::Visitor* visitor) {
  visitor->Trace(temporary_storage_);
  visitor->Trace(persistent_storage_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
