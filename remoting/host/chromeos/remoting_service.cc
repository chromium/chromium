// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remoting_service.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "remoting/host/chromeos/file_session_storage.h"
#include "remoting/host/chromeos/remote_support_host_ash.h"

namespace remoting {

namespace {

class RemotingServiceImpl : public RemotingService {
 public:
  RemotingServiceImpl();
  RemotingServiceImpl(const RemotingServiceImpl&) = delete;
  RemotingServiceImpl& operator=(const RemotingServiceImpl&) = delete;
  ~RemotingServiceImpl() override;

  // RemotingService implementation.
  RemoteSupportHostAsh& GetSupportHost() override;
  void GetReconnectableEnterpriseSessionId(SessionIdCallback callback) override;

  FileSessionStorage& GetSessionStorage() { return session_storage_; }

 private:
  void ReleaseSupportHost();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RemoteSupportHostAsh> remote_support_host_
      GUARDED_BY_CONTEXT(sequence_checker_);

  FileSessionStorage session_storage_ GUARDED_BY_CONTEXT(sequence_checker_);
};

RemotingServiceImpl::RemotingServiceImpl() = default;
RemotingServiceImpl::~RemotingServiceImpl() = default;

RemoteSupportHostAsh& RemotingServiceImpl::GetSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!remote_support_host_) {
    remote_support_host_ = std::make_unique<RemoteSupportHostAsh>(
        base::BindOnce(&RemotingServiceImpl::ReleaseSupportHost,
                       base::Unretained(this)),
        session_storage_);
  }
  return *remote_support_host_;
}

void RemotingServiceImpl::GetReconnectableEnterpriseSessionId(
    SessionIdCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  session_storage_.HasSession(  //
      base::BindOnce([](bool has_session) {
        return has_session ? std::make_optional(kEnterpriseSessionId)
                           : std::nullopt;
      }).Then(std::move(callback)));
}

void RemotingServiceImpl::ReleaseSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_support_host_.reset();
}

RemotingServiceImpl& GetInstance() {
  static base::NoDestructor<RemotingServiceImpl> instance;
  return *instance;
}

}  // namespace

RemotingService& RemotingService::Get() {
  return GetInstance();
}

// static
void RemotingService::SetSessionStorageDirectoryForTesting(
    const base::FilePath& dir) {
  GetInstance()
      .GetSessionStorage()                  //
      .SetStorageDirectoryForTesting(dir);  // IN-TEST
}

}  // namespace remoting
