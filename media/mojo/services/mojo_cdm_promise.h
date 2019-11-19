// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_

#include <stdint.h>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/cdm_promise.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"

namespace media {

// media::CdmPromiseTemplate implementations backed by base::Callbacks.
// TODO(xhwang): We need a new type F to solve the issue where parameters in the
// callback can be passed in by value or as const-refs. Find a better solution
// to handle this.
template <typename F, typename... T>
class MojoCdmPromise : public CdmPromiseTemplate<T...> {
 public:
  using CallbackType = base::OnceCallback<F>;

  explicit MojoCdmPromise(CallbackType callback);
  ~MojoCdmPromise() final;

  // CdmPromiseTemplate<> implementation.

  void resolve(const T&... result) final;
  void reject(CdmPromise::Exception exception,
              uint32_t system_code,
              const std::string& error_message) final;

 private:
  using CdmPromiseTemplate<T...>::IsPromiseSettled;
  using CdmPromiseTemplate<T...>::MarkPromiseSettled;
  using CdmPromiseTemplate<T...>::RejectPromiseOnDestruction;

  CallbackType callback_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_PROMISE_H_
