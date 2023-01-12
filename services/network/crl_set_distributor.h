// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_CRL_SET_DISTRIBUTOR_H_
#define SERVICES_NETWORK_CRL_SET_DISTRIBUTOR_H_

#include <stdint.h>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "net/cert/crl_set.h"

namespace network {

// CRLSetDistributor is a helper class to handle fan-out distribution of
// new CRLSets. As new encoded CRLSets are received (via OnNewCRLSet), they
// will be parsed and, if successful and a later sequence than the current
// CRLSet, dispatched to CRLSetDistributor::Observers' OnNewCRLSet().
class COMPONENT_EXPORT(NETWORK_SERVICE) CRLSetDistributor {
 public:
  class Observer {
   public:
    // Called whenever a new CRLSet, |crl_set|, has been received.
    virtual void OnNewCRLSet(scoped_refptr<net::CRLSet> crl_set) = 0;

   protected:
    virtual ~Observer() = default;
  };

  CRLSetDistributor();
  ~CRLSetDistributor();

  // Adds an observer to be notified when new CRLSets are available.
  // Note: Newly-added observers are not notified on the current |crl_set()|,
  // only newly configured CRLSets after the AddObserver call.
  void AddObserver(Observer* observer);
  // Removes a previously registered observer.
  void RemoveObserver(Observer* observer);

  // Returns the currently configured CRLSet, or nullptr if one has not yet
  // been configured.
  scoped_refptr<net::CRLSet> crl_set() const { return crl_set_; }

  // Notifies the distributor that a new encoded CRLSet, |crl_set|, has been
  // received. If the CRLSet successfully decodes and is newer than the
  // current CRLSet, all observers will be notified. |callback| will be
  // notified once all observers have been notified. |callback| is guaranteed
  // to run (e.g. even if this object is deleted prior to it being run).
  void OnNewCRLSet(base::span<const uint8_t> crl_set,
                   base::OnceClosure callback);

 private:
  void OnCRLSetParsed(scoped_refptr<net::CRLSet> crl_set);

  base::ObserverList<Observer,
                     true /*check_empty*/,
                     false /*allow_reentrancy*/>::Unchecked observers_;
  scoped_refptr<net::CRLSet> crl_set_;

  base::WeakPtrFactory<CRLSetDistributor> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_CRL_SET_DISTRIBUTOR_H_
