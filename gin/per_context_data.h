// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_CONTEXT_DATA_H_
#define GIN_PER_CONTEXT_DATA_H_

#include <map>

#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "gin/gin_export.h"
#include "gin/public/wrapper_info.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/visitor.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace gin {

class ContextHolder;

// There is one instance of PerContextData per v8::Context managed by Gin. This
// class stores all the Gin-related data that varies per context. Arbitrary data
// can be associated with this class by way of the SupportsUserData methods.
// Instances of this class (and any associated user data) are destroyed before
// the associated v8::Context.
class GIN_EXPORT PerContextData
    : public cppgc::GarbageCollected<PerContextData>,
      public base::SupportsUserData {
 public:
  PerContextData(ContextHolder* context_holder,
                 v8::Local<v8::Context> context);
  PerContextData(const PerContextData&) = delete;
  PerContextData& operator=(const PerContextData&) = delete;
  ~PerContextData() override;

  void Trace(cppgc::Visitor* visitor) const;
  void Detach();

  // Can return NULL after the ContextHolder has detached from context.
  static PerContextData* From(v8::Local<v8::Context> context);

  void SetObjectTemplate(const WrapperInfo* info,
                         v8::Local<v8::ObjectTemplate> object_template);
  v8::Local<v8::ObjectTemplate> GetObjectTemplate(const WrapperInfo* info);

  ContextHolder* context_holder() { return context_holder_; }

 private:
  raw_ptr<ContextHolder, DanglingUntriaged> context_holder_;
  std::map<const WrapperInfo*, v8::Global<v8::ObjectTemplate>>
      object_templates_;
};

}  // namespace gin

#endif  // GIN_PER_CONTEXT_DATA_H_
