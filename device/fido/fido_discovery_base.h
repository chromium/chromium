// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_
#define DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_

#include <vector>

#include <ostream>

#include "base/check.h"
#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoAuthenticator;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDiscoveryBase {
 public:
  // EventStream is an unbuffered pipe that can be passed around and late-bound
  // to the receiver.
  template <typename T>
  class EventStream {
   public:
    using Callback = base::RepeatingCallback<void(T)>;

    // New returns a callback for writing events, and ownership of an
    // |EventStream| that can be connected to in order to receive the events.
    // The callback may outlive the |EventStream|. Any events written when
    // either the |EventStream| has been deleted, or not yet connected, are
    // dropped.
    static std::pair<Callback, std::unique_ptr<EventStream<T>>> New() {
      auto stream = std::make_unique<EventStream<T>>();
      auto cb = base::BindRepeating(&EventStream::Transmit,
                                    stream->weak_factory_.GetWeakPtr());
      return std::make_pair(std::move(cb), std::move(stream));
    }

    void Connect(Callback connection) { connection_ = std::move(connection); }

   private:
    void Transmit(T t) {
      if (connection_) {
        connection_.Run(std::move(t));
      }
    }

    Callback connection_;
    base::WeakPtrFactory<EventStream<T>> weak_factory_{this};
  };

  FidoDiscoveryBase(const FidoDiscoveryBase&) = delete;
  FidoDiscoveryBase& operator=(const FidoDiscoveryBase&) = delete;

  virtual ~FidoDiscoveryBase();

  class COMPONENT_EXPORT(DEVICE_FIDO) Observer {
   public:
    virtual ~Observer();

    // It is guaranteed that this is never invoked synchronously from Start().
    // |authenticators| is the list of authenticators discovered upon start.
    virtual void DiscoveryStarted(
        FidoDiscoveryBase* discovery,
        bool success,
        std::vector<FidoAuthenticator*> authenticators = {}) {}

    // Called after DiscoveryStarted for any devices discovered after
    // initialization.
    virtual void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                                    FidoAuthenticator* authenticator) = 0;
    virtual void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                                      FidoAuthenticator* authenticator) = 0;
  };

  // Start authenticator discovery. The Observer must have been set before this
  // method is invoked. DiscoveryStarted must be invoked asynchronously from
  // this method.
  virtual void Start() = 0;

  // Stop discovering new devices.
  virtual void Stop();

  Observer* observer() const { return observer_; }
  void set_observer(Observer* observer) {
    DCHECK(!observer_ || !observer) << "Only one observer is supported.";
    observer_ = observer;
  }
  FidoTransportProtocol transport() const { return transport_; }

 protected:
  explicit FidoDiscoveryBase(FidoTransportProtocol transport);

 private:
  const FidoTransportProtocol transport_;
  raw_ptr<Observer> observer_ = nullptr;
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DISCOVERY_BASE_H_
