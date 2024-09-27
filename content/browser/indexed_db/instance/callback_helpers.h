// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INSTANCE_CALLBACK_HELPERS_H_
#define CONTENT_BROWSER_INDEXED_DB_INSTANCE_CALLBACK_HELPERS_H_

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/instance/transaction.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/leveldatabase/env_chromium.h"

// Since functions in this file use templates, they must be in a header file
// and can't be placed in a definition file.  Please ensure any including file
// is a definition file, itself, and never include this into a header file.

namespace content::indexed_db {
namespace indexed_db_callback_helpers_internal {

template <typename T>
Status InvokeOrSucceed(base::WeakPtr<T> ptr,
                       Transaction::Operation operation,
                       Transaction* transaction) {
  if (ptr) {
    return std::move(operation).Run(transaction);
  }
  return Status::OK();
}

template <typename R>
R AbortCallback(base::WeakPtr<Transaction> transaction) {
  if (transaction) {
    transaction->IncrementNumErrorsSent();
  }
  DatabaseError error(blink::mojom::IDBException::kIgnorableAbortError,
                      "Backend aborted error");
  return R::Struct::NewErrorResult(
      blink::mojom::IDBError::New(error.code(), error.message()));
}

template <typename R>
base::OnceCallback<R()> CreateAbortCallback(
    base::WeakPtr<Transaction> transaction) {
  return base::BindOnce(&AbortCallback<R>, std::move(transaction));
}

// CallbackAbortOnDestruct wraps a callback in a class with a destructor that
// invokes that callback.  When the CallbackAbortOnDestruct is instantiated,
// it expects a separate callback that, when called at destruct, will return
// the arguments that should be passed to the wrapped callback.
//
// This class is loosely based on //mojo/public/cpp/bindings/callback_helpers.h
// WrapCallbackWithDefaultInvokeIfNotRun.  The difference is that the destructor
// calls |callback_| with args it gets on destruct rather than using static args
// given when the wrapper is created.
template <typename T, typename R>
class CallbackAbortOnDestruct {
 public:
  CallbackAbortOnDestruct(T callback, base::WeakPtr<Transaction> transaction)
      : callback_(std::move(callback)),
        args_at_destroy_(CreateAbortCallback<R>(transaction)),
        called_(false) {}

  CallbackAbortOnDestruct(const CallbackAbortOnDestruct&) = delete;
  CallbackAbortOnDestruct& operator=(const CallbackAbortOnDestruct&) = delete;

  ~CallbackAbortOnDestruct() {
    if (called_) {
      return;
    }
    R args = std::move(args_at_destroy_).Run();
    std::move(callback_).Run(std::move(args));
  }

  void Run(R ptr) {
    called_ = true;
    std::move(callback_).Run(std::move(ptr));
  }

 private:
  T callback_;
  base::OnceCallback<R()> args_at_destroy_;
  bool called_;
};

}  //  namespace indexed_db_callback_helpers_internal

// CreateCallbackAbortOnDestruct is a helper function to create an instance
// of CallbackAbortOnDestruct that returns a callback.  By using this helper
// function, the wrapping callback can exist with the same type signature as
// the wrapped callback.
template <typename T, typename R>
T CreateCallbackAbortOnDestruct(T cb, base::WeakPtr<Transaction> transaction) {
  return base::BindOnce(
      &indexed_db_callback_helpers_internal::CallbackAbortOnDestruct<T, R>::Run,
      std::make_unique<
          indexed_db_callback_helpers_internal::CallbackAbortOnDestruct<T, R>>(
          std::move(cb), std::move(transaction)));
}

// This allows us to bind a function with a return value to a weak ptr, and if
// the weak pointer is invalidated then we just return a default (success).
template <typename T, typename Functor, typename... Args>
Transaction::Operation BindWeakOperation(Functor&& functor,
                                         base::WeakPtr<T> weak_ptr,
                                         Args&&... args) {
  DCHECK(weak_ptr);
  T* ptr = weak_ptr.get();
  return base::BindOnce(
      &indexed_db_callback_helpers_internal::InvokeOrSucceed<T>,
      std::move(weak_ptr),
      base::BindOnce(std::forward<Functor>(functor), base::Unretained(ptr),
                     std::forward<Args>(args)...));
}

}  // namespace content::indexed_db

#endif  // CONTENT_BROWSER_INDEXED_DB_INSTANCE_CALLBACK_HELPERS_H_
