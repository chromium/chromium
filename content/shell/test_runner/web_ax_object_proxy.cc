// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/test_runner/web_ax_object_proxy.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "gin/handle.h"
#include "third_party/blink/public/platform/web_float_rect.h"
#include "third_party/blink/public/platform/web_point.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace test_runner {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining layout tests.
std::string RoleToString(ax::mojom::Role role) {
  std::string result = "AXRole: AX";
  switch (role) {
    case ax::mojom::Role::kAbbr:
      return result.append("Abbr");
    case ax::mojom::Role::kAlertDialog:
      return result.append("AlertDialog");
    case ax::mojom::Role::kAlert:
      return result.append("Alert");
    case ax::mojom::Role::kAnchor:
      return result.append("Anchor");
    case ax::mojom::Role::kAnnotation:
      return result.append("Annotation");
    case ax::mojom::Role::kApplication:
      return result.append("Application");
    case ax::mojom::Role::kArticle:
      return result.append("Article");
    case ax::mojom::Role::kAudio:
      return result.append("Audio");
    case ax::mojom::Role::kBanner:
      return result.append("Banner");
    case ax::mojom::Role::kBlockquote:
      return result.append("Blockquote");
    case ax::mojom::Role::kButton:
      return result.append("Button");
    case ax::mojom::Role::kCanvas:
      return result.append("Canvas");
    case ax::mojom::Role::kCaption:
      return result.append("Caption");
    case ax::mojom::Role::kCell:
      return result.append("Cell");
    case ax::mojom::Role::kCheckBox:
      return result.append("CheckBox");
    case ax::mojom::Role::kColorWell:
      return result.append("ColorWell");
    case ax::mojom::Role::kColumnHeader:
      return result.append("ColumnHeader");
    case ax::mojom::Role::kColumn:
      return result.append("Column");
    case ax::mojom::Role::kComboBoxGrouping:
      return result.append("ComboBoxGrouping");
    case ax::mojom::Role::kComboBoxMenuButton:
      return result.append("ComboBoxMenuButton");
    case ax::mojom::Role::kComplementary:
      return result.append("Complementary");
    case ax::mojom::Role::kContentDeletion:
      return result.append("ContentDeletion");
    case ax::mojom::Role::kContentInsertion:
      return result.append("ContentInsertion");
    case ax::mojom::Role::kContentInfo:
      return result.append("ContentInfo");
    case ax::mojom::Role::kDate:
      return result.append("DateField");
    case ax::mojom::Role::kDateTime:
      return result.append("DateTimeField");
    case ax::mojom::Role::kDefinition:
      return result.append("Definition");
    case ax::mojom::Role::kDescriptionListDetail:
      return result.append("DescriptionListDetail");
    case ax::mojom::Role::kDescriptionList:
      return result.append("DescriptionList");
    case ax::mojom::Role::kDescriptionListTerm:
      return result.append("DescriptionListTerm");
    case ax::mojom::Role::kDetails:
      return result.append("Details");
    case ax::mojom::Role::kDialog:
      return result.append("Dialog");
    case ax::mojom::Role::kDirectory:
      return result.append("Directory");
    case ax::mojom::Role::kDisclosureTriangle:
      return result.append("DisclosureTriangle");
    case ax::mojom::Role::kDocAbstract:
      return result.append("DocAbstract");
    case ax::mojom::Role::kDocAcknowledgments:
      return result.append("DocAcknowledgments");
    case ax::mojom::Role::kDocAfterword:
      return result.append("DocAfterword");
    case ax::mojom::Role::kDocAppendix:
      return result.append("DocAppendix");
    case ax::mojom::Role::kDocBackLink:
      return result.append("DocBackLink");
    case ax::mojom::Role::kDocBiblioEntry:
      return result.append("DocBiblioEntry");
    case ax::mojom::Role::kDocBibliography:
      return result.append("DocBibliography");
    case ax::mojom::Role::kDocBiblioRef:
      return result.append("DocBiblioRef");
    case ax::mojom::Role::kDocChapter:
      return result.append("DocChapter");
    case ax::mojom::Role::kDocColophon:
      return result.append("DocColophon");
    case ax::mojom::Role::kDocConclusion:
      return result.append("DocConclusion");
    case ax::mojom::Role::kDocCover:
      return result.append("DocCover");
    case ax::mojom::Role::kDocCredit:
      return result.append("DocCredit");
    case ax::mojom::Role::kDocCredits:
      return result.append("DocCredits");
    case ax::mojom::Role::kDocDedication:
      return result.append("DocDedication");
    case ax::mojom::Role::kDocEndnote:
      return result.append("DocEndnote");
    case ax::mojom::Role::kDocEndnotes:
      return result.append("DocEndnotes");
    case ax::mojom::Role::kDocEpigraph:
      return result.append("DocEpigraph");
    case ax::mojom::Role::kDocEpilogue:
      return result.append("DocEpilogue");
    case ax::mojom::Role::kDocErrata:
      return result.append("DocErrata");
    case ax::mojom::Role::kDocExample:
      return result.append("DocExample");
    case ax::mojom::Role::kDocFootnote:
      return result.append("DocFootnote");
    case ax::mojom::Role::kDocForeword:
      return result.append("DocForeword");
    case ax::mojom::Role::kDocGlossary:
      return result.append("DocGlossary");
    case ax::mojom::Role::kDocGlossRef:
      return result.append("DocGlossRef");
    case ax::mojom::Role::kDocIndex:
      return result.append("DocIndex");
    case ax::mojom::Role::kDocIntroduction:
      return result.append("DocIntroduction");
    case ax::mojom::Role::kDocNoteRef:
      return result.append("DocNoteRef");
    case ax::mojom::Role::kDocNotice:
      return result.append("DocNotice");
    case ax::mojom::Role::kDocPageBreak:
      return result.append("DocPageBreak");
    case ax::mojom::Role::kDocPageList:
      return result.append("DocPageList");
    case ax::mojom::Role::kDocPart:
      return result.append("DocPart");
    case ax::mojom::Role::kDocPreface:
      return result.append("DocPreface");
    case ax::mojom::Role::kDocPrologue:
      return result.append("DocPrologue");
    case ax::mojom::Role::kDocPullquote:
      return result.append("DocPullquote");
    case ax::mojom::Role::kDocQna:
      return result.append("DocQna");
    case ax::mojom::Role::kDocSubtitle:
      return result.append("DocSubtitle");
    case ax::mojom::Role::kDocTip:
      return result.append("DocTip");
    case ax::mojom::Role::kDocToc:
      return result.append("DocToc");
    case ax::mojom::Role::kDocument:
      return result.append("Document");
    case ax::mojom::Role::kEmbeddedObject:
      return result.append("EmbeddedObject");
    case ax::mojom::Role::kFigcaption:
      return result.append("Figcaption");
    case ax::mojom::Role::kFigure:
      return result.append("Figure");
    case ax::mojom::Role::kFooter:
      return result.append("Footer");
    case ax::mojom::Role::kForm:
      return result.append("Form");
    case ax::mojom::Role::kGenericContainer:
      return result.append("GenericContainer");
    case ax::mojom::Role::kGraphicsDocument:
      return result.append("GraphicsDocument");
    case ax::mojom::Role::kGraphicsObject:
      return result.append("GraphicsObject");
    case ax::mojom::Role::kGraphicsSymbol:
      return result.append("GraphicsSymbol");
    case ax::mojom::Role::kGrid:
      return result.append("Grid");
    case ax::mojom::Role::kGroup:
      return result.append("Group");
    case ax::mojom::Role::kHeading:
      return result.append("Heading");
    case ax::mojom::Role::kIgnored:
      return result.append("Ignored");
    case ax::mojom::Role::kImageMap:
      return result.append("ImageMap");
    case ax::mojom::Role::kImage:
      return result.append("Image");
    case ax::mojom::Role::kInlineTextBox:
      return result.append("InlineTextBox");
    case ax::mojom::Role::kInputTime:
      return result.append("InputTime");
    case ax::mojom::Role::kLabelText:
      return result.append("Label");
    case ax::mojom::Role::kLayoutTable:
      return result.append("LayoutTable");
    case ax::mojom::Role::kLayoutTableCell:
      return result.append("LayoutTableCell");
    case ax::mojom::Role::kLayoutTableColumn:
      return result.append("LayoutTableColumn");
    case ax::mojom::Role::kLayoutTableRow:
      return result.append("LayoutTableRow");
    case ax::mojom::Role::kLegend:
      return result.append("Legend");
    case ax::mojom::Role::kLink:
      return result.append("Link");
    case ax::mojom::Role::kLineBreak:
      return result.append("LineBreak");
    case ax::mojom::Role::kListBoxOption:
      return result.append("ListBoxOption");
    case ax::mojom::Role::kListBox:
      return result.append("ListBox");
    case ax::mojom::Role::kListItem:
      return result.append("ListItem");
    case ax::mojom::Role::kListMarker:
      return result.append("ListMarker");
    case ax::mojom::Role::kList:
      return result.append("List");
    case ax::mojom::Role::kLog:
      return result.append("Log");
    case ax::mojom::Role::kMain:
      return result.append("Main");
    case ax::mojom::Role::kMark:
      return result.append("Mark");
    case ax::mojom::Role::kMarquee:
      return result.append("Marquee");
    case ax::mojom::Role::kMath:
      return result.append("Math");
    case ax::mojom::Role::kMenuBar:
      return result.append("MenuBar");
    case ax::mojom::Role::kMenuButton:
      return result.append("MenuButton");
    case ax::mojom::Role::kMenuItem:
      return result.append("MenuItem");
    case ax::mojom::Role::kMenuItemCheckBox:
      return result.append("MenuItemCheckBox");
    case ax::mojom::Role::kMenuItemRadio:
      return result.append("MenuItemRadio");
    case ax::mojom::Role::kMenuListOption:
      return result.append("MenuListOption");
    case ax::mojom::Role::kMenuListPopup:
      return result.append("MenuListPopup");
    case ax::mojom::Role::kMenu:
      return result.append("Menu");
    case ax::mojom::Role::kMeter:
      return result.append("Meter");
    case ax::mojom::Role::kNavigation:
      return result.append("Navigation");
    case ax::mojom::Role::kNone:
      return result.append("None");
    case ax::mojom::Role::kNote:
      return result.append("Note");
    case ax::mojom::Role::kParagraph:
      return result.append("Paragraph");
    case ax::mojom::Role::kPopUpButton:
      return result.append("PopUpButton");
    case ax::mojom::Role::kPre:
      return result.append("Pre");
    case ax::mojom::Role::kPresentational:
      return result.append("Presentational");
    case ax::mojom::Role::kProgressIndicator:
      return result.append("ProgressIndicator");
    case ax::mojom::Role::kRadioButton:
      return result.append("RadioButton");
    case ax::mojom::Role::kRadioGroup:
      return result.append("RadioGroup");
    case ax::mojom::Role::kRegion:
      return result.append("Region");
    case ax::mojom::Role::kRowHeader:
      return result.append("RowHeader");
    case ax::mojom::Role::kRow:
      return result.append("Row");
    case ax::mojom::Role::kRuby:
      return result.append("Ruby");
    case ax::mojom::Role::kSvgRoot:
      return result.append("SVGRoot");
    case ax::mojom::Role::kScrollBar:
      return result.append("ScrollBar");
    case ax::mojom::Role::kSearch:
      return result.append("Search");
    case ax::mojom::Role::kSearchBox:
      return result.append("SearchBox");
    case ax::mojom::Role::kSlider:
      return result.append("Slider");
    case ax::mojom::Role::kSliderThumb:
      return result.append("SliderThumb");
    case ax::mojom::Role::kSpinButton:
      return result.append("SpinButton");
    case ax::mojom::Role::kSplitter:
      return result.append("Splitter");
    case ax::mojom::Role::kStaticText:
      return result.append("StaticText");
    case ax::mojom::Role::kStatus:
      return result.append("Status");
    case ax::mojom::Role::kSwitch:
      return result.append("Switch");
    case ax::mojom::Role::kTabList:
      return result.append("TabList");
    case ax::mojom::Role::kTabPanel:
      return result.append("TabPanel");
    case ax::mojom::Role::kTab:
      return result.append("Tab");
    case ax::mojom::Role::kTableHeaderContainer:
      return result.append("TableHeaderContainer");
    case ax::mojom::Role::kTable:
      return result.append("Table");
    case ax::mojom::Role::kTextField:
      return result.append("TextField");
    case ax::mojom::Role::kTextFieldWithComboBox:
      return result.append("TextFieldWithComboBox");
    case ax::mojom::Role::kTime:
      return result.append("Time");
    case ax::mojom::Role::kTimer:
      return result.append("Timer");
    case ax::mojom::Role::kToggleButton:
      return result.append("ToggleButton");
    case ax::mojom::Role::kToolbar:
      return result.append("Toolbar");
    case ax::mojom::Role::kTreeGrid:
      return result.append("TreeGrid");
    case ax::mojom::Role::kTreeItem:
      return result.append("TreeItem");
    case ax::mojom::Role::kTree:
      return result.append("Tree");
    case ax::mojom::Role::kUnknown:
      return result.append("Unknown");
    case ax::mojom::Role::kTooltip:
      return result.append("UserInterfaceTooltip");
    case ax::mojom::Role::kVideo:
      return result.append("Video");
    case ax::mojom::Role::kRootWebArea:
      return result.append("WebArea");
    default:
      return result.append("Unknown");
  }
}

std::string GetStringValue(const blink::WebAXObject& object) {
  std::string value;
  if (object.Role() == ax::mojom::Role::kColorWell) {
    unsigned int color = object.ColorValue();
    unsigned int red = (color >> 16) & 0xFF;
    unsigned int green = (color >> 8) & 0xFF;
    unsigned int blue = color & 0xFF;
    value = base::StringPrintf("rgba(%d, %d, %d, 1)", red, green, blue);
  } else {
    value = object.StringValue().Utf8();
  }
  return value.insert(0, "AXValue: ");
}

std::string GetRole(const blink::WebAXObject& object) {
  std::string role_string = RoleToString(object.Role());

  // Special-case canvas with fallback content because Chromium wants to treat
  // this as essentially a separate role that it can map differently depending
  // on the platform.
  if (object.Role() == ax::mojom::Role::kCanvas &&
      object.CanvasHasFallbackContent()) {
    role_string += "WithFallbackContent";
  }

  return role_string;
}

std::string GetValueDescription(const blink::WebAXObject& object) {
  std::string value_description = object.ValueDescription().Utf8();
  return value_description.insert(0, "AXValueDescription: ");
}

std::string GetLanguage(const blink::WebAXObject& object) {
  std::string language = object.Language().Utf8();
  return language.insert(0, "AXLanguage: ");
}

std::string GetAttributes(const blink::WebAXObject& object) {
  std::string attributes(object.GetName().Utf8());
  attributes.append("\n");
  attributes.append(GetRole(object));
  return attributes;
}

// New bounds calculation algorithm.  Retrieves the frame-relative bounds
// of an object by calling getRelativeBounds and then applying the offsets
// and transforms recursively on each container of this object.
blink::WebFloatRect BoundsForObject(const blink::WebAXObject& object) {
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  object.GetRelativeBounds(container, bounds, matrix);
  gfx::RectF computedBounds(0, 0, bounds.width, bounds.height);
  while (!container.IsDetached()) {
    computedBounds.Offset(bounds.x, bounds.y);
    computedBounds.Offset(-container.GetScrollOffset().x,
                          -container.GetScrollOffset().y);
    if (!matrix.isIdentity()) {
      gfx::Transform transform(matrix);
      transform.TransformRect(&computedBounds);
    }
    container.GetRelativeBounds(container, bounds, matrix);
  }
  return blink::WebFloatRect(computedBounds.x(), computedBounds.y(),
                             computedBounds.width(), computedBounds.height());
}

blink::WebRect BoundsForCharacter(const blink::WebAXObject& object,
                                  int characterIndex) {
  DCHECK_EQ(object.Role(), ax::mojom::Role::kStaticText);
  int end = 0;
  for (unsigned i = 0; i < object.ChildCount(); i++) {
    blink::WebAXObject inline_text_box = object.ChildAt(i);
    DCHECK_EQ(inline_text_box.Role(), ax::mojom::Role::kInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.GetName();
    end += name.length();
    if (characterIndex < start || characterIndex >= end)
      continue;

    blink::WebFloatRect inline_text_box_rect = BoundsForObject(inline_text_box);

    int localIndex = characterIndex - start;
    blink::WebVector<int> character_offsets;
    inline_text_box.CharacterOffsets(character_offsets);
    if (character_offsets.size() != name.length())
      return blink::WebRect();

    switch (inline_text_box.GetTextDirection()) {
      case ax::mojom::TextDirection::kLtr: {
        if (localIndex) {
          int left = inline_text_box_rect.x + character_offsets[localIndex - 1];
          int width =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y, width,
                                inline_text_box_rect.height);
        }
        return blink::WebRect(inline_text_box_rect.x, inline_text_box_rect.y,
                              character_offsets[0],
                              inline_text_box_rect.height);
      }
      case ax::mojom::TextDirection::kRtl: {
        int right = inline_text_box_rect.x + inline_text_box_rect.width;

        if (localIndex) {
          int left = right - character_offsets[localIndex];
          int width =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(left, inline_text_box_rect.y, width,
                                inline_text_box_rect.height);
        }
        int left = right - character_offsets[0];
        return blink::WebRect(left, inline_text_box_rect.y,
                              character_offsets[0],
                              inline_text_box_rect.height);
      }
      case ax::mojom::TextDirection::kTtb: {
        if (localIndex) {
          int top = inline_text_box_rect.y + character_offsets[localIndex - 1];
          int height =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(inline_text_box_rect.x, top,
                                inline_text_box_rect.width, height);
        }
        return blink::WebRect(inline_text_box_rect.x, inline_text_box_rect.y,
                              inline_text_box_rect.width, character_offsets[0]);
      }
      case ax::mojom::TextDirection::kBtt: {
        int bottom = inline_text_box_rect.y + inline_text_box_rect.height;

        if (localIndex) {
          int top = bottom - character_offsets[localIndex];
          int height =
              character_offsets[localIndex] - character_offsets[localIndex - 1];
          return blink::WebRect(inline_text_box_rect.x, top,
                                inline_text_box_rect.width, height);
        }
        int top = bottom - character_offsets[0];
        return blink::WebRect(inline_text_box_rect.x, top,
                              inline_text_box_rect.width, character_offsets[0]);
      }
      default:
        NOTREACHED();
        return blink::WebRect();
    }
  }

  DCHECK(false);
  return blink::WebRect();
}

std::vector<std::string> GetMisspellings(blink::WebAXObject& object) {
  std::vector<std::string> misspellings;
  std::string text(object.GetName().Utf8());

  blink::WebVector<ax::mojom::MarkerType> marker_types;
  blink::WebVector<int> marker_starts;
  blink::WebVector<int> marker_ends;
  object.Markers(marker_types, marker_starts, marker_ends);
  DCHECK_EQ(marker_types.size(), marker_starts.size());
  DCHECK_EQ(marker_starts.size(), marker_ends.size());

  for (size_t i = 0; i < marker_types.size(); ++i) {
    if (marker_types[i] == ax::mojom::MarkerType::kSpelling) {
      misspellings.push_back(
          text.substr(marker_starts[i], marker_ends[i] - marker_starts[i]));
    }
  }

  return misspellings;
}

void GetBoundariesForOneWord(const blink::WebAXObject& object,
                             int character_index,
                             int& word_start,
                             int& word_end) {
  int end = 0;
  for (size_t i = 0; i < object.ChildCount(); i++) {
    blink::WebAXObject inline_text_box = object.ChildAt(i);
    DCHECK_EQ(inline_text_box.Role(), ax::mojom::Role::kInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.GetName();
    end += name.length();
    if (end <= character_index)
      continue;
    int localIndex = character_index - start;

    blink::WebVector<int> starts;
    blink::WebVector<int> ends;
    inline_text_box.GetWordBoundaries(starts, ends);
    size_t word_count = starts.size();
    DCHECK_EQ(ends.size(), word_count);

    // If there are no words, use the InlineTextBox boundaries.
    if (!word_count) {
      word_start = start;
      word_end = end;
      return;
    }

    // Look for a character within any word other than the last.
    for (size_t j = 0; j < word_count - 1; j++) {
      if (localIndex <= ends[j]) {
        word_start = start + starts[j];
        word_end = start + ends[j];
        return;
      }
    }

    // Return the last word by default.
    word_start = start + starts[word_count - 1];
    word_end = start + ends[word_count - 1];
    return;
  }
}

// Collects attributes into a string, delimited by dashes. Used by all methods
// that output lists of attributes: attributesOfLinkedUIElementsCallback,
// AttributesOfChildrenCallback, etc.
class AttributesCollector {
 public:
  AttributesCollector() {}
  ~AttributesCollector() {}

  void CollectAttributes(const blink::WebAXObject& object) {
    attributes_.append("\n------------\n");
    attributes_.append(GetAttributes(object));
  }

  std::string attributes() const { return attributes_; }

 private:
  std::string attributes_;

  DISALLOW_COPY_AND_ASSIGN(AttributesCollector);
};

class SparseAttributeAdapter : public blink::WebAXSparseAttributeClient {
 public:
  SparseAttributeAdapter() {}
  ~SparseAttributeAdapter() override {}

  std::map<blink::WebAXBoolAttribute, bool> bool_attributes;
  std::map<blink::WebAXStringAttribute, blink::WebString> string_attributes;
  std::map<blink::WebAXObjectAttribute, blink::WebAXObject> object_attributes;
  std::map<blink::WebAXObjectVectorAttribute,
           blink::WebVector<blink::WebAXObject>>
      object_vector_attributes;

 private:
  void AddBoolAttribute(blink::WebAXBoolAttribute attribute,
                        bool value) override {
    bool_attributes[attribute] = value;
  }

  void AddStringAttribute(blink::WebAXStringAttribute attribute,
                          const blink::WebString& value) override {
    string_attributes[attribute] = value;
  }

  void AddObjectAttribute(blink::WebAXObjectAttribute attribute,
                          const blink::WebAXObject& value) override {
    object_attributes[attribute] = value;
  }

  void AddObjectVectorAttribute(
      blink::WebAXObjectVectorAttribute attribute,
      const blink::WebVector<blink::WebAXObject>& value) override {
    object_vector_attributes[attribute] = value;
  }
};

}  // namespace

gin::WrapperInfo WebAXObjectProxy::kWrapperInfo = {gin::kEmbedderNativeGin};

WebAXObjectProxy::WebAXObjectProxy(const blink::WebAXObject& object,
                                   WebAXObjectProxy::Factory* factory)
    : accessibility_object_(object), factory_(factory) {}

WebAXObjectProxy::~WebAXObjectProxy() {}

gin::ObjectTemplateBuilder WebAXObjectProxy::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  return gin::Wrappable<WebAXObjectProxy>::GetObjectTemplateBuilder(isolate)
      .SetProperty("role", &WebAXObjectProxy::Role)
      .SetProperty("stringValue", &WebAXObjectProxy::StringValue)
      .SetProperty("language", &WebAXObjectProxy::Language)
      .SetProperty("x", &WebAXObjectProxy::X)
      .SetProperty("y", &WebAXObjectProxy::Y)
      .SetProperty("width", &WebAXObjectProxy::Width)
      .SetProperty("height", &WebAXObjectProxy::Height)
      .SetProperty("inPageLinkTarget", &WebAXObjectProxy::InPageLinkTarget)
      .SetProperty("intValue", &WebAXObjectProxy::IntValue)
      .SetProperty("minValue", &WebAXObjectProxy::MinValue)
      .SetProperty("maxValue", &WebAXObjectProxy::MaxValue)
      .SetProperty("stepValue", &WebAXObjectProxy::StepValue)
      .SetProperty("valueDescription", &WebAXObjectProxy::ValueDescription)
      .SetProperty("childrenCount", &WebAXObjectProxy::ChildrenCount)
      .SetProperty("selectionAnchorObject",
                   &WebAXObjectProxy::SelectionAnchorObject)
      .SetProperty("selectionAnchorOffset",
                   &WebAXObjectProxy::SelectionAnchorOffset)
      .SetProperty("selectionAnchorAffinity",
                   &WebAXObjectProxy::SelectionAnchorAffinity)
      .SetProperty("selectionFocusObject",
                   &WebAXObjectProxy::SelectionFocusObject)
      .SetProperty("selectionFocusOffset",
                   &WebAXObjectProxy::SelectionFocusOffset)
      .SetProperty("selectionFocusAffinity",
                   &WebAXObjectProxy::SelectionFocusAffinity)
      .SetProperty("selectionStart", &WebAXObjectProxy::SelectionStart)
      .SetProperty("selectionEnd", &WebAXObjectProxy::SelectionEnd)
      .SetProperty("selectionStartLineNumber",
                   &WebAXObjectProxy::SelectionStartLineNumber)
      .SetProperty("selectionEndLineNumber",
                   &WebAXObjectProxy::SelectionEndLineNumber)
      .SetProperty("isAtomic", &WebAXObjectProxy::IsAtomic)
      .SetProperty("isAutofillAvailable",
                   &WebAXObjectProxy::IsAutofillAvailable)
      .SetProperty("isBusy", &WebAXObjectProxy::IsBusy)
      .SetProperty("isRequired", &WebAXObjectProxy::IsRequired)
      .SetProperty("isEditable", &WebAXObjectProxy::IsEditable)
      .SetProperty("isEditableRoot", &WebAXObjectProxy::IsEditableRoot)
      .SetProperty("isRichlyEditable", &WebAXObjectProxy::IsRichlyEditable)
      .SetProperty("isFocused", &WebAXObjectProxy::IsFocused)
      .SetProperty("isFocusable", &WebAXObjectProxy::IsFocusable)
      .SetProperty("isModal", &WebAXObjectProxy::IsModal)
      .SetProperty("isSelected", &WebAXObjectProxy::IsSelected)
      .SetProperty("isSelectable", &WebAXObjectProxy::IsSelectable)
      .SetProperty("isMultiLine", &WebAXObjectProxy::IsMultiLine)
      .SetProperty("isMultiSelectable", &WebAXObjectProxy::IsMultiSelectable)
      .SetProperty("isSelectedOptionActive",
                   &WebAXObjectProxy::IsSelectedOptionActive)
      .SetProperty("isExpanded", &WebAXObjectProxy::IsExpanded)
      .SetProperty("checked", &WebAXObjectProxy::Checked)
      .SetProperty("isVisible", &WebAXObjectProxy::IsVisible)
      .SetProperty("isOffScreen", &WebAXObjectProxy::IsOffScreen)
      .SetProperty("isCollapsed", &WebAXObjectProxy::IsCollapsed)
      .SetProperty("hasPopup", &WebAXObjectProxy::HasPopup)
      .SetProperty("isValid", &WebAXObjectProxy::IsValid)
      .SetProperty("isReadOnly", &WebAXObjectProxy::IsReadOnly)
      .SetProperty("restriction", &WebAXObjectProxy::Restriction)
      .SetProperty("activeDescendant", &WebAXObjectProxy::ActiveDescendant)
      .SetProperty("backgroundColor", &WebAXObjectProxy::BackgroundColor)
      .SetProperty("color", &WebAXObjectProxy::Color)
      .SetProperty("colorValue", &WebAXObjectProxy::ColorValue)
      .SetProperty("fontFamily", &WebAXObjectProxy::FontFamily)
      .SetProperty("fontSize", &WebAXObjectProxy::FontSize)
      .SetProperty("autocomplete", &WebAXObjectProxy::Autocomplete)
      .SetProperty("current", &WebAXObjectProxy::Current)
      .SetProperty("invalid", &WebAXObjectProxy::Invalid)
      .SetProperty("keyShortcuts", &WebAXObjectProxy::KeyShortcuts)
      .SetProperty("live", &WebAXObjectProxy::Live)
      .SetProperty("orientation", &WebAXObjectProxy::Orientation)
      .SetProperty("relevant", &WebAXObjectProxy::Relevant)
      .SetProperty("roleDescription", &WebAXObjectProxy::RoleDescription)
      .SetProperty("sort", &WebAXObjectProxy::Sort)
      .SetProperty("hierarchicalLevel", &WebAXObjectProxy::HierarchicalLevel)
      .SetProperty("posInSet", &WebAXObjectProxy::PosInSet)
      .SetProperty("setSize", &WebAXObjectProxy::SetSize)
      .SetProperty("clickPointX", &WebAXObjectProxy::ClickPointX)
      .SetProperty("clickPointY", &WebAXObjectProxy::ClickPointY)
      .SetProperty("rowCount", &WebAXObjectProxy::RowCount)
      .SetProperty("rowHeadersCount", &WebAXObjectProxy::RowHeadersCount)
      .SetProperty("columnCount", &WebAXObjectProxy::ColumnCount)
      .SetProperty("columnHeadersCount", &WebAXObjectProxy::ColumnHeadersCount)
      .SetProperty("isClickable", &WebAXObjectProxy::IsClickable)
      //
      // NEW bounding rect calculation - high-level interface
      //
      .SetProperty("boundsX", &WebAXObjectProxy::BoundsX)
      .SetProperty("boundsY", &WebAXObjectProxy::BoundsY)
      .SetProperty("boundsWidth", &WebAXObjectProxy::BoundsWidth)
      .SetProperty("boundsHeight", &WebAXObjectProxy::BoundsHeight)
      .SetMethod("allAttributes", &WebAXObjectProxy::AllAttributes)
      .SetMethod("attributesOfChildren",
                 &WebAXObjectProxy::AttributesOfChildren)
      .SetMethod("ariaActiveDescendantElement",
                 &WebAXObjectProxy::AriaActiveDescendantElement)
      .SetMethod("ariaControlsElementAtIndex",
                 &WebAXObjectProxy::AriaControlsElementAtIndex)
      .SetMethod("ariaDetailsElement", &WebAXObjectProxy::AriaDetailsElement)
      .SetMethod("ariaErrorMessageElement",
                 &WebAXObjectProxy::AriaErrorMessageElement)
      .SetMethod("ariaFlowToElementAtIndex",
                 &WebAXObjectProxy::AriaFlowToElementAtIndex)
      .SetMethod("ariaOwnsElementAtIndex",
                 &WebAXObjectProxy::AriaOwnsElementAtIndex)
      .SetMethod("lineForIndex", &WebAXObjectProxy::LineForIndex)
      .SetMethod("boundsForRange", &WebAXObjectProxy::BoundsForRange)
      .SetMethod("childAtIndex", &WebAXObjectProxy::ChildAtIndex)
      .SetMethod("elementAtPoint", &WebAXObjectProxy::ElementAtPoint)
      .SetMethod("rowHeaderAtIndex", &WebAXObjectProxy::RowHeaderAtIndex)
      .SetMethod("columnHeaderAtIndex", &WebAXObjectProxy::ColumnHeaderAtIndex)
      .SetMethod("rowIndexRange", &WebAXObjectProxy::RowIndexRange)
      .SetMethod("columnIndexRange", &WebAXObjectProxy::ColumnIndexRange)
      .SetMethod("cellForColumnAndRow", &WebAXObjectProxy::CellForColumnAndRow)
      .SetMethod("setSelectedTextRange",
                 &WebAXObjectProxy::SetSelectedTextRange)
      .SetMethod("setSelection", &WebAXObjectProxy::SetSelection)
      .SetMethod("isAttributeSettable", &WebAXObjectProxy::IsAttributeSettable)
      .SetMethod("isPressActionSupported",
                 &WebAXObjectProxy::IsPressActionSupported)
      .SetMethod("parentElement", &WebAXObjectProxy::ParentElement)
      .SetMethod("increment", &WebAXObjectProxy::Increment)
      .SetMethod("decrement", &WebAXObjectProxy::Decrement)
      .SetMethod("showMenu", &WebAXObjectProxy::ShowMenu)
      .SetMethod("press", &WebAXObjectProxy::Press)
      .SetMethod("setValue", &WebAXObjectProxy::SetValue)
      .SetMethod("isEqual", &WebAXObjectProxy::IsEqual)
      .SetMethod("setNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("unsetNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      .SetMethod("takeFocus", &WebAXObjectProxy::TakeFocus)
      .SetMethod("scrollToMakeVisible", &WebAXObjectProxy::ScrollToMakeVisible)
      .SetMethod("scrollToMakeVisibleWithSubFocus",
                 &WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus)
      .SetMethod("scrollToGlobalPoint", &WebAXObjectProxy::ScrollToGlobalPoint)
      .SetMethod("scrollX", &WebAXObjectProxy::ScrollX)
      .SetMethod("scrollY", &WebAXObjectProxy::ScrollY)
      .SetMethod("wordStart", &WebAXObjectProxy::WordStart)
      .SetMethod("wordEnd", &WebAXObjectProxy::WordEnd)
      .SetMethod("nextOnLine", &WebAXObjectProxy::NextOnLine)
      .SetMethod("previousOnLine", &WebAXObjectProxy::PreviousOnLine)
      .SetMethod("misspellingAtIndex", &WebAXObjectProxy::MisspellingAtIndex)
      // TODO(hajimehoshi): This is for backward compatibility. Remove them.
      .SetMethod("addNotificationListener",
                 &WebAXObjectProxy::SetNotificationListener)
      .SetMethod("removeNotificationListener",
                 &WebAXObjectProxy::UnsetNotificationListener)
      //
      // NEW accessible name and description accessors
      //
      .SetProperty("name", &WebAXObjectProxy::Name)
      .SetProperty("nameFrom", &WebAXObjectProxy::NameFrom)
      .SetMethod("nameElementCount", &WebAXObjectProxy::NameElementCount)
      .SetMethod("nameElementAtIndex", &WebAXObjectProxy::NameElementAtIndex)
      .SetProperty("description", &WebAXObjectProxy::Description)
      .SetProperty("descriptionFrom", &WebAXObjectProxy::DescriptionFrom)
      .SetProperty("placeholder", &WebAXObjectProxy::Placeholder)
      .SetProperty("misspellingsCount", &WebAXObjectProxy::MisspellingsCount)
      .SetMethod("descriptionElementCount",
                 &WebAXObjectProxy::DescriptionElementCount)
      .SetMethod("descriptionElementAtIndex",
                 &WebAXObjectProxy::DescriptionElementAtIndex)
      //
      // NEW bounding rect calculation - low-level interface
      //
      .SetMethod("offsetContainer", &WebAXObjectProxy::OffsetContainer)
      .SetMethod("boundsInContainerX", &WebAXObjectProxy::BoundsInContainerX)
      .SetMethod("boundsInContainerY", &WebAXObjectProxy::BoundsInContainerY)
      .SetMethod("boundsInContainerWidth",
                 &WebAXObjectProxy::BoundsInContainerWidth)
      .SetMethod("boundsInContainerHeight",
                 &WebAXObjectProxy::BoundsInContainerHeight)
      .SetMethod("hasNonIdentityTransform",
                 &WebAXObjectProxy::HasNonIdentityTransform);
}

v8::Local<v8::Object> WebAXObjectProxy::GetChildAtIndex(unsigned index) {
  return factory_->GetOrCreate(accessibility_object_.ChildAt(index));
}

bool WebAXObjectProxy::IsRoot() const {
  return false;
}

bool WebAXObjectProxy::IsEqualToObject(const blink::WebAXObject& other) {
  return accessibility_object_.Equals(other);
}

void WebAXObjectProxy::NotificationReceived(
    blink::WebLocalFrame* frame,
    const std::string& notification_name) {
  if (notification_callback_.IsEmpty())
    return;

  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();

  v8::Local<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, notification_name.data(),
                              v8::String::kNormalString,
                              notification_name.size()),
  };
  frame->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, notification_callback_),
      context->Global(), arraysize(argv), argv);
}

void WebAXObjectProxy::Reset() {
  notification_callback_.Reset();
}

std::string WebAXObjectProxy::Role() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetRole(accessibility_object_);
}

std::string WebAXObjectProxy::StringValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetStringValue(accessibility_object_);
}

std::string WebAXObjectProxy::Language() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetLanguage(accessibility_object_);
}

int WebAXObjectProxy::X() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).x;
}

int WebAXObjectProxy::Y() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).y;
}

int WebAXObjectProxy::Width() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).width;
}

int WebAXObjectProxy::Height() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).height;
}

v8::Local<v8::Value> WebAXObjectProxy::InPageLinkTarget() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject target = accessibility_object_.InPageLinkTarget();
  if (target.IsNull())
    return v8::Null(blink::MainThreadIsolate());
  return factory_->GetOrCreate(target);
}

int WebAXObjectProxy::IntValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  if (accessibility_object_.SupportsRangeValue()) {
    float value = 0.0f;
    accessibility_object_.ValueForRange(&value);
    return static_cast<int>(value);
  } else if (accessibility_object_.Role() == ax::mojom::Role::kHeading) {
    return accessibility_object_.HeadingLevel();
  } else {
    return atoi(accessibility_object_.StringValue().Utf8().data());
  }
}

int WebAXObjectProxy::MinValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  float min_value = 0.0f;
  accessibility_object_.MinValueForRange(&min_value);
  return min_value;
}

int WebAXObjectProxy::MaxValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  float max_value = 0.0f;
  accessibility_object_.MaxValueForRange(&max_value);
  return max_value;
}

int WebAXObjectProxy::StepValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  float step_value = 0.0f;
  accessibility_object_.StepValueForRange(&step_value);
  return step_value;
}

std::string WebAXObjectProxy::ValueDescription() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetValueDescription(accessibility_object_);
}

int WebAXObjectProxy::ChildrenCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  int count = 1;  // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = accessibility_object_.ChildCount();
  return count;
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionAnchorObject() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (anchorObject.IsNull())
    return v8::Null(blink::MainThreadIsolate());

  return factory_->GetOrCreate(anchorObject);
}

int WebAXObjectProxy::SelectionAnchorOffset() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (anchorOffset < 0)
    return -1;

  return anchorOffset;
}

std::string WebAXObjectProxy::SelectionAnchorAffinity() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  return anchorAffinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                              : "downstream";
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionFocusObject() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (focusObject.IsNull())
    return v8::Null(blink::MainThreadIsolate());

  return factory_->GetOrCreate(focusObject);
}

int WebAXObjectProxy::SelectionFocusOffset() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  if (focusOffset < 0)
    return -1;

  return focusOffset;
}

std::string WebAXObjectProxy::SelectionFocusAffinity() {
  accessibility_object_.UpdateLayoutAndCheckValidity();

  blink::WebAXObject anchorObject;
  int anchorOffset = -1;
  ax::mojom::TextAffinity anchorAffinity;
  blink::WebAXObject focusObject;
  int focusOffset = -1;
  ax::mojom::TextAffinity focusAffinity;
  accessibility_object_.Selection(anchorObject, anchorOffset, anchorAffinity,
                                  focusObject, focusOffset, focusAffinity);
  return focusAffinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                             : "downstream";
}

int WebAXObjectProxy::SelectionStart() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SelectionStart();
}

int WebAXObjectProxy::SelectionEnd() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SelectionEnd();
}

int WebAXObjectProxy::SelectionStartLineNumber() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SelectionStartLineNumber();
}

int WebAXObjectProxy::SelectionEndLineNumber() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SelectionEndLineNumber();
}

bool WebAXObjectProxy::IsAtomic() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.LiveRegionAtomic();
}

bool WebAXObjectProxy::IsAutofillAvailable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsAutofillAvailable();
}

bool WebAXObjectProxy::IsBusy() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .bool_attributes[blink::WebAXBoolAttribute::kAriaBusy];
}

std::string WebAXObjectProxy::Restriction() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.Restriction()) {
    case blink::kWebAXRestrictionReadOnly:
      return "readOnly";
    case blink::kWebAXRestrictionDisabled:
      return "disabled";
    case blink::kWebAXRestrictionNone:
      break;
  }
  return "none";
}

bool WebAXObjectProxy::IsRequired() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsRequired();
}

bool WebAXObjectProxy::IsEditableRoot() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsEditableRoot();
}

bool WebAXObjectProxy::IsEditable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsEditable();
}

bool WebAXObjectProxy::IsRichlyEditable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsRichlyEditable();
}

bool WebAXObjectProxy::IsFocused() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsFocused();
}

bool WebAXObjectProxy::IsFocusable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.CanSetFocusAttribute();
}

bool WebAXObjectProxy::IsModal() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsModal();
}

bool WebAXObjectProxy::IsSelected() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsSelected() == blink::kWebAXSelectedStateTrue;
}

bool WebAXObjectProxy::IsSelectable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsSelected() !=
         blink::kWebAXSelectedStateUndefined;
}

bool WebAXObjectProxy::IsMultiLine() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsMultiline();
}

bool WebAXObjectProxy::IsMultiSelectable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsMultiSelectable();
}

bool WebAXObjectProxy::IsSelectedOptionActive() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsSelectedOptionActive();
}

bool WebAXObjectProxy::IsExpanded() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsExpanded() == blink::kWebAXExpandedExpanded;
}

std::string WebAXObjectProxy::Checked() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.CheckedState()) {
    case ax::mojom::CheckedState::kTrue:
      return "true";
    case ax::mojom::CheckedState::kMixed:
      return "mixed";
    case ax::mojom::CheckedState::kFalse:
      return "false";
    default:
      return std::string();
  }
}

bool WebAXObjectProxy::IsCollapsed() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsExpanded() == blink::kWebAXExpandedCollapsed;
}

bool WebAXObjectProxy::IsVisible() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsVisible();
}

bool WebAXObjectProxy::IsOffScreen() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsOffScreen();
}

bool WebAXObjectProxy::IsValid() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return !accessibility_object_.IsDetached();
}

bool WebAXObjectProxy::IsReadOnly() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.Restriction() ==
         blink::kWebAXRestrictionReadOnly;
}

v8::Local<v8::Object> WebAXObjectProxy::ActiveDescendant() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject element = accessibility_object_.AriaActiveDescendant();
  return factory_->GetOrCreate(element);
}

unsigned int WebAXObjectProxy::BackgroundColor() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.BackgroundColor();
}

unsigned int WebAXObjectProxy::Color() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  unsigned int color = accessibility_object_.GetColor();
  // Remove the alpha because it's always 1 and thus not informative.
  return color & 0xFFFFFF;
}

// For input elements of type color.
unsigned int WebAXObjectProxy::ColorValue() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.ColorValue();
}

std::string WebAXObjectProxy::FontFamily() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  std::string font_family(accessibility_object_.FontFamily().Utf8());
  return font_family.insert(0, "AXFontFamily: ");
}

float WebAXObjectProxy::FontSize() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.FontSize();
}

std::string WebAXObjectProxy::Autocomplete() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.AriaAutoComplete().Utf8();
}

std::string WebAXObjectProxy::Current() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.AriaCurrentState()) {
    case ax::mojom::AriaCurrentState::kFalse:
      return "false";
    case ax::mojom::AriaCurrentState::kTrue:
      return "true";
    case ax::mojom::AriaCurrentState::kPage:
      return "page";
    case ax::mojom::AriaCurrentState::kStep:
      return "step";
    case ax::mojom::AriaCurrentState::kLocation:
      return "location";
    case ax::mojom::AriaCurrentState::kDate:
      return "date";
    case ax::mojom::AriaCurrentState::kTime:
      return "time";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::HasPopup() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.HasPopup()) {
    case ax::mojom::HasPopup::kTrue:
      return "true";
    case ax::mojom::HasPopup::kMenu:
      return "menu";
    case ax::mojom::HasPopup::kListbox:
      return "listbox";
    case ax::mojom::HasPopup::kTree:
      return "tree";
    case ax::mojom::HasPopup::kGrid:
      return "grid";
    case ax::mojom::HasPopup::kDialog:
      return "dialog";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::Invalid() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.InvalidState()) {
    case ax::mojom::InvalidState::kFalse:
      return "false";
    case ax::mojom::InvalidState::kTrue:
      return "true";
    case ax::mojom::InvalidState::kSpelling:
      return "spelling";
    case ax::mojom::InvalidState::kGrammar:
      return "grammar";
    case ax::mojom::InvalidState::kOther:
      return "other";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::KeyShortcuts() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .string_attributes[blink::WebAXStringAttribute::kAriaKeyShortcuts]
      .Utf8();
}

std::string WebAXObjectProxy::Live() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.LiveRegionStatus().Utf8();
}

std::string WebAXObjectProxy::Orientation() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (accessibility_object_.Orientation() == blink::kWebAXOrientationVertical)
    return "AXOrientation: AXVerticalOrientation";
  else if (accessibility_object_.Orientation() ==
           blink::kWebAXOrientationHorizontal)
    return "AXOrientation: AXHorizontalOrientation";

  return std::string();
}

std::string WebAXObjectProxy::Relevant() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.LiveRegionRelevant().Utf8();
}

std::string WebAXObjectProxy::RoleDescription() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .string_attributes[blink::WebAXStringAttribute::kAriaRoleDescription]
      .Utf8();
}

std::string WebAXObjectProxy::Sort() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  switch (accessibility_object_.SortDirection()) {
    case ax::mojom::SortDirection::kAscending:
      return "ascending";
    case ax::mojom::SortDirection::kDescending:
      return "descending";
    case ax::mojom::SortDirection::kOther:
      return "other";
    default:
      return std::string();
  }
}

int WebAXObjectProxy::HierarchicalLevel() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.HierarchicalLevel();
}

int WebAXObjectProxy::PosInSet() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.PosInSet();
}

int WebAXObjectProxy::SetSize() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SetSize();
}

int WebAXObjectProxy::ClickPointX() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebFloatRect bounds = BoundsForObject(accessibility_object_);
  return bounds.x + bounds.width / 2;
}

int WebAXObjectProxy::ClickPointY() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebFloatRect bounds = BoundsForObject(accessibility_object_);
  return bounds.y + bounds.height / 2;
}

int32_t WebAXObjectProxy::RowCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.RowCount());
}

int32_t WebAXObjectProxy::RowHeadersCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

int32_t WebAXObjectProxy::ColumnCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return static_cast<int32_t>(accessibility_object_.ColumnCount());
}

int32_t WebAXObjectProxy::ColumnHeadersCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

bool WebAXObjectProxy::IsClickable() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.IsClickable();
}

v8::Local<v8::Object> WebAXObjectProxy::AriaActiveDescendantElement() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebAXObject element =
      attribute_adapter.object_attributes
          [blink::WebAXObjectAttribute::kAriaActiveDescendant];
  return factory_->GetOrCreate(element);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaControlsElementAtIndex(
    unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::kAriaControls];
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaDetailsElement() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebAXObject element =
      attribute_adapter
          .object_attributes[blink::WebAXObjectAttribute::kAriaDetails];
  return factory_->GetOrCreate(element);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaErrorMessageElement() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebAXObject element =
      attribute_adapter
          .object_attributes[blink::WebAXObjectAttribute::kAriaErrorMessage];
  return factory_->GetOrCreate(element);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaFlowToElementAtIndex(
    unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::kAriaFlowTo];
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaOwnsElementAtIndex(unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> elements;
  accessibility_object_.AriaOwns(elements);
  size_t elementCount = elements.size();
  if (index >= elementCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

std::string WebAXObjectProxy::AllAttributes() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetAttributes(accessibility_object_);
}

std::string WebAXObjectProxy::AttributesOfChildren() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  AttributesCollector collector;
  unsigned size = accessibility_object_.ChildCount();
  for (unsigned i = 0; i < size; ++i)
    collector.CollectAttributes(accessibility_object_.ChildAt(i));
  return collector.attributes();
}

int WebAXObjectProxy::LineForIndex(int index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<int> line_breaks;
  accessibility_object_.LineBreaks(line_breaks);
  int line = 0;
  int vector_size = static_cast<int>(line_breaks.size());
  while (line < vector_size && line_breaks[line] <= index)
    line++;
  return line;
}

std::string WebAXObjectProxy::BoundsForRange(int start, int end) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return std::string();

  if (!accessibility_object_.UpdateLayoutAndCheckValidity())
    return std::string();

  int len = end - start;

  // Get the bounds for each character and union them into one large rectangle.
  // This is just for testing so it doesn't need to be efficient.
  blink::WebRect bounds = BoundsForCharacter(accessibility_object_, start);
  for (int i = 1; i < len; i++) {
    blink::WebRect next = BoundsForCharacter(accessibility_object_, start + i);
    int right = std::max(bounds.x + bounds.width, next.x + next.width);
    int bottom = std::max(bounds.y + bounds.height, next.y + next.height);
    bounds.x = std::min(bounds.x, next.x);
    bounds.y = std::min(bounds.y, next.y);
    bounds.width = right - bounds.x;
    bounds.height = bottom - bounds.y;
  }

  return base::StringPrintf("{x: %d, y: %d, width: %d, height: %d}", bounds.x,
                            bounds.y, bounds.width, bounds.height);
}

v8::Local<v8::Object> WebAXObjectProxy::ChildAtIndex(int index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetChildAtIndex(index);
}

v8::Local<v8::Object> WebAXObjectProxy::ElementAtPoint(int x, int y) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebPoint point(x, y);
  blink::WebAXObject obj = accessibility_object_.HitTest(point);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::RowHeaderAtIndex(unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  size_t headerCount = headers.size();
  if (index >= headerCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(headers[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::ColumnHeaderAtIndex(unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  size_t headerCount = headers.size();
  if (index >= headerCount)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(headers[index]);
}

std::string WebAXObjectProxy::RowIndexRange() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  unsigned row_index = accessibility_object_.CellRowIndex();
  unsigned row_span = accessibility_object_.CellRowSpan();
  return base::StringPrintf("{%d, %d}", row_index, row_span);
}

std::string WebAXObjectProxy::ColumnIndexRange() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  unsigned column_index = accessibility_object_.CellColumnIndex();
  unsigned column_span = accessibility_object_.CellColumnSpan();
  return base::StringPrintf("{%d, %d}", column_index, column_span);
}

v8::Local<v8::Object> WebAXObjectProxy::CellForColumnAndRow(int column,
                                                            int row) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject obj =
      accessibility_object_.CellForColumnAndRow(column, row);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

void WebAXObjectProxy::SetSelectedTextRange(int selection_start, int length) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.SetSelection(accessibility_object_, selection_start,
                                     accessibility_object_,
                                     selection_start + length);
}

bool WebAXObjectProxy::SetSelection(v8::Local<v8::Value> anchor_object,
                                    int anchor_offset,
                                    v8::Local<v8::Value> focus_object,
                                    int focus_offset) {
  if (anchor_object.IsEmpty() || focus_object.IsEmpty() ||
      !anchor_object->IsObject() || !focus_object->IsObject() ||
      anchor_offset < 0 || focus_offset < 0) {
    return false;
  }

  WebAXObjectProxy* web_ax_anchor = nullptr;
  if (!gin::ConvertFromV8(blink::MainThreadIsolate(), anchor_object,
                          &web_ax_anchor)) {
    return false;
  }
  DCHECK(web_ax_anchor);

  WebAXObjectProxy* web_ax_focus = nullptr;
  if (!gin::ConvertFromV8(blink::MainThreadIsolate(), focus_object,
                          &web_ax_focus)) {
    return false;
  }
  DCHECK(web_ax_focus);

  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.SetSelection(
      web_ax_anchor->accessibility_object_, anchor_offset,
      web_ax_focus->accessibility_object_, focus_offset);
}

bool WebAXObjectProxy::IsAttributeSettable(const std::string& attribute) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  bool settable = false;
  if (attribute == "AXValue")
    settable = accessibility_object_.CanSetValueAttribute();
  return settable;
}

bool WebAXObjectProxy::IsPressActionSupported() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.CanPress();
}

v8::Local<v8::Object> WebAXObjectProxy::ParentElement() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject parent_object = accessibility_object_.ParentObject();
  while (parent_object.AccessibilityIsIgnored())
    parent_object = parent_object.ParentObject();
  return factory_->GetOrCreate(parent_object);
}

void WebAXObjectProxy::Increment() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.Increment();
}

void WebAXObjectProxy::Decrement() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.Decrement();
}

void WebAXObjectProxy::ShowMenu() {
  accessibility_object_.ShowContextMenu();
}

void WebAXObjectProxy::Press() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.Click();
}

bool WebAXObjectProxy::SetValue(const std::string& value) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (accessibility_object_.Restriction() != blink::kWebAXRestrictionNone ||
      accessibility_object_.StringValue().IsEmpty())
    return false;

  accessibility_object_.SetValue(blink::WebString::FromUTF8(value));
  return true;
}

bool WebAXObjectProxy::IsEqual(v8::Local<v8::Object> proxy) {
  WebAXObjectProxy* unwrapped_proxy = nullptr;
  if (!gin::ConvertFromV8(blink::MainThreadIsolate(), proxy, &unwrapped_proxy))
    return false;
  return unwrapped_proxy->IsEqualToObject(accessibility_object_);
}

void WebAXObjectProxy::SetNotificationListener(
    v8::Local<v8::Function> callback) {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  notification_callback_.Reset(isolate, callback);
}

void WebAXObjectProxy::UnsetNotificationListener() {
  notification_callback_.Reset();
}

void WebAXObjectProxy::TakeFocus() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.Focus();
}

void WebAXObjectProxy::ScrollToMakeVisible() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.ScrollToMakeVisible();
}

void WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus(int x,
                                                       int y,
                                                       int width,
                                                       int height) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.ScrollToMakeVisibleWithSubFocus(
      blink::WebRect(x, y, width, height));
}

void WebAXObjectProxy::ScrollToGlobalPoint(int x, int y) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.ScrollToGlobalPoint(blink::WebPoint(x, y));
}

int WebAXObjectProxy::ScrollX() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.GetScrollOffset().x;
}

int WebAXObjectProxy::ScrollY() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.GetScrollOffset().y;
}

float WebAXObjectProxy::BoundsX() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).x;
}

float WebAXObjectProxy::BoundsY() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).y;
}

float WebAXObjectProxy::BoundsWidth() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).width;
}

float WebAXObjectProxy::BoundsHeight() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return BoundsForObject(accessibility_object_).height;
}

int WebAXObjectProxy::WordStart(int character_index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_start;
}

int WebAXObjectProxy::WordEnd(int character_index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_end;
}

v8::Local<v8::Object> WebAXObjectProxy::NextOnLine() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.NextOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::PreviousOnLine() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject obj = accessibility_object_.PreviousOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

std::string WebAXObjectProxy::MisspellingAtIndex(int index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  if (index < 0 || index >= MisspellingsCount())
    return std::string();
  return GetMisspellings(accessibility_object_)[index];
}

std::string WebAXObjectProxy::Name() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return accessibility_object_.GetName().Utf8();
}

std::string WebAXObjectProxy::NameFrom() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom = ax::mojom::NameFrom::kUninitialized;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  switch (nameFrom) {
    case ax::mojom::NameFrom::kUninitialized:
    case ax::mojom::NameFrom::kNone:
      return "";
    case ax::mojom::NameFrom::kAttribute:
      return "attribute";
    case ax::mojom::NameFrom::kAttributeExplicitlyEmpty:
      return "attributeExplicitlyEmpty";
    case ax::mojom::NameFrom::kCaption:
      return "caption";
    case ax::mojom::NameFrom::kContents:
      return "contents";
    case ax::mojom::NameFrom::kPlaceholder:
      return "placeholder";
    case ax::mojom::NameFrom::kRelatedElement:
      return "relatedElement";
    case ax::mojom::NameFrom::kValue:
      return "value";
    case ax::mojom::NameFrom::kTitle:
      return "title";
  }

  NOTREACHED();
  return std::string();
}

int WebAXObjectProxy::NameElementCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  return static_cast<int>(nameObjects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::NameElementAtIndex(unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  if (index >= nameObjects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(nameObjects[index]);
}

std::string WebAXObjectProxy::Description() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  ax::mojom::DescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  return accessibility_object_
      .Description(nameFrom, descriptionFrom, descriptionObjects)
      .Utf8();
}

std::string WebAXObjectProxy::DescriptionFrom() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  ax::mojom::DescriptionFrom descriptionFrom =
      ax::mojom::DescriptionFrom::kUninitialized;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.Description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  switch (descriptionFrom) {
    case ax::mojom::DescriptionFrom::kUninitialized:
    case ax::mojom::DescriptionFrom::kNone:
      return "";
    case ax::mojom::DescriptionFrom::kAttribute:
      return "attribute";
    case ax::mojom::DescriptionFrom::kContents:
      return "contents";
    case ax::mojom::DescriptionFrom::kPlaceholder:
      return "placeholder";
    case ax::mojom::DescriptionFrom::kRelatedElement:
      return "relatedElement";
  }

  NOTREACHED();
  return std::string();
}

std::string WebAXObjectProxy::Placeholder() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  return accessibility_object_.Placeholder(nameFrom).Utf8();
}

int WebAXObjectProxy::MisspellingsCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  return GetMisspellings(accessibility_object_).size();
}

int WebAXObjectProxy::DescriptionElementCount() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  ax::mojom::DescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.Description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  return static_cast<int>(descriptionObjects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::DescriptionElementAtIndex(
    unsigned index) {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  ax::mojom::NameFrom nameFrom;
  blink::WebVector<blink::WebAXObject> nameObjects;
  accessibility_object_.GetName(nameFrom, nameObjects);
  ax::mojom::DescriptionFrom descriptionFrom;
  blink::WebVector<blink::WebAXObject> descriptionObjects;
  accessibility_object_.Description(nameFrom, descriptionFrom,
                                    descriptionObjects);
  if (index >= descriptionObjects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(descriptionObjects[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::OffsetContainer() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return factory_->GetOrCreate(container);
}

float WebAXObjectProxy::BoundsInContainerX() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.x;
}

float WebAXObjectProxy::BoundsInContainerY() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.y;
}

float WebAXObjectProxy::BoundsInContainerWidth() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.width;
}

float WebAXObjectProxy::BoundsInContainerHeight() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.height;
}

bool WebAXObjectProxy::HasNonIdentityTransform() {
  accessibility_object_.UpdateLayoutAndCheckValidity();
  accessibility_object_.UpdateLayoutAndCheckValidity();
  blink::WebAXObject container;
  blink::WebFloatRect bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return !matrix.isIdentity();
}

RootWebAXObjectProxy::RootWebAXObjectProxy(const blink::WebAXObject& object,
                                           Factory* factory)
    : WebAXObjectProxy(object, factory) {}

v8::Local<v8::Object> RootWebAXObjectProxy::GetChildAtIndex(unsigned index) {
  if (index)
    return v8::Local<v8::Object>();

  return factory()->GetOrCreate(accessibility_object());
}

bool RootWebAXObjectProxy::IsRoot() const {
  return true;
}

WebAXObjectProxyList::WebAXObjectProxyList()
    : elements_(blink::MainThreadIsolate()) {}

WebAXObjectProxyList::~WebAXObjectProxyList() {
  Clear();
}

void WebAXObjectProxyList::Clear() {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = nullptr;
    bool result =
        gin::ConvertFromV8(isolate, elements_.Get(i), &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    unwrapped_object->Reset();
  }
  elements_.Clear();
}

v8::Local<v8::Object> WebAXObjectProxyList::GetOrCreate(
    const blink::WebAXObject& object) {
  if (object.IsNull())
    return v8::Local<v8::Object>();

  v8::Isolate* isolate = blink::MainThreadIsolate();

  size_t elementCount = elements_.Size();
  for (size_t i = 0; i < elementCount; i++) {
    WebAXObjectProxy* unwrapped_object = nullptr;
    bool result =
        gin::ConvertFromV8(isolate, elements_.Get(i), &unwrapped_object);
    DCHECK(result);
    DCHECK(unwrapped_object);
    if (unwrapped_object->IsEqualToObject(object))
      return elements_.Get(i);
  }

  v8::Local<v8::Value> value_handle =
      gin::CreateHandle(isolate, new WebAXObjectProxy(object, this)).ToV8();
  if (value_handle.IsEmpty())
    return v8::Local<v8::Object>();
  v8::Local<v8::Object> handle = value_handle->ToObject(isolate);
  elements_.Append(handle);
  return handle;
}

}  // namespace test_runner
