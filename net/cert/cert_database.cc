// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/cert_database.h"

#include "base/metrics/histogram_functions.h"
#include "base/observer_list_threadsafe.h"
#include "build/build_config.h"
#include "net/log/net_log.h"
#include "net/log/net_log_values.h"

namespace net {

namespace {

void RecordNotificationHistogram(CertDatabase::HistogramNotificationType type) {
  base::UmaHistogramEnumeration("Net.Certificate.ChangeNotification", type);
}

}  // namespace

// static
CertDatabase* CertDatabase::GetInstance() {
  static base::NoDestructor<CertDatabase> cert_database;
  return cert_database.get();
}

void CertDatabase::AddObserver(Observer* observer) {
  observer_list_->AddObserver(observer);
}

void CertDatabase::RemoveObserver(Observer* observer) {
  observer_list_->RemoveObserver(observer);
}

void CertDatabase::NotifyObserversTrustStoreChanged() {
  // Log to NetLog as it may help debug issues like https://crbug.com/915463
  // This isn't guarded with net::NetLog::Get()->IsCapturing()) because an
  // AddGlobalEntry() call without much computation is really cheap.
  net::NetLog::Get()->AddGlobalEntry(
      NetLogEventType::CERTIFICATE_DATABASE_TRUST_STORE_CHANGED);

  RecordNotificationHistogram(HistogramNotificationType::kTrust);

  observer_list_->Notify(FROM_HERE, &Observer::OnTrustStoreChanged);
}

void CertDatabase::NotifyObserversClientCertStoreChanged() {
  // Log to NetLog as it may help debug issues like https://crbug.com/915463
  // This isn't guarded with net::NetLog::Get()->IsCapturing()) because an
  // AddGlobalEntry() call without much computation is really cheap.
  net::NetLog::Get()->AddGlobalEntry(
      NetLogEventType::CERTIFICATE_DATABASE_CLIENT_CERT_STORE_CHANGED);

  RecordNotificationHistogram(HistogramNotificationType::kClientCert);

  observer_list_->Notify(FROM_HERE, &Observer::OnClientCertStoreChanged);
}

CertDatabase::CertDatabase()
    : observer_list_(
          base::MakeRefCounted<base::ObserverListThreadSafe<Observer>>()) {}

}  // namespace net
