// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SERIAL_RUNNER_H_
#define MEDIA_BASE_SERIAL_RUNNER_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_export.h"
#include "media/base/pipeline_status.h"

namespace media {

// Runs a series of bound functions accepting Closures or
// PipelineStatusCallback. SerialRunner doesn't use regular
// OnceClosure/PipelineStatusCallbacks as it late binds the completion callback
// as the series progresses.
class MEDIA_EXPORT SerialRunner {
 public:
  typedef base::OnceCallback<void(base::OnceClosure)> BoundClosure;
  typedef base::OnceCallback<void(PipelineStatusCallback)>
      BoundPipelineStatusCallback;

  // Serial queue of bound functions to run.
  class MEDIA_EXPORT Queue {
   public:
    Queue();
    Queue(Queue&& other);
    ~Queue();

    void Push(base::OnceClosure closure);
    void Push(BoundClosure bound_fn);
    void Push(BoundPipelineStatusCallback bound_fn);

   private:
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    friend class SerialRunner;

    BoundPipelineStatusCallback Pop();
    bool empty();

    base::circular_deque<BoundPipelineStatusCallback> bound_fns_;
  };

  // Executes the bound functions in series, executing |done_cb| when finished.
  //
  // All bound functions are executed on the thread that Run() is called on,
  // including |done_cb|.
  //
  // To eliminate an unnecessary posted task, the first function is executed
  // immediately on the caller's stack. It is *strongly advised* to ensure
  // the calling code does no more work after the call to Run().
  //
  // In all cases, |done_cb| is guaranteed to execute on a separate calling
  // stack.
  //
  // Deleting the object will prevent execution of any unstarted bound
  // functions, including |done_cb|.
  static std::unique_ptr<SerialRunner> Run(Queue&& bound_fns,
                                           PipelineStatusCallback done_cb);

  SerialRunner(const SerialRunner&) = delete;
  SerialRunner& operator=(const SerialRunner&) = delete;

 private:
  friend std::default_delete<SerialRunner>;

  SerialRunner(Queue&& bound_fns, PipelineStatusCallback done_cb);
  ~SerialRunner();

  void RunNextInSeries(PipelineStatus last_status);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  Queue bound_fns_;
  PipelineStatusCallback done_cb_;

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<SerialRunner> weak_factory_{this};
};

}  // namespace media

#endif  // MEDIA_BASE_SERIAL_RUNNER_H_
