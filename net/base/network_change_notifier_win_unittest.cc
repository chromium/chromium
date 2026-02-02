// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_change_notifier_win.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_os_info_override_win.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/win/windows_version.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_change_notifier_factory.h"
#include "net/base/network_cost_change_notifier_win.h"
#include "net/test/test_connection_cost_observer.h"
#include "net/test/test_with_task_environment.h"
#include "net/test/win/fake_network_cost_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AtLeast;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;

namespace net {

namespace {
constexpr auto kNumPollsOnAddressChange =
    NetworkChangeNotifierWin::kNumPollsOnAddressChange;
}  // namespace

// Subclass of NetworkChangeNotifierWin that overrides functions so that no
// Windows API networking function results effect tests.
class TestNetworkChangeNotifierWin : public NetworkChangeNotifierWin {
 public:
  // Called to get the connection type on each call to
  // RecomputeCurrentConnectionTypeOnBlockingSequence(). The default one used by
  // a TestNetworkChangeNotifierWin returns
  // NetworkChangeNotifier::CONNECTION_UNKNOWN unconditionally.
  using GetConnectionTypeCallback =
      base::RepeatingCallback<NetworkChangeNotifier::ConnectionType()>;

  TestNetworkChangeNotifierWin() {
    last_computed_connection_type_ = NetworkChangeNotifier::CONNECTION_UNKNOWN;
    sequence_runner_for_registration_ =
        base::SequencedTaskRunner::GetCurrentDefault();
  }

  TestNetworkChangeNotifierWin(const TestNetworkChangeNotifierWin&) = delete;
  TestNetworkChangeNotifierWin& operator=(const TestNetworkChangeNotifierWin&) =
      delete;

  ~TestNetworkChangeNotifierWin() override {
    // This is needed so we don't try to stop watching for IP address changes,
    // as we never actually started.
    set_is_watching(false);
  }

  // From NetworkChangeNotifierWin.
  void RecomputeCurrentConnectionTypeOnBlockingSequence(
      base::OnceCallback<void(ConnectionType)> reply_callback) const override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(reply_callback), get_connection_type_.Run()));
  }

  // From NetworkChangeNotifierWin.
  MOCK_METHOD0(WatchForAddressChangeInternal, bool());

  // Allow tests to compare results with the default implementation that does
  // not depend on the `INetworkCostManager` Windows OS API.  The default
  // implementation is used as a fall back when `INetworkCostManager` fails.
  ConnectionCost GetCurrentConnectionCostFromDefaultImplementationForTesting() {
    return NetworkChangeNotifier::GetCurrentConnectionCost();
  }

  void set_get_connection_type(GetConnectionTypeCallback get_connection_type) {
    get_connection_type_ = get_connection_type;
  }

 private:
  GetConnectionTypeCallback get_connection_type_{base::BindRepeating(
      []() { return NetworkChangeNotifier::CONNECTION_UNKNOWN; })};
};

class TestIPAddressObserver : public NetworkChangeNotifier::IPAddressObserver {
 public:
  TestIPAddressObserver() { NetworkChangeNotifier::AddIPAddressObserver(this); }

  TestIPAddressObserver(const TestIPAddressObserver&) = delete;
  TestIPAddressObserver& operator=(const TestIPAddressObserver&) = delete;

  ~TestIPAddressObserver() override {
    NetworkChangeNotifier::RemoveIPAddressObserver(this);
  }

  MOCK_METHOD1(OnIPAddressChanged,
               void(NetworkChangeNotifier::IPAddressChangeType));
};

class TestConnectionTypeObserver
    : public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  TestConnectionTypeObserver() {
    NetworkChangeNotifier::AddConnectionTypeObserver(this);
  }

  TestConnectionTypeObserver(const TestIPAddressObserver&) = delete;
  TestConnectionTypeObserver& operator=(const TestIPAddressObserver&) = delete;

  ~TestConnectionTypeObserver() override {
    EXPECT_FALSE(expected_connection_type_);
    EXPECT_FALSE(expected_time_);
    NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
  }

  // Sets the details of the next expected OnConnectionTypeChanged() call. Only
  // one call may be expected at a time. If called twice in a row without an
  // intervening OnConnectionTypeChanged() invocation, or if the notification
  // is never received, the test will fail.
  void SetExpectedConnectionTypeChange(
      NetworkChangeNotifier::ConnectionType expected_connection_type,
      base::Time expected_time) {
    EXPECT_FALSE(expected_connection_type_);
    EXPECT_FALSE(expected_time_);
    expected_connection_type_ = expected_connection_type;
    expected_time_ = expected_time;
  }

  // NetworkChangeNotifier::ConnectionTypeObserver implementation:
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType connection_type) override {
    EXPECT_EQ(expected_connection_type_, connection_type);
    EXPECT_EQ(expected_time_, base::Time::Now());
    expected_connection_type_ = std::nullopt;
    expected_time_ = std::nullopt;
  }

 private:
  std::optional<NetworkChangeNotifier::ConnectionType>
      expected_connection_type_;
  std::optional<base::Time> expected_time_;
};

// A test case for a NetworkChangeNotifierWinPollTest test. Each test case sets
// up an initial state for the NCN, and then triggers an address change. It also
// sets the results of each of the polls that change triggers, and has
// expectations for when network connection type change notifications are
// received. Some tests also trigger subsequent address change notifications.
struct PollTestCase {
  // Each event corresponds to a call of
  // NetworkChangeNotifierWin::RecomputeCurrentConnectionType() off of the main
  // thread. Events specify when they occur, what the method should return,
  // whether a ConnectionTypeObserver call is expected (with the
  // `connection_type_to_return` as an argument), and anything else the test
  // fixture should simulate immediately after the call.
  //
  // When one poll should return the same ConnectionType as the last specified
  // event (or `first_polled_connection_type` if there was no previous event),
  // and no notification is expected, including the corresponding Event is
  // optional.
  struct Event {
    // Number of seconds since the start of the test. Polls are only triggered
    // after an exact number of seconds relative to the inciting address change
    // notification.
    int seconds_from_start;

    // This type will be returned from `time_from_start` until the time of
    // the next event is reached.
    NetworkChangeNotifier::ConnectionType connection_type_to_return;

    // If true, expects a connect type changed notification event.
    bool expect_connection_type_changed_notification = false;

    // If populated, NetworkChangeObserverWin::NotifyObservers() method will be
    // invoked, simulating a new notification of an address change change with
    // the specified ConnectionType detected. Updates future polling to return
    // this connection type, rather than `connection_type_to_return`.
    std::optional<NetworkChangeNotifier::ConnectionType>
        call_notify_observers_with_connection_type;
  };

  std::string_view test_case_name;

  // The initial connection type before any change notification is triggered.
  NetworkChangeNotifier::ConnectionType initial_connection_type;

  // The connection type associated with the IP address change notification.
  NetworkChangeNotifier::ConnectionType first_polled_connection_type;

  // This specifies what each call to
  // RecomputeCurrentConnectionTypeOnBlockingSequence() should return.
  std::vector<Event> events;

  // The expected total number of polls. If there are no additional IP address
  // change notifications, this will be `kNumPollsOnAddressChange`.
  int expected_poll_count = kNumPollsOnAddressChange;
};

class NetworkChangeNotifierWinTest : public TestWithTaskEnvironment {
 public:
  NetworkChangeNotifierWinTest()
      : TestWithTaskEnvironment(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  // Calls WatchForAddressChange, and simulates a WatchForAddressChangeInternal
  // success.  Expects that |network_change_notifier_| has just been created, so
  // it's not watching anything yet, and there have been no previous
  // WatchForAddressChangeInternal failures.
  void StartWatchingAndSucceed() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .WillOnce(Return(true));

    network_change_notifier_.WatchForAddressChange();

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run to trigger a failure.
    base::RunLoop().RunUntilIdle();
  }

  // Calls WatchForAddressChange, and simulates a WatchForAddressChangeInternal
  // failure.
  void StartWatchingAndFail() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));

    network_change_notifier_.WatchForAddressChange();

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run.
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change event, resulting in a call to OnObjectSignaled.
  // The resulting call to WatchForAddressChangeInternal then succeeds.
  void SignalAndSucceed() {
    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(1);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .WillOnce(Return(true));

    network_change_notifier_.OnObjectSignaled(INVALID_HANDLE_VALUE);

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    // Run the task to notify observers of the IP address change event.
    base::RunLoop().RunUntilIdle();
  }

  // Simulates a network change event, resulting in a call to OnObjectSignaled.
  // The resulting call to WatchForAddressChangeInternal then fails.
  void SignalAndFail() {
    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(1);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly(Return(false));

    network_change_notifier_.OnObjectSignaled(INVALID_HANDLE_VALUE);

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    // Run the task to notify observers of the IP address change event.
    base::RunLoop().RunUntilIdle();
  }

  // Runs the message loop until WatchForAddressChange is called again, as a
  // result of the already posted task after a WatchForAddressChangeInternal
  // failure.  Simulates a success on the resulting call to
  // WatchForAddressChangeInternal.
  void RetryAndSucceed() {
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    base::RunLoop run_loop;

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .WillOnce(Invoke(&run_loop, &base::RunLoop::QuitWhenIdle));
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        .WillOnce(Return(true));

    run_loop.Run();

    EXPECT_TRUE(network_change_notifier_.is_watching());
    EXPECT_EQ(0, network_change_notifier_.sequential_failures());
  }

  // Runs the message loop until WatchForAddressChange is called again, as a
  // result of the already posted task after a WatchForAddressChangeInternal
  // failure.  Simulates a failure on the resulting call to
  // WatchForAddressChangeInternal.
  void RetryAndFail() {
    base::RunLoop loop;
    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(0, network_change_notifier_.sequential_failures());

    int initial_sequential_failures =
        network_change_notifier_.sequential_failures();

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(0);
    EXPECT_CALL(network_change_notifier_, WatchForAddressChangeInternal())
        // Due to an expected race, it's theoretically possible for more than
        // one call to occur, though unlikely.
        .Times(AtLeast(1))
        .WillRepeatedly([&loop]() {
          loop.QuitWhenIdle();
          return false;
        });

    loop.Run();

    EXPECT_FALSE(network_change_notifier_.is_watching());
    EXPECT_LT(initial_sequential_failures,
              network_change_notifier_.sequential_failures());

    // If a task to notify observers of the IP address change event was
    // incorrectly posted, make sure it gets run.
    base::RunLoop().RunUntilIdle();
  }

  NetworkChangeNotifier::ConnectionCost GetCurrentConnectionCost() {
    return network_change_notifier_.GetCurrentConnectionCost();
  }

  NetworkChangeNotifier::ConnectionCost
  GetCurrentConnectionCostFromDefaultImplementationForTesting() {
    return network_change_notifier_
        .GetCurrentConnectionCostFromDefaultImplementationForTesting();
  }

  base::TimeDelta time_since_start() const {
    return base::Time::Now() - start_time_;
  }

  // Runs a PollTestCase, simulating one or more IP address change notifications
  // and subsequent poll rules, and expecting a specific set of network change
  // notifications. See that struct for detailed explanation.
  void RunPollTestCase(const PollTestCase& test_case) {
    network_change_notifier_.SetCurrentConnectionType(
        test_case.initial_connection_type);
    network_change_notifier_.set_last_announced_offline_for_testing(
        test_case.initial_connection_type ==
        NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

    NetworkChangeNotifier::ConnectionType polled_connection_type =
        test_case.first_polled_connection_type;
    size_t next_event = 0;
    int num_polls = 0;
    TestConnectionTypeObserver observer;
    auto main_task_runner = base::SequencedTaskRunner::GetCurrentDefault();

    network_change_notifier_.set_get_connection_type(
        base::BindLambdaForTesting([&]() {
          // Polling should start at 1 second from start, and all polls should
          // be 1 second apart, until we stop polling.
          ++num_polls;
          EXPECT_EQ(time_since_start(), base::Seconds(num_polls));

          // Clear expected IPAddressObserver notifications, which checks that
          // the notification was invoked exactly when expected.
          EXPECT_CALL(test_ip_address_observer_,
                      OnIPAddressChanged(
                          NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
              .Times(0);

          // If no more events left, or the time of the next event in the queue
          // hasn't been reached yet, return `polled_connection_type`.
          if (next_event == test_case.events.size() ||
              base::Seconds(test_case.events[next_event].seconds_from_start) >
                  time_since_start()) {
            return polled_connection_type;
          }

          const auto& event = test_case.events[next_event];
          next_event++;
          polled_connection_type = event.connection_type_to_return;
          NetworkChangeNotifier::ConnectionType connection_type =
              polled_connection_type;
          if (event.expect_connection_type_changed_notification) {
            observer.SetExpectedConnectionTypeChange(polled_connection_type,
                                                     base::Time::Now());
          }
          if (event.call_notify_observers_with_connection_type) {
            // There should be a new call to `test_ip_address_observer_`.
            EXPECT_CALL(test_ip_address_observer_,
                        OnIPAddressChanged(
                            NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
                .Times(1);

            // Update `polled_connection_type` but not the returned
            // ConnectionType. The test fixture supporting different types for
            // the last poll and the NotifyObservers() call is probably not very
            // useful, but seems better to allow it.
            polled_connection_type =
                *event.call_notify_observers_with_connection_type;
            main_task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&NetworkChangeNotifierWin::NotifyObservers,
                               base::Unretained(&network_change_notifier_),
                               polled_connection_type));
          }
          return connection_type;
        }));

    EXPECT_CALL(
        test_ip_address_observer_,
        OnIPAddressChanged(NetworkChangeNotifier::IP_ADDRESS_CHANGE_NORMAL))
        .Times(1);
    network_change_notifier_.NotifyObservers(polled_connection_type);

    // More than enough simulated time for all events to run.
    FastForwardBy(base::Seconds(100));
    // All events should have been triggered, and the expected number of polls
    // should have been made.
    EXPECT_EQ(next_event, test_case.events.size());
    EXPECT_EQ(num_polls, test_case.expected_poll_count);
  }

 protected:
  base::Time start_time_ = base::Time::Now();

  FakeNetworkCostManagerEnvironment fake_network_cost_manager_environment_;

 private:
  // Note that the order of declaration here is important.

  // Allows creating a new NetworkChangeNotifier.  Must be created before
  // |network_change_notifier_| and destroyed after it to avoid DCHECK failures.
  NetworkChangeNotifier::DisableForTest disable_for_test_;

  StrictMock<TestNetworkChangeNotifierWin> network_change_notifier_;

  // Must be created after |network_change_notifier_|, so it can add itself as
  // an IPAddressObserver.
  StrictMock<TestIPAddressObserver> test_ip_address_observer_;
};

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinBasic) {
  StartWatchingAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStart) {
  StartWatchingAndFail();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStartOnce) {
  StartWatchingAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailStartTwice) {
  StartWatchingAndFail();
  RetryAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinSignal) {
  StartWatchingAndSucceed();
  SignalAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailSignalOnce) {
  StartWatchingAndSucceed();
  SignalAndFail();
  RetryAndSucceed();
}

TEST_F(NetworkChangeNotifierWinTest, NetChangeWinFailSignalTwice) {
  StartWatchingAndSucceed();
  SignalAndFail();
  RetryAndFail();
  RetryAndSucceed();
}

const PollTestCase kPollTestCases[] = {
    // Test the case where, when the user is offline, there's an IPAddress
    // change notification, but no online state is ever observed. There should
    // be a ConnectionTypeChange notification, but only after polling the
    // connection type for `kNumPollsOnAddressChange` seconds.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOffline",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/kNumPollsOnAddressChange,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}}},

    // Test the case where, when the user is online, there's an IPAddress change
    // notification, but the connection type is ever observed. There should be a
    // ConnectionTypeChange notification after 1 section with the same online
    // state as before.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOnline",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when offline, and we detect immediately
    // that we're now online, the connection type change notification is sent
    // one second after the IP address change is observed, after a second poll.
    // We poll a total of `kNumPollsOnAddressChange` times, ignoring the initial
    // calculation of the
    // network state.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineImmediately",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when offline, and we only detect that
    // we're online one second later, the connection type change notification is
    // sent one second after the IP address change is observed, after the second
    // poll. We poll a total of `kNumPollsOnAddressChange` times, ignoring the
    // initial calculation of
    // the network state.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineAfterOneSecond",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // Test the case where an offline to online transition is only detected 10
    // seconds after the IP address change notification.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineAfterTenSeconds",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // Test the case where an offline to online transition is only detected
    // `kNumPollsOnAddressChange` seconds after the IP address change
    // notification, on the final connection type poll.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineLastPoll",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/kNumPollsOnAddressChange,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when online, and we detect immediately
    // that we're now offline, the connection type change notification is sent
    // one second after the IP address change is observed, after a second poll.
    // We poll a total of `kNumPollsOnAddressChange` times, ignoring the initial
    // calculation of the
    // network state.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineImmediately",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when online, and we discover we're
    // offline, we send the notification immediately.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineAfterOneSecond",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when online, but we continue receiving an
    // online status until 5 seconds in, we should send both an online and then
    // an offline notification.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineAfterTenSeconds",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}}},

    // If there's an IP address change when online, and we discover we're
    // offline, we wait until the last poll before sending out a connection
    // change notification.
    //
    // If the first poll after a second has passed says we're still online, we
    // will send out an online notification before the eventual offline
    // notification.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineLastPoll",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/kNumPollsOnAddressChange,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}}},

    // Tests the case where, when online, there's an offline notification,
    // but the device starts reading as online again before polling completes.
    // Both offline and online notifications should be sent.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineOnline",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}}},

    // Mirror of the above OnlineOfflineOnline test. Not nearly as likely in the
    // real world, but here for test coverage.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineOffline",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/kNumPollsOnAddressChange,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/false}}},

    // Test the online to offline case where a second IP address change restarts
    // the polling counter. There should be two notifications - one immediately,
    // and one only after the restarted polling counter expires.
    {/*test_case_name=*/"ConnectionTypeChangeOnlineOfflineWithIpAddressChange",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/false,
       /*call_notify_observers_with_connection_type=*/
       NetworkChangeNotifier::CONNECTION_NONE},
      {/*seconds_from_start=*/31,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true}},
     /*expected_poll_count=*/31},

    // Test the offline to online case where a second IP address triggers a
    // second online connection type notification of the same type, and also
    // restarts the polling counter. Note that the second notification, like the
    // first one, occurs exactly 1 second after the
    // NetworkChangeNotifierWin::NotifyObservers call.
    {/*test_case_name=*/"ConnectionTypeChangeOfflineOnlineWithIpAddressChange",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/false,
       /*call_notify_observers_with_connection_type=*/
       NetworkChangeNotifier::CONNECTION_UNKNOWN},
      {/*seconds_from_start=*/11,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}},
     /*expected_poll_count=*/31},

    // Tests the case where, when online, there's an offline notification,
    // but the device receives a new notification about being online before
    // polling completes. There should be two ConnectionType notifications, both
    // for offline and for online, and polling should be extended from when the
    // notification originally occurred.
    {/*test_case_name=*/
     "ConnectionTypeChangeOnlineOfflineOnlineWithIpAddressChange",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_NONE,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_NONE,
       /*expect_connection_type_changed_notification=*/false,
       /*call_notify_observers_with_connection_type=*/
       NetworkChangeNotifier::CONNECTION_UNKNOWN},
      {/*seconds_from_start=*/11,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}},
     /*expected_poll_count=*/31},

    // Mirror of the above OnlineOfflineOnline test. Not nearly as likely in the
    // real world, but here for test coverage.
    {/*test_case_name=*/
     "ConnectionTypeChangeOfflineOnlineOfflineWithIpAddressChange",
     /*initial_connection_type=*/NetworkChangeNotifier::CONNECTION_NONE,
     /*first_polled_connection_type=*/
     NetworkChangeNotifier::CONNECTION_UNKNOWN,
     {{/*seconds_from_start=*/1,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true},
      {/*seconds_from_start=*/10,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/false,
       /*call_notify_observers_with_connection_type=*/
       NetworkChangeNotifier::CONNECTION_NONE},
      {/*seconds_from_start=*/11,
       /*connection_type_to_return=*/NetworkChangeNotifier::CONNECTION_UNKNOWN,
       /*expect_connection_type_changed_notification=*/true}},
     /*expected_poll_count=*/31},
};

class NetworkChangeNotifierWinPollTest
    : public NetworkChangeNotifierWinTest,
      public ::testing::WithParamInterface<PollTestCase> {};

INSTANTIATE_TEST_SUITE_P(,
                         NetworkChangeNotifierWinPollTest,
                         testing::ValuesIn(kPollTestCases),
                         [](const auto& info) {
                           return std::string(info.param.test_case_name);
                         });

TEST_P(NetworkChangeNotifierWinPollTest, PollTest) {
  RunPollTestCase(GetParam());
}

TEST_F(NetworkChangeNotifierWinTest, GetCurrentCost) {
  if (base::win::GetVersion() <
      NetworkCostChangeNotifierWin::kSupportedOsVersion) {
    GTEST_SKIP();
  }

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  // Wait for `NetworkCostChangeNotifierWin` to finish initializing.
  RunUntilIdle();

  EXPECT_EQ(GetCurrentConnectionCost(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  // Wait for `NetworkCostChangeNotifierWin` to handle the cost changed event.
  RunUntilIdle();

  EXPECT_EQ(GetCurrentConnectionCost(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);
}

TEST_F(NetworkChangeNotifierWinTest, CostChangeObserver) {
  if (base::win::GetVersion() <
      NetworkCostChangeNotifierWin::kSupportedOsVersion) {
    GTEST_SKIP();
  }

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_UNMETERED);

  // Wait for `NetworkCostChangeNotifierWin` to finish initializing.
  RunUntilIdle();

  TestConnectionCostObserver cost_observer;
  NetworkChangeNotifier::AddConnectionCostObserver(&cost_observer);

  fake_network_cost_manager_environment_.SetCost(
      NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  cost_observer.WaitForConnectionCostChanged();

  ASSERT_EQ(cost_observer.cost_changed_calls(), 1u);
  EXPECT_EQ(cost_observer.last_cost_changed_input(),
            NetworkChangeNotifier::ConnectionCost::CONNECTION_COST_METERED);

  NetworkChangeNotifier::RemoveConnectionCostObserver(&cost_observer);
}

// Uses the fake implementation of `INetworkCostManager` to simulate `GetCost()`
// returning an error `HRESULT`.
class NetworkChangeNotifierWinCostErrorTest
    : public NetworkChangeNotifierWinTest {
  void SetUp() override {
    if (base::win::GetVersion() <
        NetworkCostChangeNotifierWin::kSupportedOsVersion) {
      GTEST_SKIP();
    }

    fake_network_cost_manager_environment_.SimulateError(
        NetworkCostManagerStatus::kErrorGetCostFailed);

    NetworkChangeNotifierWinTest::SetUp();
  }
};

TEST_F(NetworkChangeNotifierWinCostErrorTest, CostError) {
  // Wait for `NetworkCostChangeNotifierWin` to finish initializing, which
  // should fail with an error.
  RunUntilIdle();

  // `NetworkChangeNotifierWin` must use the default implementation when
  // `NetworkCostChangeNotifierWin` returns an unknown cost.
  EXPECT_EQ(GetCurrentConnectionCost(),
            GetCurrentConnectionCostFromDefaultImplementationForTesting());
}

// Override the Windows OS version to simulate running on an OS that does not
// support `INetworkCostManager`.
class NetworkChangeNotifierWinCostUnsupportedOsTest
    : public NetworkChangeNotifierWinTest {
 public:
  NetworkChangeNotifierWinCostUnsupportedOsTest()
      : os_override_(base::test::ScopedOSInfoOverride::Type::kWinServer2016) {}

 protected:
  base::test::ScopedOSInfoOverride os_override_;
};

TEST_F(NetworkChangeNotifierWinCostUnsupportedOsTest, CostWithUnsupportedOS) {
  // Wait for `NetworkCostChangeNotifierWin` to finish initializing, which
  // should initialize with an unknown cost on an unsupported OS.
  RunUntilIdle();

  // `NetworkChangeNotifierWin` must use the default implementation when
  // `NetworkCostChangeNotifierWin` returns an unknown cost.
  EXPECT_EQ(GetCurrentConnectionCost(),
            GetCurrentConnectionCostFromDefaultImplementationForTesting());
}

}  // namespace net
