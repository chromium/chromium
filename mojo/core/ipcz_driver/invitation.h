// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CORE_IPCZ_DRIVER_INVITATION_H_
#define MOJO_CORE_IPCZ_DRIVER_INVITATION_H_

#include <cstdint>
#include <string>

#include "base/containers/span.h"
#include "mojo/core/ipcz_driver/object.h"
#include "mojo/core/scoped_ipcz_handle.h"
#include "mojo/public/c/system/invitation.h"
#include "mojo/public/c/system/types.h"
#include "third_party/ipcz/include/ipcz/ipcz.h"

namespace mojo::core::ipcz_driver {

// A Mojo invitation. Note that ipcz has no notion of invitation objects, so
// this object exists to implement a reasonable approximation of Mojo invitation
// behavior. See comments within the implementation for gritty details.
class Invitation : public Object<Invitation> {
 public:
  // This limit translates to the maximum number of initial portals Mojo will
  // request on any ConnectNode() call issued by this invitation. The limit is
  // not strictly specified by ipcz, but it does guarantee that ConnectNode()
  // must support at least 8 initial portals, so we cap our limit there, minus
  // one reserved initial portal for internal driver use. In practice no known
  // Mojo consumer today uses more than 4 attachments.
  static constexpr size_t kMaxAttachments = 7;

  explicit Invitation();

  static Type object_type() { return kInvitation; }

  static void SetDefaultProcessErrorHandler(
      MojoDefaultProcessErrorHandler handler);
  static void InvokeDefaultProcessErrorHandler(const std::string& error);

  // Attaches a new pipe to this invitation using the given `name`. Returns
  // the attached pipe's peer in `handle` if successful.
  MojoResult Attach(base::span<const uint8_t> name, MojoHandle* handle);

  // Extracts a pipe from the invitation, identified by the given `name`.
  MojoResult Extract(base::span<const uint8_t> name, MojoHandle* handle);

  // Uses ipcz ConnectNode() to effectively simulate sending this invitation
  // over the given transport.
  MojoResult Send(const MojoPlatformProcessHandle* process_handle,
                  const MojoInvitationTransportEndpoint* transport_endpoint,
                  MojoProcessErrorHandler error_handler,
                  uintptr_t error_handler_context,
                  const MojoSendInvitationOptions* options);

  // Uses ipcz ConnectNode() to effectively simulate accepting a new invitation
  // over the givern transport. Returns a new boxed Invitation handle.
  static MojoHandle Accept(
      const MojoInvitationTransportEndpoint* transport_endpoint,
      const MojoAcceptInvitationOptions* options);

  // ObjectBase:
  void Close() override;

 private:
  ~Invitation() override;

  // An array of pipe attachments. An invitation to be sent must have exactly
  // `num_attachments_` contiguous attachments starting from the first element
  // of this array, but attachments may be added out-of-order up to that point.
  std::array<ScopedIpczHandle, kMaxAttachments> attachments_ = {};

  // The total number of pipes attached so far.
  size_t num_attachments_ = 0;

  // The highest index of any attached pipe so far.
  size_t max_attachment_index_ = 0;
};

}  // namespace mojo::core::ipcz_driver

#endif  // MOJO_CORE_IPCZ_DRIVER_INVITATION_H_
