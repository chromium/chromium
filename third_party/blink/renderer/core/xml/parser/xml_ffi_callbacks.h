// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_FFI_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_FFI_CALLBACKS_H_

#include "third_party/rust/cxx/v1/cxx.h"

namespace xml_ffi {

struct AttributesIterator;
struct NamespacesIterator;
enum class StandaloneInfo : ::std::int8_t;

class XmlCallbacks {
 public:
  virtual void StartDocument(rust::Str version,
                             rust::Str encoding,
                             xml_ffi::StandaloneInfo standaloneInfo) = 0;
  virtual void ProcessingInstruction(rust::Str target, rust::Str data) = 0;
  virtual void StartElementNs(rust::Str local_name,
                              bool has_prefix,
                              rust::Str prefix,
                              bool has_ns,
                              rust::Str ns,
                              AttributesIterator& attributes,
                              NamespacesIterator& namespaces) = 0;
  virtual void EndElementNs(rust::Str local_name,
                            rust::Str prefix,
                            rust::Str ns) = 0;
  virtual void Characters(rust::Str characters) = 0;
  virtual void CData(rust::Str data) = 0;
  virtual void Comment(rust::Str comment) = 0;
  virtual void DocType(rust::Str name,
                       rust::Str public_id,
                       rust::Str system_id) = 0;
  virtual void EndDocument() = 0;
};

}  // namespace xml_ffi

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_XML_PARSER_XML_FFI_CALLBACKS_H_
