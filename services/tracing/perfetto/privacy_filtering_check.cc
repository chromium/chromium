// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/privacy_filtering_check.h"

#include <sstream>

#include "base/logging.h"
#include "services/tracing/perfetto/privacy_filtered_fields-inl.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"

namespace tracing {
namespace {

using perfetto::protos::pbzero::InternedData;
using perfetto::protos::pbzero::TracePacket;
using perfetto::protos::pbzero::TrackEvent;
using protozero::ProtoDecoder;

int FindIndexOfValue(const int* const arr, uint32_t value) {
  for (unsigned i = 0; arr[i] != -1; ++i) {
    if (static_cast<int>(value) == arr[i])
      return i;
  }
  return -1;
}

// Recursively verifies that the |proto| contains only accepted field IDs
// including all sub messages. Keeps track of |parent_ids| for printing error
// message.
void VerifyProtoRecursive(const MessageInfo* root,
                          ProtoDecoder* proto,
                          std::vector<uint32_t>* parent_ids) {
  proto->Reset();
  for (auto f = proto->ReadField(); f.valid(); f = proto->ReadField()) {
    int index = FindIndexOfValue(root->accepted_field_ids, f.id());
    if (index == -1) {
      std::stringstream error;
      error << " Unexpected field in TracePacket proto. IDs from root to child";
      for (int a : *parent_ids) {
        error << " : " << a;
      }
      error << " : " << f.id();
      DCHECK(false) << error.rdbuf();
      continue;
    }
    if (root->sub_messages && root->sub_messages[index] != nullptr) {
      ProtoDecoder decoder(f.data(), f.size());
      parent_ids->push_back(f.id());
      VerifyProtoRecursive(root->sub_messages[index], &decoder, parent_ids);
      parent_ids->pop_back();
    }
  }
}

// Verifies that the |proto| contains only accepted fields.
void VerifyProto(const MessageInfo* root, ProtoDecoder* proto) {
  std::vector<uint32_t> parent_ids;
  VerifyProtoRecursive(root, proto, &parent_ids);
}

}  // namespace

PrivacyFilteringCheck::PrivacyFilteringCheck() = default;
PrivacyFilteringCheck::~PrivacyFilteringCheck() = default;

// static
void PrivacyFilteringCheck::CheckProtoForUnexpectedFields(
    const std::string& serialized_trace_proto) {
  perfetto::protos::pbzero::Trace::Decoder trace(
      reinterpret_cast<const uint8_t*>(serialized_trace_proto.data()),
      serialized_trace_proto.size());

  for (auto it = trace.packet(); !!it; ++it) {
    TracePacket::Decoder packet(*it);
    const MessageInfo* root = &kTracePacket;
    VerifyProto(root, &packet);

    if (packet.has_track_event()) {
      ++stats_.track_event;
    } else if (packet.has_process_descriptor()) {
      ++stats_.process_desc;
    } else if (packet.has_thread_descriptor()) {
      ++stats_.thread_desc;
    }
    if (packet.has_interned_data()) {
      InternedData::Decoder interned_data(packet.interned_data().data,
                                          packet.interned_data().size);
      stats_.has_interned_names |= interned_data.has_event_names();
      stats_.has_interned_categories |= interned_data.has_event_categories();
      stats_.has_interned_source_locations |=
          interned_data.has_source_locations();
    }
  }
}

}  // namespace tracing
