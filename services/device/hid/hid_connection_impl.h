// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_HID_HID_CONNECTION_IMPL_H_
#define SERVICES_DEVICE_HID_HID_CONNECTION_IMPL_H_

#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/device/hid/hid_connection.h"
#include "services/device/public/mojom/hid.mojom.h"

namespace device {

// HidConnectionImpl is reponsible for handling mojo communications from
// clients. It delegates to HidConnection the real work of creating
// connections in different platforms.
class HidConnectionImpl final : public mojom::HidConnection,
                                public HidConnection::Client {
 public:
  // Creates a strongly-bound HidConnectionImpl owned by |receiver| and
  // |watcher|. |connection| provides access to the HID device. If
  // |connection_client| is bound, it will be notified when input reports are
  // received. |watcher|, if bound, will be disconnected when the connection is
  // closed.
  static void Create(
      scoped_refptr<device::HidConnection> connection,
      mojo::PendingReceiver<mojom::HidConnection> receiver,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher);

  HidConnectionImpl(const HidConnectionImpl&) = delete;
  HidConnectionImpl& operator=(const HidConnectionImpl&) = delete;

  // HidConnection::Client implementation:
  void OnInputReport(scoped_refptr<base::RefCountedBytes> buffer,
                     size_t size) override;

  // mojom::HidConnection implementation:
  void Read(ReadCallback callback) override;
  void Write(uint8_t report_id,
             const std::vector<uint8_t>& buffer,
             WriteCallback callback) override;
  void GetFeatureReport(uint8_t report_id,
                        GetFeatureReportCallback callback) override;
  void SendFeatureReport(uint8_t report_id,
                         const std::vector<uint8_t>& buffer,
                         SendFeatureReportCallback callback) override;

 private:
  friend class HidConnectionImplTest;

  HidConnectionImpl(
      scoped_refptr<device::HidConnection> connection,
      mojo::PendingReceiver<mojom::HidConnection> receiver,
      mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
      mojo::PendingRemote<mojom::HidConnectionWatcher> watcher);
  ~HidConnectionImpl() final;
  void OnRead(ReadCallback callback,
              bool success,
              scoped_refptr<base::RefCountedBytes> buffer,
              size_t size);
  void OnWrite(WriteCallback callback, bool success);
  void OnGetFeatureReport(GetFeatureReportCallback callback,
                          bool success,
                          scoped_refptr<base::RefCountedBytes> buffer,
                          size_t size);
  void OnSendFeatureReport(SendFeatureReportCallback callback, bool success);

  mojo::Receiver<mojom::HidConnection> receiver_;

  scoped_refptr<device::HidConnection> hid_connection_;

  // Client interfaces.
  mojo::Remote<mojom::HidConnectionClient> client_;
  mojo::Remote<mojom::HidConnectionWatcher> watcher_;

  base::WeakPtrFactory<HidConnectionImpl> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_HID_HID_CONNECTION_IMPL_H_
