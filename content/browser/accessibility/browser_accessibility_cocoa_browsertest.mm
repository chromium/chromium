// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "url/gurl.h"

namespace content {

namespace {

class BrowserAccessibilityCocoaBrowserTest : public ContentBrowserTest {
 public:
  BrowserAccessibilityCocoaBrowserTest() {}
  ~BrowserAccessibilityCocoaBrowserTest() override {}

 protected:
  BrowserAccessibility* FindNode(ax::mojom::Role role) {
    BrowserAccessibility* root = GetManager()->GetRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role);
  }

  BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

 private:
  BrowserAccessibility* FindNodeInSubtree(BrowserAccessibility& node,
                                          ax::mojom::Role role) {
    if (node.GetRole() == role)
      return &node;
    for (BrowserAccessibility::PlatformChildIterator it =
             node.PlatformChildrenBegin();
         it != node.PlatformChildrenEnd(); ++it) {
      BrowserAccessibility* result = FindNodeInSubtree(*it, role);
      if (result)
        return result;
    }
    return nullptr;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXTextMarkerForTextEdit) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(R"HTML(data:text/html,
             <input />)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* text_field = FindNode(ax::mojom::Role::kTextField);
  ASSERT_NE(nullptr, text_field);
  EXPECT_TRUE(content::ExecuteScript(
      shell()->web_contents(), "document.querySelector('input').focus()"));

  content::SimulateKeyPress(shell()->web_contents(),
                            ui::DomKey::FromCharacter('B'), ui::DomCode::US_B,
                            ui::VKEY_B, false, false, false, false);

  base::scoped_nsobject<BrowserAccessibilityCocoa> cocoa_text_field(
      [ToBrowserAccessibilityCocoa(text_field) retain]);
  AccessibilityNotificationWaiter value_waiter(shell()->web_contents(),
                                               ui::kAXModeComplete,
                                               ax::mojom::Event::kValueChanged);
  value_waiter.WaitForNotification();
  AXTextEdit text_edit = [cocoa_text_field computeTextEdit];
  EXPECT_NE(text_edit.edit_text_marker, nil);

  EXPECT_EQ(
      content::AXTextMarkerToPosition(text_edit.edit_text_marker)->ToString(),
      "TextPosition anchor_id=4 text_offset=1 affinity=downstream "
      "annotated_text=B<>");
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXCellForColumnAndRow) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(R"HTML(data:text/html,
             <table>
               <thead style=display:block>
                 <tr>
                   <th>Name</th>
                   <th>LDAP</th>
                 </tr>
               </thead>
               <tbody style=display:block>
                 <tr>
                   <td>John Doe</td>
                   <td>johndoe@</td>
                 </tr>
                 <tr>
                   <td>Jenny Doe</td>
                   <td>jennydoe@</td>
                 </tr>
               </tbody>
             </table>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  waiter.WaitForNotification();

  BrowserAccessibility* table = FindNode(ax::mojom::Role::kTable);
  ASSERT_NE(nullptr, table);
  base::scoped_nsobject<BrowserAccessibilityCocoa> cocoa_table(
      [ToBrowserAccessibilityCocoa(table) retain]);

  // Test AXCellForColumnAndRow for four coordinates
  for (unsigned col = 0; col < 2; col++) {
    for (unsigned row = 0; row < 2; row++) {
      base::scoped_nsobject<BrowserAccessibilityCocoa> cell(
          [[cocoa_table accessibilityCellForColumn:col row:row] retain]);

      // It should be a cell.
      EXPECT_NSEQ(@"AXCell", [cell accessibilityRole]);

      // The column index and row index of the cell should match what we asked
      // for.
      EXPECT_NSEQ(NSMakeRange(col, 1), [cell accessibilityColumnIndexRange]);
      EXPECT_NSEQ(NSMakeRange(row, 1), [cell accessibilityRowIndexRange]);
    }
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestUnlabeledImageRoleDescription) {
  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(3);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                   "foo");
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kRoleDescription,
                                   "bar");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(tree, nullptr));

  for (int child_index = 0; child_index < int{tree.nodes[0].child_ids.size()};
       ++child_index) {
    BrowserAccessibility* child =
        manager->GetRoot()->PlatformGetChild(child_index);
    base::scoped_nsobject<BrowserAccessibilityCocoa> child_obj(
        [ToBrowserAccessibilityCocoa(child) retain]);

    EXPECT_NSEQ(@"Unlabeled image", [child_obj roleDescription]);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestAnnotatedImageDescription) {
  std::vector<const char*> expected_descriptions;

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(11);
  tree.nodes[0].id = 1;
  tree.nodes[0].child_ids = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  // If the status is EligibleForAnnotation and there's no existing label,
  // the description should be the discoverability string.
  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kImage;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[1].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_descriptions.push_back(
      "To get missing image descriptions, open the context menu.");

  // If the status is EligibleForAnnotation, the discoverability string
  // should be appended to the existing name.
  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kImage;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[2].SetName("ExistingLabel");
  tree.nodes[2].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kEligibleForAnnotation);
  expected_descriptions.push_back(
      "ExistingLabel. To get missing image descriptions, open the context "
      "menu.");

  // If the status is SilentlyEligibleForAnnotation, the discoverability string
  // should not be appended to the existing name.
  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kImage;
  tree.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[3].SetName("ExistingLabel");
  tree.nodes[3].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kSilentlyEligibleForAnnotation);
  expected_descriptions.push_back("ExistingLabel");

  // If the status is IneligibleForAnnotation, nothing should be appended.
  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kImage;
  tree.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[4].SetName("ExistingLabel");
  tree.nodes[4].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kIneligibleForAnnotation);
  expected_descriptions.push_back("ExistingLabel");

  // If the status is AnnotationPending, pending text should be appended
  // to the name.
  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kImage;
  tree.nodes[5].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[5].SetName("ExistingLabel");
  tree.nodes[5].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationPending);
  expected_descriptions.push_back("ExistingLabel. Getting description…");

  // If the status is AnnotationSucceeded, and there's no annotation,
  // nothing should be appended. (Ideally this shouldn't happen.)
  tree.nodes[6].id = 7;
  tree.nodes[6].role = ax::mojom::Role::kImage;
  tree.nodes[6].SetName("ExistingLabel");
  tree.nodes[6].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_descriptions.push_back("ExistingLabel");

  // If the status is AnnotationSucceeded, the annotation should be appended
  // to the existing label.
  tree.nodes[7].id = 8;
  tree.nodes[7].role = ax::mojom::Role::kImage;
  tree.nodes[7].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[7].SetName("ExistingLabel");
  tree.nodes[7].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationSucceeded);
  expected_descriptions.push_back("ExistingLabel. Annotation");

  // If the status is AnnotationEmpty, failure text should be added to the
  // name.
  tree.nodes[8].id = 9;
  tree.nodes[8].role = ax::mojom::Role::kImage;
  tree.nodes[8].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[8].SetName("ExistingLabel");
  tree.nodes[8].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationEmpty);
  expected_descriptions.push_back("ExistingLabel. No description available.");

  // If the status is AnnotationAdult, appropriate text should be appended
  // to the name.
  tree.nodes[9].id = 10;
  tree.nodes[9].role = ax::mojom::Role::kImage;
  tree.nodes[9].AddStringAttribute(ax::mojom::StringAttribute::kImageAnnotation,
                                   "Annotation");
  tree.nodes[9].SetName("ExistingLabel");
  tree.nodes[9].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationAdult);
  expected_descriptions.push_back("ExistingLabel. Appears to contain adult "
                                  "content. No description available.");

  // If the status is AnnotationProcessFailed, failure text should be added
  // to the name.
  tree.nodes[10].id = 11;
  tree.nodes[10].role = ax::mojom::Role::kImage;
  tree.nodes[10].AddStringAttribute(
      ax::mojom::StringAttribute::kImageAnnotation, "Annotation");
  tree.nodes[10].SetName("ExistingLabel");
  tree.nodes[10].SetImageAnnotationStatus(
      ax::mojom::ImageAnnotationStatus::kAnnotationProcessFailed);
  expected_descriptions.push_back("ExistingLabel. No description available.");

  // We should have one expected description per child of the root.
  ASSERT_EQ(expected_descriptions.size(), tree.nodes[0].child_ids.size());
  int child_count = static_cast<int>(expected_descriptions.size());

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(tree, nullptr));

  for (int child_index = 0; child_index < child_count; child_index++) {
    BrowserAccessibility* child =
        manager->GetRoot()->PlatformGetChild(child_index);
    base::scoped_nsobject<BrowserAccessibilityCocoa> child_obj(
        [ToBrowserAccessibilityCocoa(child) retain]);

    EXPECT_NSEQ(base::SysUTF8ToNSString(expected_descriptions[child_index]),
                [child_obj descriptionForAccessibility]);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestTableGetRowNodesNestedRows) {
  // rootWebArea(#1)
  // ++grid(#2)
  // ++++row(#3)
  // ++++++columnHeader(#4)
  // ++++++columnHeader(#5)
  // ++++genericContainer(#6)
  // ++++++row(#7)
  // ++++++++cell(#8)
  // ++++++++cell(#9)
  // ++++++row(#10)
  // ++++++++cell(#11)
  // ++++++++cell(#12)

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(12);
  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kGrid;
  tree.nodes[1].child_ids = {3, 6};

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kRow;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName, "row1");
  tree.nodes[2].child_ids = {4, 5};

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kColumnHeader;
  tree.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   "header1");

  tree.nodes[4].id = 5;
  tree.nodes[4].role = ax::mojom::Role::kColumnHeader;
  tree.nodes[4].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   "header2");

  tree.nodes[5].id = 6;
  tree.nodes[5].role = ax::mojom::Role::kGenericContainer;
  tree.nodes[5].child_ids = {7, 10};

  tree.nodes[6].id = 7;
  tree.nodes[6].role = ax::mojom::Role::kRow;
  tree.nodes[6].AddStringAttribute(ax::mojom::StringAttribute::kName, "row2");
  tree.nodes[6].child_ids = {8, 9};

  tree.nodes[7].id = 8;
  tree.nodes[7].role = ax::mojom::Role::kCell;
  tree.nodes[7].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   "cell1_row2");

  tree.nodes[8].id = 9;
  tree.nodes[8].role = ax::mojom::Role::kCell;
  tree.nodes[8].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   "cell2_row2");

  tree.nodes[9].id = 10;
  tree.nodes[9].role = ax::mojom::Role::kRow;
  tree.nodes[9].AddStringAttribute(ax::mojom::StringAttribute::kName, "row3");
  tree.nodes[9].child_ids = {11, 12};

  tree.nodes[10].id = 11;
  tree.nodes[10].role = ax::mojom::Role::kCell;
  tree.nodes[10].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                    "cell1_row3");

  tree.nodes[11].id = 12;
  tree.nodes[11].role = ax::mojom::Role::kCell;
  tree.nodes[11].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                    "cell2_row3");

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(tree, nullptr));

  BrowserAccessibility* table = manager->GetRoot()->PlatformGetChild(0);
  base::scoped_nsobject<BrowserAccessibilityCocoa> table_obj(
      [ToBrowserAccessibilityCocoa(table) retain]);
  NSArray* row_nodes = [table_obj rows];

  EXPECT_EQ(3U, [row_nodes count]);
  EXPECT_NSEQ(@"AXRow", [row_nodes[0] role]);
  EXPECT_NSEQ(@"row1", [row_nodes[0] descriptionForAccessibility]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[1] role]);
  EXPECT_NSEQ(@"row2", [row_nodes[1] descriptionForAccessibility]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[2] role]);
  EXPECT_NSEQ(@"row3", [row_nodes[2] descriptionForAccessibility]);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestTableGetRowNodesIndirectChildIds) {
  // rootWebArea(#1)
  // ++column(#2), indirectChildIds={3, 4}
  // ++row(#3)
  // ++row(#4)

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.nodes.resize(4);

  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2, 3, 4};

  tree.nodes[1].id = 2;
  tree.nodes[1].role = ax::mojom::Role::kColumn;
  tree.nodes[1].AddStringAttribute(ax::mojom::StringAttribute::kName,
                                   "column1");
  tree.nodes[1].AddIntListAttribute(
      ax::mojom::IntListAttribute::kIndirectChildIds,
      std::vector<int32_t>{3, 4});

  tree.nodes[2].id = 3;
  tree.nodes[2].role = ax::mojom::Role::kRow;
  tree.nodes[2].AddStringAttribute(ax::mojom::StringAttribute::kName, "row1");

  tree.nodes[3].id = 4;
  tree.nodes[3].role = ax::mojom::Role::kRow;
  tree.nodes[3].AddStringAttribute(ax::mojom::StringAttribute::kName, "row2");

  std::unique_ptr<BrowserAccessibilityManagerMac> manager(
      new BrowserAccessibilityManagerMac(tree, nullptr));

  BrowserAccessibility* column = manager->GetRoot()->PlatformGetChild(0);
  base::scoped_nsobject<BrowserAccessibilityCocoa> col_obj(
      [ToBrowserAccessibilityCocoa(column) retain]);
  EXPECT_NSEQ(@"AXColumn", [col_obj role]);
  EXPECT_NSEQ(@"column1", [col_obj descriptionForAccessibility]);

  NSArray* row_nodes = [col_obj rows];
  EXPECT_NSEQ(@"AXRow", [row_nodes[0] role]);
  EXPECT_NSEQ(@"row1", [row_nodes[0] descriptionForAccessibility]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[1] role]);
  EXPECT_NSEQ(@"row2", [row_nodes[1] descriptionForAccessibility]);
}
}  // namespace content
