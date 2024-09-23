// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "testing/libfuzzer/fuzzers/mach/mach_message_converter.h"

#include <string.h>
#include <sys/types.h>

#include <utility>

#include "base/apple/mach_logging.h"
#include "base/containers/buffer_iterator.h"
#include "base/mac/scoped_mach_msg_destroy.h"

namespace mach_fuzzer {

namespace {

SendablePort ConvertPort(const MachPortType& port_proto) {
  struct {
    bool insert_send_right;
    bool deallocate_receive_right;
    mach_msg_type_name_t disposition;
  } recipe;
  switch (port_proto) {
    case RECEIVE:
      recipe = {true, false, MACH_MSG_TYPE_MOVE_RECEIVE};
      break;
    case SEND:
      recipe = {false, false, MACH_MSG_TYPE_MAKE_SEND};
      break;
    case SEND_ONCE:
      recipe = {false, false, MACH_MSG_TYPE_MAKE_SEND_ONCE};
      break;
    case DEAD_NAME:
      recipe = {true, true, MACH_MSG_TYPE_COPY_SEND};
      break;
    case RECEIVE_NO_SENDERS:
      recipe = {false, false, MACH_MSG_TYPE_MOVE_RECEIVE};
      break;
  }

  SendablePort port;
  kern_return_t kr = mach_port_allocate(
      mach_task_self(), MACH_PORT_RIGHT_RECEIVE,
      base::apple::ScopedMachReceiveRight::Receiver(port.receive_right).get());
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_allocate";

  port.name = port.receive_right.get();
  port.disposition = recipe.disposition;
  port.proto_type = port_proto;

  if (recipe.insert_send_right) {
    kr = mach_port_insert_right(mach_task_self(), port.name, port.name,
                                MACH_MSG_TYPE_MAKE_SEND);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "mach_port_insert_right";
    port.send_right.reset(port.name);
  }

  if (recipe.deallocate_receive_right) {
    port.receive_right.reset();
  }

  return port;
}

bool ConvertDescriptor(base::BufferIterator<uint8_t>* iterator,
                       const Descriptor& descriptor_proto,
                       SendablePort* opt_port) {
  switch (descriptor_proto.descriptor_oneof_case()) {
    case Descriptor::kPort: {
      auto* descriptor = iterator->MutableObject<mach_msg_port_descriptor_t>();
      SendablePort port = ConvertPort(descriptor_proto.port());
      descriptor->name = port.name;
      descriptor->pad1 = 0;
      descriptor->pad2 = 0;
      descriptor->disposition = port.disposition;
      descriptor->type = MACH_MSG_PORT_DESCRIPTOR;
      *opt_port = std::move(port);
      return true;
    }
    case Descriptor::kOol: {
      auto* descriptor = iterator->MutableObject<mach_msg_ool_descriptor_t>();
      descriptor->address =
          const_cast<char*>(descriptor_proto.ool().data().data());
      descriptor->size = descriptor_proto.ool().data().size();
      descriptor->copy = MACH_MSG_VIRTUAL_COPY;
      descriptor->pad1 = 0;
      descriptor->type = MACH_MSG_OOL_DESCRIPTOR;
      return true;
    }
    default:
      return false;
  }
}

}  // namespace

SendableMessage ConvertProtoToMachMessage(const MachMessage& proto) {
  SendableMessage message;

  const size_t descriptor_count = proto.descriptors().size();
  const size_t data_size = proto.data().size();
  const bool include_body =
      proto.include_body_if_not_complex() || descriptor_count > 0;

  // This is the maximum size of the message. Depending on the descriptor type,
  // the actual msgh_size may be less.
  const size_t message_size =
      sizeof(mach_msg_header_t) + (include_body ? sizeof(mach_msg_body_t) : 0) +
      (sizeof(mach_msg_descriptor_t) * descriptor_count) + data_size;
  message.buffer = std::make_unique<uint8_t[]>(round_msg(message_size));

  base::BufferIterator<uint8_t> iterator(message.buffer.get(), message_size);

  auto* header = iterator.MutableObject<mach_msg_header_t>();
  message.header = header;
  header->msgh_id = proto.id();

  if (proto.has_local_port()) {
    SendablePort port = ConvertPort(proto.local_port());
    auto disposition = port.disposition;
    // It's not legal to have a receive reply report.
    if (disposition != MACH_MSG_TYPE_MOVE_RECEIVE) {
      header->msgh_bits |= MACH_MSGH_BITS(0, disposition);
      header->msgh_local_port = port.name;
      message.ports.push_back(std::move(port));
    }
  }

  if (include_body) {
    auto* body = iterator.MutableObject<mach_msg_body_t>();
    body->msgh_descriptor_count = descriptor_count;
  }

  if (descriptor_count > 0) {
    header->msgh_bits |= MACH_MSGH_BITS_COMPLEX;
    for (const auto& descriptor : proto.descriptors()) {
      SendablePort opt_port;
      if (!ConvertDescriptor(&iterator, descriptor, &opt_port)) {
        return SendableMessage();
      }
      if (opt_port.name != MACH_PORT_NULL) {
        message.ports.push_back(std::move(opt_port));
      }
    }
  }

  auto data = iterator.MutableSpan<uint8_t>(data_size);
  memcpy(data.data(), proto.data().data(), proto.data().size());

  header->msgh_size = round_msg(iterator.position());

  return message;
}

SendResult SendMessage(mach_port_t remote_port, const MachMessage& proto) {
  SendResult result;
  result.message = ConvertProtoToMachMessage(proto);
  if (!result.message.header) {
    result.kr = KERN_FAILURE;
    return result;
  }

  result.message.header->msgh_remote_port = remote_port;
  result.message.header->msgh_bits |=
      MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);

  base::ScopedMachMsgDestroy scoped_message(result.message.header);

  result.kr = mach_msg(result.message.header, MACH_SEND_MSG | MACH_SEND_TIMEOUT,
                       result.message.header->msgh_size, 0, MACH_PORT_NULL,
                       /*timeout=*/0, MACH_PORT_NULL);

  if (result.kr == KERN_SUCCESS) {
    scoped_message.Disarm();
  }

  return result;
}

}  // namespace mach_fuzzer
