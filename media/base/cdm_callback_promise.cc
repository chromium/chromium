// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_callback_promise.h"

#include "base/check.h"
#include "base/functional/callback_helpers.h"

namespace media {

template <typename... T>
CdmCallbackPromise<T...>::CdmCallbackPromise(
    base::OnceCallback<void(const T&...)> resolve_cb,
    PromiseRejectedCB reject_cb)
    : resolve_cb_(std::move(resolve_cb)), reject_cb_(std::move(reject_cb)) {
  DCHECK(resolve_cb_);
  DCHECK(reject_cb_);
}

template <typename... T>
CdmCallbackPromise<T...>::~CdmCallbackPromise() {
  if (IsPromiseSettled())
    return;

  DCHECK(resolve_cb_ && reject_cb_);
  RejectPromiseOnDestruction();
}

template <typename... T>
void CdmCallbackPromise<T...>::resolve(const T&... result) {
  MarkPromiseSettled();
  std::move(resolve_cb_).Run(result...);
}

template <typename... T>
void CdmCallbackPromise<T...>::reject(CdmPromise::Exception exception_code,
                                      uint32_t system_code,
                                      const std::string& error_message) {
  MarkPromiseSettled();
  std::move(reject_cb_).Run(exception_code, system_code, error_message);
}

// Explicit template instantiation for the Promises needed.
template class MEDIA_EXPORT CdmCallbackPromise<>;
template class MEDIA_EXPORT CdmCallbackPromise<std::string>;

}  // namespace media
