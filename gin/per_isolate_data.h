// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PER_ISOLATE_DATA_H_
#define GIN_PER_ISOLATE_DATA_H_

#include <map>
#include <memory>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "gin/gin_export.h"
#include "gin/public/isolate_holder.h"
#include "gin/public/wrapper_info.h"
#include "gin/v8_foreground_task_runner_base.h"
#include "v8/include/v8-array-buffer.h"
#include "v8/include/v8-forward.h"

namespace gin {

class V8IdleTaskRunner;
class IndexedPropertyInterceptor;
class NamedPropertyInterceptor;
class WrappableBase;

// There is one instance of PerIsolateData per v8::Isolate managed by Gin. This
// class stores all the Gin-related data that varies per isolate.
class GIN_EXPORT PerIsolateData {
 public:
  class DisposeObserver : public base::CheckedObserver {
   public:
    // Called just before the isolate is about to be disposed. The isolate will
    // be entered before the observer is notified, but there will not be a
    // handle scope by default.
    virtual void OnBeforeDispose(v8::Isolate* isolate) = 0;
    // Called just after the isolate has been disposed.
    virtual void OnDisposed() = 0;
  };

  PerIsolateData(
      v8::Isolate* isolate,
      v8::ArrayBuffer::Allocator* allocator,
      IsolateHolder::AccessMode access_mode,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> user_visible_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> best_effort_task_runner);
  PerIsolateData(const PerIsolateData&) = delete;
  PerIsolateData& operator=(const PerIsolateData&) = delete;
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

  void AddDisposeObserver(DisposeObserver* observer);
  void RemoveDisposeObserver(DisposeObserver* observer);
  void NotifyBeforeDispose();
  void NotifyDisposed();

  void EnableIdleTasks(std::unique_ptr<V8IdleTaskRunner> idle_task_runner);

  v8::Isolate* isolate() { return isolate_; }
  v8::ArrayBuffer::Allocator* allocator() { return allocator_; }
  std::shared_ptr<v8::TaskRunner> task_runner() { return task_runner_; }
  std::shared_ptr<v8::TaskRunner> user_visible_task_runner() {
    return user_visible_task_runner_;
  }
  std::shared_ptr<v8::TaskRunner> best_effort_task_runner() {
    return best_effort_task_runner_;
  }

 private:
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::ObjectTemplate> > ObjectTemplateMap;
  typedef std::map<
      WrapperInfo*, v8::Eternal<v8::FunctionTemplate> > FunctionTemplateMap;
  typedef std::map<WrappableBase*,
                   raw_ptr<IndexedPropertyInterceptor, CtnExperimental>>
      IndexedPropertyInterceptorMap;
  typedef std::map<WrappableBase*,
                   raw_ptr<NamedPropertyInterceptor, CtnExperimental>>
      NamedPropertyInterceptorMap;

  // PerIsolateData doesn't actually own |isolate_|. Instead, the isolate is
  // owned by the IsolateHolder, which also owns the PerIsolateData.
  raw_ptr<v8::Isolate, AcrossTasksDanglingUntriaged> isolate_;
  raw_ptr<v8::ArrayBuffer::Allocator, DanglingUntriaged> allocator_;
  ObjectTemplateMap object_templates_;
  FunctionTemplateMap function_templates_;
  IndexedPropertyInterceptorMap indexed_interceptors_;
  NamedPropertyInterceptorMap named_interceptors_;
  base::ObserverList<DisposeObserver> dispose_observers_;
  std::shared_ptr<V8ForegroundTaskRunnerBase> task_runner_;
  std::shared_ptr<V8ForegroundTaskRunnerBase> user_visible_task_runner_;
  std::shared_ptr<V8ForegroundTaskRunnerBase> best_effort_task_runner_;
};

}  // namespace gin

#endif  // GIN_PER_ISOLATE_DATA_H_
