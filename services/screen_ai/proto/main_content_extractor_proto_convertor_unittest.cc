// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/screen_ai/proto/main_content_extractor_proto_convertor.h"

#include <string_view>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_proto_loader.h"
#include "build/build_config.h"
#include "services/screen_ai/proto/view_hierarchy.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_update.h"

namespace {

// To update the test expectations:
//  1- Set the following to 'true' to get debug protos.
//  2- Replace the expected proto by the generated file.
#define WRITE_DEBUG_PROTO false

// A dummy tree node definition for PreOrderTreeGeneration.
constexpr int kMaxChildInTemplate = 3;
struct NodeTemplate {
  ui::AXNodeID node_id;
  int child_count;
  ui::AXNodeID child_ids[kMaxChildInTemplate];
};

ui::AXTreeUpdate CreateAXTreeUpdateFromTemplate(int root_id,
                                                NodeTemplate* nodes_template,
                                                int nodes_count) {
  ui::AXTreeUpdate update;
  update.root_id = root_id;

  for (int i = 0; i < nodes_count; i++) {
    ui::AXNodeData node;
    node.id = nodes_template[i].node_id;
    for (int j = 0; j < nodes_template[i].child_count; j++)
      node.child_ids.push_back(nodes_template[i].child_ids[j]);
    node.relative_bounds.bounds = gfx::RectF(0, 0, 100, 100);
    update.nodes.push_back(node);
  }
  return update;
}

int GetAxNodeID(const ::screenai::UiElement& ui_element) {
  for (const auto& attribute : ui_element.attributes()) {
    if (attribute.name() == "axnode_id")
      return attribute.int_value();
  }
  return static_cast<int>(ui::kInvalidAXNodeID);
}

base::FilePath GetTestFilePath(std::string_view file_name) {
  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
  return path.AppendASCII("services/test/data/screen_ai")
      .AppendASCII(file_name);
}

void WriteDebugProto(const std::string& serialized_proto,
                     const std::string& file_name) {
  if (!WRITE_DEBUG_PROTO)
    return;

  base::FilePath path;
  EXPECT_TRUE(base::PathService::Get(base::DIR_TEMP, &path));
  path = path.AppendASCII(file_name);

  if (base::WriteFile(path, serialized_proto)) {
    LOG(INFO) << "Debug proto is written to: " << path;
  } else {
    LOG(ERROR) << "Could not write debug proto to: " << path;
  }
}

std::string ConvertProtoToText(const std::string& proto_binary) {
  base::FilePath descriptor_full_path;
  if (!base::PathService::Get(base::DIR_GEN_TEST_DATA_ROOT,
                              &descriptor_full_path)) {
    LOG(ERROR) << "Generated test data root not found!";
    return "";
  }
  descriptor_full_path = descriptor_full_path.AppendASCII(
      "services/screen_ai/proto/view_hierarchy.descriptor");

  screenai::ViewHierarchy proto;
  base::TestProtoLoader loader(descriptor_full_path, proto.GetTypeName());
  std::string serialized_message;
  loader.PrintToText(proto_binary, serialized_message);
  return serialized_message;
}

ui::AXTree CreateSampleTree() {
  // AXTree title=Test Tree
  // id=1 rootWebArea child_ids=2,4,6 (4, 8)-(15, 16)
  // non_atomic_text_field_root=true
  // ++id=2 paragraph child_ids=3 (0, 0)-(0, 0) is_line_breaking_object=true
  // ++++id=3 staticText EDITABLE name=StaticText00 (0, 0)-(0, 0)
  // ++id=4 paragraph child_ids=5 (0, 0)-(0, 0) is_line_breaking_object=true
  // ++++id=5 staticText name=StaticText10 (0, 0)-(0, 0)
  // ++id=6 paragraph child_ids=7 (0, 0)-(0, 0) is_line_breaking_object=true
  // ++++id=7 link LINKED child_ids=8,9 (0, 0)-(0, 0)
  // ++++++id=8 staticText name=StaticText200 (0, 0)-(0, 0)
  // ++++++id=9 staticText name=StaticText201 (0, 0)-(0, 0)

  ui::AXNodeData root;
  ui::AXNodeData paragraph_0;
  ui::AXNodeData static_text_0_0;
  ui::AXNodeData paragraph_1;
  ui::AXNodeData static_text_1_0;
  ui::AXNodeData paragraph_2;
  ui::AXNodeData link_2_0;
  ui::AXNodeData static_text_2_0_0;
  ui::AXNodeData static_text_2_0_1;

  root.id = 1;
  paragraph_0.id = 2;
  static_text_0_0.id = 3;
  paragraph_1.id = 4;
  static_text_1_0.id = 5;
  paragraph_2.id = 6;
  link_2_0.id = 7;
  static_text_2_0_0.id = 8;
  static_text_2_0_1.id = 9;

  root.role = ax::mojom::Role::kRootWebArea;
  root.AddBoolAttribute(ax::mojom::BoolAttribute::kNonAtomicTextFieldRoot,
                        true);
  root.relative_bounds.bounds = gfx::RectF(4, 8, 15, 16);
  root.child_ids = {paragraph_0.id, paragraph_1.id, paragraph_2.id};

  paragraph_0.role = ax::mojom::Role::kParagraph;
  paragraph_0.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  paragraph_0.child_ids = {static_text_0_0.id};

  static_text_0_0.role = ax::mojom::Role::kStaticText;
  static_text_0_0.AddState(ax::mojom::State::kEditable);
  static_text_0_0.SetName("StaticText00");

  paragraph_1.role = ax::mojom::Role::kParagraph;
  paragraph_1.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  paragraph_1.child_ids = {static_text_1_0.id};

  static_text_1_0.role = ax::mojom::Role::kStaticText;
  static_text_1_0.SetName("StaticText10");

  paragraph_2.role = ax::mojom::Role::kParagraph;
  paragraph_2.AddBoolAttribute(ax::mojom::BoolAttribute::kIsLineBreakingObject,
                               true);
  paragraph_2.child_ids = {link_2_0.id};

  link_2_0.role = ax::mojom::Role::kLink;
  link_2_0.AddState(ax::mojom::State::kLinked);
  link_2_0.child_ids = {static_text_2_0_0.id, static_text_2_0_1.id};

  static_text_2_0_0.role = ax::mojom::Role::kStaticText;
  static_text_2_0_0.SetName("StaticText200");

  static_text_2_0_1.role = ax::mojom::Role::kStaticText;
  static_text_2_0_1.SetName("StaticText201");

  ui::AXTreeUpdate initial_state;
  initial_state.root_id = root.id;
  initial_state.nodes = {root,        paragraph_0,       static_text_0_0,
                         paragraph_1, static_text_1_0,   paragraph_2,
                         link_2_0,    static_text_2_0_0, static_text_2_0_1};
  initial_state.has_tree_data = true;

  ui::AXTreeData tree_data;
  tree_data.tree_id = ui::AXTreeID::CreateNewAXTreeID();
  tree_data.title = "Test Tree";
  initial_state.tree_data = tree_data;

  return ui::AXTree(initial_state);
}

}  // namespace

namespace screen_ai {

using MainContentExtractorProtoConvertorTest = testing::Test;

// Tests if the given tree is properly traversed and new ids are assigned.
TEST_F(MainContentExtractorProtoConvertorTest, PreOrderTreeGeneration) {
  // Input Tree:
  // +-- 1
  //     +-- 2
  //         +-- 7
  //         +-- 8
  //             +-- 3
  //     +-- 4
  //         +-- 5
  //         +-- 6
  //         +-- 9
  //     +-- -20

  // Input tree is added in shuffled order to avoid order assumption.
  NodeTemplate input_tree[] = {
      {1, 3, {2, 4, -20}}, {4, 3, {5, 6, 9}}, {6, 0, {}}, {5, 0, {}},
      {2, 2, {7, 8}},      {8, 1, {3}},       {3, 0, {}}, {7, 0, {}},
      {9, 0, {}},          {-20, 0, {}}};
  const int nodes_count = sizeof(input_tree) / sizeof(NodeTemplate);

  // Expected order of nodes in the output.
  int expected_order[] = {1, 2, 7, 8, 3, 4, 5, 6, 9, -20};

  // Create the tree, convert it, and decode from proto.
  ui::AXTreeUpdate tree_update =
      CreateAXTreeUpdateFromTemplate(1, input_tree, nodes_count);
  ui::AXTree tree(tree_update);
  std::optional<ViewHierarchyAndTreeSize> result =
      SnapshotToViewHierarchy(tree);
  ASSERT_TRUE(result);
  screenai::ViewHierarchy view_hierarchy;
  ASSERT_TRUE(view_hierarchy.ParseFromString(result->serialized_proto));

  // Verify.
  EXPECT_EQ(view_hierarchy.ui_elements().size(), nodes_count);
  for (int i = 0; i < nodes_count; i++) {
    const screenai::UiElement& ui_element = view_hierarchy.ui_elements(i);

    // Expect node to be correctly re-ordered.
    EXPECT_EQ(expected_order[i], GetAxNodeID(ui_element));

    // Expect node at index 'i' has id 'i'
    EXPECT_EQ(ui_element.id(), i);
  }
}

TEST_F(MainContentExtractorProtoConvertorTest, ProtoTest) {
  ui::AXTree tree = CreateSampleTree();

  std::optional<ViewHierarchyAndTreeSize> result =
      SnapshotToViewHierarchy(tree);
  ASSERT_TRUE(result);
  std::string generated_proto = ConvertProtoToText(result->serialized_proto);
  WriteDebugProto(generated_proto, "expected_proto.pbtxt");
  std::string expected_proto;
  ASSERT_TRUE(base::ReadFileToString(GetTestFilePath("expected_proto.pbtxt"),
                                     &expected_proto));
  ASSERT_EQ(generated_proto, expected_proto);
}

}  // namespace screen_ai
