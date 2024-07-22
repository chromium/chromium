// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_TFLITE_BUFFER_STATE_H_
#define SERVICES_WEBNN_TFLITE_BUFFER_STATE_H_

#include <queue>

#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"

namespace webnn::tflite {

class BufferContent;
class BufferTask;

// The state of an MLBuffer. This class is reference counted so that operations
// that are in progress can keep the buffers they are using alive until they
// are complete.
//
// This class may not be passed between threads. Use the underlying
// `BufferContent` instance for that.
class BufferState : public base::RefCounted<BufferState> {
 public:
  explicit BufferState(size_t size);

  BufferState(const BufferState&) = delete;
  BufferState& operator=(const BufferState&) = delete;

  bool CanLock(bool exclusive) const;
  void Lock(bool exclusive);
  void Unlock();

  void EnqueueTask(scoped_refptr<BufferTask> task);
  BufferTask* PeekTask() const;
  scoped_refptr<BufferTask> PopTask();

  const scoped_refptr<BufferContent>& GetContent() const;

 private:
  friend class base::RefCounted<BufferState>;

  enum class State {
    kUnlocked,
    kLockedShared,
    kLockedExclusive,
  };

  ~BufferState();

  const scoped_refptr<BufferContent> content_;
  State state_ = State::kUnlocked;
  std::queue<scoped_refptr<BufferTask>> waiting_tasks_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace webnn::tflite

#endif  // SERVICES_WEBNN_TFLITE_BUFFER_STATE_H_
