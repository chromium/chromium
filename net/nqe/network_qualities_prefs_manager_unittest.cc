// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/nqe/network_qualities_prefs_manager.h"

#include <algorithm>
#include <map>
#include <memory>

#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "net/nqe/network_quality_store.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

class TestPrefDelegate : public NetworkQualitiesPrefsManager::PrefDelegate {
 public:
  TestPrefDelegate() = default;

  TestPrefDelegate(const TestPrefDelegate&) = delete;
  TestPrefDelegate& operator=(const TestPrefDelegate&) = delete;

  ~TestPrefDelegate() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  void SetDictionaryValue(const base::Value::Dict& dict) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    write_count_++;
    value_ = dict.Clone();
    ASSERT_EQ(dict.size(), value_.size());
  }

  base::Value::Dict GetDictionaryValue() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    read_count_++;
    return value_.Clone();
  }

  size_t write_count() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return write_count_;
  }

  size_t read_count() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return read_count_;
  }

 private:
  // Number of times prefs were written and read, respectively..
  size_t write_count_ = 0;
  size_t read_count_ = 0;

  // Current value of the prefs.
  base::Value::Dict value_;

  SEQUENCE_CHECKER(sequence_checker_);
};

using NetworkQualitiesPrefManager = TestWithTaskEnvironment;

TEST_F(NetworkQualitiesPrefManager, Write) {
  // Force set the ECT to Slow 2G so that the ECT does not match the default
  // ECT for the current connection type. This forces the prefs to be written
  // for the current connection.
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "Slow-2G";
  TestNetworkQualityEstimator estimator(variation_params);

  auto prefs_delegate = std::make_unique<TestPrefDelegate>();
  TestPrefDelegate* prefs_delegate_ptr = prefs_delegate.get();

  NetworkQualitiesPrefsManager manager(std::move(prefs_delegate));
  manager.InitializeOnNetworkThread(&estimator);
  base::RunLoop().RunUntilIdle();

  // Prefs must be read at when NetworkQualitiesPrefsManager is constructed.
  EXPECT_EQ(2u, prefs_delegate_ptr->read_count());

  estimator.SimulateNetworkChange(
      NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN, "test");
  EXPECT_EQ(3u, prefs_delegate_ptr->write_count());
  // Network quality generated from the default observation must be written.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, prefs_delegate_ptr->write_count());

  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_2G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality.
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, prefs_delegate_ptr->write_count());

  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_3G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality..
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, prefs_delegate_ptr->write_count());

  // Prefs should not be read again.
  EXPECT_EQ(2u, prefs_delegate_ptr->read_count());

  manager.ShutdownOnPrefSequence();
}

TEST_F(NetworkQualitiesPrefManager, WriteWhenMatchingExpectedECT) {
  // Force set the ECT to Slow 2G so that the ECT does not match the default
  // ECT for the current connection type. This forces the prefs to be written
  // for the current connection.
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "Slow-2G";
  TestNetworkQualityEstimator estimator(variation_params);

  auto prefs_delegate = std::make_unique<TestPrefDelegate>();
  TestPrefDelegate* prefs_delegate_ptr = prefs_delegate.get();

  NetworkQualitiesPrefsManager manager(std::move(prefs_delegate));
  manager.InitializeOnNetworkThread(&estimator);
  base::RunLoop().RunUntilIdle();

  // Prefs must be read at when NetworkQualitiesPrefsManager is constructed.
  EXPECT_EQ(2u, prefs_delegate_ptr->read_count());

  const nqe::internal::NetworkID network_id(
      NetworkChangeNotifier::ConnectionType::CONNECTION_4G, "test", INT32_MIN);

  estimator.SimulateNetworkChange(network_id.type, network_id.id);
  EXPECT_EQ(3u, prefs_delegate_ptr->write_count());
  // Network quality generated from the default observation must be written.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(3u, prefs_delegate_ptr->write_count());

  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_2G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality.
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(4u, prefs_delegate_ptr->write_count());

  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_3G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality..
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(5u, prefs_delegate_ptr->write_count());

  // Prefs should not be read again.
  EXPECT_EQ(2u, prefs_delegate_ptr->read_count());

  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());
  EXPECT_EQ(EFFECTIVE_CONNECTION_TYPE_3G,
            manager.ForceReadPrefsForTesting()
                .find(network_id)
                ->second.effective_connection_type());

  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_4G);
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();

  // Network Quality should be persisted to disk even if it matches the typical
  // quality of the network. See crbug.com/890859.
  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());
  EXPECT_EQ(1u, manager.ForceReadPrefsForTesting().count(network_id));
  EXPECT_EQ(6u, prefs_delegate_ptr->write_count());

  manager.ShutdownOnPrefSequence();
}

TEST_F(NetworkQualitiesPrefManager, WriteAndReadWithMultipleNetworkIDs) {
  static const size_t kMaxCacheSize = 20u;

  // Force set the ECT to Slow 2G so that the ECT does not match the default
  // ECT for the current connection type. This forces the prefs to be written
  // for the current connection.
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "Slow-2G";
  TestNetworkQualityEstimator estimator(variation_params);

  auto prefs_delegate = std::make_unique<TestPrefDelegate>();

  NetworkQualitiesPrefsManager manager(std::move(prefs_delegate));
  manager.InitializeOnNetworkThread(&estimator);
  base::RunLoop().RunUntilIdle();

  estimator.SimulateNetworkChange(
      NetworkChangeNotifier::ConnectionType::CONNECTION_2G, "test");

  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());

  estimator.set_recent_effective_connection_type(
      EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality.
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  // Verify that the observer was notified, and the updated network quality was
  // written to the prefs.
  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());

  // Change the network ID.
  for (size_t i = 0; i < kMaxCacheSize; ++i) {
    estimator.SimulateNetworkChange(
        NetworkChangeNotifier::ConnectionType::CONNECTION_2G,
        "test" + base::NumberToString(i));

    estimator.RunOneRequest();
    base::RunLoop().RunUntilIdle();

    EXPECT_EQ(std::min(i + 3, kMaxCacheSize),
              manager.ForceReadPrefsForTesting().size());
  }

  std::map<nqe::internal::NetworkID, nqe::internal::CachedNetworkQuality>
      read_prefs = manager.ForceReadPrefsForTesting();

  // Verify the contents of the prefs.
  size_t count_2g_entries = 0;
  for (std::map<nqe::internal::NetworkID,
                nqe::internal::CachedNetworkQuality>::const_iterator it =
           read_prefs.begin();
       it != read_prefs.end(); ++it) {
    if (it->first.type ==
        NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN) {
      continue;
    }
    EXPECT_EQ(0u, it->first.id.find("test", 0u));
    EXPECT_EQ(NetworkChangeNotifier::ConnectionType::CONNECTION_2G,
              it->first.type);
    EXPECT_EQ(EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
              it->second.effective_connection_type());
    ++count_2g_entries;
  }

  // At most one entry should be for the network with connection type
  // NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN.
  EXPECT_LE(kMaxCacheSize - 1, count_2g_entries);

  estimator.OnPrefsRead(read_prefs);

  manager.ShutdownOnPrefSequence();
}

// Verifies that the prefs are cleared correctly.
TEST_F(NetworkQualitiesPrefManager, ClearPrefs) {
  // Force set the ECT to Slow 2G so that the ECT does not match the default
  // ECT for the current connection type. This forces the prefs to be written
  // for the current connection.
  std::map<std::string, std::string> variation_params;
  variation_params["force_effective_connection_type"] = "Slow-2G";
  TestNetworkQualityEstimator estimator(variation_params);

  auto prefs_delegate = std::make_unique<TestPrefDelegate>();

  NetworkQualitiesPrefsManager manager(std::move(prefs_delegate));
  manager.InitializeOnNetworkThread(&estimator);
  base::RunLoop().RunUntilIdle();

  estimator.SimulateNetworkChange(
      NetworkChangeNotifier::ConnectionType::CONNECTION_UNKNOWN, "test");

  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());

  estimator.set_recent_effective_connection_type(
      EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality.
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  // Verify that the observer was notified, and the updated network quality was
  // written to the prefs.
  EXPECT_EQ(2u, manager.ForceReadPrefsForTesting().size());

  // Prefs must be completely cleared.
  manager.ClearPrefs();
  EXPECT_EQ(0u, manager.ForceReadPrefsForTesting().size());
  estimator.set_recent_effective_connection_type(EFFECTIVE_CONNECTION_TYPE_2G);
  // Run a request so that effective connection type is recomputed, and
  // observers are notified of change in the network quality.
  estimator.RunOneRequest();
  base::RunLoop().RunUntilIdle();
  // Verify that the observer was notified, and the updated network quality was
  // written to the prefs.
  EXPECT_EQ(1u, manager.ForceReadPrefsForTesting().size());
  manager.ShutdownOnPrefSequence();
}

}  // namespace

}  // namespace net
