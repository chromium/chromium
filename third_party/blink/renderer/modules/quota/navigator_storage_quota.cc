/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/quota/navigator_storage_quota.h"

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/deprecation/deprecation.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/navigator.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/modules/quota/deprecated_storage_quota.h"
#include "third_party/blink/renderer/modules/quota/storage_manager.h"

namespace blink {

NavigatorStorageQuota::NavigatorStorageQuota(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator) {}

const char NavigatorStorageQuota::kSupplementName[] = "NavigatorStorageQuota";

NavigatorStorageQuota& NavigatorStorageQuota::From(NavigatorBase& navigator) {
  NavigatorStorageQuota* supplement =
      Supplement<NavigatorBase>::From<NavigatorStorageQuota>(navigator);
  if (!supplement) {
    supplement = MakeGarbageCollected<NavigatorStorageQuota>(navigator);
    ProvideTo(navigator, supplement);
  }
  return *supplement;
}

DeprecatedStorageQuota* NavigatorStorageQuota::webkitTemporaryStorage(
    Navigator& navigator) {
  NavigatorStorageQuota& navigator_storage = From(navigator);
  if (!navigator_storage.temporary_storage_) {
    navigator_storage.temporary_storage_ =
        MakeGarbageCollected<DeprecatedStorageQuota>(navigator.DomWindow());
  }

  // Record metrics for usage in third-party contexts.
  if (navigator.DomWindow()) {
    navigator.DomWindow()->CountUseOnlyInCrossSiteIframe(
        WebFeature::kPrefixedStorageQuotaThirdPartyContext);
  }

  return navigator_storage.temporary_storage_.Get();
}

DeprecatedStorageQuota* NavigatorStorageQuota::webkitPersistentStorage(
    Navigator& navigator) {
  // Show deprecation message and record usage for persistent storage type.
  if (navigator.DomWindow()) {
    Deprecation::CountDeprecation(navigator.DomWindow(),
                                  WebFeature::kPersistentQuotaType);
  }
  // Persistent quota type is deprecated as of crbug.com/1233525.
  return webkitTemporaryStorage(navigator);
}

StorageManager* NavigatorStorageQuota::storage(NavigatorBase& navigator) {
  NavigatorStorageQuota& navigator_storage = From(navigator);
  if (!navigator_storage.storage_manager_) {
    navigator_storage.storage_manager_ =
        MakeGarbageCollected<StorageManager>(navigator.GetExecutionContext());
  }
  return navigator_storage.storage_manager_.Get();
}

void NavigatorStorageQuota::Trace(Visitor* visitor) const {
  visitor->Trace(temporary_storage_);
  visitor->Trace(storage_manager_);
  Supplement<NavigatorBase>::Trace(visitor);
}

}  // namespace blink
