// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_
#define CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/sequenced_task_runner.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/system/invitation.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/mojom/connector.mojom.h"

namespace service_manager {
class Connector;
}

namespace content {

// Helper class to establish a connection between the Service Manager and a
// single child process. Process hosts can use this when launching new processes
// which should be registered with the service manager.
class CONTENT_EXPORT ChildConnection {
 public:
  // Prepares a new child connection for a child process which will be
  // identified to the service manager as |child_identity|. |child_identity|'s
  // instance field must be unique among all child connections using the same
  // service name. |connector| is the connector to use to establish the
  // connection.
  ChildConnection(const service_manager::Identity& child_identity,
                  mojo::OutgoingInvitation* invitation,
                  service_manager::Connector* connector,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~ChildConnection();

  // Binds an implementation of |interface_name| to |interface_pipe| in the
  // child.
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe);

  const service_manager::Identity& child_identity() const {
    return child_identity_;
  }

  // A token which must be passed to the child process via
  // |service_manager::switches::kServicePipeToken| in order for the child to
  // initialize its end of the Service Manager connection pipe.
  std::string service_token() const { return service_token_; }

  // Sets the child connection's process. This should be called as soon
  // as the process has been launched, and the connection will not be fully
  // functional until this is called.
  void SetProcess(base::Process process);

 private:
  class IOThreadContext;

  scoped_refptr<IOThreadContext> context_;
  service_manager::Identity child_identity_;
  std::string service_token_;

  base::WeakPtrFactory<ChildConnection> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ChildConnection);
};

}  // namespace content

#endif  // CONTENT_COMMON_SERVICE_MANAGER_CHILD_CONNECTION_H_
