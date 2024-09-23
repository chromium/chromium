// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/log/net_log.h"

#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "base/threading/simple_thread.h"
#include "base/values.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
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

base::Value::Dict NetCaptureModeParams(NetLogCaptureMode capture_mode) {
  base::Value::Dict dict;
  dict.Set("capture_mode", CaptureModeToValue(capture_mode));
  return dict;
}

TEST(NetLogTest, BasicGlobalEvents) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  RecordingNetLogObserver net_log_observer;
  auto entries = net_log_observer.GetEntries();
  EXPECT_EQ(0u, entries.size());

  task_environment.FastForwardBy(base::Seconds(1234));
  base::TimeTicks ticks0 = base::TimeTicks::Now();

  NetLog::Get()->AddGlobalEntry(NetLogEventType::CANCELLED);

  task_environment.FastForwardBy(base::Seconds(5678));
  base::TimeTicks ticks1 = base::TimeTicks::Now();
  EXPECT_LE(ticks0, ticks1);

  NetLog::Get()->AddGlobalEntry(NetLogEventType::FAILED);

  task_environment.FastForwardBy(base::Seconds(91011));
  EXPECT_LE(ticks1, base::TimeTicks::Now());

  entries = net_log_observer.GetEntries();
  ASSERT_EQ(2u, entries.size());

  EXPECT_EQ(NetLogEventType::CANCELLED, entries[0].type);
  EXPECT_EQ(NetLogSourceType::NONE, entries[0].source.type);
  EXPECT_NE(NetLogSource::kInvalidId, entries[0].source.id);
  EXPECT_EQ(ticks0, entries[0].source.start_time);
  EXPECT_EQ(NetLogEventPhase::NONE, entries[0].phase);
  EXPECT_EQ(ticks0, entries[0].time);
  EXPECT_FALSE(entries[0].HasParams());

  EXPECT_EQ(NetLogEventType::FAILED, entries[1].type);
  EXPECT_EQ(NetLogSourceType::NONE, entries[1].source.type);
  EXPECT_NE(NetLogSource::kInvalidId, entries[1].source.id);
  EXPECT_LT(entries[0].source.id, entries[1].source.id);
  EXPECT_EQ(ticks1, entries[1].source.start_time);
  EXPECT_EQ(NetLogEventPhase::NONE, entries[1].phase);
  EXPECT_EQ(ticks1, entries[1].time);
  EXPECT_FALSE(entries[1].HasParams());
}

TEST(NetLogTest, BasicEventsWithSource) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  RecordingNetLogObserver net_log_observer;
  auto entries = net_log_observer.GetEntries();
  EXPECT_EQ(0u, entries.size());

  task_environment.FastForwardBy(base::Seconds(9876));
  base::TimeTicks source0_start_ticks = base::TimeTicks::Now();

  NetLogWithSource source0 =
      NetLogWithSource::Make(NetLogSourceType::URL_REQUEST);
  task_environment.FastForwardBy(base::Seconds(1));
  base::TimeTicks source0_event0_ticks = base::TimeTicks::Now();
  source0.BeginEvent(NetLogEventType::REQUEST_ALIVE);

  task_environment.FastForwardBy(base::Seconds(5432));
  base::TimeTicks source1_start_ticks = base::TimeTicks::Now();

  NetLogWithSource source1 = NetLogWithSource::Make(NetLogSourceType::SOCKET);
  task_environment.FastForwardBy(base::Seconds(1));
  base::TimeTicks source1_event0_ticks = base::TimeTicks::Now();
  source1.BeginEvent(NetLogEventType::SOCKET_ALIVE);
  task_environment.FastForwardBy(base::Seconds(10));
  base::TimeTicks source1_event1_ticks = base::TimeTicks::Now();
  source1.EndEvent(NetLogEventType::SOCKET_ALIVE);

  task_environment.FastForwardBy(base::Seconds(1));
  base::TimeTicks source0_event1_ticks = base::TimeTicks::Now();
  source0.EndEvent(NetLogEventType::REQUEST_ALIVE);

  task_environment.FastForwardBy(base::Seconds(123));

  entries = net_log_observer.GetEntries();
  ASSERT_EQ(4u, entries.size());

  EXPECT_EQ(NetLogEventType::REQUEST_ALIVE, entries[0].type);
  EXPECT_EQ(NetLogSourceType::URL_REQUEST, entries[0].source.type);
  EXPECT_NE(NetLogSource::kInvalidId, entries[0].source.id);
  EXPECT_EQ(source0_start_ticks, entries[0].source.start_time);
  EXPECT_EQ(NetLogEventPhase::BEGIN, entries[0].phase);
  EXPECT_EQ(source0_event0_ticks, entries[0].time);
  EXPECT_FALSE(entries[0].HasParams());

  EXPECT_EQ(NetLogEventType::SOCKET_ALIVE, entries[1].type);
  EXPECT_EQ(NetLogSourceType::SOCKET, entries[1].source.type);
  EXPECT_NE(NetLogSource::kInvalidId, entries[1].source.id);
  EXPECT_LT(entries[0].source.id, entries[1].source.id);
  EXPECT_EQ(source1_start_ticks, entries[1].source.start_time);
  EXPECT_EQ(NetLogEventPhase::BEGIN, entries[1].phase);
  EXPECT_EQ(source1_event0_ticks, entries[1].time);
  EXPECT_FALSE(entries[1].HasParams());

  EXPECT_EQ(NetLogEventType::SOCKET_ALIVE, entries[2].type);
  EXPECT_EQ(NetLogSourceType::SOCKET, entries[2].source.type);
  EXPECT_EQ(entries[1].source.id, entries[2].source.id);
  EXPECT_EQ(source1_start_ticks, entries[2].source.start_time);
  EXPECT_EQ(NetLogEventPhase::END, entries[2].phase);
  EXPECT_EQ(source1_event1_ticks, entries[2].time);
  EXPECT_FALSE(entries[2].HasParams());

  EXPECT_EQ(NetLogEventType::REQUEST_ALIVE, entries[3].type);
  EXPECT_EQ(NetLogSourceType::URL_REQUEST, entries[3].source.type);
  EXPECT_EQ(entries[0].source.id, entries[3].source.id);
  EXPECT_EQ(source0_start_ticks, entries[3].source.start_time);
  EXPECT_EQ(NetLogEventPhase::END, entries[3].phase);
  EXPECT_EQ(source0_event1_ticks, entries[3].time);
  EXPECT_FALSE(entries[3].HasParams());
}

// Check that the correct CaptureMode is sent to NetLog Value callbacks.
TEST(NetLogTest, CaptureModes) {
  NetLogCaptureMode kModes[] = {
      NetLogCaptureMode::kDefault,
      NetLogCaptureMode::kIncludeSensitive,
      NetLogCaptureMode::kEverything,
  };

  RecordingNetLogObserver net_log_observer;

  for (NetLogCaptureMode mode : kModes) {
    net_log_observer.SetObserverCaptureMode(mode);

    NetLog::Get()->AddGlobalEntry(NetLogEventType::SOCKET_ALIVE,
                                  [&](NetLogCaptureMode capture_mode) {
                                    return NetCaptureModeParams(capture_mode);
                                  });

    auto entries = net_log_observer.GetEntries();

    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(NetLogEventType::SOCKET_ALIVE, entries[0].type);
    EXPECT_EQ(NetLogSourceType::NONE, entries[0].source.type);
    EXPECT_NE(NetLogSource::kInvalidId, entries[0].source.id);
    EXPECT_GE(base::TimeTicks::Now(), entries[0].source.start_time);
    EXPECT_EQ(NetLogEventPhase::NONE, entries[0].phase);
    EXPECT_GE(base::TimeTicks::Now(), entries[0].time);

    ASSERT_EQ(CaptureModeToInt(mode),
              GetIntegerValueFromParams(entries[0], "capture_mode"));

    net_log_observer.Clear();
  }
}

class CountingObserver : public NetLog::ThreadSafeObserver {
 public:
  CountingObserver() = default;

  ~CountingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override { ++count_; }

  int count() const { return count_; }

 private:
  int count_ = 0;
};

class LoggingObserver : public NetLog::ThreadSafeObserver {
 public:
  LoggingObserver() = default;

  ~LoggingObserver() override {
    if (net_log())
      net_log()->RemoveObserver(this);
  }

  void OnAddEntry(const NetLogEntry& entry) override {
    // TODO(crbug.com/40257546): This should be updated to be a
    // base::Value::Dict instead of a std::unique_ptr.
    std::unique_ptr<base::Value::Dict> dict =
        std::make_unique<base::Value::Dict>(entry.ToDict());
    ASSERT_TRUE(dict);
    values_.push_back(std::move(dict));
  }

  size_t GetNumValues() const { return values_.size(); }
  base::Value::Dict* GetDict(size_t index) const {
    return values_[index].get();
  }

 private:
  std::vector<std::unique_ptr<base::Value::Dict>> values_;
};

void AddEvent(NetLog* net_log) {
  net_log->AddGlobalEntry(NetLogEventType::CANCELLED,
                          [&](NetLogCaptureMode capture_mode) {
                            return NetCaptureModeParams(capture_mode);
                          });
}

// A thread that waits until an event has been signalled before calling
// RunTestThread.
class NetLogTestThread : public base::SimpleThread {
 public:
  NetLogTestThread() : base::SimpleThread("NetLogTest") {}

  NetLogTestThread(const NetLogTestThread&) = delete;
  NetLogTestThread& operator=(const NetLogTestThread&) = delete;

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
  raw_ptr<NetLog> net_log_ = nullptr;

 private:
  // Only triggered once all threads have been created, to make it less likely
  // each thread completes before the next one starts.
  raw_ptr<base::WaitableEvent> start_event_ = nullptr;
};

// A thread that adds a bunch of events to the NetLog.
class AddEventsTestThread : public NetLogTestThread {
 public:
  AddEventsTestThread() = default;

  AddEventsTestThread(const AddEventsTestThread&) = delete;
  AddEventsTestThread& operator=(const AddEventsTestThread&) = delete;

  ~AddEventsTestThread() override = default;

 private:
  void RunTestThread() override {
    for (int i = 0; i < kEvents; ++i)
      AddEvent(net_log_);
  }
};

// A thread that adds and removes an observer from the NetLog repeatedly.
class AddRemoveObserverTestThread : public NetLogTestThread {
 public:
  AddRemoveObserverTestThread() = default;

  AddRemoveObserverTestThread(const AddRemoveObserverTestThread&) = delete;
  AddRemoveObserverTestThread& operator=(const AddRemoveObserverTestThread&) =
      delete;

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
};

// Creates |kThreads| threads of type |ThreadType| and then runs them all
// to completion.
template <class ThreadType>
void RunTestThreads(NetLog* net_log) {
  // Must outlive `threads`.
  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  ThreadType threads[kThreads];
  for (size_t i = 0; i < std::size(threads); ++i) {
    threads[i].Init(net_log, &start_event);
    threads[i].Start();
  }

  start_event.Signal();

  for (size_t i = 0; i < std::size(threads); ++i)
    threads[i].Join();
}

// Makes sure that events on multiple threads are dispatched to all observers.
TEST(NetLogTest, NetLogEventThreads) {
  // Attach some observers.  They'll safely detach themselves on destruction.
  CountingObserver observers[3];
  for (auto& observer : observers) {
    NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kEverything);
  }

  // Run a bunch of threads to completion, each of which will emit events to
  // |net_log|.
  RunTestThreads<AddEventsTestThread>(NetLog::Get());

  // Check that each observer saw the emitted events.
  const int kTotalEvents = kThreads * kEvents;
  for (const auto& observer : observers)
    EXPECT_EQ(kTotalEvents, observer.count());
}

// Test adding and removing a single observer.
TEST(NetLogTest, NetLogAddRemoveObserver) {
  CountingObserver observer;

  AddEvent(NetLog::Get());
  EXPECT_EQ(0, observer.count());
  EXPECT_EQ(nullptr, observer.net_log());
  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  // Add the observer and add an event.
  NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kIncludeSensitive);
  EXPECT_TRUE(NetLog::Get()->IsCapturing());
  EXPECT_EQ(NetLog::Get(), observer.net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer.capture_mode());
  EXPECT_TRUE(NetLog::Get()->IsCapturing());

  AddEvent(NetLog::Get());
  EXPECT_EQ(1, observer.count());

  AddEvent(NetLog::Get());
  EXPECT_EQ(2, observer.count());

  // Remove observer and add an event.
  NetLog::Get()->RemoveObserver(&observer);
  EXPECT_EQ(nullptr, observer.net_log());
  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  AddEvent(NetLog::Get());
  EXPECT_EQ(2, observer.count());

  // Add the observer a final time, this time with a different capture mdoe, and
  // add an event.
  NetLog::Get()->AddObserver(&observer, NetLogCaptureMode::kEverything);
  EXPECT_EQ(NetLog::Get(), observer.net_log());
  EXPECT_EQ(NetLogCaptureMode::kEverything, observer.capture_mode());
  EXPECT_TRUE(NetLog::Get()->IsCapturing());

  AddEvent(NetLog::Get());
  EXPECT_EQ(3, observer.count());
}

// Test adding and removing two observers at different log levels.
TEST(NetLogTest, NetLogTwoObservers) {
  LoggingObserver observer[2];

  // Add first observer.
  NetLog::Get()->AddObserver(&observer[0],
                             NetLogCaptureMode::kIncludeSensitive);
  EXPECT_EQ(NetLog::Get(), observer[0].net_log());
  EXPECT_EQ(nullptr, observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_TRUE(NetLog::Get()->IsCapturing());

  // Add second observer observer.
  NetLog::Get()->AddObserver(&observer[1], NetLogCaptureMode::kEverything);
  EXPECT_EQ(NetLog::Get(), observer[0].net_log());
  EXPECT_EQ(NetLog::Get(), observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_EQ(NetLogCaptureMode::kEverything, observer[1].capture_mode());
  EXPECT_TRUE(NetLog::Get()->IsCapturing());

  // Add event and make sure both observers receive it at their respective log
  // levels.
  std::optional<int> param;
  AddEvent(NetLog::Get());
  ASSERT_EQ(1U, observer[0].GetNumValues());
  param = observer[0].GetDict(0)->FindDict("params")->FindInt("capture_mode");
  ASSERT_TRUE(param);
  EXPECT_EQ(CaptureModeToInt(observer[0].capture_mode()), param.value());
  ASSERT_EQ(1U, observer[1].GetNumValues());
  param = observer[1].GetDict(0)->FindDict("params")->FindInt("capture_mode");
  ASSERT_TRUE(param);
  EXPECT_EQ(CaptureModeToInt(observer[1].capture_mode()), param.value());

  // Remove second observer.
  NetLog::Get()->RemoveObserver(&observer[1]);
  EXPECT_EQ(NetLog::Get(), observer[0].net_log());
  EXPECT_EQ(nullptr, observer[1].net_log());
  EXPECT_EQ(NetLogCaptureMode::kIncludeSensitive, observer[0].capture_mode());
  EXPECT_TRUE(NetLog::Get()->IsCapturing());

  // Add event and make sure only second observer gets it.
  AddEvent(NetLog::Get());
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());

  // Remove first observer.
  NetLog::Get()->RemoveObserver(&observer[0]);
  EXPECT_EQ(nullptr, observer[0].net_log());
  EXPECT_EQ(nullptr, observer[1].net_log());
  EXPECT_FALSE(NetLog::Get()->IsCapturing());

  // Add event and make sure neither observer gets it.
  AddEvent(NetLog::Get());
  EXPECT_EQ(2U, observer[0].GetNumValues());
  EXPECT_EQ(1U, observer[1].GetNumValues());
}

// Makes sure that adding and removing observers simultaneously on different
// threads works.
TEST(NetLogTest, NetLogAddRemoveObserverThreads) {
  // Run a bunch of threads to completion, each of which will repeatedly add
  // and remove an observer, and set its logging level.
  RunTestThreads<AddRemoveObserverTestThread>(NetLog::Get());
}

// Tests that serializing a NetLogEntry with empty parameters omits a value for
// "params".
TEST(NetLogTest, NetLogEntryToValueEmptyParams) {
  // NetLogEntry with no params.
  NetLogEntry entry1(NetLogEventType::REQUEST_ALIVE, NetLogSource(),
                     NetLogEventPhase::BEGIN, base::TimeTicks(),
                     base::Value::Dict());

  ASSERT_TRUE(entry1.params.empty());
  ASSERT_FALSE(entry1.ToDict().Find("params"));
}

}  // namespace

}  // namespace net
