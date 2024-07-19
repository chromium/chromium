// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include <string.h>

#include <sstream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "services/tracing/perfetto/privacy_filtered_fields-inl.h"
#include "third_party/perfetto/include/perfetto/protozero/proto_utils.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace tracing {
namespace {

using perfetto::protos::pbzero::InternedData;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackDescriptor;
using perfetto::protos::pbzero::TrackEvent;
using protozero::ProtoDecoder;

// Find the index of |value| in |arr|.
int FindIndexOfValue(const int* const arr, uint32_t value) {
  for (unsigned i = 0; arr[i] != -1; ++i) {
    if (static_cast<int>(value) == arr[i])
      return i;
  }
  return -1;
}

#if DCHECK_IS_ON()
// Logs the disallowed field with the list of parent field IDs.
void LogDisallowedField(std::vector<uint32_t>* parent_ids, uint32_t field_id) {
  std::stringstream error;
  error << "Skipping field in Trace proto. IDs from root to child";
  for (int a : *parent_ids) {
    error << " : " << a;
  }
  error << " : " << field_id;
  VLOG(1) << error.rdbuf();
}
#endif  // DCHECK_IS_ON()

uint8_t* OffsetToPtr(size_t offset, std::string& str) {
  DCHECK_LT(offset, str.size());
  return reinterpret_cast<uint8_t*>(&str[0] + offset);
}

// Recursively copies the |proto|'s accepted field IDs including all sub
// messages, over to |output|. Keeps track of |parent_ids| - field id in parent
// message, in order of most recent child last.
bool FilterProtoRecursively(const MessageInfo* root,
                            ProtoDecoder* proto,
                            std::vector<uint32_t>* parent_ids,
                            std::string& output) {
  // Write any allowed fields of the message (the message's "payload") into
  // |output| at the |out_msg_start_offset|. This will not include the field ID
  // or size of the current message yet. We add those back below once we know
  // the final message size. Emitting the message payload into |output| saves
  // allocations for extra buffer, but will still require a memmove below. Other
  // alternative is to just use the max length bytes like protozero does.
  bool has_blocked_fields = false;
  const size_t out_msg_start_offset = output.size();

  proto->Reset();
  const uint8_t* current_field_start = proto->begin();
  const uint8_t* next_field_start = nullptr;
  for (auto f = proto->ReadField(); f.valid();
       f = proto->ReadField(), current_field_start = next_field_start) {
    next_field_start = proto->begin() + proto->read_offset();

    // If the field is not available in the accepted fields, then skip copying.
    int index = FindIndexOfValue(root->accepted_field_ids, f.id());
    if (index == -1) {
#if DCHECK_IS_ON()
      LogDisallowedField(parent_ids, f.id());
#endif
      has_blocked_fields = true;
      continue;
    }

    // If the field is allowed, then either the field is a nested message, or a
    // POD. If it's a nested message, then the message description will be
    // part of |sub_messages| list. If the message description is nullptr, then
    // assume it is POD.
    if (!root->sub_messages || root->sub_messages[index] == nullptr) {
      // PODs can just be copied over to output. Packed fields can be treated
      // just like primitive fields, by just copying over the full data. Note
      // that there cannot be packed nested messages. Note that we cannot use
      // |f.data()| here since it does not include the preamble (field id and
      // possibly length), so we need to keep track of |current_field_start|.
      output.append(current_field_start, next_field_start);
    } else {
      // Make recursive call to filter the nested message.
      ProtoDecoder decoder(f.data(), f.size());
      parent_ids->push_back(f.id());
      has_blocked_fields |= FilterProtoRecursively(
          root->sub_messages[index], &decoder, parent_ids, output);
      parent_ids->pop_back();
    }
  }

  const uint32_t payload_size = output.size() - out_msg_start_offset;

  // The format is <field id><payload size><message data>.
  // This function wrote the payload of the current message starting from the
  // end of output. We need to insert the preamble (<field id><payload size>),
  // after moving the payload by the size of the preamble.
  const uint32_t field_id =
      protozero::proto_utils::MakeTagLengthDelimited(parent_ids->back());
  uint8_t field_id_buf[protozero::proto_utils::kMaxTagEncodedSize];
  uint8_t* field_id_end =
      protozero::proto_utils::WriteVarInt(field_id, field_id_buf);
  const uint8_t field_id_length = field_id_end - field_id_buf;

  uint8_t payload_size_buf[protozero::proto_utils::kMessageLengthFieldSize];
  uint8_t* payload_size_end =
      protozero::proto_utils::WriteVarInt(payload_size, payload_size_buf);
  const uint8_t payload_size_length = payload_size_end - payload_size_buf;

  output.append(field_id_length + payload_size_length, 0);
  if (payload_size != 0) {
    // Resize |output| and move the payload, by size of the preamble.
    const size_t out_payload_start_offset =
        out_msg_start_offset + field_id_length + payload_size_length;
    memmove(OffsetToPtr(out_payload_start_offset, output),
            OffsetToPtr(out_msg_start_offset, output), payload_size);
  }

  // Insert field id and payload length.
  memcpy(OffsetToPtr(out_msg_start_offset, output), field_id_buf,
         field_id_length);
  memcpy(OffsetToPtr(out_msg_start_offset + field_id_length, output),
         payload_size_buf, payload_size_length);

  return has_blocked_fields;
}

bool FilterProto(const std::string& serialized_trace_proto,
                 std::string& output) {
  constexpr uint32_t kTracePacketFieldId = 1;
  // DO NOT use Trace::Decoder or TracePacket::Decoder since it sets the
  // TypedProtoDecoder does a memset of 0 for all field IDs. TracePacket is
  // especially bad because the max field ID is up to 1000s due to extensions.
  ProtoDecoder trace(
      reinterpret_cast<const uint8_t*>(serialized_trace_proto.data()),
      serialized_trace_proto.size());
  // Try to allocate all the memory before parsing the proto, so the parser runs
  // faster.
  output.reserve(serialized_trace_proto.size());
  std::vector<uint32_t> parent_ids;
  parent_ids.reserve(20);
  parent_ids.push_back(kTracePacketFieldId);

  bool has_blocked_fields = false;
  for (auto f = trace.ReadField(); f.valid(); f = trace.ReadField()) {
    CHECK_EQ(f.id(), kTracePacketFieldId);
    ProtoDecoder packet(f.data(), f.size());
    const MessageInfo* root = &kTracePacket;
    has_blocked_fields |=
        FilterProtoRecursively(root, &packet, &parent_ids, output);
  }
  return has_blocked_fields;
}

}  // namespace

PrivacyFilteringCheck::PrivacyFilteringCheck() = default;
PrivacyFilteringCheck::~PrivacyFilteringCheck() = default;

// static
void PrivacyFilteringCheck::RemoveBlockedFields(
    std::string& serialized_trace_proto) {
  std::string output;
  FilterProto(serialized_trace_proto, output);
  serialized_trace_proto.swap(output);
}

void PrivacyFilteringCheck::CheckProtoForUnexpectedFields(
    const std::string& serialized_trace_proto) {
  std::string output;
  bool has_blocked_fields = FilterProto(serialized_trace_proto, output);
  DCHECK(!has_blocked_fields);

  perfetto::protos::pbzero::Trace::Decoder trace(
      reinterpret_cast<const uint8_t*>(serialized_trace_proto.data()),
      serialized_trace_proto.size());
  for (auto it = trace.packet(); !!it; ++it) {
    TracePacket::Decoder packet(*it);

    if (packet.has_track_event()) {
      ++stats_.track_event;
    } else if (packet.has_track_descriptor()) {
      TrackDescriptor::Decoder track_decoder(packet.track_descriptor());
      if (track_decoder.has_process()) {
        ++stats_.process_desc;
      } else if (track_decoder.has_thread()) {
        ++stats_.thread_desc;
      }
    }
    if (packet.has_interned_data()) {
      InternedData::Decoder interned_data(packet.interned_data().data,
                                          packet.interned_data().size);
      stats_.has_interned_names |= interned_data.has_event_names();
      stats_.has_interned_categories |= interned_data.has_event_categories();
      stats_.has_interned_source_locations |=
          interned_data.has_source_locations();
      stats_.has_interned_log_messages |= interned_data.has_log_message_body();
    }
  }
}

}  // namespace tracing
