// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/log/net_log.h"

#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "base/values.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/test_net_log.h"
#include "net/log/test_net_log_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kThreads = 10;
const int kEvents = 100;

int CaptureModeToInt(NetLogCaptureMode capture_mode) {
  return static_cast<int>(capture_mode);
}

base::Value CaptureModeToValue(NetLogCaptureMode capture_mode) {
  return base::Value(CaptureModeToInt(capture_mode));
}

base::Value NetCaptureModeParams(NetLogCaptureMode capture_mode) {
  base::DictionaryValue dict;
  dict.SetKey("capture_mode", CaptureModeToValue(capture_mode));
  return std::move(dict);
}

TEST(NetLogTest, Basic) {
  TestNetLog net_log;
  auto entries = net_log.GetEntries();
  EXPECT_EQ(0u, entries.size());

  net_log.AddGlobalEntry(NetLogEventType::CANCELLED);

  entries = net_log.GetEntries();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(NetLogEventType::CANCELLED, entries[0].type);
  EXPECT_EQ(NetLogSourceType::NONE, entries[0].source.type);
  EXPECT_NE(NetLogSource::kInvalidId, entries[0].source.id);
  EXPECT_EQ(NetLogEventPhase::NONE, entries[0].phase);
  EXPECT_GE(base::TimeTicks::Now(), entries[0].time);
  EXPECT_FALSE(entries[0].HasParams());
}

// Check that the correct CaptureMode is sent to NetLog Value callbacks.
TEST(NetLogTest, CaptureModes) {
  NetLogCaptureMode kModes[] = {
      NetLogCaptureMode::kDefault,
      NetLogCaptureMode::kIncludeSensitive,
      NetLogCaptureMode::kEverything,
  };

  TestNetLog net_log;

  for (NetLogCaptureMode mode : kModes) {
    net_log.SetObserverCaptureMode(mode);
    EXPECT_EQ(mode, net_log.GetObserver()->capture_mode());

    net_log.AddGlobalEntry(NetLogEventType::SOCKET_ALIVE,
                           [&](NetLogCaptureMode capture_mode) {
                             return NetCaptureModeParams(capture_mode);
                           });

    auto entries = net_log.GetEntries();

    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(NetLogEventType::SOCKET_ALIVE, entries[0].type);
    EXPECT_EQ(NetLogSourceType::NONE, entries[0].source.type);
    EXPECT_NE(NetLogSource::kInvalidId, entries[0].source.id);
    EXPECT_EQ(NetLogEventPhase::NONE, entries[0].phase);
    EXPECT_GE(base::TimeTicks::Now(), entries[0].time);

    ASSERT_EQ(CaptureModeToInt(mode),
              GetIntegerValueFromParams(entries[0], "capture_mode"));

    net_log.Clear();
  }
}

class CountingObserver : public NetLog::ThreadSafeObserver {
 public:
  CountingObserver() : count_(0) {}

  ~CountingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override { ++count_; }

  int count() const { return count_; }

 private:
  int count_;
};

class LoggingObserver : public NetLog::ThreadSafeObserver {
 public:
  LoggingObserver() = default;

  ~LoggingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override {
    std::unique_ptr<base::DictionaryValue> dict = base::DictionaryValue::From(
        base::Value::ToUniquePtrValue(entry.ToValue()));
    ASSERT_TRUE(dict);
    values_.push_back(std::move(dict));
  }

  size_t GetNumValues() const { return values_.size(); }
  base::DictionaryValue* GetValue(size_t index) const {
    return values_[index].get();
  }

 private:
  std::vector<std::unique_ptr<base::DictionaryValue>> values_;
};

void AddEvent(NetLog* net_log) {
  net_log->AddGlobalEntry(NetLogEventType::CANCELLED,
                          [&](NetLogCaptureMode capture_mode) {
                            return CaptureModeToValue(capture_mode);
                          });
}

// A thread that waits until an event has been signalled before calling
// RunTestThread.
class NetLogTestThread : public base::SimpleThread {
 public:
  NetLogTestThread()
      : base::SimpleThread("NetLogTest"),
        net_log_(nullptr),
        start_event_(nullptr) {}

  // We'll wait for |start_event| to be triggered before calling a subclass's
  // subclass's RunTestThread() function.
  void Init(NetLog* net_log, base::WaitableEvent* start_event) {
    start_event_ = start_event;
    net_log_ = net_log;
  }

  void Run() override {
    start_event_->Wait();
    RunTestThread();
  }

  // Subclasses must override this with the code they want to run on their
  // thread.
  virtual void RunTestThread() = 0;

 protected:
  NetLog* net_log_;

 private:
  // Only triggered once all threads have been created, to make it less likely
  // each thread completes before the next one starts.
  base::WaitableEvent* start_event_;

  DISALLOW_COPY_AND_ASSIGN(NetLogTestThread);
};

// A thread that adds a bunch of events to the NetLog.
class AddEventsTestThread : public NetLogTestThread {
 public:
  AddEventsTestThread() = default;
  ~AddEventsTestThread() override = default;

 private:
  void RunTestThread() override {
    for (int i = 0; i < kEvents; ++i)
      AddEvent(net_log_);
  }

  DISALLOW_COPY_AND_ASSIGN(AddEventsTestThread);
};

// A thread that adds and removes an observer from the NetLog repeatedly.
class AddRemoveObserverTestThread : public NetLogTestThread {
 public:
  AddRemoveObserverTestThread() = default;

  ~AddRemoveObserverTestThread() override { EXPECT_TRUE(!observer_.net_log()); }

 private:
  void RunTestThread() override {
    for (int i = 0; i < kEvents; ++i) {
      ASSERT_FALSE(observer_.net_log());

      net_log_->AddObserver(&observer_, NetLogCaptureMode::kIncludeSensitive);
      ASSERT_EQ(net_log_, observer_.net_log());
      ASSERT_EQ(NetLogCaptureMode::kIncludeSensitive, observer_.capture_mode());

      net_log_->RemoveObserver(&observer_);
      ASSERT_TRUE(!observer_.net_log());
    }
  }

  CountingObserver observer_;

  DISALLOW_COPY_AND_ASSIGN(AddRemoveObserverTestThread);
};

// Creates |kThreads| threads of type |ThreadType| and then runs them all
// to completion.
template <class ThreadType>
void RunTestThreads(NetLog* net_log) {
  ThreadType threads[kThreads];
  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  for (size_t i = 0; i < base::size(threads); ++i) {
    threads[i].Init(net_log, &start_event);
    threads[i].Start();
  }

  start_event.Signal();

  for (size_t i = 0; i < base::size(threads); ++i)
    threads[i].Join();
}

// Makes sure that events on multiple threads are dispatched to all observers.
TEST(NetLogTest, NetLogEventThreads) {
  NetLog net_log;

  // Attach some observers.  Since they're created after |net_log|, they'll
  // safely detach themselves on destruction.
  CountingObserver observers[3];
  for (size_t i = 0; i < base::size(observers); ++i) {
    net_log.AddObserver(&observers[i], NetLogCaptureMode::kEverything);
  }

  // Run a bunch of threads to completion, each of which will emit events to
  // |net_log|.
  RunTestThreads<AddEventsTestThread>(&net_log);

  // Check that each observer saw the emitted events.
  const int kTotalEvents = kThreads * kEvents;
  for (size_t i = 0; i < base::size(observers); ++i)
    EXPECT_EQ(kTotalEvents, observers[i].count());
}

// Test adding and removing a single observer.
TEST(NetLogTest, NetLogAddRemoveObserver) {
  NetLog net_log;
  CountingObserver observer;

  AddEvent(&net_log);
  EXPECT_EQ(0, observer.count());
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_FALSE(net_log.IsCapturing());

  // Add the observer and add an event.
  net_log.AddObserver(&observer, NetLogCaptureMode::kIncludeSensitive);
  EXPECT_TRUE(net_log.IsCapturing());
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer.capture_mode());
  EXPECT_TRUE(net_log.IsCapturing());

  AddEvent(&net_log);
  EXPECT_EQ(1, observer.count());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Remove observer and add an event.
  net_log.RemoveObserver(&observer);
  EXPECT_EQ(NULL, observer.net_log());
  EXPECT_FALSE(net_log.IsCapturing());

  AddEvent(&net_log);
  EXPECT_EQ(2, observer.count());

  // Add the observer a final time, this time with a different capture mdoe, and
  // add an event.
  net_log.AddObserver(&observer, NetLogCaptureMode::kEverything);
  EXPECT_EQ(&net_log, observer.net_log());
  EXPECT_EQ(NetLogCaptureMode::kEverything, observer.capture_mode());
  EXPECT_TRUE(net_log.IsCapturing());

  AddEvent(&net_log);
  EXPECT_EQ(3, observer.count());
}

// Test adding and removing two observers at different log levels.
TEST(NetLogTest, NetLogTwoObservers) {
  NetLog net_log;
  LoggingObserver observer[2];

  // Add first observer.
  net_log.AddObserver(&observer[0], NetLogCaptureMode::kIncludeSensitive);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_TRUE(net_log.IsCapturing());

  // Add second observer observer.
  net_log.AddObserver(&observer[1], NetLogCaptureMode::kEverything);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(&net_log, observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_EQ(NetLogCaptureMode::kEverything, observer[1].capture_mode());
  EXPECT_TRUE(net_log.IsCapturing());

  // Add event and make sure both observers receive it at their respective log
  // levels.
  int param;
  AddEvent(&net_log);
  ASSERT_EQ(1U, observer[0].GetNumValues());
  ASSERT_TRUE(observer[0].GetValue(0)->GetInteger("params", &param));
  EXPECT_EQ(CaptureModeToInt(observer[0].capture_mode()), param);
  ASSERT_EQ(1U, observer[1].GetNumValues());
  ASSERT_TRUE(observer[1].GetValue(0)->GetInteger("params", &param));
  EXPECT_EQ(CaptureModeToInt(observer[1].capture_mode()), param);

  // Remove second observer.
  net_log.RemoveObserver(&observer[1]);
  EXPECT_EQ(&net_log, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_TRUE(net_log.IsCapturing());

  // Add event and make sure only second observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());

  // Remove first observer.
  net_log.RemoveObserver(&observer[0]);
  EXPECT_EQ(NULL, observer[0].net_log());
  EXPECT_EQ(NULL, observer[1].net_log());
  EXPECT_FALSE(net_log.IsCapturing());

  // Add event and make sure neither observer gets it.
  AddEvent(&net_log);
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());
}

// Makes sure that adding and removing observers simultaneously on different
// threads works.
TEST(NetLogTest, NetLogAddRemoveObserverThreads) {
  NetLog net_log;

  // Run a bunch of threads to completion, each of which will repeatedly add
  // and remove an observer, and set its logging level.
  RunTestThreads<AddRemoveObserverTestThread>(&net_log);
}

// Tests that serializing a NetLogEntry with empty parameters omits a value for
// "params".
TEST(NetLogTest, NetLogEntryToValueEmptyParams) {
  // NetLogEntry with no params.
  NetLogEntry entry1(NetLogEventType::REQUEST_ALIVE, NetLogSource(),
                     NetLogEventPhase::BEGIN, base::TimeTicks(), base::Value());

  ASSERT_TRUE(entry1.params.is_none());
  ASSERT_FALSE(entry1.ToValue().FindKey("params"));
}

}  // namespace

}  // namespace net
