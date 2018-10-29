// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/accessibility_browser_test_utils.h"
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
    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role);
      if (result)
        return result;
    }
    return nullptr;
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(BrowserAccessibilityCocoaBrowserTest,
                       AXCellForColumnAndRow) {
  NavigateToURL(shell(), GURL(url::kAboutBlankURL));

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

  NavigateToURL(shell(), url);
  waiter.WaitForNotification();

  BrowserAccessibility* table = FindNode(ax::mojom::Role::kTable);
  ASSERT_NE(nullptr, table);
  base::scoped_nsobject<BrowserAccessibilityCocoa> cocoa_table(
      [ToBrowserAccessibilityCocoa(table) retain]);

  // Test AXCellForColumnAndRow for four coordinates
  for (unsigned col = 0; col < 2; col++) {
    for (unsigned row = 0; row < 2; row++) {
      id parameter = [[[NSMutableArray alloc] initWithCapacity:2] autorelease];
      [parameter addObject:[NSNumber numberWithInt:col]];
      [parameter addObject:[NSNumber numberWithInt:row]];
      base::scoped_nsobject<BrowserAccessibilityCocoa> cell(
          [[cocoa_table accessibilityAttributeValue:@"AXCellForColumnAndRow"
                                       forParameter:parameter] retain]);

      // It should be a cell.
      EXPECT_NSEQ(@"AXCell", [cell role]);

      // The column index and row index of the cell should match what we asked
      // for.
      EXPECT_EQ(col, [[cell accessibilityAttributeValue:@"AXColumnIndexRange"]
                         rangeValue]
                         .location);
      EXPECT_EQ(row, [[cell accessibilityAttributeValue:@"AXRowIndexRange"]
                         rangeValue]
                         .location);
    }
  }
}

}  // namespace content
