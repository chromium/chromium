// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/messaging/native_message_port.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/browser/api/messaging/native_message_port_dispatcher.h"
#include "extensions/common/api/messaging/message.h"
#include "extensions/common/api/messaging/port_id.h"
#include "extensions/common/mojom/message_port.mojom-shared.h"

namespace extensions {

NativeMessagePort::NativeMessagePort(
    base::WeakPtr<ChannelDelegate> channel_delegate,
    const PortId& port_id,
    std::unique_ptr<NativeMessageHost> native_message_host)
    : MessagePort(std::move(channel_delegate), port_id),
      host_task_runner_(native_message_host->task_runner()) {
  dispatcher_ = ExtensionsAPIClient::Get()->CreateNativeMessagePortDispatcher(
      std::move(native_message_host), weak_factory_.GetWeakPtr(),
      base::SingleThreadTaskRunner::GetCurrentDefault());
}

NativeMessagePort::~NativeMessagePort() {
  DCHECK(thread_checker_.CalledOnValidThread());
  host_task_runner_->DeleteSoon(FROM_HERE, dispatcher_.release());
}

bool NativeMessagePort::IsValidPort() {
  // The native message port is immediately connected after construction, so it
  // is not possible to invalidate the port between construction and connection.
  return true;
}

void NativeMessagePort::DispatchOnMessage(const Message& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  dispatcher_->DispatchOnMessage(message.data());
}

void NativeMessagePort::PostMessageFromNativeHost(const std::string& message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    // Native messaging always uses JSON since a native host doesn't understand
    // structured cloning serialization.
    weak_channel_delegate_->PostMessage(
        port_id_, Message(message, mojom::SerializationFormat::kJson,
                          false /* user_gesture */));
  }
}

void NativeMessagePort::CloseChannel(const std::string& error_message) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (weak_channel_delegate_) {
    weak_channel_delegate_->CloseChannel(port_id_, error_message);
  }
}

}  // namespace extensions
