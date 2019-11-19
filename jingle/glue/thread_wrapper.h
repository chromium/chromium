// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_THREAD_WRAPPER_H_
#define JINGLE_GLUE_THREAD_WRAPPER_H_

#include <stdint.h>

#include <list>
#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop_current.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "third_party/webrtc/rtc_base/thread.h"

namespace jingle_glue {

// JingleThreadWrapper implements rtc::Thread interface on top of
// Chromium's SingleThreadTaskRunner interface. Currently only the bare minimum
// that is used by P2P part of libjingle is implemented. There are two ways to
// create this object:
//
// - Call EnsureForCurrentMessageLoop(). This approach works only on threads
//   that have MessageLoop In this case JingleThreadWrapper deletes itself
//   automatically when MessageLoop is destroyed.
// - Using JingleThreadWrapper() constructor. In this case the creating code
//   must pass a valid task runner for the current thread and also delete the
//   wrapper later.
class JingleThreadWrapper
    : public base::MessageLoopCurrent::DestructionObserver,
      public rtc::Thread {
 public:
  // Create JingleThreadWrapper for the current thread if it hasn't been created
  // yet. The thread wrapper is destroyed automatically when the current
  // MessageLoop is destroyed.
  static void EnsureForCurrentMessageLoop();

  // Creates JingleThreadWrapper for |task_runner| that runs tasks on the
  // current thread.
  static std::unique_ptr<JingleThreadWrapper> WrapTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // Returns thread wrapper for the current thread or nullptr if it doesn't
  // exist.
  static JingleThreadWrapper* current();

  ~JingleThreadWrapper() override;

  // Sets whether the thread can be used to send messages
  // synchronously to another thread using Send() method. Set to false
  // by default to avoid potential jankiness when Send() used on
  // renderer thread. It should be set explicitly for threads that
  // need to call Send() for other threads.
  void set_send_allowed(bool allowed) { send_allowed_ = allowed; }

  // MessageLoopCurrent::DestructionObserver implementation.
  void WillDestroyCurrentMessageLoop() override;

  // rtc::MessageQueue overrides.
  void Post(const rtc::Location& posted_from,
            rtc::MessageHandler* phandler,
            uint32_t id,
            rtc::MessageData* pdata,
            bool time_sensitive) override;
  void PostDelayed(const rtc::Location& posted_from,
                   int delay_ms,
                   rtc::MessageHandler* handler,
                   uint32_t id,
                   rtc::MessageData* data) override;
  void Clear(rtc::MessageHandler* handler,
             uint32_t id,
             rtc::MessageList* removed) override;
  void Dispatch(rtc::Message* message) override;
  void Send(const rtc::Location& posted_from,
            rtc::MessageHandler* handler,
            uint32_t id,
            rtc::MessageData* data) override;

  // Quitting is not supported (see below); this method performs
  // NOTIMPLEMENTED_LOG_ONCE() and returns false.
  // TODO(https://crbug.com/webrtc/10364): When rtc::MessageQueue::Post()
  // returns a bool, !IsQuitting() will not be needed to infer success and we
  // may implement this as NOTREACHED() like the rest of the methods.
  bool IsQuitting() override;
  // Following methods are not supported. They are overriden just to
  // ensure that they are not called (each of them contain NOTREACHED
  // in the body). Some of this methods can be implemented if it
  // becomes neccessary to use libjingle code that calls them.
  void Quit() override;
  void Restart() override;
  bool Get(rtc::Message* message, int delay_ms, bool process_io) override;
  bool Peek(rtc::Message* message, int delay_ms) override;
  void PostAt(const rtc::Location& posted_from,
              uint32_t timestamp,
              rtc::MessageHandler* handler,
              uint32_t id,
              rtc::MessageData* data) override;
  void ReceiveSends() override;
  int GetDelay() override;

  // rtc::Thread overrides.
  void Stop() override;
  void Run() override;

 private:
  typedef std::map<int, rtc::Message> MessagesQueue;
  struct PendingSend;

  explicit JingleThreadWrapper(
     scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  void PostTaskInternal(const rtc::Location& posted_from,
                        int delay_ms,
                        rtc::MessageHandler* handler,
                        uint32_t message_id,
                        rtc::MessageData* data);
  void RunTask(int task_id);
  void ProcessPendingSends();

  // Task runner used to execute messages posted on this thread.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  bool send_allowed_;

  // |lock_| must be locked when accessing |messages_|.
  base::Lock lock_;
  int last_task_id_;
  MessagesQueue messages_;
  std::list<PendingSend*> pending_send_messages_;
  base::WaitableEvent pending_send_event_;

  base::WeakPtr<JingleThreadWrapper> weak_ptr_;
  base::WeakPtrFactory<JingleThreadWrapper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JingleThreadWrapper);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_THREAD_WRAPPER_H_
