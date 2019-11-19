// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/task_traits.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/browser/tracing_delegate.h"

namespace content {

class TracingController;

// TracingController is used on the browser processes to enable/disable
// tracing status and collect trace data. Only the browser UI thread is allowed
// to interact with the TracingController object. All callbacks are called on
// the UI thread.
class TracingController {
 public:
  CONTENT_EXPORT static TracingController* GetInstance();

  // An interface for trace data consumer. An implementation of this interface
  // is passed to StopTracing() and receives the trace data followed by a
  // notification that all child processes have completed tracing and the data
  // collection is over. All methods are called on the UI thread.
  // ReceiveTraceFinalContents method will be called exactly once and no methods
  // will be called after that.
  class CONTENT_EXPORT TraceDataEndpoint
      : public base::RefCountedThreadSafe<TraceDataEndpoint> {
   public:
    virtual void ReceiveTraceChunk(std::unique_ptr<std::string> chunk) = 0;
    virtual void ReceivedTraceFinalContents() = 0;

   protected:
    friend class base::RefCountedThreadSafe<TraceDataEndpoint>;
    virtual ~TraceDataEndpoint() {}
  };

  // Create a trace endpoint that may be supplied to StopTracing
  // to capture the trace data as a string.
  using CompletionCallback =
      base::OnceCallback<void(std::unique_ptr<std::string>)>;
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateStringEndpoint(
      CompletionCallback callback);

  // Create a trace endpoint that may be supplied to StopTracing
  // to dump the trace data to a file.
  CONTENT_EXPORT static scoped_refptr<TraceDataEndpoint> CreateFileEndpoint(
      const base::FilePath& file_path,
      base::OnceClosure callback,
      base::TaskPriority write_priority = base::TaskPriority::BEST_EFFORT);

  // Get a set of category groups. The category groups can change as
  // new code paths are reached.
  //
  // Once all child processes have acked to the GetCategories request,
  // GetCategoriesDoneCallback is called back with a set of category
  // groups.
  typedef base::OnceCallback<void(const std::set<std::string>&)>
      GetCategoriesDoneCallback;
  virtual bool GetCategories(GetCategoriesDoneCallback callback) = 0;

  // Start tracing (recording traces) on all processes.
  //
  // Tracing begins immediately locally, and asynchronously on child processes
  // as soon as they receive the StartTracing request.
  //
  // Once all child processes have acked to the StartTracing request,
  // StartTracingDoneCallback will be called back.
  //
  // |category_filter| is a filter to control what category groups should be
  // traced. A filter can have an optional '-' prefix to exclude category groups
  // that contain a matching category. Having both included and excluded
  // category patterns in the same list would not be supported.
  //
  // Examples: "test_MyTest*",
  //           "test_MyTest*,test_OtherStuff",
  //           "-excluded_category1,-excluded_category2"
  //
  // |trace_config| controls what kind of tracing is enabled.
  typedef base::OnceCallback<void()> StartTracingDoneCallback;
  virtual bool StartTracing(const base::trace_event::TraceConfig& trace_config,
                            StartTracingDoneCallback callback) = 0;

  // Stop tracing (recording traces) on all processes.
  //
  // Child processes typically are caching trace data and only rarely flush
  // and send trace data back to the browser process. That is because it may be
  // an expensive operation to send the trace data over IPC, and we would like
  // to avoid much runtime overhead of tracing. So, to end tracing, we must
  // asynchronously ask all child processes to flush any pending trace data.
  //
  // Once all child processes have acked to the StopTracing request,
  // TracingFileResultCallback will be called back with a file that contains
  // the traced data.
  //
  // If |trace_data_endpoint| is not null, it will receive chunks of trace data
  // as JSON-stringified events, followed by a notification that the trace
  // collection is finished.
  //
  virtual bool StopTracing(
      const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint) = 0;
  virtual bool StopTracing(
      const scoped_refptr<TraceDataEndpoint>& trace_data_endpoint,
      const std::string& agent_label,
      bool privacy_filtering_enabled = false) = 0;

  // Get the maximum across processes of trace buffer percent full state.
  // When the TraceBufferUsage value is determined, the callback is
  // called.
  typedef base::OnceCallback<void(float, size_t)> GetTraceBufferUsageCallback;
  virtual bool GetTraceBufferUsage(GetTraceBufferUsageCallback callback) = 0;

  // Check if the tracing system is tracing
  virtual bool IsTracing() = 0;

 protected:
  virtual ~TracingController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_TRACING_CONTROLLER_H_
