// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"

#include <utility>

#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"

namespace blink {

// static
const char NFCProxy::kSupplementName[] = "NFCProxy";

// static
NFCProxy* NFCProxy::From(LocalDOMWindow& window) {
  NFCProxy* nfc_proxy = Supplement<LocalDOMWindow>::From<NFCProxy>(window);
  if (!nfc_proxy) {
    nfc_proxy = MakeGarbageCollected<NFCProxy>(window);
    Supplement<LocalDOMWindow>::ProvideTo(window, nfc_proxy);
  }
  return nfc_proxy;
}

// NFCProxy
NFCProxy::NFCProxy(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window),
      nfc_remote_(window.GetExecutionContext()),
      client_receiver_(this, window.GetExecutionContext()) {}

NFCProxy::~NFCProxy() = default;

void NFCProxy::Trace(Visitor* visitor) const {
  visitor->Trace(client_receiver_);
  visitor->Trace(nfc_remote_);
  visitor->Trace(writers_);
  visitor->Trace(readers_);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

void NFCProxy::StartReading(NDEFReader* reader,
                            device::mojom::blink::NFC::WatchCallback callback) {
  DCHECK(reader);
  DCHECK(!readers_.Contains(reader));

  EnsureMojoConnection();
  nfc_remote_->Watch(next_watch_id_,
                     WTF::BindOnce(&NFCProxy::OnReaderRegistered,
                                   WrapPersistent(this), WrapPersistent(reader),
                                   next_watch_id_, std::move(callback)));
  readers_.insert(reader, next_watch_id_);
  next_watch_id_++;
}

void NFCProxy::StopReading(NDEFReader* reader) {
  DCHECK(reader);
  auto iter = readers_.find(reader);
  if (iter != readers_.end()) {
    if (nfc_remote_)
      nfc_remote_->CancelWatch(iter->value);
    readers_.erase(iter);
  }
}

bool NFCProxy::IsReading(const NDEFReader* reader) {
  DCHECK(reader);
  return readers_.Contains(const_cast<NDEFReader*>(reader));
}

void NFCProxy::AddWriter(NDEFReader* writer) {
  if (!writers_.Contains(writer))
    writers_.insert(writer);
}

void NFCProxy::Push(device::mojom::blink::NDEFMessagePtr message,
                    device::mojom::blink::NDEFWriteOptionsPtr options,
                    device::mojom::blink::NFC::PushCallback cb) {
  EnsureMojoConnection();
  nfc_remote_->Push(std::move(message), std::move(options), std::move(cb));
}

void NFCProxy::CancelPush() {
  if (!nfc_remote_)
    return;
  nfc_remote_->CancelPush();
}

void NFCProxy::MakeReadOnly(
    device::mojom::blink::NFC::MakeReadOnlyCallback cb) {
  EnsureMojoConnection();
  nfc_remote_->MakeReadOnly(std::move(cb));
}

void NFCProxy::CancelMakeReadOnly() {
  if (!nfc_remote_)
    return;
  nfc_remote_->CancelMakeReadOnly();
}

// device::mojom::blink::NFCClient implementation.
void NFCProxy::OnWatch(const Vector<uint32_t>& watch_ids,
                       const String& serial_number,
                       device::mojom::blink::NDEFMessagePtr message) {
  // Dispatch the event to all matched readers. We iterate on a copy of
  // |readers_| because a reader's onreading event handler may remove itself
  // from |readers_| just during the iteration process. This loop is O(n^2),
  // however, we assume the number of readers to be small so it'd be just OK.
  ReaderMap copy = readers_;
  for (auto& pair : copy) {
    if (watch_ids.Contains(pair.value))
      pair.key->OnReading(serial_number, *message);
  }
}

void NFCProxy::OnError(device::mojom::blink::NDEFErrorPtr error) {
  // Dispatch the event to all readers. We iterate on a copy of |readers_|
  // because a reader's onreadingerror event handler may remove itself from
  // |readers_| just during the iteration process.
  ReaderMap copy = readers_;
  for (auto& pair : copy) {
    pair.key->OnReadingError(error->error_message);
  }
}

void NFCProxy::OnReaderRegistered(
    NDEFReader* reader,
    uint32_t watch_id,
    device::mojom::blink::NFC::WatchCallback callback,
    device::mojom::blink::NDEFErrorPtr error) {
  DCHECK(reader);
  // |reader| may have already stopped reading.
  if (!readers_.Contains(reader))
    return;

  // |reader| already stopped reading for the previous |watch_id| request and
  // started a new one, let's just ignore this response callback as we do not
  // need to notify |reader| of anything for an obsoleted session.
  if (readers_.at(reader) != watch_id)
    return;

  if (error) {
    readers_.erase(reader);
    std::move(callback).Run(std::move(error));
    return;
  }

  std::move(callback).Run(nullptr);

  // It's good the watch request has been accepted, next we just wait for
  // message notifications in OnWatch().
}

void NFCProxy::EnsureMojoConnection() {
  if (nfc_remote_)
    return;

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI);

  GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
      nfc_remote_.BindNewPipeAndPassReceiver(task_runner));
  nfc_remote_.set_disconnect_handler(WTF::BindOnce(
      &NFCProxy::OnMojoConnectionError, WrapWeakPersistent(this)));

  // Set client for OnWatch event.
  nfc_remote_->SetClient(
      client_receiver_.BindNewPipeAndPassRemote(task_runner));
}

// This method will be called if either the NFC service is unavailable (such
// as if the feature flag is disabled) or when the user revokes the NFC
// permission after the Mojo connection has already been opened. It is
// currently impossible to distinguish between these two cases.
//
// In the future this code may also handle the case where an out-of-process
// Device Service encounters a fatal error and must be restarted.
void NFCProxy::OnMojoConnectionError() {
  nfc_remote_.reset();
  client_receiver_.reset();

  // Notify all active readers about the connection error.
  ReaderMap readers = std::move(readers_);
  for (auto& pair : readers) {
    pair.key->ReadOnMojoConnectionError();
  }

  // Each connection maintains its own watch ID numbering, so reset to 1 on
  // connection error.
  next_watch_id_ = 1;

  // Notify all writers about the connection error and clear the list.
  for (auto& writer : writers_) {
    writer->WriteOnMojoConnectionError();
    writer->MakeReadOnlyOnMojoConnectionError();
  }
  writers_.clear();
}

}  // namespace blink
