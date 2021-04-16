// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_CONTEXT_DATA_H_
#define GIN_PER_CONTEXT_DATA_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "gin/gin_export.h"
#include "v8/include/v8.h"

namespace gin {

class ContextHolder;
class Runner;

// There is one instance of PerContextData per v8::Context managed by Gin. This
// class stores all the Gin-related data that varies per context. Arbitrary data
// can be associated with this class by way of the SupportsUserData methods.
// Instances of this class (and any associated user data) are destroyed before
// the associated v8::Context.
class GIN_EXPORT PerContextData : public base::SupportsUserData {
 public:
  PerContextData(ContextHolder* context_holder,
                 v8::Local<v8::Context> context);
  ~PerContextData() override;

  // Can return NULL after the ContextHolder has detached from context.
  static PerContextData* From(v8::Local<v8::Context> context);

  // The Runner associated with this context. To execute script in this context,
  // please use the appropriate API on Runner.
  Runner* runner() const { return runner_; }
  void set_runner(Runner* runner) { runner_ = runner; }

  ContextHolder* context_holder() { return context_holder_; }

 private:
  ContextHolder* context_holder_;
  Runner* runner_;

  DISALLOW_COPY_AND_ASSIGN(PerContextData);
};

}  // namespace gin

#endif  // GIN_PER_CONTEXT_DATA_H_
