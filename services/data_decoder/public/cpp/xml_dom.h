// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_
#define SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_

#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/types/pass_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

class XmlDocument;

namespace data_decoder {
class XmlDomTest;  // Forward declaration for friending
}

class XmlNode {
 public:
  enum class Type { kELEMENT, kTEXT };

  ~XmlNode();

  // Get the type of the node
  Type GetType() const;

  // Get the tag name (only for ELEMENT nodes)
  const std::string& GetTagName() const;

  // Get the text content (only for TEXT nodes)
  const std::string& GetTextContent() const;

  // Get an attribute value by name (only for ELEMENT nodes)
  std::optional<std::string> GetAttribute(std::string_view name) const;

  // Get all child nodes
  const std::vector<std::unique_ptr<XmlNode>>& GetChildren() const;

  // Get parent node
  const XmlNode* GetParent() const;

  // Find all direct child elements with a specific tag name
  std::vector<const XmlNode*> GetChildrenByTagName(
      std::string_view tag_name) const;

  // Get the first direct child element with a specific tag name
  const XmlNode* GetFirstChildByTagName(std::string_view tag_name) const;

  // Testing methods.
  void AddChildForTesting(std::unique_ptr<XmlNode> child);
  void SetAttributeForTesting(const std::string& name,
                              const std::string& value);

 private:
  friend class XmlDocument;
  // Private constructor for all node types.
  XmlNode(base::PassKey<XmlDocument>,
          Type type,
          const std::string& tag_name,
          const std::string& text_content);

  struct ElementData {
    ElementData();
    explicit ElementData(std::string tag_name);
    ElementData(ElementData&&);
    ElementData& operator=(ElementData&&);
    ~ElementData();

    std::string tag;
    absl::flat_hash_map<std::string, std::string> attributes;
    std::vector<std::unique_ptr<XmlNode>> children;
  };
  std::variant<ElementData, std::string> data_;
  raw_ptr<XmlNode> parent_;
};

class XmlDocument {
 public:
  XmlDocument();
  ~XmlDocument();

  // Disallow copy and assign
  XmlDocument(const XmlDocument&) = delete;
  XmlDocument& operator=(const XmlDocument&) = delete;

  // Allow move
  XmlDocument(XmlDocument&&);
  XmlDocument& operator=(XmlDocument&&);

  // Get the root element of the document
  const XmlNode* GetRoot() const;

  // Find the first element with a given tag name (DFS)
  const XmlNode* FindFirstElementByTagName(std::string_view tag_name) const;

  // Testing methods.
  void SetRootForTesting(std::unique_ptr<XmlNode> root);
  static std::unique_ptr<XmlNode> CreateElementNodeForTesting(
      const std::string& tag_name);
  static std::unique_ptr<XmlNode> CreateTextNodeForTesting(
      const std::string& text_content);

 private:
  std::unique_ptr<XmlNode> root_;

  const XmlNode* FindFirstElementByTagNameRecursive(
      const XmlNode* node,
      std::string_view tag_name) const;
};

#endif  // SERVICES_DATA_DECODER_PUBLIC_CPP_XML_DOM_H_
