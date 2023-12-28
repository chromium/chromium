// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_
#define MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/raw_event_subscriber.h"

namespace media {
namespace cast {

class CastEnvironment;

// A thread-safe receiver of logging events that manages an active list of
// EventSubscribers and dispatches the logging events to them on the MAIN
// thread.  All methods, constructor, and destructor can be invoked on any
// thread.
class LogEventDispatcher {
 public:
  // |env| outlives this instance (and generally owns this instance).
  explicit LogEventDispatcher(CastEnvironment* env);

  LogEventDispatcher(const LogEventDispatcher&) = delete;
  LogEventDispatcher& operator=(const LogEventDispatcher&) = delete;

  ~LogEventDispatcher();

  // Called on any thread to schedule the sending of event(s) to all
  // EventSubscribers on the MAIN thread.
  void DispatchFrameEvent(std::unique_ptr<FrameEvent> event) const;
  void DispatchPacketEvent(std::unique_ptr<PacketEvent> event) const;
  void DispatchBatchOfEvents(
      std::unique_ptr<std::vector<FrameEvent>> frame_events,
      std::unique_ptr<std::vector<PacketEvent>> packet_events) const;

  // Adds |subscriber| to the active list to begin receiving events on MAIN
  // thread.  Unsubscribe() must be called before |subscriber| is destroyed.
  void Subscribe(RawEventSubscriber* subscriber);

  // Removes |subscriber| from the active list.  Once this method returns, the
  // |subscriber| is guaranteed not to receive any more events.
  void Unsubscribe(RawEventSubscriber* subscriber);

 private:
  // The part of the implementation that runs exclusively on the MAIN thread.
  class Impl : public base::RefCountedThreadSafe<Impl> {
   public:
    Impl();

    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;

    void DispatchFrameEvent(std::unique_ptr<FrameEvent> event) const;
    void DispatchPacketEvent(std::unique_ptr<PacketEvent> event) const;
    void DispatchBatchOfEvents(
        std::unique_ptr<std::vector<FrameEvent>> frame_events,
        std::unique_ptr<std::vector<PacketEvent>> packet_events) const;
    void Subscribe(RawEventSubscriber* subscriber);
    void Unsubscribe(RawEventSubscriber* subscriber);

   private:
    friend class base::RefCountedThreadSafe<Impl>;

    ~Impl();

    std::vector<raw_ptr<RawEventSubscriber, VectorExperimental>> subscribers_;
  };

  const raw_ptr<CastEnvironment> env_;  // Owner of this instance.
  const scoped_refptr<Impl> impl_;
};

}  // namespace cast
}  // namespace media

#endif  // MEDIA_CAST_LOGGING_LOG_EVENT_DISPATCHER_H_
