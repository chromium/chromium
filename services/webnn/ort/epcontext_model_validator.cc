// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/epcontext_model_validator.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "services/webnn/public/cpp/execution_providers_info.h"

// Implementation of ValidateEpContextModel; see the header for what it checks
// and why.

namespace webnn::ort {

namespace {

using Reader = base::SpanReader<const uint8_t>;

// ---------------------------------------------------------------------------
// Minimal, fixed-depth protobuf reader for the ONNX envelope.
//
// This walks only the fields needed to count nodes and read each node's op_type
// and domain, and never materializes the model. All reads are bounds-checked
// by `base::SpanReader`; anything malformed or truncated is treated as invalid.
// ---------------------------------------------------------------------------

// Reads a base-128 varint. Returns false on truncation or a value that would
// exceed 64 bits.
bool ReadVarint(Reader& reader, uint64_t& value) {
  value = 0;
  for (int shift = 0; shift < 64; shift += 7) {
    uint8_t byte = 0;
    if (!reader.ReadU8BigEndian(byte)) {
      return false;
    }
    // The 10th byte (shift == 63) can only carry bit 63; any other payload bit
    // being set means the value does not fit in 64 bits.
    if (shift == 63 && (byte & 0x7Eu) != 0) {
      return false;
    }
    value |= static_cast<uint64_t>(byte & 0x7Fu) << shift;
    if ((byte & 0x80u) == 0) {
      return true;
    }
  }
  return false;
}

// Reads a length-delimited (wire type 2) payload, setting `out` to a view over
// the payload bytes.
bool ReadLengthDelimited(Reader& reader, base::span<const uint8_t>& out) {
  uint64_t length = 0;
  if (!ReadVarint(reader, length) || length > reader.remaining()) {
    return false;
  }
  return reader.ReadInto(static_cast<size_t>(length), out);
}

// Protobuf field numbers are in [1, 2^29-1] and tags are uint32_t; upstream
// parsers reject anything outside that range, so reject it here too rather than
// truncating a larger varint into a valid-looking field number.
constexpr uint32_t kMaxFieldNumber = (1u << 29) - 1u;

// Reads a protobuf tag, setting `field`/`wire`. Returns false on truncation or
// an out-of-range field number.
bool ReadTag(Reader& reader, uint32_t& field, uint32_t& wire) {
  uint64_t tag = 0;
  if (!ReadVarint(reader, tag) || tag > std::numeric_limits<uint32_t>::max()) {
    return false;
  }
  field = static_cast<uint32_t>(tag >> 3);
  wire = static_cast<uint32_t>(tag & 0x7u);
  return field != 0 && field <= kMaxFieldNumber;
}

// Skips one field with the given wire type. The ONNX Model/Graph/Node messages
// walked here only ever carry varint (wire 0) and length-delimited (wire 2)
// fields; any other wire type (fixed32/64, groups) means malformed input.
bool SkipField(Reader& reader, uint32_t wire_type) {
  if (wire_type == 0) {  // varint
    uint64_t ignored = 0;
    return ReadVarint(reader, ignored);
  }
  if (wire_type == 2) {  // length-delimited
    base::span<const uint8_t> ignored;
    return ReadLengthDelimited(reader, ignored);
  }
  return false;
}

bool SpanEquals(base::span<const uint8_t> bytes, std::string_view text) {
  return bytes == base::as_byte_span(text);
}

// ONNX protobuf field numbers.
constexpr uint32_t kModelProtoGraph = 7;
constexpr uint32_t kGraphProtoNode = 1;
constexpr uint32_t kNodeProtoOpType = 4;
constexpr uint32_t kNodeProtoDomain = 7;

// Returns whether `node` is an EPContext (com.microsoft) NodeProto, reading
// only op_type and domain, or std::nullopt if it is malformed.
//
// TODO(crbug.com/574079183): Require an explicit `embed_mode` attribute equal
// to 1. With embed_mode=0, `ep_cache_context` is a file path the EP opens.
std::optional<bool> IsEpContextNode(base::span<const uint8_t> node) {
  Reader reader(node);
  base::span<const uint8_t> op_type;
  base::span<const uint8_t> domain;
  while (reader.remaining() != 0) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!ReadTag(reader, field, wire)) {
      return std::nullopt;
    }
    if (field == kNodeProtoOpType && wire == 2) {
      if (!ReadLengthDelimited(reader, op_type)) {
        return std::nullopt;
      }
    } else if (field == kNodeProtoDomain && wire == 2) {
      if (!ReadLengthDelimited(reader, domain)) {
        return std::nullopt;
      }
    } else if (!SkipField(reader, wire)) {
      return std::nullopt;
    }
  }
  return SpanEquals(op_type, "EPContext") &&
         SpanEquals(domain, "com.microsoft");
}

// Walks a GraphProto, counting its nodes. Any node that is not an EPContext
// node is rejected: the graph must contain EPContext nodes and nothing else.
//
// TODO(crbug.com/574059455): Allowlist the fields at every level instead of
// skipping the unknown ones. ORT still acts on some of them, e.g. external
// data in `initializer`/`sparse_initializer`, GRAPH attributes, which become
// subgraphs on any node, and ModelProto `functions`.
EpContextModelResult ParseGraph(base::span<const uint8_t> graph,
                                size_t& node_count) {
  Reader reader(graph);
  while (reader.remaining() != 0) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!ReadTag(reader, field, wire)) {
      return EpContextModelResult::kMalformed;
    }
    if (field == kGraphProtoNode && wire == 2) {
      base::span<const uint8_t> node;
      if (!ReadLengthDelimited(reader, node)) {
        return EpContextModelResult::kMalformed;
      }
      std::optional<bool> is_ep_context = IsEpContextNode(node);
      if (!is_ep_context.has_value()) {
        return EpContextModelResult::kMalformed;
      }
      if (!*is_ep_context) {
        return EpContextModelResult::kNonEpContextNode;
      }
      ++node_count;
    } else if (!SkipField(reader, wire)) {
      return EpContextModelResult::kMalformed;
    }
  }
  return EpContextModelResult::kAccepted;
}

std::string_view ResultToString(EpContextModelResult result) {
  switch (result) {
    case EpContextModelResult::kAccepted:
      return "accepted";
    case EpContextModelResult::kMalformed:
      return "malformed EPContext model";
    case EpContextModelResult::kNoGraph:
      return "missing graph";
    case EpContextModelResult::kDuplicateGraph:
      return "duplicate graph";
    case EpContextModelResult::kNonEpContextNode:
      return "non-EPContext node";
    case EpContextModelResult::kWrongNodeCount:
      return "expected exactly one EPContext node";
  }
  NOTREACHED();
}

}  // namespace

EpContextModelResult ValidateEpContextModel(
    base::span<const uint8_t> model_data) {
  Reader reader(model_data);
  bool saw_graph = false;
  size_t node_count = 0;
  while (reader.remaining() != 0) {
    uint32_t field = 0;
    uint32_t wire = 0;
    if (!ReadTag(reader, field, wire)) {
      return EpContextModelResult::kMalformed;
    }
    if (field == kModelProtoGraph && wire == 2) {
      // A ModelProto has a single `graph`.
      if (saw_graph) {
        return EpContextModelResult::kDuplicateGraph;
      }
      base::span<const uint8_t> graph;
      if (!ReadLengthDelimited(reader, graph)) {
        return EpContextModelResult::kMalformed;
      }
      EpContextModelResult graph_result = ParseGraph(graph, node_count);
      if (graph_result != EpContextModelResult::kAccepted) {
        return graph_result;
      }
      saw_graph = true;
    } else if (!SkipField(reader, wire)) {
      return EpContextModelResult::kMalformed;
    }
  }
  if (!saw_graph) {
    return EpContextModelResult::kNoGraph;
  }
  if (node_count != 1u) {
    return EpContextModelResult::kWrongNodeCount;
  }
  return EpContextModelResult::kAccepted;
}

bool PrepareCompiledModel(mojo_base::BigBuffer& model_data,
                          std::string_view ep_name,
                          mojo::ReportBadMessageCallback report_bad_message) {
  // The BigBuffer may be shared memory the Compiler can still write, so
  // validating it in place would be a TOCTOU.
  model_data.MakePrivateBytes();

  if (EpCompiledModelFormat(ep_name) != CompiledModelFormat::kEpContext) {
    return true;
  }

  EpContextModelResult result = ValidateEpContextModel(model_data);
  base::UmaHistogramEnumeration("WebNN.ORT.EpContextModelResult", result);
  if (result != EpContextModelResult::kAccepted) {
    std::move(report_bad_message)
        .Run(base::StrCat(
            {"Rejected compiled model: ", ResultToString(result)}));
    return false;
  }
  return true;
}

}  // namespace webnn::ort
