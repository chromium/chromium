#include "third_party/blink/renderer/platform/scheduler/common/thread_load_tracker.h"

#include "base/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

using testing::ElementsAre;

namespace blink {
namespace scheduler {

namespace {

void AddToVector(Vector<std::pair<base::TimeTicks, double>>* vector,
                 base::TimeTicks time,
                 double load) {
  vector->push_back(std::make_pair(time, load));
}

base::TimeTicks SecondsToTime(int seconds) {
  return base::TimeTicks() + base::TimeDelta::FromSeconds(seconds);
}

base::TimeTicks MillisecondsToTime(int milliseconds) {
  return base::TimeTicks() + base::TimeDelta::FromMilliseconds(milliseconds);
}

}  // namespace

TEST(ThreadLoadTrackerTest, RecordTasks) {
  Vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1),
      base::BindRepeating(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1));
  thread_load_tracker.Resume(SecondsToTime(1));

  thread_load_tracker.RecordTaskTime(SecondsToTime(1), SecondsToTime(3));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(4300),
                                     MillisecondsToTime(4400));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(5900),
                                     MillisecondsToTime(6100));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(6700),
                                     MillisecondsToTime(6800));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(7500),
                                     MillisecondsToTime(8500));

  thread_load_tracker.RecordIdle(MillisecondsToTime(10500));

  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(2), 1.0),
                                  std::make_pair(SecondsToTime(3), 1.0),
                                  std::make_pair(SecondsToTime(4), 0),
                                  std::make_pair(SecondsToTime(5), 0.1),
                                  std::make_pair(SecondsToTime(6), 0.1),
                                  std::make_pair(SecondsToTime(7), 0.2),
                                  std::make_pair(SecondsToTime(8), 0.5),
                                  std::make_pair(SecondsToTime(9), 0.5),
                                  std::make_pair(SecondsToTime(10), 0)));
}

TEST(ThreadLoadTrackerTest, PauseAndResume) {
  Vector<std::pair<base::TimeTicks, double>> result;

  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1),
      base::BindRepeating(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1));
  thread_load_tracker.Resume(SecondsToTime(1));

  thread_load_tracker.RecordTaskTime(SecondsToTime(2), SecondsToTime(3));
  thread_load_tracker.Pause(SecondsToTime(5));
  thread_load_tracker.RecordTaskTime(SecondsToTime(6), SecondsToTime(7));
  thread_load_tracker.Resume(SecondsToTime(9));
  thread_load_tracker.RecordTaskTime(MillisecondsToTime(10900),
                                     MillisecondsToTime(11100));

  thread_load_tracker.Pause(SecondsToTime(12));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(12100),
                                     MillisecondsToTime(12200));

  thread_load_tracker.Resume(SecondsToTime(13));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(13100),
                                     MillisecondsToTime(13400));

  thread_load_tracker.RecordIdle(SecondsToTime(14));

  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(2), 0),
                                  std::make_pair(SecondsToTime(3), 1.0),
                                  std::make_pair(SecondsToTime(4), 0),
                                  std::make_pair(SecondsToTime(5), 0),
                                  std::make_pair(SecondsToTime(10), 0),
                                  std::make_pair(SecondsToTime(11), 0.1),
                                  std::make_pair(SecondsToTime(12), 0.1),
                                  std::make_pair(SecondsToTime(14), 0.3)));
}

TEST(ThreadLoadTrackerTest, DisabledByDefault) {
  Vector<std::pair<base::TimeTicks, double>> result;
  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1),
      base::BindRepeating(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1));

  // ThreadLoadTracker should be disabled and these tasks should be
  // ignored.
  thread_load_tracker.RecordTaskTime(SecondsToTime(1), SecondsToTime(3));
  thread_load_tracker.RecordTaskTime(SecondsToTime(4), SecondsToTime(7));

  thread_load_tracker.Resume(SecondsToTime(8));

  thread_load_tracker.RecordTaskTime(SecondsToTime(9), SecondsToTime(10));

  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(9), 0),
                                  std::make_pair(SecondsToTime(10), 1)));
}

TEST(ThreadLoadTrackerTest, Reset) {
  Vector<std::pair<base::TimeTicks, double>> result;
  ThreadLoadTracker thread_load_tracker(
      SecondsToTime(1),
      base::BindRepeating(&AddToVector, base::Unretained(&result)),
      base::TimeDelta::FromSeconds(1));

  thread_load_tracker.Resume(SecondsToTime(1));

  thread_load_tracker.RecordTaskTime(MillisecondsToTime(1500),
                                     MillisecondsToTime(4500));

  thread_load_tracker.Reset(SecondsToTime(100));

  thread_load_tracker.RecordTaskTime(SecondsToTime(101), SecondsToTime(102));

  EXPECT_THAT(result, ElementsAre(std::make_pair(SecondsToTime(2), 0.5),
                                  std::make_pair(SecondsToTime(3), 1.0),
                                  std::make_pair(SecondsToTime(4), 1.0),
                                  std::make_pair(SecondsToTime(101), 0),
                                  std::make_pair(SecondsToTime(102), 1)));
}

}  // namespace scheduler
}  // namespace blink
