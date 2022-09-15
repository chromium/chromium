// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_SHARED_URL_LOADER_FACTORY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_SHARED_URL_LOADER_FACTORY_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace network {

class PendingSharedURLLoaderFactory;

// This is a ref-counted C++ interface to replace raw mojom::URLLoaderFactory
// pointers and various factory getters.
// A SharedURLLoaderFactory instance is supposed to be used on a single
// sequence. To use it on a different sequence, use Clone() and pass the
// resulting PendingSharedURLLoaderFactory instance to the target sequence. On
// the target sequence, call SharedURLLoaderFactory::Create() to convert it to a
// new SharedURLLoaderFactory.
class COMPONENT_EXPORT(NETWORK_CPP) SharedURLLoaderFactory
    : public base::RefCounted<SharedURLLoaderFactory>,
      public mojom::URLLoaderFactory {
 public:
  static scoped_refptr<SharedURLLoaderFactory> Create(
      std::unique_ptr<PendingSharedURLLoaderFactory> pending_factory);

  // From mojom::URLLoaderFactory:
  void Clone(mojo::PendingReceiver<mojom::URLLoaderFactory> receiver) override =
      0;

  // If implemented, creates a PendingSharedURLLoaderFactory that can be used on
  // any thread to create a SharedURLLoaderFactory that works there.
  //
  // A simple way of providing this API is to use
  // CrossThreadPendingSharedURLLoaderFactory; which will implement it at cost
  // of a single thread hop on any different-thread fetch.
  virtual std::unique_ptr<PendingSharedURLLoaderFactory> Clone() = 0;

  // If this returns true, any redirect safety checks should be bypassed.
  virtual bool BypassRedirectChecks() const;

 protected:
  friend class base::RefCounted<SharedURLLoaderFactory>;
  ~SharedURLLoaderFactory() override;
};

// PendingSharedURLLoaderFactory contains necessary information to construct a
// SharedURLLoaderFactory. It is not sequence safe but can be passed across
// sequences. Please see the comments of SharedURLLoaderFactory for how this
// class is used.
class COMPONENT_EXPORT(NETWORK_CPP) PendingSharedURLLoaderFactory {
 public:
  PendingSharedURLLoaderFactory();

  PendingSharedURLLoaderFactory(const PendingSharedURLLoaderFactory&) = delete;
  PendingSharedURLLoaderFactory& operator=(
      const PendingSharedURLLoaderFactory&) = delete;

  virtual ~PendingSharedURLLoaderFactory();

 protected:
  friend class SharedURLLoaderFactory;

  // Creates a SharedURLLoaderFactory. It should only be called by
  // SharedURLLoaderFactory::Create(), which makes sense that CreateFactory() is
  // never called multiple times for each PendingSharedURLLoaderFactory
  // instance.
  virtual scoped_refptr<SharedURLLoaderFactory> CreateFactory() = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_SHARED_URL_LOADER_FACTORY_H_
