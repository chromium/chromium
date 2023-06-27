// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/remoting_service.h"

#include <memory>
#include <utility>

#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
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

 private:
  void ReleaseSupportHost();

  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<RemoteSupportHostAsh> remote_support_host_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

RemotingServiceImpl::RemotingServiceImpl() = default;
RemotingServiceImpl::~RemotingServiceImpl() = default;

RemoteSupportHostAsh& RemotingServiceImpl::GetSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!remote_support_host_) {
    remote_support_host_ =
        std::make_unique<RemoteSupportHostAsh>(base::BindOnce(
            &RemotingServiceImpl::ReleaseSupportHost, base::Unretained(this)));
  }
  return *remote_support_host_;
}

void RemotingServiceImpl::ReleaseSupportHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  remote_support_host_.reset();
}

}  // namespace

RemotingService& RemotingService::Get() {
  static base::NoDestructor<RemotingServiceImpl> instance;
  return *instance;
}

}  // namespace remoting
