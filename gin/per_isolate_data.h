// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_ISOLATE_DATA_H_
#define GIN_PER_ISOLATE_DATA_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "gin/gin_export.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/wrapper_info.h"
#include "gin/v8_foreground_task_runner_base.h"
#include "v8/include/v8.h"

namespace gin {

class V8IdleTaskRunner;
class IndexedPropertyInterceptor;
class NamedPropertyInterceptor;
class WrappableBase;

// There is one instance of PerIsolateData per v8::Isolate managed by Gin. This
// class stores all the Gin-related data that varies per isolate.
class GIN_EXPORT PerIsolateData {
 public:
  PerIsolateData(v8::Isolate* isolate,
                 v8::ArrayBuffer::Allocator* allocator,
                 IsolateHolder::AccessMode access_mode,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  ~PerIsolateData();

  static PerIsolateData* From(v8::Isolate* isolate);

  // Each isolate is associated with a collection of v8::ObjectTemplates and
  // v8::FunctionTemplates. Typically these template objects are created
  // lazily.
  void SetObjectTemplate(WrapperInfo* info,
                         v8::Local<v8::ObjectTemplate> object_template);
  void SetFunctionTemplate(WrapperInfo* info,
                           v8::Local<v8::FunctionTemplate> function_template);

  // These are low-level functions for retrieving object or function templates
  // stored in this object. Because these templates are often created lazily,
  // most clients should call higher-level functions that know how to populate
  // these templates if they haven't already been created.
  v8::Local<v8::ObjectTemplate> GetObjectTemplate(WrapperInfo* info);
  v8::Local<v8::FunctionTemplate> GetFunctionTemplate(WrapperInfo* info);

  // We maintain a map from Wrappable objects that derive from one of the
  // interceptor interfaces to the interceptor interface pointers.
  void SetIndexedPropertyInterceptor(WrappableBase* base,
                                     IndexedPropertyInterceptor* interceptor);
  void SetNamedPropertyInterceptor(WrappableBase* base,
                                   NamedPropertyInterceptor* interceptor);

  void ClearIndexedPropertyInterceptor(WrappableBase* base,
                                       IndexedPropertyInterceptor* interceptor);
  void ClearNamedPropertyInterceptor(WrappableBase* base,
                                     NamedPropertyInterceptor* interceptor);

  IndexedPropertyInterceptor* GetIndexedPropertyInterceptor(
      WrappableBase* base);
  NamedPropertyInterceptor* GetNamedPropertyInterceptor(WrappableBase* base);

  void EnableIdleTasks(std::unique_ptr<V8IdleTaskRunner> idle_task_runner);

  v8::Isolate* isolate() { return isolate_; }
  v8::ArrayBuffer::Allocator* allocator() { return allocator_; }
  std::shared_ptr<v8::TaskRunner> task_runner() { return task_runner_; }

 private:
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::ObjectTemplate> > ObjectTemplateMap;
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::FunctionTemplate> > FunctionTemplateMap;
  typedef std::map<WrappableBase*, IndexedPropertyInterceptor*>
      IndexedPropertyInterceptorMap;
  typedef std::map<WrappableBase*, NamedPropertyInterceptor*>
      NamedPropertyInterceptorMap;

  // PerIsolateData doesn't actually own |isolate_|. Instead, the isolate is
  // owned by the IsolateHolder, which also owns the PerIsolateData.
  v8::Isolate* isolate_;
  v8::ArrayBuffer::Allocator* allocator_;
  ObjectTemplateMap object_templates_;
  FunctionTemplateMap function_templates_;
  IndexedPropertyInterceptorMap indexed_interceptors_;
  NamedPropertyInterceptorMap named_interceptors_;
  std::shared_ptr<V8ForegroundTaskRunnerBase> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(PerIsolateData);
};

}  // namespace gin

#endif  // GIN_PER_ISOLATE_DATA_H_
