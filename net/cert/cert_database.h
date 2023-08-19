// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_CERT_DATABASE_H_
#define NET_CERT_CERT_DATABASE_H_

#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/observer_list_threadsafe.h"
#include "build/build_config.h"
#include "net/base/net_export.h"

namespace net {

// This class allows callers to observe changes to the underlying certificate
// stores.
//
// TODO(davidben): This class is really just a giant global ObserverList. It
// does not do anything with the platform certificate and, in principle, //net's
// dependency on the platform is abstracted behind the CertVerifier and
// ClientCertStore interfaces. Ideally these signals would originate out of
// those interfaces' platform implementations.

class NET_EXPORT CertDatabase {
 public:
  // A CertDatabase::Observer will be notified on certificate database changes.
  // The change could be either a user certificate is added/removed or trust on
  // a certificate is changed. Observers can be registered via
  // CertDatabase::AddObserver, and can un-register with
  // CertDatabase::RemoveObserver.
  class NET_EXPORT Observer {
   public:
    Observer(const Observer&) = delete;
    Observer& operator=(const Observer&) = delete;

    virtual ~Observer() = default;

    // Called whenever the Cert Database is known to have changed.
    // Typically, this will be in response to a CA certificate being added,
    // removed, or its trust changed.
    virtual void OnTrustStoreChanged() {}

    // Called when a potential change to client certificates is detected. (Some
    // platforms don't provide precise notifications and this may be notified
    // on unrelated changes.)
    virtual void OnClientCertStoreChanged() {}

   protected:
    Observer() = default;
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class HistogramNotificationType {
    kTrust = 0,
    kClientCert = 1,
    kMaxValue = kClientCert
  };

  ~CertDatabase() = delete;

  // Returns the CertDatabase singleton.
  static CertDatabase* GetInstance();

  CertDatabase(const CertDatabase&) = delete;
  CertDatabase& operator=(const CertDatabase&) = delete;

  // Registers |observer| to receive notifications of certificate changes.  The
  // thread on which this is called is the thread on which |observer| will be
  // called back with notifications.
  void AddObserver(Observer* observer);

  // Unregisters |observer| from receiving notifications.  This must be called
  // on the same thread on which AddObserver() was called.
  void RemoveObserver(Observer* observer);

#if BUILDFLAG(IS_MAC)
  // Start observing and forwarding events from Keychain services. May be
  // called multiple times, and may be called on any thread.
  static void StartListeningForKeychainEvents();
#endif

  // Synthetically injects notifications to all observers. In general, this
  // should only be called by the creator of the CertDatabase. Used to inject
  // notifications from other DB interfaces.
  void NotifyObserversTrustStoreChanged();
  void NotifyObserversClientCertStoreChanged();

 private:
  friend base::NoDestructor<CertDatabase>;

  CertDatabase();

  const scoped_refptr<base::ObserverListThreadSafe<Observer>> observer_list_;
};

}  // namespace net

#endif  // NET_CERT_CERT_DATABASE_H_
