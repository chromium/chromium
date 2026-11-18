// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/epcontext_model_validator_test_support.h"

namespace webnn::ort {

namespace {

// ---- protobuf encoding helpers (ONNX envelope) ----

void AppendBytes(Bytes& out, base::span<const uint8_t> bytes) {
  out.insert(out.end(), bytes.begin(), bytes.end());
}

void AppendVarint(Bytes& out, uint64_t value) {
  do {
    uint8_t byte = static_cast<uint8_t>(value & 0x7F);
    value >>= 7;
    if (value) {
      byte |= 0x80;
    }
    out.push_back(byte);
  } while (value);
}

void AppendTag(Bytes& out, uint32_t field, uint32_t wire) {
  AppendVarint(out, (static_cast<uint64_t>(field) << 3) | wire);
}

void AppendLenField(Bytes& out,
                    uint32_t field,
                    base::span<const uint8_t> bytes) {
  AppendTag(out, field, /*wire=*/2);
  AppendVarint(out, bytes.size());
  AppendBytes(out, bytes);
}

void AppendStrField(Bytes& out, uint32_t field, std::string_view text) {
  AppendLenField(out, field, base::as_byte_span(text));
}

}  // namespace

Bytes BuildStringAttribute(std::string_view name,
                           base::span<const uint8_t> value) {
  Bytes attr;
  AppendStrField(attr, /*field=*/1, name);  // name
  AppendTag(attr, /*field=*/20, /*wire=*/0);
  AppendVarint(attr, 3);                     // type = STRING
  AppendLenField(attr, /*field=*/4, value);  // s (bytes)
  return attr;
}

Bytes BuildIntAttribute(std::string_view name,
                        std::optional<int64_t> value,
                        bool with_type) {
  Bytes attr;
  AppendStrField(attr, /*field=*/1, name);  // name
  if (with_type) {
    AppendTag(attr, /*field=*/20, /*wire=*/0);
    AppendVarint(attr, 2);  // type = INT
  }
  if (value.has_value()) {
    AppendTag(attr, /*field=*/3, /*wire=*/0);  // i
    AppendVarint(attr, static_cast<uint64_t>(*value));
  }
  return attr;
}

Bytes BuildNode(std::string_view op_type,
                std::string_view domain,
                const std::vector<Bytes>& attributes) {
  Bytes node;
  AppendStrField(node, /*field=*/4, op_type);  // op_type
  AppendStrField(node, /*field=*/7, domain);   // domain
  for (const Bytes& attr : attributes) {
    AppendLenField(node, /*field=*/5, attr);  // attribute
  }
  return node;
}

Bytes BuildEpContextNode(base::span<const uint8_t> blob) {
  std::vector<Bytes> attributes = {BuildIntAttribute("embed_mode", 1)};
  if (!blob.empty()) {
    attributes.push_back(BuildStringAttribute("ep_cache_context", blob));
  }
  return BuildNode("EPContext", "com.microsoft", attributes);
}

Bytes BuildGraph(const std::vector<Bytes>& nodes) {
  Bytes graph;
  for (const Bytes& node : nodes) {
    AppendLenField(graph, /*field=*/1, node);  // node
  }
  return graph;
}

Bytes BuildModelFromGraphs(const std::vector<Bytes>& graphs) {
  Bytes model;
  for (const Bytes& graph : graphs) {
    AppendLenField(model, /*field=*/7, graph);  // graph
  }
  return model;
}

Bytes BuildModel(const std::vector<Bytes>& nodes) {
  return BuildModelFromGraphs({BuildGraph(nodes)});
}

}  // namespace webnn::ort
