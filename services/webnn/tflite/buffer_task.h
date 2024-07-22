// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_BUFFER_TASK_H_
#define SERVICES_WEBNN_TFLITE_BUFFER_TASK_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"

namespace webnn::tflite {

class BufferState;

// Represents a task performed against one or more BufferState instances.
class BufferTask : public base::RefCounted<BufferTask> {
 public:
  BufferTask(std::vector<scoped_refptr<BufferState>> shared_buffers,
             std::vector<scoped_refptr<BufferState>> exclusive_buffers,
             base::OnceCallback<void(base::OnceClosure)> task);

  // Checks if the require buffers can be locked. If so they are and
  // `task` is run immediately, otherwise this task is added to the
  // queues for each of the buffers and `task` will be run when they
  // can be locked.
  void Enqueue();

 private:
  friend class base::RefCounted<BufferTask>;

  ~BufferTask();

  bool CanExecute();
  void Execute(bool dequeue);
  void Complete();

  const std::vector<scoped_refptr<BufferState>> shared_buffers_;
  const std::vector<scoped_refptr<BufferState>> exclusive_buffers_;
  base::OnceCallback<void(base::OnceClosure)> task_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_BUFFER_TASK_H_
