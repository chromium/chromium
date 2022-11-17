// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_CROSS_THREAD_PENDING_SHARED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_CROSS_THREAD_PENDING_SHARED_URL_LOADER_FACTORY_H_

#include "base/component_export.h"
#include "base/memory/scoped_refptr.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {

// A PendingSharedURLLoaderFactory that wraps a SharedURLLoaderFactory.  The
// PendingSharedURLLoaderFactory can be used on any thread to create a new
// SharedURLLoaderFactory that will post tasks to another thread to invoke
// methods on the original factory. SharedURLLoaderFactory subclasses can use
// this class to easily implement the Clone() method.
//
// It must be created on the thread |base_factory| lives on.  Note that if
// objects created via it are indeed used on a different thread from
// |base_factory|'s, an extra thread hop will be introduced.
class COMPONENT_EXPORT(NETWORK_CPP) CrossThreadPendingSharedURLLoaderFactory
    : public PendingSharedURLLoaderFactory {
 public:
  explicit CrossThreadPendingSharedURLLoaderFactory(
      scoped_refptr<SharedURLLoaderFactory> base_factory);
  ~CrossThreadPendingSharedURLLoaderFactory() override;

 protected:
  scoped_refptr<SharedURLLoaderFactory> CreateFactory() override;

 private:
  friend class CrossThreadSharedURLLoaderFactory;

  class State;
  struct StateDeleterTraits;

  // This constructor is used when something equivalent to
  // this->CreateFactory()->Clone() occurs, sharing information on underlying
  // SharedURLLoaderFactory and its task runner with the new
  // CrossThreadPendingSharedURLLoaderFactory object.
  explicit CrossThreadPendingSharedURLLoaderFactory(scoped_refptr<State> state);

  scoped_refptr<State> state_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_CROSS_THREAD_PENDING_SHARED_URL_LOADER_FACTORY_H_
