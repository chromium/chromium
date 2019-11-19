// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/child_connection.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/service.mojom.h"

namespace content {

class ChildConnection::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext> {
 public:
  IOThreadContext() {}

  void Initialize(const service_manager::Identity& child_identity,
                  service_manager::Connector* connector,
                  mojo::ScopedMessagePipeHandle service_pipe,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
    DCHECK(!io_task_runner_);
    io_task_runner_ = io_task_runner;
    std::unique_ptr<service_manager::Connector> io_thread_connector;
    if (connector)
      connector_ = connector->Clone();
    child_identity_ = child_identity;
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::InitializeOnIOThread, this,
                                  child_identity, std::move(service_pipe)));
  }

  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IOThreadContext::BindInterfaceOnIOThread, this,
                       interface_name, std::move(interface_pipe)));
  }

  void ShutDown() {
    if (!io_task_runner_)
      return;
    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  void BindInterfaceOnIOThread(const std::string& interface_name,
                               mojo::ScopedMessagePipeHandle request_handle) {
    if (connector_) {
      connector_->BindInterface(child_identity_, interface_name,
                                std::move(request_handle));
    }
  }

  void SetProcess(base::Process process) {
    DCHECK(io_task_runner_);
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::SetProcessOnIOThread, this,
                                  std::move(process)));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  virtual ~IOThreadContext() {}

  void InitializeOnIOThread(
      const service_manager::Identity& child_identity,
      mojo::ScopedMessagePipeHandle service_pipe) {
    auto metadata_receiver = remote_metadata_.BindNewPipeAndPassReceiver();
    if (connector_) {
      connector_->RegisterServiceInstance(
          child_identity,
          mojo::PendingRemote<service_manager::mojom::Service>(
              std::move(service_pipe), 0),
          std::move(metadata_receiver));
    }
  }

  void ShutDownOnIOThread() {
    connector_.reset();
    remote_metadata_.reset();
  }

  void SetProcessOnIOThread(base::Process process) {
    DCHECK(remote_metadata_);
    remote_metadata_->SetPID(process.Pid());
    remote_metadata_.reset();
    process_ = std::move(process);
  }

  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  // Usable from the IO thread only.
  std::unique_ptr<service_manager::Connector> connector_;
  service_manager::Identity child_identity_;
  mojo::Remote<service_manager::mojom::ProcessMetadata> remote_metadata_;
  // Hold onto the process, and thus its process handle, so that the pid will
  // remain valid.
  base::Process process_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

ChildConnection::ChildConnection(
    const service_manager::Identity& child_identity,
    mojo::OutgoingInvitation* invitation,
    service_manager::Connector* connector,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : context_(new IOThreadContext), child_identity_(child_identity) {
  service_token_ = base::NumberToString(base::RandUint64());
  context_->Initialize(child_identity_, connector,
                       invitation->AttachMessagePipe(service_token_),
                       io_task_runner);
}

ChildConnection::~ChildConnection() {
  context_->ShutDown();
}

void ChildConnection::BindInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  context_->BindInterface(interface_name, std::move(interface_pipe));
}

void ChildConnection::SetProcess(base::Process process) {
  context_->SetProcess(std::move(process));
}

}  // namespace content
