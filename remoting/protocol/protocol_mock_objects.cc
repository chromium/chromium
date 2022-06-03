// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/protocol_mock_objects.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/threading/thread_task_runner_handle.h"
#include "remoting/protocol/session_plugin.h"
#include "remoting/protocol/video_stream.h"
#include "remoting/signaling/signaling_address.h"

namespace remoting {
namespace protocol {

MockAuthenticator::MockAuthenticator() = default;
MockAuthenticator::~MockAuthenticator() = default;

MockConnectionToClientEventHandler::MockConnectionToClientEventHandler() =
    default;
MockConnectionToClientEventHandler::~MockConnectionToClientEventHandler() =
    default;

MockClipboardStub::MockClipboardStub() = default;
MockClipboardStub::~MockClipboardStub() = default;

MockInputStub::MockInputStub() = default;
MockInputStub::~MockInputStub() = default;

MockHostStub::MockHostStub() = default;
MockHostStub::~MockHostStub() = default;

MockClientStub::MockClientStub() = default;
MockClientStub::~MockClientStub() = default;

MockCursorShapeStub::MockCursorShapeStub() = default;
MockCursorShapeStub::~MockCursorShapeStub() = default;

MockVideoStub::MockVideoStub() = default;
MockVideoStub::~MockVideoStub() = default;

MockSession::MockSession() = default;
MockSession::~MockSession() = default;

MockSessionManager::MockSessionManager() = default;
MockSessionManager::~MockSessionManager() = default;

MockPairingRegistryDelegate::MockPairingRegistryDelegate() = default;
MockPairingRegistryDelegate::~MockPairingRegistryDelegate() = default;

std::unique_ptr<base::ListValue> MockPairingRegistryDelegate::LoadAll() {
  std::unique_ptr<base::ListValue> result(new base::ListValue());
  for (Pairings::const_iterator i = pairings_.begin(); i != pairings_.end();
       ++i) {
    result->Append(i->second.ToValue());
  }
  return result;
}

bool MockPairingRegistryDelegate::DeleteAll() {
  pairings_.clear();
  return true;
}

protocol::PairingRegistry::Pairing MockPairingRegistryDelegate::Load(
    const std::string& client_id) {
  Pairings::const_iterator i = pairings_.find(client_id);
  if (i != pairings_.end()) {
    return i->second;
  } else {
    return protocol::PairingRegistry::Pairing();
  }
}

bool MockPairingRegistryDelegate::Save(
    const protocol::PairingRegistry::Pairing& pairing) {
  pairings_[pairing.client_id()] = pairing;
  return true;
}

bool MockPairingRegistryDelegate::Delete(const std::string& client_id) {
  pairings_.erase(client_id);
  return true;
}

SynchronousPairingRegistry::SynchronousPairingRegistry(
    std::unique_ptr<Delegate> delegate)
    : PairingRegistry(base::ThreadTaskRunnerHandle::Get(),
                      std::move(delegate)) {}
SynchronousPairingRegistry::~SynchronousPairingRegistry() = default;

void SynchronousPairingRegistry::PostTask(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const base::Location& from_here,
    base::OnceClosure task) {
  DCHECK(task_runner->BelongsToCurrentThread());
  std::move(task).Run();
}

}  // namespace protocol
}  // namespace remoting
