// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/network_connection_tracker.h"

#include <memory>
#include <tuple>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mock_network_change_notifier.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class TestNetworkConnectionObserver
    : public NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit TestNetworkConnectionObserver(NetworkConnectionTracker* tracker)
      : num_notifications_(0),
        tracker_(tracker),
        expected_connection_type_(
            network::mojom::ConnectionType::CONNECTION_UNKNOWN),
        connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {
    tracker_->AddNetworkConnectionObserver(this);
  }

  TestNetworkConnectionObserver(const TestNetworkConnectionObserver&) = delete;
  TestNetworkConnectionObserver& operator=(
      const TestNetworkConnectionObserver&) = delete;

  ~TestNetworkConnectionObserver() override {
    tracker_->RemoveNetworkConnectionObserver(this);
  }

  // Helper to synchronously get connection type from NetworkConnectionTracker.
  network::mojom::ConnectionType GetConnectionTypeSync() {
    network::mojom::ConnectionType type;
    base::RunLoop run_loop;
    bool sync = tracker_->GetConnectionType(
        &type, base::BindOnce(
                   &TestNetworkConnectionObserver::GetConnectionTypeCallback,
                   &run_loop, &type));
    if (!sync)
      run_loop.Run();
    return type;
  }

  // NetworkConnectionObserver implementation:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    EXPECT_EQ(type, GetConnectionTypeSync());

    num_notifications_++;
    connection_type_ = type;
    if (run_loop_ && expected_connection_type_ == type)
      run_loop_->Quit();
  }

  size_t num_notifications() const { return num_notifications_; }
  void WaitForNotification(
      network::mojom::ConnectionType expected_connection_type) {
    expected_connection_type_ = expected_connection_type;

    if (connection_type_ == expected_connection_type)
      return;
    // WaitForNotification() should not be called twice.
    EXPECT_EQ(nullptr, run_loop_);
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
  }

  network::mojom::ConnectionType connection_type() const {
    return connection_type_;
  }

 private:
  static void GetConnectionTypeCallback(base::RunLoop* run_loop,
                                        network::mojom::ConnectionType* out,
                                        network::mojom::ConnectionType type) {
    *out = type;
    run_loop->Quit();
  }

  size_t num_notifications_;
  raw_ptr<NetworkConnectionTracker> tracker_;
  // May be null.
  std::unique_ptr<base::RunLoop> run_loop_;
  network::mojom::ConnectionType expected_connection_type_;
  network::mojom::ConnectionType connection_type_;
};

class TestLeakyNetworkConnectionObserver
    : public NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  explicit TestLeakyNetworkConnectionObserver(NetworkConnectionTracker* tracker)
      : run_loop_(std::make_unique<base::RunLoop>()),
        connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {
    tracker->AddLeakyNetworkConnectionObserver(this);
  }

  TestLeakyNetworkConnectionObserver(
      const TestLeakyNetworkConnectionObserver&) = delete;
  TestLeakyNetworkConnectionObserver& operator=(
      const TestLeakyNetworkConnectionObserver&) = delete;

  // NetworkConnectionObserver implementation:
  void OnConnectionChanged(network::mojom::ConnectionType type) override {
    connection_type_ = type;
    run_loop_->Quit();
  }

  void WaitForNotification() {
    run_loop_->Run();
    run_loop_ = std::make_unique<base::RunLoop>();
  }

  network::mojom::ConnectionType connection_type() const {
    return connection_type_;
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  network::mojom::ConnectionType connection_type_;
};

// A helper class to call NetworkConnectionTracker::GetConnectionType().
class ConnectionTypeGetter {
 public:
  explicit ConnectionTypeGetter(NetworkConnectionTracker* tracker)
      : tracker_(tracker),
        connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN) {}

  ConnectionTypeGetter(const ConnectionTypeGetter&) = delete;
  ConnectionTypeGetter& operator=(const ConnectionTypeGetter&) = delete;

  ~ConnectionTypeGetter() {}

  bool GetConnectionType() {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return tracker_->GetConnectionType(
        &connection_type_,
        base::BindOnce(&ConnectionTypeGetter::OnGetConnectionType,
                       base::Unretained(this)));
  }

  void WaitForConnectionType(
      network::mojom::ConnectionType expected_connection_type) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    run_loop_.Run();
    EXPECT_EQ(expected_connection_type, connection_type_);
  }

  network::mojom::ConnectionType connection_type() const {
    return connection_type_;
  }

 private:
  void OnGetConnectionType(network::mojom::ConnectionType type) {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    connection_type_ = type;
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  raw_ptr<NetworkConnectionTracker> tracker_;
  network::mojom::ConnectionType connection_type_;
  THREAD_CHECKER(thread_checker_);
};

}  // namespace

class NetworkConnectionTrackerTest : public testing::Test {
 public:
  NetworkConnectionTrackerTest()
      : mock_network_change_notifier_(
            net::test::MockNetworkChangeNotifier::Create()) {}

  NetworkConnectionTrackerTest(const NetworkConnectionTrackerTest&) = delete;
  NetworkConnectionTrackerTest& operator=(const NetworkConnectionTrackerTest&) =
      delete;

  ~NetworkConnectionTrackerTest() override {}

  void Initialize() {
    mojo::PendingRemote<network::mojom::NetworkService> network_service_remote;
    network_service_ = NetworkService::Create(
        network_service_remote.InitWithNewPipeAndPassReceiver());
    tracker_ = std::make_unique<NetworkConnectionTracker>(base::BindRepeating(
        &NetworkConnectionTrackerTest::BindReceiver, base::Unretained(this)));
    observer_ = std::make_unique<TestNetworkConnectionObserver>(tracker_.get());
  }

  void BindReceiver(
      mojo::PendingReceiver<network::mojom::NetworkChangeManager> receiver) {
    network_service_->GetNetworkChangeManager(std::move(receiver));
  }

  NetworkConnectionTracker* network_connection_tracker() {
    return tracker_.get();
  }

  TestNetworkConnectionObserver* network_connection_observer() {
    return observer_.get();
  }

  // Simulates a connection type change and broadcast it to observers.
  void SimulateConnectionTypeChange(
      net::NetworkChangeNotifier::ConnectionType type) {
    mock_network_change_notifier_->NotifyObserversOfNetworkChangeForTests(type);
  }

  // Sets the current connection type of the mock network change notifier.
  void SetConnectionType(net::NetworkChangeNotifier::ConnectionType type) {
    mock_network_change_notifier_->SetConnectionType(type);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<net::test::MockNetworkChangeNotifier>
      mock_network_change_notifier_;
  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkConnectionTracker> tracker_;
  std::unique_ptr<TestNetworkConnectionObserver> observer_;
};

TEST_F(NetworkConnectionTrackerTest, ObserverNotified) {
  Initialize();
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_UNKNOWN,
            network_connection_observer()->connection_type());

  // Simulate a network change.
  SimulateConnectionTypeChange(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);

  network_connection_observer()->WaitForNotification(
      network::mojom::ConnectionType::CONNECTION_3G);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_3G,
            network_connection_observer()->connection_type());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1u, network_connection_observer()->num_notifications());
}

TEST_F(NetworkConnectionTrackerTest, UnregisteredObserverNotNotified) {
  Initialize();
  auto network_connection_observer2 =
      std::make_unique<TestNetworkConnectionObserver>(
          network_connection_tracker());

  // Simulate a network change.
  SimulateConnectionTypeChange(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);

  network_connection_observer2->WaitForNotification(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_WIFI,
            network_connection_observer2->connection_type());
  network_connection_observer()->WaitForNotification(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_WIFI,
            network_connection_observer()->connection_type());
  base::RunLoop().RunUntilIdle();

  network_connection_observer2.reset();

  // Simulate an another network change.
  SimulateConnectionTypeChange(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_2G);
  network_connection_observer()->WaitForNotification(
      network::mojom::ConnectionType::CONNECTION_2G);
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_2G,
            network_connection_observer()->connection_type());
  EXPECT_EQ(2u, network_connection_observer()->num_notifications());
}

TEST_F(NetworkConnectionTrackerTest, LeakyObserversCanLeak) {
  Initialize();
  auto leaky_network_connection_observer =
      std::make_unique<TestLeakyNetworkConnectionObserver>(
          network_connection_tracker());

  // Simulate a network change.
  SimulateConnectionTypeChange(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);

  leaky_network_connection_observer->WaitForNotification();
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_3G,
            leaky_network_connection_observer->connection_type());
  base::RunLoop().RunUntilIdle();
  // The leaky observer is never unregistered.
}

TEST_F(NetworkConnectionTrackerTest, GetConnectionType) {
  SetConnectionType(net::NetworkChangeNotifier::ConnectionType::CONNECTION_3G);
  // Creates a  NetworkService now so it initializes a NetworkChangeManager
  // with initial connection type as CONNECTION_3G.
  Initialize();

  ConnectionTypeGetter getter1(network_connection_tracker());
  ConnectionTypeGetter getter2(network_connection_tracker());
  // These two GetConnectionType() will finish asynchonously because network
  // service is not yet set up.
  EXPECT_FALSE(getter1.GetConnectionType());
  EXPECT_FALSE(getter2.GetConnectionType());

  getter1.WaitForConnectionType(
      /*expected_connection_type=*/network::mojom::ConnectionType::
          CONNECTION_3G);
  getter2.WaitForConnectionType(
      /*expected_connection_type=*/network::mojom::ConnectionType::
          CONNECTION_3G);

  ConnectionTypeGetter getter3(network_connection_tracker());
  // This GetConnectionType() should finish synchronously.
  EXPECT_TRUE(getter3.GetConnectionType());
  EXPECT_EQ(network::mojom::ConnectionType::CONNECTION_3G,
            getter3.connection_type());
}

// Tests that GetConnectionType returns false and doesn't modify its |type|
// parameter when the connection type is unavailable.
TEST_F(NetworkConnectionTrackerTest, GetConnectionTypeUnavailable) {
  // Returns a dummy network service that has not been initialized.
  mojo::Remote<network::mojom::NetworkService>* network_service_remote =
      new mojo::Remote<network::mojom::NetworkService>;

  std::ignore = network_service_remote->BindNewPipeAndPassReceiver();
  NetworkConnectionTracker::BindingCallback callback = base::BindRepeating(
      [](network::mojom::NetworkService* service,
         mojo::PendingReceiver<network::mojom::NetworkChangeManager> receiver) {
        return service->GetNetworkChangeManager(std::move(receiver));
      },
      base::Unretained(network_service_remote->get()));

  auto tracker = std::make_unique<NetworkConnectionTracker>(callback);
  auto type = network::mojom::ConnectionType::CONNECTION_3G;
  bool sync = tracker->GetConnectionType(&type, base::DoNothing());

  EXPECT_FALSE(sync);
  EXPECT_EQ(type, network::mojom::ConnectionType::CONNECTION_3G);
  delete network_service_remote;
}

// Tests GetConnectionType() on a different thread.
class NetworkGetConnectionTest : public NetworkConnectionTrackerTest {
 public:
  NetworkGetConnectionTest()
      : getter_thread_("NetworkGetConnectionTestThread") {
    getter_thread_.Start();
    Initialize();
  }

  NetworkGetConnectionTest(const NetworkGetConnectionTest&) = delete;
  NetworkGetConnectionTest& operator=(const NetworkGetConnectionTest&) = delete;

  ~NetworkGetConnectionTest() override {}

  void GetConnectionType() {
    DCHECK(getter_thread_.task_runner()->RunsTasksInCurrentSequence());
    getter_ =
        std::make_unique<ConnectionTypeGetter>(network_connection_tracker());
    EXPECT_FALSE(getter_->GetConnectionType());
  }

  void WaitForConnectionType(
      network::mojom::ConnectionType expected_connection_type) {
    DCHECK(getter_thread_.task_runner()->RunsTasksInCurrentSequence());
    getter_->WaitForConnectionType(expected_connection_type);
  }

  base::Thread* getter_thread() { return &getter_thread_; }

 private:
  base::Thread getter_thread_;

  // Accessed on |getter_thread_|.
  std::unique_ptr<ConnectionTypeGetter> getter_;
};

TEST_F(NetworkGetConnectionTest, GetConnectionTypeOnDifferentThread) {
  // Flush pending OnInitialConnectionType() notification and force |tracker| to
  // use async for GetConnectionType() calls.
  base::RunLoop().RunUntilIdle();
  base::subtle::NoBarrier_Store(&network_connection_tracker()->connection_type_,
                                -1);
  {
    base::RunLoop run_loop;
    getter_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&NetworkGetConnectionTest::GetConnectionType,
                       base::Unretained(this)),
        base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                       base::Unretained(&run_loop)));
    run_loop.Run();
  }

  network_connection_tracker()->OnInitialConnectionType(
      network::mojom::ConnectionType::CONNECTION_3G);
  {
    base::RunLoop run_loop;
    getter_thread()->task_runner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&NetworkGetConnectionTest::WaitForConnectionType,
                       base::Unretained(this),
                       /*expected_connection_type=*/
                       network::mojom::ConnectionType::CONNECTION_3G),
        base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                       base::Unretained(&run_loop)));
    run_loop.Run();
  }
}

}  // namespace network
