// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include <string.h>

#include <sstream>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/string_view_util.h"
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
  for (unsigned i = 0; UNSAFE_TODO(arr[i]) != -1; ++i) {
    if (static_cast<int>(value) == UNSAFE_TODO(arr[i])) {
      return i;
    }
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
  base::span<const uint8_t> remaining_proto = base::span(*proto);
  for (auto f = proto->ReadField(); f.valid(); f = proto->ReadField()) {
    // ReadField() advanced the internal decoder pointer. The field we just read
    // is the difference between the previous remaining bytes and the current.
    base::span<const uint8_t> field_span = remaining_proto.take_first(
        remaining_proto.size() - proto->bytes_left());

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
    if (!root->sub_messages ||
        UNSAFE_TODO(root->sub_messages[index]) == nullptr) {
      //  PODs can just be copied over to output. Packed fields can be treated
      //  just like primitive fields, by just copying over the full data. Note
      //  that there cannot be packed nested messages. Note that we cannot use
      //  |f.data()| here since it does not include the preamble (field id and
      //  possibly length), so we need to use |field_span|.
      output.append(base::as_string_view(field_span));
    } else {
      // Make recursive call to filter the nested message.
      ProtoDecoder decoder(f.data(), f.size());
      parent_ids->push_back(f.id());
      has_blocked_fields |= FilterProtoRecursively(
          UNSAFE_TODO(root->sub_messages[index]), &decoder, parent_ids, output);
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
  std::array<uint8_t, protozero::proto_utils::kMaxTagEncodedSize +
                          protozero::proto_utils::kMessageLengthFieldSize>
      storage = {};
  auto [field_id_max_storage, payload_size_max_storage] =
      base::span(storage)
          .split_at<protozero::proto_utils::kMaxTagEncodedSize>();
  uint8_t* const field_id_end = protozero::proto_utils::WriteVarInt(
      field_id, field_id_max_storage.data());
  auto field_id_buf = field_id_max_storage.first(
      static_cast<size_t>(field_id_end - field_id_max_storage.data()));

  uint8_t* const payload_size_end = protozero::proto_utils::WriteVarInt(
      payload_size, payload_size_max_storage.data());
  auto payload_size_buf = payload_size_max_storage.first(
      static_cast<size_t>(payload_size_end - payload_size_max_storage.data()));
  const size_t preamble_size = payload_size_buf.size() + field_id_buf.size();
  output.append(preamble_size, 0);
  auto message_span =
      base::as_writable_byte_span(output).subspan(out_msg_start_offset);

  if (payload_size != 0) {
    // Move the payload to make room for the preamble at the beginning.
    // message_span is [original_payload][empty_space]
    // destination is [empty_space][final_payload_position]
    message_span.subspan(preamble_size)
        .copy_from(message_span.first(payload_size));
  }

  auto [field_id_dest, payload_size_dest] =
      message_span.take_first(preamble_size).split_at(field_id_buf.size());
  field_id_dest.copy_from(field_id_buf);
  payload_size_dest.copy_from(payload_size_buf);

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
