// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/browser_accessibility_cocoa.h"

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/context_menu_interceptor.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/base/data_url.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/accessibility/platform/ax_private_webkit_constants_mac.h"
#include "ui/accessibility/platform/ax_utils_mac.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_mac.h"
#include "ui/accessibility/platform/browser_accessibility_manager.h"
#include "ui/accessibility/platform/browser_accessibility_manager_mac.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_mac.h"
#include "ui/accessibility/platform/test_ax_node_id_delegate.h"
#include "url/gurl.h"

namespace {

#include <string>

// Returns HTML that creates a 60-row table.
std::string GetLargeTableHTML() {
  return R"delim(data:text/html,
    <html>
      <head>
        <title>Large Table</title>
        <script type="text/javascript">

          function CreateTable() {
          const numColumns = 5;
          const numRows = 60;

          const table = document.createElement('table');
          for (let i = 0; i < numRows; i++) {
            const row = document.createElement('tr');
            for (let j = 0; j < numColumns; j++) {
              const cell = document.createElement('td');
              cell.textContent = `row ${i+1}, column ${j+1}`;
              row.appendChild(cell);
            }
            table.appendChild(row);
          }

          document.body.appendChild(table);
        }

        </script>
      </head>
      <body onLoad="CreateTable()">

      </body>
    </html>)delim";
}

// Returns the accessibility frame assigned to `element`.
NSRect GetAXFrame(AXUIElementRef element) {
  AXValueRef value;

  CGPoint position = CGPointZero;
  if (AXUIElementCopyAttributeValue(element, kAXPositionAttribute,
                                    (CFTypeRef*)&value) == kAXErrorSuccess) {
    if (AXValueGetType(value) == kAXValueCGPointType) {
      AXValueGetValue(value, (AXValueType)kAXValueCGPointType, &position);
      CFRelease(value);
    }
  }

  CGSize size = CGSizeZero;
  if (AXUIElementCopyAttributeValue(element, kAXSizeAttribute,
                                    (CFTypeRef*)&value) == kAXErrorSuccess) {
    if (AXValueGetType(value) == kAXValueCGSizeType) {
      AXValueGetValue(value, (AXValueType)kAXValueCGSizeType, &size);
      CFRelease(value);
    }
  }

  return NSMakeRect(position.x, position.y, size.width, size.height);
}

// Returns the string associated with `element`.
NSString* GetAXUIElementStringValue(AXUIElementRef element) {
  CFTypeRef value;
  NSString* stringValue = nil;

  if (AXUIElementCopyAttributeValue(element, kAXValueAttribute, &value) ==
      kAXErrorSuccess) {
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
      stringValue = [NSString stringWithString:(__bridge NSString*)value];
    }
    CFRelease(value);
  }

  return stringValue;
}

}  // namespace

// A table cell as seen by an assitive technology when navigating a web page.
@interface ATTestTableCell : NSObject
@property(nonatomic) NSRect frame;
@property(nonatomic, strong) NSString* value;
- (instancetype)initFromElement:(AXUIElementRef)cellElement;
@end

@implementation ATTestTableCell
@synthesize frame = _frame;
@synthesize value = _value;

- (instancetype)initFromElement:(AXUIElementRef)cellElement {
  self = [super init];
  if (self) {
    _frame = GetAXFrame(cellElement);
    _value = GetAXUIElementStringValue(cellElement);
  }
  return self;
}
@end

// A table row as seen by an assitive technology when navigating a web page.
@interface ATTestTableRow : NSObject
@property(nonatomic) NSRect frame;
@property(nonatomic, strong) NSMutableArray<ATTestTableCell*>* cells;
- (instancetype)initFromElement:(AXUIElementRef)rowElement;
@end

@implementation ATTestTableRow
@synthesize frame = _frame;
@synthesize cells = _cells;

- (instancetype)initFromElement:(AXUIElementRef)rowElement {
  self = [super init];
  if (self) {
    _frame = GetAXFrame(rowElement);
    _cells = [NSMutableArray array];
    [self populateCellsFromRowElement:rowElement];
  }
  return self;
}

- (void)populateCellsFromRowElement:(AXUIElementRef)rowElement {
  CFArrayRef cellsArray = NULL;

  if (AXUIElementCopyAttributeValue(rowElement, kAXChildrenAttribute,
                                    (CFTypeRef*)&cellsArray) ==
          kAXErrorSuccess &&
      cellsArray) {
    CFIndex cellCount = CFArrayGetCount(cellsArray);

    for (CFIndex i = 0; i < cellCount; i++) {
      AXUIElementRef cellElement =
          (AXUIElementRef)CFArrayGetValueAtIndex(cellsArray, i);
      ATTestTableCell* cell =
          [[ATTestTableCell alloc] initFromElement:cellElement];
      [_cells addObject:cell];
    }

    CFRelease(cellsArray);
  }
}

@end

// A table as seen by an assitive technology when navigating a web page.
@interface AXTestTable : NSObject
@property(nonatomic, strong) NSMutableArray<ATTestTableRow*>* rows;
- (instancetype)initWithAXUIElement:(AXUIElementRef)tableElement;
@end

@implementation AXTestTable
@synthesize rows = _rows;

- (instancetype)initWithAXUIElement:(AXUIElementRef)tableElement {
  self = [super init];
  if (self) {
    _rows = [NSMutableArray array];
    [self populateTableFromElement:tableElement];
  }
  return self;
}

- (void)populateTableFromElement:(AXUIElementRef)tableElement {
  CFArrayRef rowsArray = NULL;

  if (AXUIElementCopyAttributeValue(tableElement, kAXRowsAttribute,
                                    (CFTypeRef*)&rowsArray) ==
          kAXErrorSuccess &&
      rowsArray) {
    CFIndex rowCount = CFArrayGetCount(rowsArray);

    for (CFIndex i = 0; i < rowCount; i++) {
      AXUIElementRef rowElement =
          (AXUIElementRef)CFArrayGetValueAtIndex(rowsArray, i);
      ATTestTableRow* row = [[ATTestTableRow alloc] initFromElement:rowElement];
      [_rows addObject:row];
    }

    CFRelease(rowsArray);
  }
}

@end

namespace content {

class BrowserAccessibilityCocoaBrowserTest : public ContentBrowserTest {
 public:
  BrowserAccessibilityCocoaBrowserTest() {}
  ~BrowserAccessibilityCocoaBrowserTest() override {}

 protected:
  ui::BrowserAccessibility* FindNode(ax::mojom::Role role) {
    ui::BrowserAccessibility* root =
        GetManager()->GetBrowserAccessibilityRoot();
    CHECK(root);
    return FindNodeInSubtree(*root, role);
  }

  ui::BrowserAccessibilityManager* GetManager() {
    WebContentsImpl* web_contents =
        static_cast<WebContentsImpl*>(shell()->web_contents());
    return web_contents->GetRootBrowserAccessibilityManager();
  }

  // Trigger a context menu for the provided element without showing it. Returns
  // the coordinates where the context menu was invoked (calculated based on
  // the provided element). These coordinates are relative to the RenderView
  // origin.
  gfx::Point TriggerContextMenuAndGetMenuLocation(
      NSAccessibilityElement* element,
      ContextMenuInterceptor* interceptor) {
    // accessibilityPerformAction is deprecated, but it's still used internally
    // by AppKit.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [element accessibilityPerformAction:NSAccessibilityShowMenuAction];
    interceptor->Wait();

    blink::UntrustworthyContextMenuParams context_menu_params =
        interceptor->get_params();
    return gfx::Point(context_menu_params.x, context_menu_params.y);
#pragma clang diagnostic pop
  }

  void FocusAccessibilityElementAndWaitForFocusChange(
      NSAccessibilityElement* element) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    [element setAccessibilityFocused:YES];
#pragma clang diagnostic pop
    WaitForAccessibilityFocusChange();
  }

  NSDictionary* GetUserInfoForSelectedTextChangedNotification() {
    auto* manager =
        static_cast<ui::BrowserAccessibilityManagerMac*>(GetManager());
    return manager->GetUserInfoForSelectedTextChangedNotification();
  }

  ui::AXTextEdit GetTextEditForNodeId(int32_t id) {
    auto* manager =
        static_cast<ui::BrowserAccessibilityManagerMac*>(GetManager());
    return manager->text_edits_[id];
  }

  ui::TestAXNodeIdDelegate node_id_delegate_;

 private:
  ui::BrowserAccessibility* FindNodeInSubtree(ui::BrowserAccessibility& node,
                                              ax::mojom::Role role) {
    if (node.GetRole() == role)
      return &node;
    for (ui::BrowserAccessibility::PlatformChildIterator it =
             node.PlatformChildrenBegin();
         it != node.PlatformChildrenEnd(); ++it) {
      ui::BrowserAccessibility* result = FindNodeInSubtree(*it, role);
      if (result)
        return result;
    }
    return nullptr;
  }
};

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       ATCanScrollLargeTable) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  // Load a large table.
  GURL url(GetLargeTableHTML());

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  pid_t pid = getpid();
  base::apple::ScopedCFTypeRef<AXUIElementRef> axApplication(
      AXUIElementCreateApplication(pid));
  base::apple::ScopedCFTypeRef<AXUIElementRef> table =
      ui::FindAXUIElement(axApplication.get(), "AXTable");

  ASSERT_TRUE(table);

  AXTestTable* axTable = [[AXTestTable alloc] initWithAXUIElement:table.get()];
  NSArray* rows = [axTable rows];

  EXPECT_TRUE(rows.count == 60);

  // A very tall table will have some number of cells hidden (clipped by its
  // container or the window). For VoiceOver and other ATs to work properly,
  // all rows and cells must have non-zero sizes, even if clipped.
  int rowNumber = 0;
  for (ATTestTableRow* row in rows) {
    EXPECT_FALSE(NSIsEmptyRect(row.frame))
        << "zero extent for row " << rowNumber;
    EXPECT_TRUE(row.cells.count);
    EXPECT_FALSE(NSIsEmptyRect(row.cells[0].frame))
        << "zero extent for row " << rowNumber << ", cell "
        << row.cells[0].value;

    rowNumber++;
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXTextMarkerForTextEdit) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(R"HTML(data:text/html,
             <input />)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* text_field = FindNode(ax::mojom::Role::kTextField);
  ASSERT_NE(nullptr, text_field);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.querySelector('input').focus()"));

  SimulateKeyPress(shell()->web_contents(), ui::DomKey::FromCharacter('B'),
                   ui::DomCode::US_B, ui::VKEY_B, false, false, false, false);

  BrowserAccessibilityCocoa* cocoa_text_field =
      text_field->GetNativeViewAccessible();
  AccessibilityNotificationWaiter value_waiter(shell()->web_contents(),
                                               ui::kAXModeComplete,
                                               ax::mojom::Event::kValueChanged);
  ASSERT_TRUE(value_waiter.WaitForNotification());
  ui::AXTextEdit text_edit = [cocoa_text_field computeTextEdit];
  EXPECT_NE(text_edit.edit_text_marker, nil);

  auto ax_position = ui::AXTextMarkerToAXPosition(text_edit.edit_text_marker);
  std::string expected_string = "TextPosition anchor_id=";
  expected_string += base::NumberToString(ax_position->anchor_id());
  expected_string += " text_offset=1 affinity=downstream annotated_text=B<>";
  EXPECT_EQ(ax_position->ToString(), expected_string);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXTextMarkerForTextEditContentEditable) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);
  GURL url(R"HTML(data:text/html,
                  <div id="editable" contenteditable="true" dir="auto">
                    <p>One</p>
                    <p>Two</p>
                    <p><br></p>
                    <p>Three</p>
                    <p>Four</p>
                    <p>Five</p>
                  </div>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* content_editable =
      GetManager()->GetBrowserAccessibilityRoot()->PlatformGetChild(0);
  ASSERT_NE(nullptr, content_editable);
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "document.querySelector('#editable').focus()"));
  {
    SimulateKeyPress(shell()->web_contents(), ui::DomKey::FromCharacter('B'),
                     ui::DomCode::US_B, ui::VKEY_B, false, false, false, false);

    AccessibilityNotificationWaiter value_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ax::mojom::Event::kValueChanged);
    ASSERT_TRUE(value_waiter.WaitForNotification());

    ui::AXTextEdit text_edit = GetTextEditForNodeId(content_editable->GetId());
    EXPECT_NE(text_edit.edit_text_marker, nil);
    EXPECT_EQ(u"", text_edit.deleted_text);
    EXPECT_EQ(u"B", text_edit.inserted_text);

    auto ax_position = ui::AXTextMarkerToAXPosition(text_edit.edit_text_marker);
    std::string expected_string = "TextPosition anchor_id=";
    expected_string += base::NumberToString(ax_position->anchor_id());
    expected_string += " text_offset=0 affinity=downstream annotated_text=";
    expected_string += "<B>OneTwo\nThreeFourFive";

    EXPECT_EQ(ax_position->ToString(), expected_string);
  }

  // Move Cursor down to the fourth paragraph.
  for (int i = 0; i < 4; ++i) {
    SimulateKeyPressWithoutChar(shell()->web_contents(), ui::DomKey::ARROW_DOWN,
                                ui::DomCode::ARROW_DOWN, ui::VKEY_DOWN,
                                /*control=*/false, /*shift=*/false,
                                /*alt=*/false,
                                /*command=*/false);
  }
  {
    SimulateKeyPress(shell()->web_contents(), ui::DomKey::FromCharacter('B'),
                     ui::DomCode::US_B, ui::VKEY_B, false, false, false, false);

    AccessibilityNotificationWaiter value_waiter(
        shell()->web_contents(), ui::kAXModeComplete,
        ax::mojom::Event::kValueChanged);
    ASSERT_TRUE(value_waiter.WaitForNotification());

    ui::AXTextEdit text_edit = GetTextEditForNodeId(content_editable->GetId());
    EXPECT_NE(text_edit.edit_text_marker, nil);
    EXPECT_EQ(u"", text_edit.deleted_text);
    EXPECT_EQ(u"B", text_edit.inserted_text);
    auto ax_position = ui::AXTextMarkerToAXPosition(text_edit.edit_text_marker);
    std::string expected_string = "TextPosition anchor_id=";
    expected_string += base::NumberToString(ax_position->anchor_id());
    expected_string += " text_offset=14 affinity=downstream annotated_text=";
    expected_string += "BOneTwo\nThreeF<B>ourFive";

    EXPECT_EQ(ax_position->ToString(), expected_string);
  }
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
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* table = FindNode(ax::mojom::Role::kTable);
  ASSERT_NE(nullptr, table);
  BrowserAccessibilityCocoa* cocoa_table = table->GetNativeViewAccessible();

  // Test AXCellForColumnAndRow for four coordinates
  for (unsigned col = 0; col < 2; col++) {
    for (unsigned row = 0; row < 2; row++) {
      BrowserAccessibilityCocoa* cell =
          [cocoa_table accessibilityCellForColumn:col row:row];

      // It should be a cell.
      EXPECT_NSEQ(@"AXCell", cell.accessibilityRole);

      // The column index and row index of the cell should match what we asked
      // for.
      EXPECT_NSEQ(NSMakeRange(col, 1), cell.accessibilityColumnIndexRange);
      EXPECT_NSEQ(NSMakeRange(row, 1), cell.accessibilityRowIndexRange);
    }
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestCoordinatesAreInScreenSpace) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));

  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(R"HTML(data:text/html, <p>Hello, world!</p>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* text = FindNode(ax::mojom::Role::kStaticText);
  ASSERT_NE(nullptr, text);

  BrowserAccessibilityCocoa* cocoa_text = text->GetNativeViewAccessible();
  ASSERT_NE(nil, cocoa_text);

  NSPoint position = cocoa_text.position.pointValue;

  NSSize size = cocoa_text.accessibilityFrame.size;
  NSRect frame = NSMakeRect(position.x, position.y, size.width, size.height);

  NSPoint p0_before = position;
  NSRect r0_before = [cocoa_text frameForRange:NSMakeRange(0, 5)];
  EXPECT_TRUE(CGRectContainsRect(frame, r0_before));

  // On macOS geometry accessibility attributes are expected to use the
  // screen coordinate system with the origin at the bottom left corner.
  // We need some creativity with testing this because it is challenging
  // to setup a text element with a precise screen position.
  //
  // Content shell's window is pinned to have an origin at (0, 0), so
  // when its height is changed the content's screen y-coordinate is
  // changed by the same amount (see below).
  //
  //      Y^      original
  //       |
  //       +--------------------------------------------+
  //       |                                            |
  //       |                                            |
  //       |                                            |
  //       |                                            |
  //     h +---------------------------+                |
  //       |      Content Shell        |                |
  //       |---------------------------|                |
  //     y |Hello, world               |                |
  //       |                           |                |
  //       |                           |          Screen|
  //       +---------------------------+----------------+-->
  //      0                                               X
  //
  //      Y^       content shell enlarged
  //       |
  //       +--------------------------------------------+
  //       |                                            |
  //       |                                            |
  //  h+dh +---------------------------+                |
  //       |      Content Shell        |                |
  //       |---------------------------|                |
  //  y+dh |Hello, world               |                |
  //       |                           |                |
  //       |                           |                |
  //       |                           |                |
  //       |                           |          Screen|
  //       +---------------------------+----------------+-->
  //      0                                               X
  //
  // This observation allows us to validate the returned
  // attribute values and catch the most glaring mistakes
  // in coordinate space handling.

  const int dh = 100;
  gfx::Size content_size = Shell::GetShellDefaultSize();
  content_size.Enlarge(0, dh);
  shell()->ResizeWebContentForTests(content_size);

  NSPoint p0_after = cocoa_text.position.pointValue;
  NSRect r0_after = [cocoa_text frameForRange:NSMakeRange(0, 5)];

  ASSERT_EQ(p0_before.y + dh, p0_after.y);
  ASSERT_EQ(r0_before.origin.y + dh, r0_after.origin.y);
  ASSERT_EQ(r0_before.size.height, r0_after.size.height);
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

  std::unique_ptr<ui::BrowserAccessibilityManagerMac> manager(
      new ui::BrowserAccessibilityManagerMac(tree, node_id_delegate_, nullptr));

  for (int child_index = 0; child_index < child_count; child_index++) {
    ui::BrowserAccessibility* child =
        manager->GetBrowserAccessibilityRoot()->PlatformGetChild(child_index);
    BrowserAccessibilityCocoa* child_obj = child->GetNativeViewAccessible();

    EXPECT_NSEQ(base::SysUTF8ToNSString(expected_descriptions[child_index]),
                child_obj.accessibilityLabel);
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

  std::unique_ptr<ui::BrowserAccessibilityManagerMac> manager(
      new ui::BrowserAccessibilityManagerMac(tree, node_id_delegate_, nullptr));

  ui::BrowserAccessibility* table =
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0);
  BrowserAccessibilityCocoa* table_obj = table->GetNativeViewAccessible();
  NSArray* row_nodes = table_obj.accessibilityRows;

  EXPECT_EQ(3U, row_nodes.count);
  EXPECT_NSEQ(@"AXRow", [row_nodes[0] role]);
  EXPECT_NSEQ(@"row1", [row_nodes[0] accessibilityLabel]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[1] role]);
  EXPECT_NSEQ(@"row2", [row_nodes[1] accessibilityLabel]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[2] role]);
  EXPECT_NSEQ(@"row3", [row_nodes[2] accessibilityLabel]);
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

  std::unique_ptr<ui::BrowserAccessibilityManagerMac> manager(
      new ui::BrowserAccessibilityManagerMac(tree, node_id_delegate_, nullptr));

  ui::BrowserAccessibility* column =
      manager->GetBrowserAccessibilityRoot()->PlatformGetChild(0);
  BrowserAccessibilityCocoa* col_obj = column->GetNativeViewAccessible();
  EXPECT_NSEQ(@"AXColumn", col_obj.role);
  EXPECT_NSEQ(@"column1", col_obj.accessibilityLabel);

  NSArray* row_nodes = col_obj.accessibilityRows;
  EXPECT_NSEQ(@"AXRow", [row_nodes[0] role]);
  EXPECT_NSEQ(@"row1", [row_nodes[0] accessibilityLabel]);

  EXPECT_NSEQ(@"AXRow", [row_nodes[1] role]);
  EXPECT_NSEQ(@"row2", [row_nodes[1] accessibilityLabel]);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestAXHeadersShouldOnlyIncludeColHeaders) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL(url::kAboutBlankURL)));
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(
      R"HTML(data:text/html,
      <table aria-label="Population per country">
        <thead style=display:block>
          <tr>
            <th aria-label="Country">Country</th>
            <th aria-label="Population">Population</th>
          </tr>
        </thead>
        <tbody>
          <tr>
            <td role=rowheader>Canada</td>
            <td>37</td>
          </tr>
          <tr>
            <td role=rowheader>USA</td>
            <td>331</td>
          </tr>
        </tbody>
      </table>
  )HTML");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* table = FindNode(ax::mojom::Role::kTable);
  BrowserAccessibilityCocoa* table_obj = table->GetNativeViewAccessible();

  EXPECT_NSEQ(@"AXTable", table_obj.role);
  EXPECT_NSEQ(@"Population per country", table_obj.accessibilityLabel);
  BrowserAccessibilityCocoa* table_header = table_obj.header;

  NSArray* children = table_header.children;
  EXPECT_EQ(2U, children.count);

  EXPECT_NSEQ(@"AXCell", [children[0] role]);
  EXPECT_NSEQ(@"Country", [children[0] accessibilityLabel]);

  EXPECT_NSEQ(@"AXCell", [children[1] role]);
  EXPECT_NSEQ(@"Population", [children[1] accessibilityLabel]);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestTreeContextMenuEvent) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(R"HTML(data:text/html,
             <div alt="tree" role="tree">
               <div tabindex="1" role="treeitem">1</div>
               <div tabindex="2" role="treeitem">2</div>
             </div>)HTML");

  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  ui::BrowserAccessibility* tree = FindNode(ax::mojom::Role::kTree);
  BrowserAccessibilityCocoa* cocoa_tree = tree->GetNativeViewAccessible();

  NSArray* tree_children = cocoa_tree.children;
  ASSERT_NSEQ(@"AXRow", [tree_children[0] role]);
  ASSERT_NSEQ(@"AXRow", [tree_children[1] role]);

  auto menu_interceptor = std::make_unique<ContextMenuInterceptor>(
      shell()->web_contents()->GetPrimaryMainFrame(),
      ContextMenuInterceptor::ShowBehavior::kPreventShow);

  gfx::Point tree_point =
      TriggerContextMenuAndGetMenuLocation(cocoa_tree, menu_interceptor.get());

  menu_interceptor->Reset();
  gfx::Point item_2_point = TriggerContextMenuAndGetMenuLocation(
      tree_children[1], menu_interceptor.get());
  EXPECT_NE(tree_point, item_2_point);

  // Now focus the second child and trigger a context menu on the tree.
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "document.body.children[0].children[1].focus();"));
  WaitForAccessibilityFocusChange();

  // Triggering a context menu on the tree should now trigger the menu
  // on the focused child.
  menu_interceptor->Reset();
  gfx::Point new_point =
      TriggerContextMenuAndGetMenuLocation(cocoa_tree, menu_interceptor.get());
  EXPECT_EQ(new_point, item_2_point);
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestEventRetargetingFocus) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(R"HTML(data:text/html,
             <div role="tree">
               <div tabindex="1" role="treeitem">1</div>
               <div tabindex="2" role="treeitem">2</div>
             </div>
             <div role="treegrid">
               <div tabindex="1" role="treeitem">1</div>
               <div tabindex="2" role="treeitem">2</div>
             </div>
             <div role="tablist">
               <div tabindex="1" role="tab">1</div>
               <div tabindex="2" role="tab">2</div>
             </div>
             <div role="table">
               <div tabindex="1" role="row">1</div>
               <div tabindex="2" role="row">2</div>
             </div>
             <div role="banner">
               <div tabindex="1" role="link">1</div>
               <div tabindex="2" role="link">2</div>
             </div>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  std::pair<ax::mojom::Role, bool> tests[] = {
      std::make_pair(ax::mojom::Role::kTree, true),
      std::make_pair(ax::mojom::Role::kTreeGrid, true),
      std::make_pair(ax::mojom::Role::kTabList, true),
      std::make_pair(ax::mojom::Role::kTable, false),
      std::make_pair(ax::mojom::Role::kBanner, false),
  };

  for (auto& test : tests) {
    BrowserAccessibilityCocoa* parent =
        FindNode(test.first)->GetNativeViewAccessible();
    BrowserAccessibilityCocoa* child = parent.children[1];

    EXPECT_NE(nullptr, parent);
    EXPECT_EQ([child owner], [child actionTarget]);
    EXPECT_EQ([parent owner], [parent actionTarget]);

    FocusAccessibilityElementAndWaitForFocusChange(child);
    ASSERT_EQ(test.second, [parent actionTarget] == [child owner]);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestEventRetargetingActiveDescendant) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(R"HTML(data:text/html,
             <div role="tree" aria-activedescendant="tree-child">
               <div tabindex="1" role="treeitem">1</div>
               <div id="tree-child" tabindex="2" role="treeitem">2</div>
             </div>
             <div role="treegrid" aria-activedescendant="treegrid-child">
               <div tabindex="1" role="treeitem">1</div>
               <div id="treegrid-child" tabindex="2" role="treeitem">2</div>
             </div>
             <div role="tablist" aria-activedescendant="tablist-child">
               <div tabindex="1" role="tab">1</div>
               <div id="tablist-child" tabindex="2" role="tab">2</div>
             </div>
             <div role="table" aria-activedescendant="table-child">
               <div tabindex="1" role="row">1</div>
               <div id="table-child" tabindex="2" role="row">2</div>
             </div>
             <div role="banner" aria-activedescendant="banner-child">
               <div tabindex="1" role="link">1</div>
               <div id="banner-child" tabindex="2" role="link">2</div>
             </div>)HTML");

  EXPECT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  std::pair<ax::mojom::Role, bool> tests[] = {
      std::make_pair(ax::mojom::Role::kTree, true),
      std::make_pair(ax::mojom::Role::kTreeGrid, true),
      std::make_pair(ax::mojom::Role::kTabList, true),
      std::make_pair(ax::mojom::Role::kTable, false),
      std::make_pair(ax::mojom::Role::kBanner, false),
  };

  for (auto& test : tests) {
    BrowserAccessibilityCocoa* parent =
        FindNode(test.first)->GetNativeViewAccessible();
    BrowserAccessibilityCocoa* first_child = parent.children[0];
    BrowserAccessibilityCocoa* second_child = parent.children[1];

    EXPECT_NE(nullptr, parent);
    EXPECT_EQ([second_child owner], [second_child actionTarget]);
    EXPECT_EQ(test.second, [second_child owner] == [parent actionTarget]);

    // aria-activedescendant should take priority over focus for determining if
    // an object is the action target.
    FocusAccessibilityElementAndWaitForFocusChange(first_child);
    EXPECT_EQ(test.second, [second_child owner] == [parent actionTarget]);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       TestNSAccessibilityTextChangeElement) {
  AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                         ui::kAXModeComplete,
                                         ax::mojom::Event::kLoadComplete);

  GURL url(R"HTML(data:text/html,
                  <div id="editable" contenteditable="true" dir="auto">
                    <p>One</p>
                    <p>Two</p>
                    <p><br></p>
                    <p>Three</p>
                    <p>Four</p>
                  </div>)HTML");

  ASSERT_TRUE(NavigateToURL(shell(), url));
  ASSERT_TRUE(waiter.WaitForNotification());

  BrowserAccessibilityCocoa* content_editable =
      GetManager()
          ->GetBrowserAccessibilityRoot()
          ->PlatformGetChild(0)
          ->GetNativeViewAccessible();
  EXPECT_EQ(content_editable.children.count, 5ul);

  WebContents* web_contents = shell()->web_contents();
  auto run_script_and_wait_for_selection_change =
      [web_contents](const char* script) {
        AccessibilityNotificationWaiter waiter(
            web_contents, ui::kAXModeComplete,
            ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
        ASSERT_TRUE(ExecJs(web_contents, script));
        ASSERT_TRUE(waiter.WaitForNotification());
      };

  FocusAccessibilityElementAndWaitForFocusChange(content_editable);

  run_script_and_wait_for_selection_change(R"script(
      let editable = document.getElementById('editable');
      const selection = window.getSelection();
      selection.collapse(editable.children[0].childNodes[0], 1);)script");

  // The focused node in the user info should be the keyboard focusable
  // ancestor.
  NSDictionary* info = GetUserInfoForSelectedTextChangedNotification();
  EXPECT_EQ(id{content_editable},
            [info objectForKey:ui::NSAccessibilityTextChangeElement]);

  AccessibilityNotificationWaiter waiter2(
      web_contents, ui::kAXModeComplete,
      ui::AXEventGenerator::Event::TEXT_SELECTION_CHANGED);
  run_script_and_wait_for_selection_change(R"script(
      let editable = document.getElementById('editable');
      const selection = window.getSelection();
      selection.collapse(editable.children[2].childNodes[0], 0);)script");

  // Even when the cursor is in the empty paragraph text node, the focused
  // object should be the keyboard focusable ancestor.
  info = GetUserInfoForSelectedTextChangedNotification();
  EXPECT_EQ(id{content_editable},
            [info objectForKey:ui::NSAccessibilityTextChangeElement]);
}

}  // namespace content
