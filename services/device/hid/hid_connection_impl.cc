// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/device/hid/hid_connection_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"

namespace device {

// static
void HidConnectionImpl::Create(
    scoped_refptr<device::HidConnection> connection,
    mojo::PendingReceiver<mojom::HidConnection> receiver,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher) {
  // This HidConnectionImpl is owned by |receiver| and |watcher|.
  new HidConnectionImpl(connection, std::move(receiver),
                        std::move(connection_client), std::move(watcher));
}

HidConnectionImpl::HidConnectionImpl(
    scoped_refptr<device::HidConnection> connection,
    mojo::PendingReceiver<mojom::HidConnection> receiver,
    mojo::PendingRemote<mojom::HidConnectionClient> connection_client,
    mojo::PendingRemote<mojom::HidConnectionWatcher> watcher)
    : receiver_(this, std::move(receiver)),
      hid_connection_(std::move(connection)),
      watcher_(std::move(watcher)) {
  receiver_.set_disconnect_handler(base::BindOnce(
      [](HidConnectionImpl* self) { delete self; }, base::Unretained(this)));
  if (watcher_) {
    watcher_.set_disconnect_handler(base::BindOnce(
        [](HidConnectionImpl* self) { delete self; }, base::Unretained(this)));
  }
  if (connection_client) {
    hid_connection_->SetClient(this);
    client_.Bind(std::move(connection_client));
  }
}

HidConnectionImpl::~HidConnectionImpl() {
  DCHECK(hid_connection_);
  hid_connection_->SetClient(nullptr);

  // Close |hid_connection_| on destruction. HidConnectionImpl is owned by
  // its mojom::HidConnection receiver and mojom::HidConnectionWatcher remote
  // and will self-destruct when either pipe is closed.
  hid_connection_->Close();
}

void HidConnectionImpl::OnInputReport(
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t size) {
  DCHECK(client_);
  DCHECK_GE(size, 1u);
  std::vector<uint8_t> data;
  if (size > 1) {
    data = std::vector<uint8_t>(buffer->data() + 1, buffer->data() + size);
  }
  client_->OnInputReport(/*report_id=*/buffer->data()[0], data);
}

void HidConnectionImpl::Read(ReadCallback callback) {
  DCHECK(hid_connection_);
  hid_connection_->Read(base::BindOnce(&HidConnectionImpl::OnRead,
                                       weak_factory_.GetWeakPtr(),
                                       std::move(callback)));
}

void HidConnectionImpl::OnRead(ReadCallback callback,
                               bool success,
                               scoped_refptr<base::RefCountedBytes> buffer,
                               size_t size) {
  if (!success) {
    std::move(callback).Run(false, 0, std::nullopt);
    return;
  }
  DCHECK(buffer);

  std::vector<uint8_t> data(buffer->data() + 1, buffer->data() + size);
  std::move(callback).Run(true, buffer->data()[0], data);
}

void HidConnectionImpl::Write(uint8_t report_id,
                              const std::vector<uint8_t>& buffer,
                              WriteCallback callback) {
  DCHECK(hid_connection_);

  auto io_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(buffer.size() + 1u);
  io_buffer->as_vector().data()[0u] = report_id;
  base::span(io_buffer->as_vector()).subspan(1u).copy_from(buffer);

  hid_connection_->Write(io_buffer, base::BindOnce(&HidConnectionImpl::OnWrite,
                                                   weak_factory_.GetWeakPtr(),
                                                   std::move(callback)));
}

void HidConnectionImpl::OnWrite(WriteCallback callback, bool success) {
  std::move(callback).Run(success);
}

void HidConnectionImpl::GetFeatureReport(uint8_t report_id,
                                         GetFeatureReportCallback callback) {
  DCHECK(hid_connection_);
  hid_connection_->GetFeatureReport(
      report_id,
      base::BindOnce(&HidConnectionImpl::OnGetFeatureReport,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HidConnectionImpl::OnGetFeatureReport(
    GetFeatureReportCallback callback,
    bool success,
    scoped_refptr<base::RefCountedBytes> buffer,
    size_t size) {
  if (!success) {
    std::move(callback).Run(false, std::nullopt);
    return;
  }
  DCHECK(buffer);

  std::vector<uint8_t> data(buffer->data(), buffer->data() + size);
  std::move(callback).Run(true, data);
}

void HidConnectionImpl::SendFeatureReport(uint8_t report_id,
                                          const std::vector<uint8_t>& buffer,
                                          SendFeatureReportCallback callback) {
  DCHECK(hid_connection_);

  auto io_buffer =
      base::MakeRefCounted<base::RefCountedBytes>(buffer.size() + 1u);
  io_buffer->as_vector()[0u] = report_id;
  base::span(io_buffer->as_vector()).subspan(1u).copy_from(buffer);

  hid_connection_->SendFeatureReport(
      io_buffer,
      base::BindOnce(&HidConnectionImpl::OnSendFeatureReport,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void HidConnectionImpl::OnSendFeatureReport(SendFeatureReportCallback callback,
                                            bool success) {
  std::move(callback).Run(success);
}

}  // namespace device
