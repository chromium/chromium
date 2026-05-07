// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_XML_DOM_BUILDER_H_
#define SERVICES_DATA_DECODER_XML_DOM_BUILDER_H_

#include "third_party/rust/cxx/v1/cxx.h"

namespace data_decoder::xml {

class Node;

namespace ffi {

std::unique_ptr<Node> create_element(rust::Str local_name);
std::unique_ptr<Node> create_text_node(rust::Str text_content);
std::unique_ptr<Node> create_cdata_node(rust::Str text_content);
void set_attribute(Node& element, rust::Str local_name, rust::Str value);
void add_child(Node& parent, std::unique_ptr<Node> child);

}  // namespace ffi
}  // namespace data_decoder::xml

#endif  // SERVICES_DATA_DECODER_XML_DOM_BUILDER_H_
