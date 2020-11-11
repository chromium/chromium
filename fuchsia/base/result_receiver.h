// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_RESULT_RECEIVER_H_
#define FUCHSIA_BASE_RESULT_RECEIVER_H_

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/optional.h"

namespace cr_fuchsia {

// Helper for capturing a "result" of an asynchronous operation that is passed
// via a Callback<ResultType>, for later retrieval. An optional Closure may also
// be configured, to be called when the result is captured.
//
// This is primarily useful for tests, which use a ResultReceiver<> configured
// with a RunLoop::QuitClosure() to pump a message-loop until an asynchronous
// operation has completed.
template <typename T>
class ResultReceiver {
 public:
  ResultReceiver() : on_result_received_() {}
  explicit ResultReceiver(base::RepeatingClosure on_result_received)
      : on_result_received_(std::move(on_result_received)) {}

  ~ResultReceiver() = default;

  // Returns a OnceCallback which will receive and store a result value.
  base::OnceCallback<void(T)> GetReceiveCallback() {
    return base::BindOnce(&ResultReceiver<T>::ReceiveResult,
                          base::Unretained(this));
  }

  void ReceiveResult(T result) {
    DCHECK(!result_.has_value());
    result_ = std::move(result);
    if (on_result_received_)
      on_result_received_.Run();
  }

  bool has_value() const { return result_.has_value(); }

  T& operator*() {
    DCHECK(result_.has_value());
    return *result_;
  }

  T* operator->() {
    DCHECK(result_.has_value());
    return &*result_;
  }

 private:
  base::Optional<T> result_;
  const base::RepeatingClosure on_result_received_;

  DISALLOW_COPY_AND_ASSIGN(ResultReceiver<T>);
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_RESULT_RECEIVER_H_
