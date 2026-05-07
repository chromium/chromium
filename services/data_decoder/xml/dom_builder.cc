// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/xml/dom_builder.h"

#include <utility>

#include "services/data_decoder/public/cpp/xml_dom.h"

namespace data_decoder::xml::ffi {

class DomBuilder {
 public:
  static base::PassKey<DomBuilder> CreatePassKey() {
    return base::PassKey<DomBuilder>();
  }
};

std::unique_ptr<Node> create_element(rust::Str local_name) {
  return Node::CreateElement(DomBuilder::CreatePassKey(),
                             std::string(local_name));
}

std::unique_ptr<Node> create_text_node(rust::Str text_content) {
  return Node::CreateTextNode(DomBuilder::CreatePassKey(),
                              std::string(text_content));
}

std::unique_ptr<Node> create_cdata_node(rust::Str text_content) {
  return Node::CreateCdataNode(DomBuilder::CreatePassKey(),
                               std::string(text_content));
}

void set_attribute(Node& element, rust::Str local_name, rust::Str value) {
  element.SetAttribute(DomBuilder::CreatePassKey(), std::string(local_name),
                       std::string(value));
}

void add_child(Node& parent, std::unique_ptr<Node> child) {
  parent.AddChild(DomBuilder::CreatePassKey(), std::move(child));
}

}  // namespace data_decoder::xml::ffi
