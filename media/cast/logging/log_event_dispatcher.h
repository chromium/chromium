// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_
#define MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media::cast {

// A thread-safe receiver of logging events that manages an active list of
// EventSubscribers and dispatches the logging events to them on `task_runner`.
// All methods, constructor, and destructor can be invoked on any thread.
class LogEventDispatcher {
 public:
  explicit LogEventDispatcher(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::OnceClosure deletion_cb);

  LogEventDispatcher(const LogEventDispatcher&) = delete;
  LogEventDispatcher(LogEventDispatcher&&) = delete;
  LogEventDispatcher& operator=(const LogEventDispatcher&) = delete;
  LogEventDispatcher& operator=(LogEventDispatcher&&) = delete;

  ~LogEventDispatcher();

  // Called on any thread to schedule the sending of event(s) to all
  // EventSubscribers on the MAIN thread.
  void DispatchFrameEvent(std::unique_ptr<FrameEvent> event) const;
  void DispatchPacketEvent(std::unique_ptr<PacketEvent> event) const;
  void DispatchBatchOfEvents(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) const;

  // Adds `subscriber` to the active list to begin receiving events on MAIN
  // thread.  Unsubscribe() must be called before `subscriber` is destroyed.
  void Subscribe(RawEventSubscriber* subscriber);

  // Removes `subscriber` from the active list.  Once this method returns, the
  // `subscriber` is guaranteed not to receive any more events.
  void Unsubscribe(RawEventSubscriber* subscriber);

 private:
  // The part of the implementation that runs exclusively on the MAIN thread.
  class Impl {
   public:
    explicit Impl(base::OnceClosure deletion_cb);

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;
    ~Impl();

    void DispatchFrameEvent(std::unique_ptr<FrameEvent> event) const;
    void DispatchPacketEvent(std::unique_ptr<PacketEvent> event) const;
    void DispatchBatchOfEvents(
        std::unique_ptr<std::vector<FrameEvent>> frame_events,
        std::unique_ptr<std::vector<PacketEvent>> packet_events) const;
    void Subscribe(RawEventSubscriber* subscriber);
    void Unsubscribe(RawEventSubscriber* subscriber);

   private:
    base::OnceClosure deletion_cb_;
    std::vector<raw_ptr<RawEventSubscriber, VectorExperimental>> subscribers_;
  };

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  std::unique_ptr<Impl> impl_;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_
