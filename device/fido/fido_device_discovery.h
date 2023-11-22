// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_
#define DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {

class FidoDevice;
class FidoDeviceAuthenticator;

class COMPONENT_EXPORT(DEVICE_FIDO) FidoDeviceDiscovery
    : public FidoDiscoveryBase {
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

  enum class State {
    kIdle,
    kStarting,
    kRunning,
    kStopped,
  };

  FidoDeviceDiscovery(const FidoDeviceDiscovery&) = delete;
  FidoDeviceDiscovery& operator=(const FidoDeviceDiscovery&) = delete;

  ~FidoDeviceDiscovery() override;

  bool is_start_requested() const { return state_ != State::kIdle; }
  bool is_running() const { return state_ == State::kRunning; }

  std::vector<const FidoDeviceAuthenticator*> GetAuthenticatorsForTesting()
      const;
  FidoDeviceAuthenticator* GetAuthenticatorForTesting(
      std::string_view authenticator_id);

  // FidoDiscoveryBase:
  void Start() override;
  void Stop() override;

 protected:
  explicit FidoDeviceDiscovery(FidoTransportProtocol transport);

  void NotifyDiscoveryStarted(bool success);

  // Convenience method that adds a FidoDeviceAuthenticator with the given
  // |device|.
  bool AddDevice(std::unique_ptr<FidoDevice> device);
  bool AddAuthenticator(std::unique_ptr<FidoDeviceAuthenticator> authenticator);
  bool RemoveDevice(std::string_view device_id);

  // Subclasses should implement this to actually start the discovery when it is
  // requested.
  //
  // The implementation should asynchronously invoke NotifyDiscoveryStarted when
  // the discovery is started.
  virtual void StartInternal() = 0;

  // Map of ID to authenticator. It is a guarantee to subclasses that the ID of
  // the authenticator equals the ID of the device.
  std::map<std::string, std::unique_ptr<FidoDeviceAuthenticator>, std::less<>>
      authenticators_;

 private:
  void NotifyAuthenticatorAdded(FidoAuthenticator* authenticator);
  void NotifyAuthenticatorRemoved(FidoAuthenticator* authenticator);

  State state_ = State::kIdle;
  base::WeakPtrFactory<FidoDeviceDiscovery> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_DEVICE_DISCOVERY_H_
