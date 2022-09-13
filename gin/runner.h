// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_RUNNER_H_
#define GIN_RUNNER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "gin/gin_export.h"
#include "gin/public/context_holder.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-isolate.h"

namespace gin {

// Runner is responsible for running code in a v8::Context.
class GIN_EXPORT Runner {
 public:
  Runner();
  Runner(const Runner&) = delete;
  Runner& operator=(const Runner&) = delete;
  virtual ~Runner();

  // Before running script in this context, you'll need to enter the runner's
  // context by creating an instance of Runner::Scope on the stack.
  virtual v8::MaybeLocal<v8::Value> Run(const std::string& source,
                                        const std::string& resource_name) = 0;
  virtual ContextHolder* GetContextHolder() = 0;

  v8::Local<v8::Object> global() {
    return GetContextHolder()->context()->Global();
  }

  // Useful for running script in this context asynchronously. Rather than
  // holding a raw pointer to the runner, consider holding a WeakPtr.
  base::WeakPtr<Runner> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  class GIN_EXPORT Scope {
   public:
    explicit Scope(Runner* runner);
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
    ~Scope();

   private:
    v8::Isolate::Scope isolate_scope_;
    v8::HandleScope handle_scope_;
    v8::Context::Scope scope_;
  };

 private:
  friend class Scope;

  base::WeakPtrFactory<Runner> weak_factory_{this};
};

}  // namespace gin

#endif  // GIN_RUNNER_H_
