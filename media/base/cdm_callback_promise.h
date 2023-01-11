// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_CALLBACK_PROMISE_H_
#define MEDIA_BASE_CDM_CALLBACK_PROMISE_H_

#include <stdint.h>

#include <string>

#include "base/functional/callback.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_export.h"

namespace media {

typedef base::OnceCallback<void(CdmPromise::Exception exception_code,
                                uint32_t system_code,
                                const std::string& error_message)>
    PromiseRejectedCB;

template <typename... T>
class MEDIA_EXPORT CdmCallbackPromise : public CdmPromiseTemplate<T...> {
 public:
  CdmCallbackPromise(base::OnceCallback<void(const T&...)> resolve_cb,
                     PromiseRejectedCB reject_cb);

  CdmCallbackPromise(const CdmCallbackPromise&) = delete;
  CdmCallbackPromise& operator=(const CdmCallbackPromise&) = delete;

  virtual ~CdmCallbackPromise();

  // CdmPromiseTemplate<T> implementation.
  virtual void resolve(const T&... result) override;
  virtual void reject(CdmPromise::Exception exception_code,
                      uint32_t system_code,
                      const std::string& error_message) override;

 private:
  using CdmPromiseTemplate<T...>::IsPromiseSettled;
  using CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  base::OnceCallback<void(const T&...)> resolve_cb_;
  PromiseRejectedCB reject_cb_;
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_CALLBACK_PROMISE_H_
