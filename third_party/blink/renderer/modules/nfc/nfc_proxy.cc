// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/nfc/nfc_proxy.h"

#include <utility>

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/modules/nfc/ndef_reader.h"
#include "third_party/blink/renderer/modules/nfc/ndef_writer.h"
#include "third_party/blink/renderer/modules/nfc/nfc_type_converters.h"
#include "third_party/blink/renderer/modules/nfc/nfc_utils.h"
#include "third_party/blink/renderer/platform/mojo/mojo_helper.h"

namespace blink {

// static
const char NFCProxy::kSupplementName[] = "NFCProxy";

// static
NFCProxy* NFCProxy::From(Document& document) {
  // https://w3c.github.io/web-nfc/#security-policies
  // WebNFC API must be only accessible from top level browsing context.
  DCHECK(document.IsInMainFrame());

  NFCProxy* nfc_proxy = Supplement<Document>::From<NFCProxy>(document);
  if (!nfc_proxy) {
    nfc_proxy = MakeGarbageCollected<NFCProxy>(document);
    Supplement<Document>::ProvideTo(document, nfc_proxy);
  }
  return nfc_proxy;
}

// NFCProxy
NFCProxy::NFCProxy(Document& document)
    : PageVisibilityObserver(document.GetPage()),
      Supplement<Document>(document),
      client_receiver_(this) {}

NFCProxy::~NFCProxy() = default;

void NFCProxy::Dispose() {
  client_receiver_.reset();
}

void NFCProxy::Trace(blink::Visitor* visitor) {
  visitor->Trace(writers_);
  visitor->Trace(readers_);
  PageVisibilityObserver::Trace(visitor);
  Supplement<Document>::Trace(visitor);
}

void NFCProxy::StartReading(NDEFReader* reader,
                            const NDEFScanOptions* options) {
  DCHECK(reader);
  if (readers_.Contains(reader))
    return;

  EnsureMojoConnection();
  nfc_remote_->Watch(
      device::mojom::blink::NDEFScanOptions::From(options), next_watch_id_,
      WTF::Bind(&NFCProxy::OnReaderRegistered, WrapPersistent(this),
                WrapPersistent(reader), next_watch_id_));
  readers_.insert(reader, next_watch_id_);
  next_watch_id_++;
}

void NFCProxy::StopReading(NDEFReader* reader) {
  DCHECK(reader);
  auto iter = readers_.find(reader);
  if (iter != readers_.end()) {
    if (nfc_remote_) {
      // We do not need to notify |reader| of anything.
      nfc_remote_->CancelWatch(
          iter->value, device::mojom::blink::NFC::CancelWatchCallback());
    }
    readers_.erase(iter);
  }
}

bool NFCProxy::IsReading(const NDEFReader* reader) {
  DCHECK(reader);
  return readers_.Contains(const_cast<NDEFReader*>(reader));
}

void NFCProxy::AddWriter(NDEFWriter* writer) {
  DCHECK(!writers_.Contains(writer));
  writers_.insert(writer);
}

void NFCProxy::Push(device::mojom::blink::NDEFMessagePtr message,
                    device::mojom::blink::NDEFPushOptionsPtr options,
                    device::mojom::blink::NFC::PushCallback cb) {
  EnsureMojoConnection();
  nfc_remote_->Push(std::move(message), std::move(options), std::move(cb));
}

void NFCProxy::CancelPush(
    const String& target,
    device::mojom::blink::NFC::CancelPushCallback callback) {
  DCHECK(nfc_remote_);
  nfc_remote_->CancelPush(StringToNDEFPushTarget(target), std::move(callback));
}

// device::mojom::blink::NFCClient implementation.
void NFCProxy::OnWatch(const Vector<uint32_t>& watch_ids,
                       const String& serial_number,
                       device::mojom::blink::NDEFMessagePtr message) {
  // Dispatch the event to all matched readers. We iterate on a copy of
  // |readers_| because the user's NDEFReader#onreading event handler may call
  // NDEFReader#stop() to modify |readers_| just during the iteration process.
  // This loop is O(n^2), however, we assume the number of readers to be small
  // so it'd be just OK.
  ReaderMap copy = readers_;
  for (auto& pair : copy) {
    if (watch_ids.Contains(pair.value))
      pair.key->OnReading(serial_number, *message);
  }
}

void NFCProxy::OnReaderRegistered(NDEFReader* reader,
                                  uint32_t watch_id,
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
    reader->OnError(error->error_type);
    readers_.erase(reader);
    return;
  }

  // It's good the watch request has been accepted, we do nothing here but just
  // wait for message notifications in OnWatch().
}

void NFCProxy::PageVisibilityChanged() {
  // If service is not initialized, there cannot be any pending NFC activities.
  if (!nfc_remote_)
    return;

  // NFC operations should be suspended.
  // https://w3c.github.io/web-nfc/#nfc-suspended
  // TODO(https://crbug.com/520391): Suspend/Resume NFC in the browser process
  // instead to prevent a compromised renderer from using NFC in the background.
  if (!GetPage()->IsPageVisible())
    nfc_remote_->SuspendNFCOperations();
  else
    nfc_remote_->ResumeNFCOperations();
}

void NFCProxy::EnsureMojoConnection() {
  if (nfc_remote_)
    return;

  GetSupplementable()->GetBrowserInterfaceBroker().GetInterface(
      nfc_remote_.BindNewPipeAndPassReceiver());
  nfc_remote_.set_disconnect_handler(
      WTF::Bind(&NFCProxy::OnMojoConnectionError, WrapWeakPersistent(this)));

  // See https://bit.ly/2S0zRAS for task types.
  auto task_runner =
      GetSupplementable()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  // Set client for OnWatch event.
  nfc_remote_->SetClient(
      client_receiver_.BindNewPipeAndPassRemote(task_runner));
}

void NFCProxy::OnMojoConnectionError() {
  nfc_remote_.reset();
  client_receiver_.reset();

  // Notify all active readers about the connection error and clear the list.
  ReaderMap readers = std::move(readers_);
  for (auto& pair : readers) {
    // The reader may call StopReading() to remove itself from |readers_| when
    // handling the error.
    pair.key->OnError(device::mojom::blink::NDEFErrorType::NOT_SUPPORTED);
  }

  // Each connection maintains its own watch ID numbering, so reset to 1 on
  // connection error.
  next_watch_id_ = 1;

  // Notify all writers about the connection error.
  for (auto& writer : writers_) {
    writer->OnMojoConnectionError();
  }
  // Clear the reader list.
  writers_.clear();
}

}  // namespace blink
