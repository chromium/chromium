// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/stream/stream_fs.h"

#include <errno.h>

#include "nacl_io/ossocket.h"
#include "nacl_io/pepper_interface.h"

namespace nacl_io {

void DispatchStart(void* work_ptr, int32_t val) {
  StreamFs::Work* work = static_cast<StreamFs::Work*>(work_ptr);

  // Delete if it fails to Start, Run will never get called.
  if (!work->Start(val))
    delete work;
}

void DispatchRun(void* work_ptr, int32_t val) {
  StreamFs::Work* work = static_cast<StreamFs::Work*>(work_ptr);

  work->Run(val);
  delete work;
}

void* StreamFs::StreamThreadThunk(void* fs_ptr) {
  StreamFs* filesystem = static_cast<StreamFs*>(fs_ptr);
  filesystem->StreamThread();
  return NULL;
}

// All work is done via completions callbacks from posted work.
void StreamFs::StreamThread() {
  {
    AUTO_LOCK(message_lock_)
    message_loop_ =
        ppapi_->GetMessageLoopInterface()->Create(ppapi()->GetInstance());
    ppapi_->GetMessageLoopInterface()->AttachToCurrentThread(message_loop_);
    pthread_cond_broadcast(&message_cond_);
  }

  // Run loop until Quit is posted.
  ppapi_->GetMessageLoopInterface()->Run(message_loop_);
}

PP_CompletionCallback StreamFs::GetStartCompletion(Work* work) {
  return PP_MakeCompletionCallback(DispatchStart, work);
}

PP_CompletionCallback StreamFs::GetRunCompletion(Work* work) {
  return PP_MakeCompletionCallback(DispatchRun, work);
}

// Place enqueue onto the socket thread.
void StreamFs::EnqueueWork(Work* work) {
  if (message_loop_ == 0) {
    AUTO_LOCK(message_lock_);

    if (message_loop_ == 0) {
      pthread_t thread;
      pthread_create(&thread, NULL, StreamThreadThunk, this);
    }

    while (message_loop_ == 0)
      pthread_cond_wait(&message_cond_, message_lock_.mutex());
  }

  PP_CompletionCallback cb = PP_MakeCompletionCallback(DispatchStart, work);
  ppapi_->GetMessageLoopInterface()->PostWork(message_loop_, cb, 0);
}

StreamFs::StreamFs() : message_loop_(0) {
  pthread_cond_init(&message_cond_, NULL);
}

StreamFs::~StreamFs() {
  if (message_loop_) {
    ppapi_->GetMessageLoopInterface()->PostQuit(message_loop_, PP_TRUE);
    ppapi_->ReleaseResource(message_loop_);
  }
  pthread_cond_destroy(&message_cond_);
}

Error StreamFs::OpenWithMode(const Path& path, int o_flags, mode_t mode,
                             ScopedNode* out_node) {
  return EACCES;
}

Error StreamFs::Unlink(const Path& path) {
  return EACCES;
}

Error StreamFs::Mkdir(const Path& path, int permissions) {
  return EACCES;
}

Error StreamFs::Rmdir(const Path& path) {
  return EACCES;
}

Error StreamFs::Remove(const Path& path) {
  return EACCES;
}

Error StreamFs::Rename(const Path& path, const Path& newpath) {
  return EACCES;
}

}  // namespace nacl_io
