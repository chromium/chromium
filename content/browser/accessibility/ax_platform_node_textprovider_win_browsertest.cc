// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/ax_platform_node_textprovider_win.h"

#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_safearray.h"
#include "base/win/scoped_variant.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/platform/ax_platform_node_textrangeprovider_win.h"
#include "ui/accessibility/platform/browser_accessibility.h"
#include "ui/accessibility/platform/browser_accessibility_com_win.h"

using Microsoft::WRL::ComPtr;

namespace content {

#define ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(safearray, expected_size)    \
  {                                                                            \
    EXPECT_EQ(sizeof(ITextRangeProvider*), ::SafeArrayGetElemsize(safearray)); \
    ASSERT_EQ(1u, SafeArrayGetDim(safearray));                                 \
    LONG array_lower_bound;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                                  \
        SafeArrayGetLBound(safearray, 1, &array_lower_bound));                 \
    LONG array_upper_bound;                                                    \
    ASSERT_HRESULT_SUCCEEDED(                                                  \
        SafeArrayGetUBound(safearray, 1, &array_upper_bound));                 \
    size_t count = array_upper_bound - array_lower_bound + 1;                  \
    ASSERT_EQ(expected_size, count);                                           \
  }

#define EXPECT_UIA_TEXTRANGE_EQ(provider, expected_content) \
  {                                                         \
    base::win::ScopedBstr provider_content;                 \
    ASSERT_HRESULT_SUCCEEDED(                               \
        provider->GetText(-1, provider_content.Receive())); \
    EXPECT_STREQ(expected_content, provider_content.Get()); \
  }

class AXPlatformNodeTextProviderWinBrowserTest : public ContentBrowserTest {
 protected:
  void LoadInitialAccessibilityTreeFromUrl(
      const GURL& url,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    AccessibilityNotificationWaiter waiter(shell()->web_contents(),
                                           accessibility_mode,
                                           ax::mojom::Event::kLoadComplete);
    EXPECT_TRUE(NavigateToURL(shell(), url));
    ASSERT_TRUE(waiter.WaitForNotification());
  }

  void LoadInitialAccessibilityTreeFromHtmlFilePath(
      const std::string& html_file_path,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    if (!embedded_test_server()->Started()) {
      ASSERT_TRUE(embedded_test_server()->Start());
    }
    ASSERT_TRUE(embedded_test_server()->Started());
    LoadInitialAccessibilityTreeFromUrl(
        embedded_test_server()->GetURL(html_file_path), accessibility_mode);
  }

  void LoadInitialAccessibilityTreeFromHtml(
      const std::string& html,
      ui::AXMode accessibility_mode = ui::kAXModeComplete) {
    LoadInitialAccessibilityTreeFromUrl(
        GURL("data:text/html," + base::EscapeQueryParamValue(html, false)),
        accessibility_mode);
  }

  ui::BrowserAccessibilityManager* GetManagerAndAssertNonNull() {
    auto GetManagerAndAssertNonNull =
        [this](ui::BrowserAccessibilityManager** result) {
          WebContentsImpl* web_contents_impl =
              static_cast<WebContentsImpl*>(shell()->web_contents());
          ASSERT_NE(nullptr, web_contents_impl);
          ui::BrowserAccessibilityManager* browser_accessibility_manager =
              web_contents_impl->GetRootBrowserAccessibilityManager();
          ASSERT_NE(nullptr, browser_accessibility_manager);
          *result = browser_accessibility_manager;
        };

    ui::BrowserAccessibilityManager* browser_accessibility_manager;
    GetManagerAndAssertNonNull(&browser_accessibility_manager);
    return browser_accessibility_manager;
  }

  ui::BrowserAccessibility* GetRootAndAssertNonNull() {
    auto GetRootAndAssertNonNull = [this](ui::BrowserAccessibility** result) {
      ui::BrowserAccessibility* root_browser_accessibility =
          GetManagerAndAssertNonNull()->GetBrowserAccessibilityRoot();
      ASSERT_NE(nullptr, result);
      *result = root_browser_accessibility;
    };

    ui::BrowserAccessibility* root_browser_accessibility;
    GetRootAndAssertNonNull(&root_browser_accessibility);
    return root_browser_accessibility;
  }

  ui::BrowserAccessibility* FindNode(ax::mojom::Role role,
                                     const std::string& name_or_value) {
    return FindNodeInSubtree(*GetRootAndAssertNonNull(), role, name_or_value);
  }

  void GetTextProviderFromTextNode(
      ComPtr<ITextProvider>& text_provider,
      ui::BrowserAccessibility* target_browser_accessibility) {
    auto* provider_simple =
        ToBrowserAccessibilityWin(target_browser_accessibility)->GetCOM();
    ASSERT_NE(nullptr, provider_simple);

    EXPECT_HRESULT_SUCCEEDED(
        provider_simple->GetPatternProvider(UIA_TextPatternId, &text_provider));
    ASSERT_NE(nullptr, text_provider.Get());
  }

 private:
  ui::BrowserAccessibility* FindNodeInSubtree(
      ui::BrowserAccessibility& node,
      ax::mojom::Role role,
      const std::string& name_or_value) {
    const std::string& name =
        node.GetStringAttribute(ax::mojom::StringAttribute::kName);
    // Note that in the case of a text field,
    // "BrowserAccessibility::GetValueForControl" has the added functionality
    // of computing the value of an ARIA text box from its inner text.
    //
    // <div contenteditable="true" role="textbox">Hello world.</div>
    // Will expose no HTML value attribute, but some screen readers, such as
    // Jaws, VoiceOver and Talkback, require one to be computed.
    const std::string value = base::UTF16ToUTF8(node.GetValueForControl());
    if (node.GetRole() == role &&
        (name == name_or_value || value == name_or_value)) {
      return &node;
    }

    for (unsigned int i = 0; i < node.PlatformChildCount(); ++i) {
      ui::BrowserAccessibility* result =
          FindNodeInSubtree(*node.PlatformGetChild(i), role, name_or_value);
      if (result) {
        return result;
      }
    }

    return nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_{::features::kUiaProvider};
};

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       GetVisibleBounds) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div style='overflow: hidden; width: 10em; height: 2.1em;'>
            <span style='white-space: pre-line;'>AAA BBB
              CCCCCC
              DDDDDD</span>
          </div>
        </body>
      </html>
  )HTML"));

  auto* node =
      FindNode(ax::mojom::Role::kStaticText, "AAA BBB\nCCCCCC\nDDDDDD");
  ASSERT_NE(nullptr, node);
  EXPECT_TRUE(node->IsLeaf());
  EXPECT_EQ(0u, node->PlatformChildCount());

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, node);

  base::win::ScopedSafearray text_provider_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(text_provider_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(text_provider_ranges.Get(), 2U);

  ITextRangeProvider** array_data;
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      text_provider_ranges.Get(), reinterpret_cast<void**>(&array_data)));

  EXPECT_UIA_TEXTRANGE_EQ(array_data[0], L"AAA BBB");
  EXPECT_UIA_TEXTRANGE_EQ(array_data[1], L"CCCCCC");

  ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(text_provider_ranges.Get()));
  text_provider_ranges.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       GetVisibleRangesPositionsOnLeafNodes) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div contenteditable="true" role="textbox" aria-label="text">
            <div><span>one two</span></div>
            <div><span>three four</span></div>
            <div><span>five six</span></div>
          </div>
        </body>
      </html>
  )HTML"));

  auto* node = FindNode(ax::mojom::Role::kTextField, "text");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, node);

  base::win::ScopedSafearray text_provider_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(text_provider_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(text_provider_ranges.Get(), 3U);

  ITextRangeProvider** array_data;
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      text_provider_ranges.Get(), reinterpret_cast<void**>(&array_data)));

  EXPECT_UIA_TEXTRANGE_EQ(array_data[0], L"one two");
  EXPECT_UIA_TEXTRANGE_EQ(array_data[1], L"three four");
  EXPECT_UIA_TEXTRANGE_EQ(array_data[2], L"five six");

  ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(text_provider_ranges.Get()));
  text_provider_ranges.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       FindTextOnRangesReturnedByGetVisibleRanges) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div contenteditable="true" role="textbox" aria-label="text">
            <div><span>one two</span></div>
            <div><span>three four</span></div>
            <div><span>five six</span></div>
          </div>
        </body>
      </html>
  )HTML"));

  auto* node = FindNode(ax::mojom::Role::kTextField, "text");
  ASSERT_NE(nullptr, node);

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, node);

  base::win::ScopedSafearray text_provider_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(text_provider_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(text_provider_ranges.Get(), 3U);

  ITextRangeProvider** array_data;
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      text_provider_ranges.Get(), reinterpret_cast<void**>(&array_data)));

  EXPECT_UIA_TEXTRANGE_EQ(array_data[0], L"one two");
  EXPECT_UIA_TEXTRANGE_EQ(array_data[1], L"three four");
  EXPECT_UIA_TEXTRANGE_EQ(array_data[2], L"five six");

  {
    base::win::ScopedBstr find_string(L"two");
    Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider_found;
    EXPECT_HRESULT_SUCCEEDED(array_data[0]->FindText(
        find_string.Get(), false, false, &text_range_provider_found));
    ASSERT_TRUE(text_range_provider_found.Get());
  }
  {
    base::win::ScopedBstr find_string(L"three");
    Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider_found;
    EXPECT_HRESULT_SUCCEEDED(array_data[1]->FindText(
        find_string.Get(), false, false, &text_range_provider_found));
    ASSERT_TRUE(text_range_provider_found.Get());
  }
  {
    base::win::ScopedBstr find_string(L"five six");
    Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider_found;
    EXPECT_HRESULT_SUCCEEDED(array_data[2]->FindText(
        find_string.Get(), false, false, &text_range_provider_found));
    ASSERT_TRUE(text_range_provider_found.Get());
  }

  ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(text_provider_ranges.Get()));
  text_provider_ranges.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       GetVisibleRangesInContentEditable) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div contenteditable="true">
            <p>hello</p>
          </div>
        </body>
      </html>
  )HTML"));

  auto* gc_node = FindNode(ax::mojom::Role::kGenericContainer, "hello");

  ASSERT_NE(nullptr, gc_node);
  EXPECT_EQ(1u, gc_node->PlatformChildCount());

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, gc_node);

  base::win::ScopedSafearray text_provider_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(text_provider_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(text_provider_ranges.Get(), 1U);

  ITextRangeProvider** array_data;
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      text_provider_ranges.Get(), reinterpret_cast<void**>(&array_data)));

  // If the `embedded_object_character` was being exposed, the search for this
  // string would fail.
  // We have to use `FindText` instead of the `EXPECT_UIA_TEXTRANGE_EQ` macro
  // since that macro uses `GetText` API which hardcodes the
  // `AXEmbeddedObjectCharacter` to be exposed, which then in this case would
  // mess up the text range. Filing a bug for `GetText`. CRBug: 1445692
  base::win::ScopedBstr find_string(L"hello");
  Microsoft::WRL::ComPtr<ITextRangeProvider> text_range_provider_found;
  EXPECT_HRESULT_SUCCEEDED(array_data[0]->FindText(
      find_string.Get(), false, false, &text_range_provider_found));
  ASSERT_TRUE(text_range_provider_found.Get());
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(text_provider_ranges.Get()));
  text_provider_ranges.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       GetVisibleRangesForTextSlightlyOutsideContainer) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
          <div role='textbox' contenteditable="true" style='height: 10px;'>
            <span style='height:20px; display:inline-block'>hello</span>
          </div>
        </body>
      </html>
  )HTML"));

  auto* gc_node = FindNode(ax::mojom::Role::kTextField, "hello");

  ASSERT_NE(nullptr, gc_node);
  EXPECT_EQ(1u, gc_node->PlatformChildCount());

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, gc_node);

  base::win::ScopedSafearray text_provider_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(text_provider_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(text_provider_ranges.Get(), 1U);

  ITextRangeProvider** array_data;
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayAccessData(
      text_provider_ranges.Get(), reinterpret_cast<void**>(&array_data)));

  EXPECT_UIA_TEXTRANGE_EQ(array_data[0], L"hello");
  ASSERT_HRESULT_SUCCEEDED(::SafeArrayUnaccessData(text_provider_ranges.Get()));
  text_provider_ranges.Reset();
}

IN_PROC_BROWSER_TEST_F(AXPlatformNodeTextProviderWinBrowserTest,
                       GetVisibleRangesRefCount) {
  LoadInitialAccessibilityTreeFromHtml(std::string(R"HTML(
      <!DOCTYPE html>
      <html>
        <body>
            hello
        </body>
      </html>
  )HTML"));

  auto* text_node = FindNode(ax::mojom::Role::kStaticText, "hello");
  ASSERT_NE(nullptr, text_node);

  ComPtr<ITextProvider> text_provider;
  GetTextProviderFromTextNode(text_provider, text_node);

  base::win::ScopedSafearray visible_ranges;
  EXPECT_HRESULT_SUCCEEDED(
      text_provider->GetVisibleRanges(visible_ranges.Receive()));
  ASSERT_UIA_SAFEARRAY_OF_TEXTRANGEPROVIDER(visible_ranges.Get(), 1U);

  LONG index = 0;
  ComPtr<ITextRangeProvider> text_range_provider;
  EXPECT_HRESULT_SUCCEEDED(SafeArrayGetElement(
      visible_ranges.Get(), &index, static_cast<void**>(&text_range_provider)));

  // Validate that there was only one reference to the `text_range_provider`.
  ASSERT_EQ(1U, text_range_provider->Release());

  // This is needed to avoid calling SafeArrayDestroy from SafeArray's dtor when
  // exiting the scope, which would crash trying to release the already
  // destroyed `text_range_provider`.
  visible_ranges.Release();
}

}  // namespace content
