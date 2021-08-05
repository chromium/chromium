// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_EVENT_EMITTER_H_
#define EXTENSIONS_RENDERER_BINDINGS_EVENT_EMITTER_H_

#include <map>
#include <vector>

#include "extensions/common/event_filtering_info.h"
#include "extensions/renderer/bindings/js_runner.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventListeners;
class ExceptionHandler;

// A gin::Wrappable Event object. One is expected to be created per event, per
// context. Note: this object *does not* clear any events, so it must be
// destroyed with the context to avoid leaking.
class EventEmitter final : public gin::Wrappable<EventEmitter> {
 public:
  EventEmitter(bool supports_filters,
               std::unique_ptr<APIEventListeners> listeners,
               ExceptionHandler* exception_handler);
  ~EventEmitter() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;
  const char* GetTypeName() final;

  // Fires the event to any listeners.
  // Warning: This can run arbitrary JS code, so the |context| may be
  // invalidated after this!
  void Fire(v8::Local<v8::Context> context,
            std::vector<v8::Local<v8::Value>>* args,
            const EventFilteringInfo* filter,
            JSRunner::ResultCallback callback);

  // Fires the event to any listeners synchronously, and returns the result.
  // This should only be used if the caller is certain that JS is already
  // running (i.e., is not blocked).
  // Warning: This can run arbitrary JS code, so the |context| may be
  // invalidated after this!
  v8::Local<v8::Value> FireSync(v8::Local<v8::Context> context,
                                std::vector<v8::Local<v8::Value>>* args,
                                const EventFilteringInfo* filter);

  // Removes all listeners and marks this object as invalid so that no more
  // are added.
  void Invalidate(v8::Local<v8::Context> context);

  // TODO(devlin): Consider making this a test-only method and exposing
  // HasListeners() instead.
  size_t GetNumListeners() const;

 private:
  // Bound methods for the Event JS object.
  void AddListener(gin::Arguments* arguments);
  void RemoveListener(gin::Arguments* arguments);
  bool HasListener(v8::Local<v8::Function> function);
  bool HasListeners();
  void Dispatch(gin::Arguments* arguments);

  // Dispatches an event synchronously to listeners, returning the result.
  v8::Local<v8::Value> DispatchSync(v8::Local<v8::Context> context,
                                    std::vector<v8::Local<v8::Value>>* args,
                                    const EventFilteringInfo* filter);

  // Dispatches an event asynchronously to listeners.
  void DispatchAsync(v8::Local<v8::Context> context,
                     std::vector<v8::Local<v8::Value>>* args,
                     const EventFilteringInfo* filter,
                     JSRunner::ResultCallback callback);
  static void DispatchAsyncHelper(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  // Whether or not this object is still valid; false upon context release.
  // When invalid, no listeners can be added or removed.
  bool valid_ = true;

  // Whether the event supports filters.
  bool supports_filters_ = false;

  std::unique_ptr<APIEventListeners> listeners_;

  // The associated exception handler; guaranteed to outlive this object.
  ExceptionHandler* const exception_handler_ = nullptr;

  // The next id to use in the pending_filters_ map.
  int next_filter_id_ = 0;
  // A constant to indicate an invalid id.
  static constexpr int kInvalidFilterId = -1;
  // The map of EventFilteringInfos for events that are pending dispatch (since
  // JS is suspended).
  std::map<int, EventFilteringInfo> pending_filters_;

  DISALLOW_COPY_AND_ASSIGN(EventEmitter);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_EVENT_EMITTER_H_
