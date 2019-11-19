// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/tools/compositor_model_bench/render_tree.h"

#include <memory>
#include <sstream>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "gpu/tools/compositor_model_bench/shaders.h"

using base::JSONReader;
using base::JSONWriter;
using base::ReadFileToString;
using base::Value;

GLenum TextureFormatFromString(const std::string& format) {
  if (format == "RGBA")
    return GL_RGBA;
  if (format == "RGB")
    return GL_RGB;
  if (format == "LUMINANCE")
    return GL_LUMINANCE;
  return GL_INVALID_ENUM;
}

const char* TextureFormatName(GLenum format) {
  switch (format) {
    case GL_RGBA:
      return "RGBA";
    case GL_RGB:
      return "RGB";
    case GL_LUMINANCE:
      return "LUMINANCE";
    default:
      return "(unknown format)";
  }
}

int FormatBytesPerPixel(GLenum format) {
  switch (format) {
    case GL_RGBA:
      return 4;
    case GL_RGB:
      return 3;
    case GL_LUMINANCE:
      return 1;
    default:
      return 0;
  }
}

RenderNode::RenderNode() {
}

RenderNode::~RenderNode() {
}

void RenderNode::Accept(RenderNodeVisitor* v) {
  v->BeginVisitRenderNode(this);
  v->EndVisitRenderNode(this);
}

ContentLayerNode::ContentLayerNode() {
}

ContentLayerNode::~ContentLayerNode() {
}

void ContentLayerNode::Accept(RenderNodeVisitor* v) {
  v->BeginVisitContentLayerNode(this);
  for (auto& child : children_) {
    child->Accept(v);
  }
  v->EndVisitContentLayerNode(this);
}

CCNode::CCNode() {
}

CCNode::~CCNode() {
}

void CCNode::Accept(RenderNodeVisitor* v) {
  v->BeginVisitCCNode(this);
  v->EndVisitCCNode(this);
}

RenderNodeVisitor::~RenderNodeVisitor() {
}

void RenderNodeVisitor::BeginVisitContentLayerNode(ContentLayerNode* v) {
  this->BeginVisitRenderNode(v);
}

void RenderNodeVisitor::BeginVisitCCNode(CCNode* v) {
  this->BeginVisitRenderNode(v);
}

void RenderNodeVisitor::EndVisitRenderNode(RenderNode* v) {
}

void RenderNodeVisitor::EndVisitContentLayerNode(ContentLayerNode* v) {
  this->EndVisitRenderNode(v);
}

void RenderNodeVisitor::EndVisitCCNode(CCNode* v) {
  this->EndVisitRenderNode(v);
}

std::unique_ptr<RenderNode> InterpretNode(const base::Value& node);

// Makes sure that the key exists and has the type we expect.
bool VerifyDictionaryEntry(const base::Value& node,
                           const std::string& key,
                           Value::Type type) {
  const Value* value = node.FindKey(key);

  if (!value) {
    LOG(ERROR) << "Missing value for key: " << key;
    return false;
  }

  if (value->type() != type) {
    LOG(ERROR) << key
               << " did not have the expected type "
                  "(expected "
               << base::Value::GetTypeName(type) << ")";
    return false;
  }

  return true;
}

// Makes sure that the list entry has the type we expect.
bool VerifyListEntry(const base::Value& list,
                     int index,
                     Value::Type type,
                     const char* listName = nullptr) {
  // Assume the index is valid (since we'll be able to generate a better
  // error message for this elsewhere.)
  if (list.GetList()[index].type() != type) {
    LOG(ERROR) << (listName ? listName : "List") << "element " << index
               << " did not have the expected type (expected "
               << base::Value::GetTypeName(type) << ")\n";
    return false;
  }

  return true;
}

bool InterpretCommonContents(const base::Value& node, RenderNode* c) {
  if (!VerifyDictionaryEntry(node, "layerID", Value::Type::INTEGER) ||
      !VerifyDictionaryEntry(node, "width", Value::Type::INTEGER) ||
      !VerifyDictionaryEntry(node, "height", Value::Type::INTEGER) ||
      !VerifyDictionaryEntry(node, "drawsContent", Value::Type::BOOLEAN) ||
      !VerifyDictionaryEntry(node, "targetSurfaceID", Value::Type::INTEGER) ||
      !VerifyDictionaryEntry(node, "transform", Value::Type::LIST)) {
    return false;
  }

  c->set_layerID(node.FindIntKey("layerID").value());
  c->set_width(node.FindIntKey("width").value());
  c->set_height(node.FindIntKey("height").value());
  c->set_drawsContent(node.FindBoolKey("drawsContent").value());
  c->set_targetSurface(node.FindIntKey("targetSurfaceID").value());

  const Value* transform = node.FindKey("transform");
  if (transform->GetList().size() != 16) {
    LOG(ERROR) << "4x4 transform matrix did not have 16 elements";
    return false;
  }
  float transform_mat[16];
  for (int i = 0; i < 16; ++i) {
    if (!VerifyListEntry(*transform, i, Value::Type::DOUBLE, "Transform"))
      return false;
    transform_mat[i] = transform->GetList()[i].GetDouble();
  }
  c->set_transform(transform_mat);

  const Value* tiles_dict = node.FindKey("tiles");
  if (!tiles_dict)
    return true;

  if (!VerifyDictionaryEntry(node, "tiles", Value::Type::DICTIONARY))
    return false;
  if (!VerifyDictionaryEntry(*tiles_dict, "dim", Value::Type::LIST))
    return false;
  const Value* dim = tiles_dict->FindKey("dim");
  if (!VerifyListEntry(*dim, 0, Value::Type::INTEGER, "Tile dimension") ||
      !VerifyListEntry(*dim, 1, Value::Type::INTEGER, "Tile dimension")) {
    return false;
  }
  c->set_tile_width(dim->GetList()[0].GetInt());
  c->set_tile_height(dim->GetList()[1].GetInt());

  if (!VerifyDictionaryEntry(*tiles_dict, "info", Value::Type::LIST))
    return false;
  const Value* tiles = tiles_dict->FindKey("info");
  for (unsigned int i = 0; i < tiles->GetList().size(); ++i) {
    if (!VerifyListEntry(*tiles, i, Value::Type::DICTIONARY, "Tile info"))
      return false;
    const Value& tdict = tiles->GetList()[i];

    if (!VerifyDictionaryEntry(tdict, "x", Value::Type::INTEGER) ||
        !VerifyDictionaryEntry(tdict, "y", Value::Type::INTEGER)) {
      return false;
    }
    Tile t;
    t.x = tdict.FindIntKey("x").value();
    t.y = tdict.FindIntKey("y").value();
    const Value* texID = tdict.FindKey("texID");
    if (texID) {
      if (!VerifyDictionaryEntry(tdict, "texID", Value::Type::INTEGER))
        return false;
      t.texID = texID->GetInt();
    } else {
      t.texID = -1;
    }
    c->add_tile(t);
  }
  return true;
}

bool InterpretCCData(const base::Value& node, CCNode* c) {
  if (!VerifyDictionaryEntry(node, "vertex_shader", Value::Type::STRING) ||
      !VerifyDictionaryEntry(node, "fragment_shader", Value::Type::STRING) ||
      !VerifyDictionaryEntry(node, "textures", Value::Type::LIST)) {
    return false;
  }

  std::string vertex_shader_name = *node.FindStringKey("vertex_shader");
  std::string fragment_shader_name = *node.FindStringKey("fragment_shader");
  c->set_vertex_shader(ShaderIDFromString(vertex_shader_name));
  c->set_fragment_shader(ShaderIDFromString(fragment_shader_name));

  const Value* textures = node.FindKey("textures");
  for (unsigned int i = 0; i < textures->GetList().size(); ++i) {
    if (!VerifyListEntry(*textures, i, Value::Type::DICTIONARY, "Tex list"))
      return false;
    const Value& tex = textures->GetList()[i];

    if (!VerifyDictionaryEntry(tex, "texID", Value::Type::INTEGER) ||
        !VerifyDictionaryEntry(tex, "height", Value::Type::INTEGER) ||
        !VerifyDictionaryEntry(tex, "width", Value::Type::INTEGER) ||
        !VerifyDictionaryEntry(tex, "format", Value::Type::STRING)) {
      return false;
    }
    Texture t;
    t.texID = tex.FindIntKey("texID").value();
    t.height = tex.FindIntKey("height").value();
    t.width = tex.FindIntKey("width").value();

    const std::string* format_name = tex.FindStringKey("format");
    t.format = TextureFormatFromString(*format_name);
    if (t.format == GL_INVALID_ENUM) {
      LOG(ERROR) << "Unrecognized texture format in layer " << c->layerID()
                 << " (format: " << *format_name
                 << ")\n"
                    "The layer had "
                 << textures->GetList().size() << " children.";
      return false;
    }

    c->add_texture(t);
  }

  if (c->vertex_shader() == SHADER_UNRECOGNIZED) {
    LOG(ERROR) << "Unrecognized vertex shader name, layer " << c->layerID()
               << " (shader: " << vertex_shader_name << ")";
    return false;
  }

  if (c->fragment_shader() == SHADER_UNRECOGNIZED) {
    LOG(ERROR) << "Unrecognized fragment shader name, layer " << c->layerID()
               << " (shader: " << fragment_shader_name << ")";
    return false;
  }

  return true;
}

std::unique_ptr<RenderNode> InterpretContentLayer(const base::Value& node) {
  auto n = std::make_unique<ContentLayerNode>();
  if (!InterpretCommonContents(node, n.get()))
    return nullptr;

  if (!VerifyDictionaryEntry(node, "type", Value::Type::STRING) ||
      !VerifyDictionaryEntry(node, "skipsDraw", Value::Type::BOOLEAN) ||
      !VerifyDictionaryEntry(node, "children", Value::Type::LIST)) {
    return nullptr;
  }

  DCHECK_EQ(*node.FindStringKey("type"), "ContentLayer");

  n->set_skipsDraw(node.FindBoolKey("skipsDraw").value());

  const Value* children = node.FindKey("children");
  for (unsigned int i = 0; i < children->GetList().size(); ++i) {
    const Value& child_node = children->GetList()[i];
    if (!child_node.is_dict())
      continue;
    std::unique_ptr<RenderNode> child = InterpretNode(child_node);
    if (child)
      n->add_child(child.release());
  }

  return std::move(n);
}

std::unique_ptr<RenderNode> InterpretCanvasLayer(const base::Value& node) {
  auto n = std::make_unique<CCNode>();
  if (!InterpretCommonContents(node, n.get()))
    return nullptr;

  if (!VerifyDictionaryEntry(node, "type", Value::Type::STRING))
    return nullptr;

  DCHECK_EQ(*node.FindStringKey("type"), "CanvasLayer");

  if (!InterpretCCData(node, n.get()))
    return nullptr;

  return std::move(n);
}

std::unique_ptr<RenderNode> InterpretVideoLayer(const base::Value& node) {
  auto n = std::make_unique<CCNode>();
  if (!InterpretCommonContents(node, n.get()))
    return nullptr;

  if (!VerifyDictionaryEntry(node, "type", Value::Type::STRING))
    return nullptr;

  DCHECK_EQ(*node.FindStringKey("type"), "VideoLayer");

  if (!InterpretCCData(node, n.get()))
    return nullptr;

  return std::move(n);
}

std::unique_ptr<RenderNode> InterpretImageLayer(const base::Value& node) {
  auto n = std::make_unique<CCNode>();
  if (!InterpretCommonContents(node, n.get()))
    return nullptr;

  if (!VerifyDictionaryEntry(node, "type", Value::Type::STRING))
    return nullptr;

  DCHECK_EQ(*node.FindStringKey("type"), "ImageLayer");

  if (!InterpretCCData(node, n.get()))
    return nullptr;

  return std::move(n);
}

std::unique_ptr<RenderNode> InterpretNode(const base::Value& node) {
  if (!VerifyDictionaryEntry(node, "type", Value::Type::STRING))
    return nullptr;

  const std::string* type = node.FindStringKey("type");
  if (*type == "ContentLayer")
    return InterpretContentLayer(node);
  if (*type == "CanvasLayer")
    return InterpretCanvasLayer(node);
  if (*type == "VideoLayer")
    return InterpretVideoLayer(node);
  if (*type == "ImageLayer")
    return InterpretImageLayer(node);

  std::string outjson;
  JSONWriter::WriteWithOptions(node, base::JSONWriter::OPTIONS_PRETTY_PRINT,
                               &outjson);
  LOG(ERROR) << "Unrecognized node type! JSON:\n\n"
                "-----------------------\n"
             << outjson << "-----------------------";

  return nullptr;
}

std::unique_ptr<RenderNode> BuildRenderTreeFromFile(
    const base::FilePath& path) {
  LOG(INFO) << "Reading " << path.LossyDisplayName();
  std::string contents;
  if (!ReadFileToString(path, &contents))
    return nullptr;

  JSONReader::ValueWithError result = JSONReader::ReadAndReturnValueWithError(
      contents, base::JSON_ALLOW_TRAILING_COMMAS);
  if (!result.value.has_value() || !result.value->is_dict()) {
    LOG(ERROR) << "Failed to parse JSON file " << path.LossyDisplayName()
               << "\n(" << result.error_message << ")";
    return nullptr;
  }

  return InterpretContentLayer(result.value.value());
}
