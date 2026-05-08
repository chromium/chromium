// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace data_decoder::xml {

namespace ffi {
class DomBuilder;
}

class Node;

// A qualified name in XML, as defined in https://www.w3.org/TR/xml-names/.
// Used for both element and attribute names.
struct Name {
  std::string_view local_name;
  std::string_view prefix;
};

// Equivalent to `Name` but owns its fields.
struct OwnedName {
  std::string local_name;
  std::string prefix;

  bool operator==(const OwnedName& other) const = default;
  bool operator==(const Name& other) const {
    return local_name == other.local_name && prefix == other.prefix;
  }

  struct absl_container_hash {
    using is_transparent = void;

    size_t operator()(const OwnedName& name) const {
      return absl::HashOf(name.local_name, name.prefix);
    }

    size_t operator()(const Name& name) const {
      return absl::HashOf(name.local_name, name.prefix);
    }
  };
};

class Document {
 public:
  explicit Document(std::unique_ptr<Node> root);
  ~Document();

  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;
  Document(Document&&);
  Document& operator=(Document&&);

  static base::expected<Document, std::string> FromBytes(
      base::span<const uint8_t> bytes);
  static base::expected<Document, std::string> FromUtf8(std::string_view str);

  const Node* GetRoot() const;

  // Depth-first search for the first element with a matching `name`, or
  // `nullptr` if there is no such element.
  const Node* FindFirstElementByTagName(Name name) const;

  // Returns a base::Value representation of the document compatible with
  // the legacy safe_xml_parser.h.
  base::Value ToValueForTesting() const;

 private:
  std::unique_ptr<Node> root_;
};

// This node representation only supports element nodes, text nodes, and CDATA
// nodes. The address of a Node is guaranteed to be stable.
//
// Unlike XML/XSLT/XPath, namespaces and attributes are not represented as a
// distinct node type and are just extra bits of data stored on element nodes,
// i.e. nodes for which `GetType() == Type::kElement` is true.
//
// TODO(dcheng): Implement namespace support.
class Node {
 public:
  enum class Type { kElement, kText, kCdata };

  ~Node();

  Type GetType() const;
  const Node* parent() const { return parent_; }

  // These methods are only usable on element nodes and will crash if called on
  // non-element nodes.
  const OwnedName& GetName() const;
  const std::string& GetLocalName() const;
  // The namespace prefix of the element name, or the empty string if the
  // element name is unprefixed. Shorthand for `GetName().prefix`.
  //   <html:br /> -> returns "html"
  //   <element /> -> returns ""
  const std::string& GetNamespacePrefix() const;

  const absl::flat_hash_map<OwnedName, std::string>& GetAttributes() const;
  // The value of the attribute with the given `name`, or `nullptr` if the
  // element does not specify an attribute with `name`.
  const std::string* GetAttribute(Name name) const;

  const absl::flat_hash_map<std::string, std::string>& GetNamespaces() const;

  const std::vector<std::unique_ptr<Node>>& GetChildren() const;
  std::vector<const Node*> GetChildrenByTagName(Name name) const;
  const Node* FindFirstChildByTagName(Name name) const;

  // These methods are only usable on text or cdata nodes and will crash if
  // called on non-text and non-cdata nodes.
  const std::string& GetTextContent() const;

  // Returns a base::Value representation of the document compatible with
  // the legacy safe_xml_parser.h.
  base::Value ToValueForTesting() const;

  // Rust FFI helpers:
  static std::unique_ptr<Node> CreateElement(base::PassKey<ffi::DomBuilder>,
                                             std::string local_name,
                                             std::string prefix);
  static std::unique_ptr<Node> CreateTextNode(base::PassKey<ffi::DomBuilder>,
                                              std::string text);
  static std::unique_ptr<Node> CreateCdataNode(base::PassKey<ffi::DomBuilder>,
                                               std::string text);
  void SetAttribute(base::PassKey<ffi::DomBuilder>,
                    std::string local_name,
                    std::string prefix,
                    std::string value);
  void SetNamespace(base::PassKey<ffi::DomBuilder>,
                    std::string prefix,
                    std::string uri);
  void AddChild(base::PassKey<ffi::DomBuilder>, std::unique_ptr<Node> child);

 private:
  template <typename T>
  explicit Node(T data) : data_(std::move(data)), parent_(nullptr) {}

  struct Element {
    Element();
    explicit Element(OwnedName name);
    Element(Element&&);
    Element& operator=(Element&&);
    ~Element();

    OwnedName name;
    absl::flat_hash_map<OwnedName, std::string> attributes;
    absl::flat_hash_map<std::string, std::string> namespaces;
    std::vector<std::unique_ptr<Node>> children;
  };
  struct Text {
    std::string text;
  };
  struct Cdata {
    std::string text;
  };
  std::variant<Element, Text, Cdata> data_;
  raw_ptr<Node> parent_;
};

}  // namespace data_decoder::xml

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_
