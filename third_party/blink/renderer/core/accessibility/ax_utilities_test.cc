// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/accessibility/ax_utilities_generated.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "ui/accessibility/ax_enums.mojom-blink.h"

namespace blink {

TEST(AXUtilitiesTest, TestIsAriaBooleanAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaAtomicAttr ||
        attr == &html_names::kAriaBusyAttr ||
        attr == &html_names::kAriaDisabledAttr ||
        attr == &html_names::kAriaModalAttr ||
        attr == &html_names::kAriaMultilineAttr ||
        attr == &html_names::kAriaMultiselectableAttr ||
        attr == &html_names::kAriaReadonlyAttr ||
        attr == &html_names::kAriaRequiredAttr) {
      EXPECT_TRUE(IsAriaBooleanAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaBooleanAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaIntegerAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaColcountAttr ||
        attr == &html_names::kAriaColindexAttr ||
        attr == &html_names::kAriaColspanAttr ||
        attr == &html_names::kAriaLevelAttr ||
        attr == &html_names::kAriaPosinsetAttr ||
        attr == &html_names::kAriaRowcountAttr ||
        attr == &html_names::kAriaRowindexAttr ||
        attr == &html_names::kAriaRowspanAttr ||
        attr == &html_names::kAriaSetsizeAttr) {
      EXPECT_TRUE(IsAriaIntegerAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaIntegerAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaDecimalAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaValuemaxAttr ||
        attr == &html_names::kAriaValueminAttr ||
        attr == &html_names::kAriaValuenowAttr) {
      EXPECT_TRUE(IsAriaDecimalAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaDecimalAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaStringAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaBraillelabelAttr ||
        attr == &html_names::kAriaBrailleroledescriptionAttr ||
        attr == &html_names::kAriaColindextextAttr ||
        attr == &html_names::kAriaDescriptionAttr ||
        attr == &html_names::kAriaKeyshortcutsAttr ||
        attr == &html_names::kAriaLabelAttr ||
        attr == &html_names::kAriaPlaceholderAttr ||
        attr == &html_names::kAriaRoledescriptionAttr ||
        attr == &html_names::kAriaRowindextextAttr ||
        attr == &html_names::kAriaValuetextAttr ||
        attr == &html_names::kAriaVirtualcontentAttr) {
      EXPECT_TRUE(IsAriaStringAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaStringAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaIdrefAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaActivedescendantAttr ||
        attr == &html_names::kAriaDetailsAttr ||
        attr == &html_names::kAriaErrormessageAttr) {
      EXPECT_TRUE(IsAriaIdrefAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaIdrefAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaIdrefListAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaActionsAttr ||
        attr == &html_names::kAriaControlsAttr ||
        attr == &html_names::kAriaDescribedbyAttr ||
        attr == &html_names::kAriaFlowtoAttr ||
        attr == &html_names::kAriaLabelledbyAttr ||
        attr == &html_names::kAriaLabeledbyAttr ||
        attr == &html_names::kAriaOwnsAttr) {
      EXPECT_TRUE(IsAriaIdrefListAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaIdrefListAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaTokenAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaAutocompleteAttr ||
        attr == &html_names::kAriaCheckedAttr ||
        attr == &html_names::kAriaCurrentAttr ||
        attr == &html_names::kAriaExpandedAttr ||
        attr == &html_names::kAriaHaspopupAttr ||
        attr == &html_names::kAriaHiddenAttr ||
        attr == &html_names::kAriaInvalidAttr ||
        attr == &html_names::kAriaLiveAttr ||
        attr == &html_names::kAriaOrientationAttr ||
        attr == &html_names::kAriaPressedAttr ||
        attr == &html_names::kAriaSelectedAttr ||
        attr == &html_names::kAriaSortAttr) {
      EXPECT_TRUE(IsAriaTokenAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaTokenAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestIsAriaTokenListAttribute) {
  for (const auto* attr : GetAriaAttributes()) {
    if (attr == &html_names::kAriaRelevantAttr) {
      EXPECT_TRUE(IsAriaTokenListAttribute(*attr));
    } else {
      EXPECT_FALSE(IsAriaTokenListAttribute(*attr));
    }
  }
}

TEST(AXUtilitiesTest, TestAriaAutocompleteValues) {
  Vector<AtomicString> values = GetAriaAutocompleteValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("inline"));
  EXPECT_TRUE(values.Contains("list"));
  EXPECT_TRUE(values.Contains("both"));
  EXPECT_TRUE(values.Contains("none"));
}

TEST(AXUtilitiesTest, TestAriaCheckedValues) {
  Vector<AtomicString> values = GetAriaCheckedValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("mixed"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaCurrentValues) {
  Vector<AtomicString> values = GetAriaCurrentValues();
  ASSERT_EQ(values.size(), 7u);
  EXPECT_TRUE(values.Contains("page"));
  EXPECT_TRUE(values.Contains("step"));
  EXPECT_TRUE(values.Contains("location"));
  EXPECT_TRUE(values.Contains("date"));
  EXPECT_TRUE(values.Contains("time"));
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
}

TEST(AXUtilitiesTest, AriaExpandedValues) {
  Vector<AtomicString> values = GetAriaExpandedValues();
  ASSERT_EQ(values.size(), 3u);
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaHaspopupValues) {
  Vector<AtomicString> values = GetAriaHaspopupValues();
  ASSERT_EQ(values.size(), 7u);
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("menu"));
  EXPECT_TRUE(values.Contains("listbox"));
  EXPECT_TRUE(values.Contains("tree"));
  EXPECT_TRUE(values.Contains("grid"));
  EXPECT_TRUE(values.Contains("dialog"));
}

TEST(AXUtilitiesTest, TestAriaHiddenValues) {
  Vector<AtomicString> values = GetAriaHiddenValues();
  ASSERT_EQ(values.size(), 3u);
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaInvalidValues) {
  Vector<AtomicString> values = GetAriaInvalidValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("grammar"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("spelling"));
  EXPECT_TRUE(values.Contains("true"));
}

TEST(AXUtilitiesTest, TestAriaLiveValues) {
  Vector<AtomicString> values = GetAriaLiveValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("off"));
  EXPECT_TRUE(values.Contains("polite"));
  EXPECT_TRUE(values.Contains("assertive"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaOrientationValues) {
  Vector<AtomicString> values = GetAriaOrientationValues();
  ASSERT_EQ(values.size(), 3u);
  EXPECT_TRUE(values.Contains("horizontal"));
  EXPECT_TRUE(values.Contains("undefined"));
  EXPECT_TRUE(values.Contains("vertical"));
}

TEST(AXUtilitiesTest, TestAriaPressedValues) {
  Vector<AtomicString> values = GetAriaPressedValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("mixed"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaRelevantValues) {
  Vector<AtomicString> values = GetAriaRelevantValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("additions"));
  EXPECT_TRUE(values.Contains("removals"));
  EXPECT_TRUE(values.Contains("text"));
  EXPECT_TRUE(values.Contains("all"));
}

TEST(AXUtilitiesTest, TestAriaSelectedValues) {
  Vector<AtomicString> values = GetAriaSelectedValues();
  ASSERT_EQ(values.size(), 3u);
  EXPECT_TRUE(values.Contains("true"));
  EXPECT_TRUE(values.Contains("false"));
  EXPECT_TRUE(values.Contains("undefined"));
}

TEST(AXUtilitiesTest, TestAriaSortValues) {
  Vector<AtomicString> values = GetAriaSortValues();
  ASSERT_EQ(values.size(), 4u);
  EXPECT_TRUE(values.Contains("ascending"));
  EXPECT_TRUE(values.Contains("descending"));
  EXPECT_TRUE(values.Contains("none"));
  EXPECT_TRUE(values.Contains("other"));
}

TEST(AXUtilitiesTest, TestAriaRoleToInternalRole) {
  // Non-deprecated ARIA roles.
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("alert")),
            ax::mojom::blink::Role::kAlert);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("alertdialog")),
            ax::mojom::blink::Role::kAlertDialog);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("application")),
            ax::mojom::blink::Role::kApplication);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("article")),
            ax::mojom::blink::Role::kArticle);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("banner")),
            ax::mojom::blink::Role::kBanner);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("blockquote")),
            ax::mojom::blink::Role::kBlockquote);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("button")),
            ax::mojom::blink::Role::kButton);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("caption")),
            ax::mojom::blink::Role::kCaption);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("cell")),
            ax::mojom::blink::Role::kCell);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("checkbox")),
            ax::mojom::blink::Role::kCheckBox);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("code")),
            ax::mojom::blink::Role::kCode);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("columnheader")),
            ax::mojom::blink::Role::kColumnHeader);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("combobox")),
            ax::mojom::blink::Role::kComboBoxGrouping);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("comment")),
            ax::mojom::blink::Role::kComment);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("complementary")),
            ax::mojom::blink::Role::kComplementary);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("contentinfo")),
            ax::mojom::blink::Role::kContentInfo);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("definition")),
            ax::mojom::blink::Role::kDefinition);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("deletion")),
            ax::mojom::blink::Role::kContentDeletion);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("dialog")),
            ax::mojom::blink::Role::kDialog);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("document")),
            ax::mojom::blink::Role::kDocument);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("emphasis")),
            ax::mojom::blink::Role::kEmphasis);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("feed")),
            ax::mojom::blink::Role::kFeed);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("figure")),
            ax::mojom::blink::Role::kFigure);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("form")),
            ax::mojom::blink::Role::kForm);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("generic")),
            ax::mojom::blink::Role::kGenericContainer);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("grid")),
            ax::mojom::blink::Role::kGrid);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("gridcell")),
            ax::mojom::blink::Role::kGridCell);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("group")),
            ax::mojom::blink::Role::kGroup);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("heading")),
            ax::mojom::blink::Role::kHeading);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("image")),
            ax::mojom::blink::Role::kImage);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("insertion")),
            ax::mojom::blink::Role::kContentInsertion);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("link")),
            ax::mojom::blink::Role::kLink);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("list")),
            ax::mojom::blink::Role::kList);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("listbox")),
            ax::mojom::blink::Role::kListBox);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("listitem")),
            ax::mojom::blink::Role::kListItem);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("log")),
            ax::mojom::blink::Role::kLog);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("main")),
            ax::mojom::blink::Role::kMain);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("mark")),
            ax::mojom::blink::Role::kMark);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("marquee")),
            ax::mojom::blink::Role::kMarquee);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("math")),
            ax::mojom::blink::Role::kMath);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("menu")),
            ax::mojom::blink::Role::kMenu);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("menubar")),
            ax::mojom::blink::Role::kMenuBar);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("menuitem")),
            ax::mojom::blink::Role::kMenuItem);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("menuitemcheckbox")),
            ax::mojom::blink::Role::kMenuItemCheckBox);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("menuitemradio")),
            ax::mojom::blink::Role::kMenuItemRadio);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("meter")),
            ax::mojom::blink::Role::kMeter);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("navigation")),
            ax::mojom::blink::Role::kNavigation);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("none")),
            ax::mojom::blink::Role::kNone);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("note")),
            ax::mojom::blink::Role::kNote);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("option")),
            ax::mojom::blink::Role::kListBoxOption);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("paragraph")),
            ax::mojom::blink::Role::kParagraph);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("progressbar")),
            ax::mojom::blink::Role::kProgressIndicator);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("radio")),
            ax::mojom::blink::Role::kRadioButton);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("radiogroup")),
            ax::mojom::blink::Role::kRadioGroup);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("region")),
            ax::mojom::blink::Role::kRegion);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("row")),
            ax::mojom::blink::Role::kRow);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("rowgroup")),
            ax::mojom::blink::Role::kRowGroup);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("rowheader")),
            ax::mojom::blink::Role::kRowHeader);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("scrollbar")),
            ax::mojom::blink::Role::kScrollBar);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("search")),
            ax::mojom::blink::Role::kSearch);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("searchbox")),
            ax::mojom::blink::Role::kSearchBox);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("separator")),
            ax::mojom::blink::Role::kSplitter);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("slider")),
            ax::mojom::blink::Role::kSlider);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("spinbutton")),
            ax::mojom::blink::Role::kSpinButton);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("status")),
            ax::mojom::blink::Role::kStatus);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("strong")),
            ax::mojom::blink::Role::kStrong);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("subscript")),
            ax::mojom::blink::Role::kSubscript);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("superscript")),
            ax::mojom::blink::Role::kSuperscript);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("switch")),
            ax::mojom::blink::Role::kSwitch);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("tab")),
            ax::mojom::blink::Role::kTab);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("table")),
            ax::mojom::blink::Role::kTable);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("tablist")),
            ax::mojom::blink::Role::kTabList);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("tabpanel")),
            ax::mojom::blink::Role::kTabPanel);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("term")),
            ax::mojom::blink::Role::kTerm);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("textbox")),
            ax::mojom::blink::Role::kTextField);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("time")),
            ax::mojom::blink::Role::kTime);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("timer")),
            ax::mojom::blink::Role::kTimer);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("toolbar")),
            ax::mojom::blink::Role::kToolbar);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("tooltip")),
            ax::mojom::blink::Role::kTooltip);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("tree")),
            ax::mojom::blink::Role::kTree);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("treegrid")),
            ax::mojom::blink::Role::kTreeGrid);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("treeitem")),
            ax::mojom::blink::Role::kTreeItem);

  // Deprecated ARIA roles.
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("directory")),
            ax::mojom::blink::Role::kList);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("img")),
            ax::mojom::blink::Role::kImage);
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("presentation")),
            ax::mojom::blink::Role::kNone);

  // Non-existent ARIA role.
  EXPECT_EQ(AriaRoleToInternalRole(AtomicString("nonexistent")),
            ax::mojom::blink::Role::kUnknown);
}

TEST(AXUtilitiesTest, TestInternalRoleToAriaRole) {
  for (auto role = static_cast<ax::mojom::blink::Role>(0);
       role <= ax::mojom::blink::Role::kWindow;
       role = static_cast<ax::mojom::blink::Role>(static_cast<int>(role) + 1)) {
    switch (role) {
      case ax::mojom::blink::Role::kNone:
        EXPECT_EQ(InternalRoleToAriaRole(role), "none");
        break;
      case ax::mojom::blink::Role::kAlert:
        EXPECT_EQ(InternalRoleToAriaRole(role), "alert");
        break;
      case ax::mojom::blink::Role::kAlertDialog:
        EXPECT_EQ(InternalRoleToAriaRole(role), "alertdialog");
        break;
      case ax::mojom::blink::Role::kApplication:
        EXPECT_EQ(InternalRoleToAriaRole(role), "application");
        break;
      case ax::mojom::blink::Role::kArticle:
        EXPECT_EQ(InternalRoleToAriaRole(role), "article");
        break;
      case ax::mojom::blink::Role::kBanner:
        EXPECT_EQ(InternalRoleToAriaRole(role), "banner");
        break;
      case ax::mojom::blink::Role::kBlockquote:
        EXPECT_EQ(InternalRoleToAriaRole(role), "blockquote");
        break;
      case ax::mojom::blink::Role::kButton:
      case ax::mojom::blink::Role::kToggleButton:
      case ax::mojom::blink::Role::kPopUpButton:
        EXPECT_EQ(InternalRoleToAriaRole(role), "button");
        break;
      case ax::mojom::blink::Role::kCaption:
        EXPECT_EQ(InternalRoleToAriaRole(role), "caption");
        break;
      case ax::mojom::blink::Role::kCell:
        EXPECT_EQ(InternalRoleToAriaRole(role), "cell");
        break;
      case ax::mojom::blink::Role::kCheckBox:
        EXPECT_EQ(InternalRoleToAriaRole(role), "checkbox");
        break;
      case ax::mojom::blink::Role::kCode:
        EXPECT_EQ(InternalRoleToAriaRole(role), "code");
        break;
      case ax::mojom::blink::Role::kColumnHeader:
        EXPECT_EQ(InternalRoleToAriaRole(role), "columnheader");
        break;
      case ax::mojom::blink::Role::kComboBoxGrouping:
      case ax::mojom::blink::Role::kComboBoxMenuButton:
      case ax::mojom::blink::Role::kComboBoxSelect:
      case ax::mojom::blink::Role::kTextFieldWithComboBox:
        EXPECT_EQ(InternalRoleToAriaRole(role), "combobox");
        break;
      case ax::mojom::blink::Role::kComplementary:
        EXPECT_EQ(InternalRoleToAriaRole(role), "complementary");
        break;
      case ax::mojom::blink::Role::kComment:
        EXPECT_EQ(InternalRoleToAriaRole(role), "comment");
        break;
      case ax::mojom::blink::Role::kContentDeletion:
        EXPECT_EQ(InternalRoleToAriaRole(role), "deletion");
        break;
      case ax::mojom::blink::Role::kContentInsertion:
        EXPECT_EQ(InternalRoleToAriaRole(role), "insertion");
        break;
      case ax::mojom::blink::Role::kContentInfo:
        EXPECT_EQ(InternalRoleToAriaRole(role), "contentinfo");
        break;
      case ax::mojom::blink::Role::kDefinition:
        EXPECT_EQ(InternalRoleToAriaRole(role), "definition");
        break;
      case ax::mojom::blink::Role::kDialog:
        EXPECT_EQ(InternalRoleToAriaRole(role), "dialog");
        break;
      case ax::mojom::blink::Role::kDocAbstract:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-abstract");
        break;
      case ax::mojom::blink::Role::kDocAcknowledgments:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-acknowledgments");
        break;
      case ax::mojom::blink::Role::kDocAfterword:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-afterword");
        break;
      case ax::mojom::blink::Role::kDocAppendix:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-appendix");
        break;
      case ax::mojom::blink::Role::kDocBackLink:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-backlink");
        break;
      case ax::mojom::blink::Role::kDocBiblioEntry:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-biblioentry");
        break;
      case ax::mojom::blink::Role::kDocBibliography:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-bibliography");
        break;
      case ax::mojom::blink::Role::kDocBiblioRef:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-biblioref");
        break;
      case ax::mojom::blink::Role::kDocChapter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-chapter");
        break;
      case ax::mojom::blink::Role::kDocColophon:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-colophon");
        break;
      case ax::mojom::blink::Role::kDocConclusion:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-conclusion");
        break;
      case ax::mojom::blink::Role::kDocCover:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-cover");
        break;
      case ax::mojom::blink::Role::kDocCredit:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-credit");
        break;
      case ax::mojom::blink::Role::kDocCredits:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-credits");
        break;
      case ax::mojom::blink::Role::kDocDedication:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-dedication");
        break;
      case ax::mojom::blink::Role::kDocEndnote:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-endnote");
        break;
      case ax::mojom::blink::Role::kDocEndnotes:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-endnotes");
        break;
      case ax::mojom::blink::Role::kDocEpigraph:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-epigraph");
        break;
      case ax::mojom::blink::Role::kDocEpilogue:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-epilogue");
        break;
      case ax::mojom::blink::Role::kDocErrata:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-errata");
        break;
      case ax::mojom::blink::Role::kDocExample:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-example");
        break;
      case ax::mojom::blink::Role::kDocFootnote:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-footnote");
        break;
      case ax::mojom::blink::Role::kDocForeword:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-foreword");
        break;
      case ax::mojom::blink::Role::kDocGlossary:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-glossary");
        break;
      case ax::mojom::blink::Role::kDocGlossRef:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-glossref");
        break;
      case ax::mojom::blink::Role::kDocIndex:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-index");
        break;
      case ax::mojom::blink::Role::kDocIntroduction:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-introduction");
        break;
      case ax::mojom::blink::Role::kDocNoteRef:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-noteref");
        break;
      case ax::mojom::blink::Role::kDocNotice:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-notice");
        break;
      case ax::mojom::blink::Role::kDocPageBreak:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-pagebreak");
        break;
      case ax::mojom::blink::Role::kDocPageFooter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-pagefooter");
        break;
      case ax::mojom::blink::Role::kDocPageHeader:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-pageheader");
        break;
      case ax::mojom::blink::Role::kDocPageList:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-pagelist");
        break;
      case ax::mojom::blink::Role::kDocPart:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-part");
        break;
      case ax::mojom::blink::Role::kDocPreface:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-preface");
        break;
      case ax::mojom::blink::Role::kDocPrologue:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-prologue");
        break;
      case ax::mojom::blink::Role::kDocPullquote:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-pullquote");
        break;
      case ax::mojom::blink::Role::kDocQna:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-qna");
        break;
      case ax::mojom::blink::Role::kDocSubtitle:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-subtitle");
        break;
      case ax::mojom::blink::Role::kDocTip:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-tip");
        break;
      case ax::mojom::blink::Role::kDocToc:
        EXPECT_EQ(InternalRoleToAriaRole(role), "doc-toc");
        break;
      case ax::mojom::blink::Role::kDocument:
        EXPECT_EQ(InternalRoleToAriaRole(role), "document");
        break;
      case ax::mojom::blink::Role::kEmphasis:
        EXPECT_EQ(InternalRoleToAriaRole(role), "emphasis");
        break;
      case ax::mojom::blink::Role::kFeed:
        EXPECT_EQ(InternalRoleToAriaRole(role), "feed");
        break;
      case ax::mojom::blink::Role::kFigure:
        EXPECT_EQ(InternalRoleToAriaRole(role), "figure");
        break;
      case ax::mojom::blink::Role::kFooter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "contentinfo");
        break;
      case ax::mojom::blink::Role::kForm:
        EXPECT_EQ(InternalRoleToAriaRole(role), "form");
        break;
      case ax::mojom::blink::Role::kGenericContainer:
        EXPECT_EQ(InternalRoleToAriaRole(role), "generic");
        break;
      case ax::mojom::blink::Role::kGraphicsDocument:
        EXPECT_EQ(InternalRoleToAriaRole(role), "graphics-document");
        break;
      case ax::mojom::blink::Role::kGraphicsObject:
        EXPECT_EQ(InternalRoleToAriaRole(role), "graphics-object");
        break;
      case ax::mojom::blink::Role::kGraphicsSymbol:
        EXPECT_EQ(InternalRoleToAriaRole(role), "graphics-symbol");
        break;
      case ax::mojom::blink::Role::kGrid:
        EXPECT_EQ(InternalRoleToAriaRole(role), "grid");
        break;
      case ax::mojom::blink::Role::kGridCell:
        EXPECT_EQ(InternalRoleToAriaRole(role), "gridcell");
        break;
      case ax::mojom::blink::Role::kGroup:
      case ax::mojom::blink::Role::kDetails:
        EXPECT_EQ(InternalRoleToAriaRole(role), "group");
        break;
      case ax::mojom::blink::Role::kHeader:
        EXPECT_EQ(InternalRoleToAriaRole(role), "banner");
        break;
      case ax::mojom::blink::Role::kHeading:
        EXPECT_EQ(InternalRoleToAriaRole(role), "heading");
        break;
      case ax::mojom::blink::Role::kImage:
        EXPECT_EQ(InternalRoleToAriaRole(role), "image");
        break;
      case ax::mojom::blink::Role::kLink:
        EXPECT_EQ(InternalRoleToAriaRole(role), "link");
        break;
      case ax::mojom::blink::Role::kList:
        EXPECT_EQ(InternalRoleToAriaRole(role), "list");
        break;
      case ax::mojom::blink::Role::kListBox:
        EXPECT_EQ(InternalRoleToAriaRole(role), "listbox");
        break;
      case ax::mojom::blink::Role::kListBoxOption:
      case ax::mojom::blink::Role::kMenuListOption:
        EXPECT_EQ(InternalRoleToAriaRole(role), "option");
        break;
      case ax::mojom::blink::Role::kListItem:
        EXPECT_EQ(InternalRoleToAriaRole(role), "listitem");
        break;
      case ax::mojom::blink::Role::kLog:
        EXPECT_EQ(InternalRoleToAriaRole(role), "log");
        break;
      case ax::mojom::blink::Role::kMain:
        EXPECT_EQ(InternalRoleToAriaRole(role), "main");
        break;
      case ax::mojom::blink::Role::kMark:
        EXPECT_EQ(InternalRoleToAriaRole(role), "mark");
        break;
      case ax::mojom::blink::Role::kMarquee:
        EXPECT_EQ(InternalRoleToAriaRole(role), "marquee");
        break;
      case ax::mojom::blink::Role::kMath:
        EXPECT_EQ(InternalRoleToAriaRole(role), "math");
        break;
      case ax::mojom::blink::Role::kMenu:
        EXPECT_EQ(InternalRoleToAriaRole(role), "menu");
        break;
      case ax::mojom::blink::Role::kMenuBar:
        EXPECT_EQ(InternalRoleToAriaRole(role), "menubar");
        break;
      case ax::mojom::blink::Role::kMenuItem:
        EXPECT_EQ(InternalRoleToAriaRole(role), "menuitem");
        break;
      case ax::mojom::blink::Role::kMenuItemCheckBox:
        EXPECT_EQ(InternalRoleToAriaRole(role), "menuitemcheckbox");
        break;
      case ax::mojom::blink::Role::kMenuItemRadio:
        EXPECT_EQ(InternalRoleToAriaRole(role), "menuitemradio");
        break;
      case ax::mojom::blink::Role::kMenuItemSeparator:
      case ax::mojom::blink::Role::kSplitter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "separator");
        break;
      case ax::mojom::blink::Role::kMeter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "meter");
        break;
      case ax::mojom::blink::Role::kNavigation:
        EXPECT_EQ(InternalRoleToAriaRole(role), "navigation");
        break;
      case ax::mojom::blink::Role::kNote:
        EXPECT_EQ(InternalRoleToAriaRole(role), "note");
        break;
      case ax::mojom::blink::Role::kParagraph:
        EXPECT_EQ(InternalRoleToAriaRole(role), "paragraph");
        break;
      case ax::mojom::blink::Role::kProgressIndicator:
        EXPECT_EQ(InternalRoleToAriaRole(role), "progressbar");
        break;
      case ax::mojom::blink::Role::kRadioButton:
        EXPECT_EQ(InternalRoleToAriaRole(role), "radio");
        break;
      case ax::mojom::blink::Role::kRadioGroup:
        EXPECT_EQ(InternalRoleToAriaRole(role), "radiogroup");
        break;
      case ax::mojom::blink::Role::kRegion:
        EXPECT_EQ(InternalRoleToAriaRole(role), "region");
        break;
      case ax::mojom::blink::Role::kRow:
        EXPECT_EQ(InternalRoleToAriaRole(role), "row");
        break;
      case ax::mojom::blink::Role::kRowGroup:
        EXPECT_EQ(InternalRoleToAriaRole(role), "rowgroup");
        break;
      case ax::mojom::blink::Role::kRowHeader:
        EXPECT_EQ(InternalRoleToAriaRole(role), "rowheader");
        break;
      case ax::mojom::blink::Role::kScrollBar:
        EXPECT_EQ(InternalRoleToAriaRole(role), "scrollbar");
        break;
      case ax::mojom::blink::Role::kSearch:
        EXPECT_EQ(InternalRoleToAriaRole(role), "search");
        break;
      case ax::mojom::blink::Role::kSearchBox:
        EXPECT_EQ(InternalRoleToAriaRole(role), "searchbox");
        break;
      case ax::mojom::blink::Role::kSectionFooter:
        EXPECT_EQ(InternalRoleToAriaRole(role), "sectionfooter");
        break;
      case ax::mojom::blink::Role::kSectionHeader:
        EXPECT_EQ(InternalRoleToAriaRole(role), "sectionheader");
        break;
      case ax::mojom::blink::Role::kSectionWithoutName:
        EXPECT_EQ(InternalRoleToAriaRole(role), "generic");
        break;
      case ax::mojom::blink::Role::kSlider:
        EXPECT_EQ(InternalRoleToAriaRole(role), "slider");
        break;
      case ax::mojom::blink::Role::kSpinButton:
        EXPECT_EQ(InternalRoleToAriaRole(role), "spinbutton");
        break;
      case ax::mojom::blink::Role::kStatus:
        EXPECT_EQ(InternalRoleToAriaRole(role), "status");
        break;
      case ax::mojom::blink::Role::kStrong:
        EXPECT_EQ(InternalRoleToAriaRole(role), "strong");
        break;
      case ax::mojom::blink::Role::kSubscript:
        EXPECT_EQ(InternalRoleToAriaRole(role), "subscript");
        break;
      case ax::mojom::blink::Role::kSuggestion:
        EXPECT_EQ(InternalRoleToAriaRole(role), "suggestion");
        break;
      case ax::mojom::blink::Role::kSuperscript:
        EXPECT_EQ(InternalRoleToAriaRole(role), "superscript");
        break;
      case ax::mojom::blink::Role::kSwitch:
        EXPECT_EQ(InternalRoleToAriaRole(role), "switch");
        break;
      case ax::mojom::blink::Role::kTab:
        EXPECT_EQ(InternalRoleToAriaRole(role), "tab");
        break;
      case ax::mojom::blink::Role::kTabList:
        EXPECT_EQ(InternalRoleToAriaRole(role), "tablist");
        break;
      case ax::mojom::blink::Role::kTabPanel:
        EXPECT_EQ(InternalRoleToAriaRole(role), "tabpanel");
        break;
      case ax::mojom::blink::Role::kTable:
        EXPECT_EQ(InternalRoleToAriaRole(role), "table");
        break;
      case ax::mojom::blink::Role::kTerm:
        EXPECT_EQ(InternalRoleToAriaRole(role), "term");
        break;
      case ax::mojom::blink::Role::kTextField:
        EXPECT_EQ(InternalRoleToAriaRole(role), "textbox");
        break;
      case ax::mojom::blink::Role::kTime:
        EXPECT_EQ(InternalRoleToAriaRole(role), "time");
        break;
      case ax::mojom::blink::Role::kTimer:
        EXPECT_EQ(InternalRoleToAriaRole(role), "timer");
        break;
      case ax::mojom::blink::Role::kToolbar:
        EXPECT_EQ(InternalRoleToAriaRole(role), "toolbar");
        break;
      case ax::mojom::blink::Role::kTooltip:
        EXPECT_EQ(InternalRoleToAriaRole(role), "tooltip");
        break;
      case ax::mojom::blink::Role::kTree:
        EXPECT_EQ(InternalRoleToAriaRole(role), "tree");
        break;
      case ax::mojom::blink::Role::kTreeGrid:
        EXPECT_EQ(InternalRoleToAriaRole(role), "treegrid");
        break;
      case ax::mojom::blink::Role::kTreeItem:
        EXPECT_EQ(InternalRoleToAriaRole(role), "treeitem");
        break;
      // All roles that return empty string
      case ax::mojom::blink::Role::kAbbr:
      case ax::mojom::blink::Role::kAudio:
      case ax::mojom::blink::Role::kCanvas:
      case ax::mojom::blink::Role::kCaret:
      case ax::mojom::blink::Role::kClient:
      case ax::mojom::blink::Role::kColorWell:
      case ax::mojom::blink::Role::kColumn:
      case ax::mojom::blink::Role::kDate:
      case ax::mojom::blink::Role::kDateTime:
      case ax::mojom::blink::Role::kDescriptionList:
      case ax::mojom::blink::Role::kDescriptionListDetailDeprecated:
      case ax::mojom::blink::Role::kDescriptionListTermDeprecated:
      case ax::mojom::blink::Role::kDesktop:
      case ax::mojom::blink::Role::kDirectoryDeprecated:
      case ax::mojom::blink::Role::kDisclosureTriangle:
      case ax::mojom::blink::Role::kDisclosureTriangleGrouped:
      case ax::mojom::blink::Role::kEmbeddedObject:
      case ax::mojom::blink::Role::kFigcaption:
      case ax::mojom::blink::Role::kIframe:
      case ax::mojom::blink::Role::kIframePresentational:
      case ax::mojom::blink::Role::kImeCandidate:
      case ax::mojom::blink::Role::kInlineTextBox:
      case ax::mojom::blink::Role::kInputTime:
      case ax::mojom::blink::Role::kKeyboard:
      case ax::mojom::blink::Role::kLabelText:
      case ax::mojom::blink::Role::kLayoutTable:
      case ax::mojom::blink::Role::kLayoutTableCell:
      case ax::mojom::blink::Role::kLayoutTableRow:
      case ax::mojom::blink::Role::kLegend:
      case ax::mojom::blink::Role::kLineBreak:
      case ax::mojom::blink::Role::kListGrid:
      case ax::mojom::blink::Role::kListMarker:
      case ax::mojom::blink::Role::kMathMLFraction:
      case ax::mojom::blink::Role::kMathMLIdentifier:
      case ax::mojom::blink::Role::kMathMLMath:
      case ax::mojom::blink::Role::kMathMLMultiscripts:
      case ax::mojom::blink::Role::kMathMLNoneScript:
      case ax::mojom::blink::Role::kMathMLNumber:
      case ax::mojom::blink::Role::kMathMLOperator:
      case ax::mojom::blink::Role::kMathMLOver:
      case ax::mojom::blink::Role::kMathMLPrescriptDelimiter:
      case ax::mojom::blink::Role::kMathMLRoot:
      case ax::mojom::blink::Role::kMathMLRow:
      case ax::mojom::blink::Role::kMathMLSquareRoot:
      case ax::mojom::blink::Role::kMathMLStringLiteral:
      case ax::mojom::blink::Role::kMathMLSub:
      case ax::mojom::blink::Role::kMathMLSubSup:
      case ax::mojom::blink::Role::kMathMLSup:
      case ax::mojom::blink::Role::kMathMLTable:
      case ax::mojom::blink::Role::kMathMLTableCell:
      case ax::mojom::blink::Role::kMathMLTableRow:
      case ax::mojom::blink::Role::kMathMLText:
      case ax::mojom::blink::Role::kMathMLUnder:
      case ax::mojom::blink::Role::kMathMLUnderOver:
      case ax::mojom::blink::Role::kMenuListPopup:
      case ax::mojom::blink::Role::kPane:
      case ax::mojom::blink::Role::kPdfActionableHighlight:
      case ax::mojom::blink::Role::kPdfRoot:
      case ax::mojom::blink::Role::kPluginObject:
      case ax::mojom::blink::Role::kPortalDeprecated:
      case ax::mojom::blink::Role::kPreDeprecated:
      case ax::mojom::blink::Role::kRootWebArea:
      case ax::mojom::blink::Role::kRuby:
      case ax::mojom::blink::Role::kRubyAnnotation:
      case ax::mojom::blink::Role::kScrollView:
      case ax::mojom::blink::Role::kSection:
      case ax::mojom::blink::Role::kStaticText:
      case ax::mojom::blink::Role::kSvgRoot:
      case ax::mojom::blink::Role::kTableHeaderContainer:
      case ax::mojom::blink::Role::kTitleBar:
      case ax::mojom::blink::Role::kUnknown:
      case ax::mojom::blink::Role::kVideo:
      case ax::mojom::blink::Role::kWebView:
      case ax::mojom::blink::Role::kWindow:
        EXPECT_EQ(InternalRoleToAriaRole(role), "");
        break;
      default:
        NOTREACHED();
    }
  }
}

TEST(AXUtilitiesTest, TestAriaRoleDataConsistency) {
  auto role_names = GetAriaRoleNames();

  // Test that all role names can be converted to internal roles and back
  for (const auto& role_name : role_names) {
    ax::mojom::blink::Role internal_role = AriaRoleToInternalRole(role_name);
    EXPECT_NE(internal_role, ax::mojom::blink::Role::kUnknown)
        << "Role '" << role_name << "' should map to a valid internal role";
    const AtomicString& back_converted = InternalRoleToAriaRole(internal_role);
    EXPECT_FALSE(back_converted.empty())
        << "Internal role for '" << role_name
        << "' should map back to some ARIA role";
  }
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaChecked) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "checkbox" || role == "menuitemcheckbox" ||
        role == "menuitemradio" || role == "option" || role == "radio" ||
        role == "switch" || role == "treeitem") {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaCheckedAttr))
          << "Role '" << role << "' should support aria-checked";
    } else {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaCheckedAttr))
          << "Role '" << role << "' should not support aria-checked";
    }
  }
  EXPECT_FALSE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                         html_names::kAriaCheckedAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaPressed) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "button") {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaPressedAttr))
          << "Role '" << role << "' should support aria-pressed";
    } else {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaPressedAttr))
          << "Role '" << role << "' should not support aria-pressed";
    }
  }
  EXPECT_FALSE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                         html_names::kAriaPressedAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaExpanded) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "application" || role == "button" || role == "checkbox" ||
        role == "columnheader" || role == "combobox" || role == "gridcell" ||
        role == "link" || role == "menuitem" || role == "menuitemcheckbox" ||
        role == "menuitemradio" || role == "row" || role == "rowheader" ||
        role == "switch" || role == "tab" || role == "treeitem") {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaExpandedAttr))
          << "Role '" << role << "' should support aria-expanded";
    } else {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaExpandedAttr))
          << "Role '" << role << "' should not support aria-expanded";
    }
  }
  EXPECT_FALSE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                         html_names::kAriaExpandedAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaAtomic) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                          html_names::kAriaAtomicAttr))
        << "Role '" << role << "' should support aria-atomic (global)";
  }
  EXPECT_TRUE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                        html_names::kAriaAtomicAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaLabel) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "caption" || role == "code" || role == "definition" ||
        role == "deletion" || role == "emphasis" || role == "generic" ||
        role == "insertion" || role == "mark" || role == "none" ||
        role == "paragraph" || role == "strong" || role == "subscript" ||
        role == "suggestion" || role == "superscript" || role == "term" ||
        role == "time") {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaLabelAttr))
          << "Role '" << role << "' should not support aria-label (prevented)";
    } else {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaLabelAttr))
          << "Role '" << role << "' should support aria-label (global)";
    }
  }
  EXPECT_TRUE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                        html_names::kAriaLabelAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaRoledescription) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "generic") {
      EXPECT_FALSE(RoleSupportsAriaAttribute(
          AriaRoleToInternalRole(role), html_names::kAriaRoledescriptionAttr))
          << "Role '" << role
          << "' should not support aria-roledescription (prevented)";
    } else {
      EXPECT_TRUE(RoleSupportsAriaAttribute(
          AriaRoleToInternalRole(role), html_names::kAriaRoledescriptionAttr))
          << "Role '" << role
          << "' should support aria-roledescription (global)";
    }
  }
  EXPECT_TRUE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                        html_names::kAriaRoledescriptionAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaSelected) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "columnheader" || role == "gridcell" || role == "option" ||
        role == "row" || role == "rowheader" || role == "tab" ||
        role == "treeitem") {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaSelectedAttr))
          << "Role '" << role << "' should support aria-selected";
    } else {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaSelectedAttr))
          << "Role '" << role << "' should not support aria-selected";
    }
  }
  EXPECT_FALSE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                         html_names::kAriaSelectedAttr));
}

TEST(AXUtilitiesTest, TestRoleSupportsAriaReadonly) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    if (role == "checkbox" || role == "columnheader" || role == "combobox" ||
        role == "grid" || role == "gridcell" || role == "listbox" ||
        role == "radiogroup" || role == "rowheader" || role == "searchbox" ||
        role == "slider" || role == "spinbutton" || role == "switch" ||
        role == "textbox" || role == "treegrid") {
      EXPECT_TRUE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                            html_names::kAriaReadonlyAttr))
          << "Role '" << role << "' should support aria-readonly";
    } else {
      EXPECT_FALSE(RoleSupportsAriaAttribute(AriaRoleToInternalRole(role),
                                             html_names::kAriaReadonlyAttr))
          << "Role '" << role << "' should not support aria-readonly";
    }
  }
  EXPECT_FALSE(RoleSupportsAriaAttribute(ax::mojom::blink::Role::kUnknown,
                                         html_names::kAriaReadonlyAttr));
}

TEST(AXUtilitiesTest, TestGetImplicitAriaOrientation) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaOrientation(AriaRoleToInternalRole(role));
    if (role == "list" || role == "listbox" || role == "menu" ||
        role == "scrollbar" || role == "tree") {
      EXPECT_EQ(result, "vertical") << "Role: " << role;
    } else if (role == "menubar" || role == "separator" || role == "slider" ||
               role == "tablist" || role == "toolbar") {
      EXPECT_EQ(result, "horizontal") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "undefined") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaOrientation(ax::mojom::blink::Role::kUnknown),
            "undefined");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaChecked) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaChecked(AriaRoleToInternalRole(role));
    if (role == "checkbox" || role == "menuitemcheckbox" ||
        role == "menuitemradio" || role == "radio" || role == "switch") {
      EXPECT_EQ(result, "false") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "undefined") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaChecked(ax::mojom::blink::Role::kUnknown),
            "undefined");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaSelected) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaSelected(AriaRoleToInternalRole(role));
    if (role == "option" || role == "tab") {
      EXPECT_EQ(result, "false") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "undefined") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaSelected(ax::mojom::blink::Role::kUnknown),
            "undefined");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaLive) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaLive(AriaRoleToInternalRole(role));
    if (role == "log" || role == "status") {
      EXPECT_EQ(result, "polite") << "Role: " << role;
    } else if (role == "alert") {
      EXPECT_EQ(result, "assertive") << "Role: " << role;
    } else if (role == "marquee" || role == "timer") {
      EXPECT_EQ(result, "off") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "undefined") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaLive(ax::mojom::blink::Role::kUnknown), "undefined");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaAtomic) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaAtomic(AriaRoleToInternalRole(role));
    if (role == "alert" || role == "status") {
      EXPECT_EQ(result, "true") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "false") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaAtomic(ax::mojom::blink::Role::kUnknown), "false");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaExpanded) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaExpanded(AriaRoleToInternalRole(role));
    if (role == "combobox") {
      EXPECT_EQ(result, "false") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "undefined") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaExpanded(ax::mojom::blink::Role::kUnknown),
            "undefined");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaHaspopup) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaHaspopup(AriaRoleToInternalRole(role));
    if (role == "combobox") {
      EXPECT_EQ(result, "listbox") << "Role: " << role;
    } else {
      EXPECT_EQ(result, "false") << "Role: " << role;
    }
  }
  EXPECT_EQ(GetImplicitAriaHaspopup(ax::mojom::blink::Role::kUnknown), "false");
}

TEST(AXUtilitiesTest, TestGetImplicitAriaLevel) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaLevel(AriaRoleToInternalRole(role));
    if (role == "heading") {
      EXPECT_EQ(result, "2") << "Role: " << role;
    } else {
      EXPECT_TRUE(result.empty()) << "Role: " << role;
    }
  }
  EXPECT_TRUE(GetImplicitAriaLevel(ax::mojom::blink::Role::kUnknown).empty());
}

TEST(AXUtilitiesTest, TestGetImplicitAriaValuemin) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaValuemin(AriaRoleToInternalRole(role));
    if (role == "meter" || role == "progressbar" || role == "scrollbar" ||
        role == "separator" || role == "slider") {
      EXPECT_EQ(result, "0") << "Role: " << role;
    } else {
      EXPECT_TRUE(result.empty()) << "Role: " << role;
    }
  }
  EXPECT_TRUE(
      GetImplicitAriaValuemin(ax::mojom::blink::Role::kUnknown).empty());
}

TEST(AXUtilitiesTest, TestGetImplicitAriaValuemax) {
  auto role_names = GetAriaRoleNames();
  for (const auto& role : role_names) {
    const AtomicString& result =
        GetImplicitAriaValuemax(AriaRoleToInternalRole(role));
    if (role == "meter" || role == "progressbar" || role == "scrollbar" ||
        role == "separator" || role == "slider") {
      EXPECT_EQ(result, "100") << "Role: " << role;
    } else {
      EXPECT_TRUE(result.empty()) << "Role: " << role;
    }
  }
  EXPECT_TRUE(
      GetImplicitAriaValuemax(ax::mojom::blink::Role::kUnknown).empty());
}

TEST(AXUtilitiesTest, TestNameFromConsistency) {
  // Verify that nameFrom relationships are consistent:
  // 1. If a role is name prohibited, it shouldn't support name from
  //    author/contents.
  // 2. If a role supports name from contents, it should also support name
  //    from author.
  for (const auto& role_name : GetAriaRoleNames()) {
    auto role = AriaRoleToInternalRole(role_name);
    bool is_prohibited = RoleIsNameProhibited(role);
    bool supports_contents = RoleSupportsNameFromContents(role);
    bool supports_author = RoleSupportsNameFromAuthor(role);

    if (is_prohibited) {
      EXPECT_FALSE(supports_author)
          << "Role '" << role_name
          << "' is name prohibited but supports author";
      EXPECT_FALSE(supports_contents)
          << "Role '" << role_name
          << "' is name prohibited but supports contents";
    }

    if (supports_contents) {
      EXPECT_TRUE(supports_author)
          << "Role '" << role_name
          << "' supports contents but not author (ARIA spec requires both)";
    }
  }
}

}  // namespace blink
