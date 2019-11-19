// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_qualities_pref_delegate.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_estimator_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

class NetworkQualitiesPrefDelegateTest : public testing::Test {
 public:
  NetworkQualitiesPrefDelegateTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO) {}

  ~NetworkQualitiesPrefDelegateTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;

  DISALLOW_COPY_AND_ASSIGN(NetworkQualitiesPrefDelegateTest);
};

// Verify that prefs are writen and read correctly.
TEST_F(NetworkQualitiesPrefDelegateTest, WritingReadingToPrefsEnabled) {
  TestingPrefServiceSimple pref_service_simple;
  net::TestNetworkQualityEstimator estimator;
  NetworkQualitiesPrefDelegate::RegisterPrefs(pref_service_simple.registry());

  base::HistogramTester initial_histogram_tester;
  NetworkQualitiesPrefDelegate pref_delegate(&pref_service_simple, &estimator);
  // NetworkQualityEstimator must be notified of the read prefs at startup.
  EXPECT_FALSE(
      initial_histogram_tester.GetAllSamples("NQE.Prefs.ReadSize").empty());

  {
    base::HistogramTester histogram_tester;
    estimator.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    estimator.set_recent_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);
    estimator.RunOneRequest();

    // Prefs are written only if persistent caching was enabled.
    EXPECT_FALSE(
        histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
    histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

    // NetworkQualityEstimator should not be notified of change in prefs.
    histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
  }

  {
    base::HistogramTester histogram_tester;
    estimator.set_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    estimator.set_recent_effective_connection_type(
        net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);
    estimator.RunOneRequest();

    // Prefs are written even if the network id was unavailable.
    EXPECT_FALSE(
        histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
    histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

    // NetworkQualityEstimator should not be notified of change in prefs.
    histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
  }

  // Verify the contents of the prefs by reading them again.
  std::map<net::nqe::internal::NetworkID,
           net::nqe::internal::CachedNetworkQuality>
      read_prefs = pref_delegate.ForceReadPrefsForTesting();
  // Number of entries must be between 1 and 2. It's possible that 2 entries
  // are added if the connection type is unknown to network quality estimator
  // at the time of startup, and shortly after it receives a notification
  // about the change in the connection type.
  EXPECT_LE(1u, read_prefs.size());
  EXPECT_GE(2u, read_prefs.size());

  // Verify that the cached network quality was written correctly.
  EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
            read_prefs.begin()->second.effective_connection_type());
  if (net::NetworkChangeNotifier::GetConnectionType() ==
      net::NetworkChangeNotifier::CONNECTION_ETHERNET) {
    // Verify that the network ID was written correctly.
    net::nqe::internal::NetworkID ethernet_network_id(
        net::NetworkChangeNotifier::CONNECTION_ETHERNET, std::string(),
        INT32_MIN);
    EXPECT_EQ(ethernet_network_id, read_prefs.begin()->first);
  }
}

}  // namespace

}  // namespace network
