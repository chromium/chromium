// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/log/test_net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
class Observer : public CertDatabase::Observer {
 public:
  void OnTrustStoreChanged() override { trust_store_change_count_++; }

  void OnClientCertStoreChanged() override { client_cert_change_count_++; }

  int trust_store_change_count_ = 0;
  int client_cert_change_count_ = 0;
};

}  // namespace

TEST(CertDatabaseTest, Notifications) {
  base::test::SingleThreadTaskEnvironment task_environment;

  CertDatabase* cert_database = CertDatabase::GetInstance();
  ASSERT_TRUE(cert_database);

  Observer observer_1;
  Observer observer_2;

  cert_database->AddObserver(&observer_1);
  cert_database->AddObserver(&observer_2);

  {
    RecordingNetLogObserver net_log_observer;
    base::HistogramTester histograms;
    cert_database->NotifyObserversClientCertStoreChanged();
    task_environment.RunUntilIdle();

    EXPECT_EQ(observer_1.trust_store_change_count_, 0);
    EXPECT_EQ(observer_1.client_cert_change_count_, 1);
    EXPECT_EQ(observer_2.trust_store_change_count_,
              observer_1.trust_store_change_count_);
    EXPECT_EQ(observer_2.client_cert_change_count_,
              observer_1.client_cert_change_count_);

    EXPECT_EQ(net_log_observer.GetEntries().size(), 1u);
    EXPECT_EQ(
        net_log_observer
            .GetEntriesWithType(
                NetLogEventType::CERTIFICATE_DATABASE_CLIENT_CERT_STORE_CHANGED)
            .size(),
        1u);

    histograms.ExpectUniqueSample(
        "Net.Certificate.ChangeNotification",
        CertDatabase::HistogramNotificationType::kClientCert, 1);
  }

  {
    RecordingNetLogObserver net_log_observer;
    base::HistogramTester histograms;
    cert_database->NotifyObserversTrustStoreChanged();
    task_environment.RunUntilIdle();

    EXPECT_EQ(observer_1.trust_store_change_count_, 1);
    EXPECT_EQ(observer_1.client_cert_change_count_, 1);
    EXPECT_EQ(observer_2.trust_store_change_count_,
              observer_1.trust_store_change_count_);
    EXPECT_EQ(observer_2.client_cert_change_count_,
              observer_1.client_cert_change_count_);

    EXPECT_EQ(net_log_observer.GetEntries().size(), 1u);
    EXPECT_EQ(net_log_observer
                  .GetEntriesWithType(
                      NetLogEventType::CERTIFICATE_DATABASE_TRUST_STORE_CHANGED)
                  .size(),
              1u);

    histograms.ExpectUniqueSample(
        "Net.Certificate.ChangeNotification",
        CertDatabase::HistogramNotificationType::kTrust, 1);
  }

  cert_database->RemoveObserver(&observer_1);
  cert_database->RemoveObserver(&observer_2);
}

}  // namespace net
