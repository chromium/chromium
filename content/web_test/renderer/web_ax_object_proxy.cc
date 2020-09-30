// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/web_test/renderer/web_ax_object_proxy.h"

#include <stddef.h>

#include <algorithm>
#include <map>
#include <utility>

#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "gin/handle.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/transform.h"

namespace content {

namespace {

// Map role value to string, matching Safari/Mac platform implementation to
// avoid rebaselining web tests.
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
    case ax::mojom::Role::kCode:
      return result.append("Code");
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
    case ax::mojom::Role::kComment:
      return result.append("Comment");
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
    case ax::mojom::Role::kEmphasis:
      return result.append("Emphasis");
    case ax::mojom::Role::kFigcaption:
      return result.append("Figcaption");
    case ax::mojom::Role::kFigure:
      return result.append("Figure");
    case ax::mojom::Role::kFooter:
      return result.append("Footer");
    case ax::mojom::Role::kFooterAsNonLandmark:
      return result.append("FooterAsNonLandmark");
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
    case ax::mojom::Role::kHeader:
      return result.append("Header");
    case ax::mojom::Role::kHeaderAsNonLandmark:
      return result.append("HeaderAsNonLandmark");
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
    case ax::mojom::Role::kPluginObject:
      return result.append("PluginObject");
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
    case ax::mojom::Role::kRow:
      return result.append("Row");
    case ax::mojom::Role::kRowGroup:
      return result.append("RowGroup");
    case ax::mojom::Role::kRowHeader:
      return result.append("RowHeader");
    case ax::mojom::Role::kRuby:
      return result.append("Ruby");
    case ax::mojom::Role::kRubyAnnotation:
      return result.append("RubyAnnotation");
    case ax::mojom::Role::kSection:
      return result.append("Section");
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
    case ax::mojom::Role::kStrong:
      return result.append("Strong");
    case ax::mojom::Role::kSwitch:
      return result.append("Switch");
    case ax::mojom::Role::kSuggestion:
      return result.append("Suggestion");
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
gfx::RectF BoundsForObject(const blink::WebAXObject& object) {
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  object.GetRelativeBounds(container, bounds, matrix);
  gfx::RectF computed_bounds(0, 0, bounds.width(), bounds.height());
  while (!container.IsDetached()) {
    computed_bounds.Offset(bounds.x(), bounds.y());
    computed_bounds.Offset(-container.GetScrollOffset().x(),
                           -container.GetScrollOffset().y());
    if (!matrix.isIdentity()) {
      gfx::Transform transform(matrix);
      transform.TransformRect(&computed_bounds);
    }
    container.GetRelativeBounds(container, bounds, matrix);
  }
  return computed_bounds;
}

blink::WebRect BoundsForCharacter(const blink::WebAXObject& object,
                                  int character_index) {
  DCHECK_EQ(object.Role(), ax::mojom::Role::kStaticText);
  int end = 0;
  for (unsigned i = 0; i < object.ChildCount(); i++) {
    blink::WebAXObject inline_text_box = object.ChildAt(i);
    DCHECK_EQ(inline_text_box.Role(), ax::mojom::Role::kInlineTextBox);
    int start = end;
    blink::WebString name = inline_text_box.GetName();
    end += name.length();
    if (character_index < start || character_index >= end)
      continue;

    gfx::RectF inline_text_box_rect = BoundsForObject(inline_text_box);

    int local_index = character_index - start;
    blink::WebVector<int> character_offsets;
    inline_text_box.CharacterOffsets(character_offsets);
    if (character_offsets.size() != name.length())
      return blink::WebRect();

    switch (inline_text_box.GetTextDirection()) {
      case ax::mojom::WritingDirection::kLtr: {
        if (local_index) {
          int left =
              inline_text_box_rect.x() + character_offsets[local_index - 1];
          int width = character_offsets[local_index] -
                      character_offsets[local_index - 1];
          return blink::WebRect(left, inline_text_box_rect.y(), width,
                                inline_text_box_rect.height());
        }
        return blink::WebRect(inline_text_box_rect.x(),
                              inline_text_box_rect.y(), character_offsets[0],
                              inline_text_box_rect.height());
      }
      case ax::mojom::WritingDirection::kRtl: {
        int right = inline_text_box_rect.x() + inline_text_box_rect.width();

        if (local_index) {
          int left = right - character_offsets[local_index];
          int width = character_offsets[local_index] -
                      character_offsets[local_index - 1];
          return blink::WebRect(left, inline_text_box_rect.y(), width,
                                inline_text_box_rect.height());
        }
        int left = right - character_offsets[0];
        return blink::WebRect(left, inline_text_box_rect.y(),
                              character_offsets[0],
                              inline_text_box_rect.height());
      }
      case ax::mojom::WritingDirection::kTtb: {
        if (local_index) {
          int top =
              inline_text_box_rect.y() + character_offsets[local_index - 1];
          int height = character_offsets[local_index] -
                       character_offsets[local_index - 1];
          return blink::WebRect(inline_text_box_rect.x(), top,
                                inline_text_box_rect.width(), height);
        }
        return blink::WebRect(
            inline_text_box_rect.x(), inline_text_box_rect.y(),
            inline_text_box_rect.width(), character_offsets[0]);
      }
      case ax::mojom::WritingDirection::kBtt: {
        int bottom = inline_text_box_rect.y() + inline_text_box_rect.height();

        if (local_index) {
          int top = bottom - character_offsets[local_index];
          int height = character_offsets[local_index] -
                       character_offsets[local_index - 1];
          return blink::WebRect(inline_text_box_rect.x(), top,
                                inline_text_box_rect.width(), height);
        }
        int top = bottom - character_offsets[0];
        return blink::WebRect(inline_text_box_rect.x(), top,
                              inline_text_box_rect.width(),
                              character_offsets[0]);
      }
      default:
        NOTREACHED();
        return blink::WebRect();
    }
  }

  DCHECK(false);
  return blink::WebRect();
}

std::vector<std::string> GetMisspellings(const blink::WebAXObject& object) {
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
    int local_index = character_index - start;

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
      if (local_index < ends[j]) {
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

WebAXObjectProxy::~WebAXObjectProxy() {
  // v8::Persistent will leak on destroy, due to the default
  // NonCopyablePersistentTraits (it claims this may change in the future).
  notification_callback_.Reset();
}

void WebAXObjectProxy::UpdateLayout() {
  blink::WebAXObject::UpdateLayout(accessibility_object_.GetDocument());
}

ui::AXNodeData WebAXObjectProxy::GetAXNodeData() const {
  ui::AXNodeData node_data;
  accessibility_object_.Serialize(&node_data, ui::kAXModeComplete);
  return node_data;
}

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
      .SetProperty("selectionIsBackward",
                   &WebAXObjectProxy::SelectionIsBackward)
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
      .SetProperty("isVisited", &WebAXObjectProxy::IsVisited)
      .SetProperty("isOffScreen", &WebAXObjectProxy::IsOffScreen)
      .SetProperty("isCollapsed", &WebAXObjectProxy::IsCollapsed)
      .SetProperty("hasPopup", &WebAXObjectProxy::HasPopup)
      .SetProperty("isValid", &WebAXObjectProxy::IsValid)
      .SetProperty("isReadOnly", &WebAXObjectProxy::IsReadOnly)
      .SetProperty("isIgnored", &WebAXObjectProxy::IsIgnored)
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
      .SetProperty("ariaColumnCount", &WebAXObjectProxy::AriaColumnCount)
      .SetProperty("ariaColumnIndex", &WebAXObjectProxy::AriaColumnIndex)
      .SetProperty("ariaColumnSpan", &WebAXObjectProxy::AriaColumnSpan)
      .SetProperty("ariaRowCount", &WebAXObjectProxy::AriaRowCount)
      .SetProperty("ariaRowIndex", &WebAXObjectProxy::AriaRowIndex)
      .SetProperty("ariaRowSpan", &WebAXObjectProxy::AriaRowSpan)
      .SetProperty("live", &WebAXObjectProxy::Live)
      .SetProperty("orientation", &WebAXObjectProxy::Orientation)
      .SetProperty("relevant", &WebAXObjectProxy::Relevant)
      .SetProperty("roleDescription", &WebAXObjectProxy::RoleDescription)
      .SetProperty("sort", &WebAXObjectProxy::Sort)
      .SetProperty("url", &WebAXObjectProxy::Url)
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
      .SetMethod("ariaDetailsElementAtIndex",
                 &WebAXObjectProxy::AriaDetailsElementAtIndex)
      .SetMethod("ariaErrorMessageElement",
                 &WebAXObjectProxy::AriaErrorMessageElement)
      .SetMethod("ariaFlowToElementAtIndex",
                 &WebAXObjectProxy::AriaFlowToElementAtIndex)
      .SetMethod("ariaOwnsElementAtIndex",
                 &WebAXObjectProxy::AriaOwnsElementAtIndex)
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
      .SetMethod("toString", &WebAXObjectProxy::ToString)
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
  UpdateLayout();
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
    const std::string& notification_name,
    const std::vector<ui::AXEventIntent>& event_intents) {
  if (notification_callback_.IsEmpty())
    return;

  v8::Local<v8::Context> context = frame->MainWorldScriptContext();
  if (context.IsEmpty())
    return;

  v8::Isolate* isolate = blink::MainThreadIsolate();

  v8::Local<v8::Array> intents_array(
      v8::Array::New(isolate, event_intents.size()));
  for (size_t i = 0; i < event_intents.size(); ++i) {
    intents_array
        ->CreateDataProperty(context, uint32_t{i},
                             v8::String::NewFromUtf8(
                                 isolate, event_intents[i].ToString().c_str())
                                 .ToLocalChecked())
        .Check();
  }

  v8::Local<v8::Value> argv[] = {
      v8::String::NewFromUtf8(isolate, notification_name.c_str())
          .ToLocalChecked(),
      intents_array};
  // TODO(aboxhall): Can we force this to run in a new task, to avoid
  // dirtying layout during post-layout hooks?
  frame->CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>::New(isolate, notification_callback_),
      context->Global(), base::size(argv), argv);
}

void WebAXObjectProxy::Reset() {
  notification_callback_.Reset();
}

std::string WebAXObjectProxy::Role() {
  UpdateLayout();
  return GetRole(accessibility_object_);
}

std::string WebAXObjectProxy::StringValue() {
  UpdateLayout();
  return GetStringValue(accessibility_object_);
}

std::string WebAXObjectProxy::Language() {
  UpdateLayout();
  return GetLanguage(accessibility_object_);
}

int WebAXObjectProxy::X() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).x();
}

int WebAXObjectProxy::Y() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).y();
}

int WebAXObjectProxy::Width() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).width();
}

int WebAXObjectProxy::Height() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).height();
}

v8::Local<v8::Value> WebAXObjectProxy::InPageLinkTarget() {
  UpdateLayout();
  blink::WebAXObject target = accessibility_object_.InPageLinkTarget();
  if (target.IsNull())
    return v8::Null(blink::MainThreadIsolate());
  return factory_->GetOrCreate(target);
}

int WebAXObjectProxy::IntValue() {
  UpdateLayout();

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
  UpdateLayout();
  float min_value = 0.0f;
  accessibility_object_.MinValueForRange(&min_value);
  return min_value;
}

int WebAXObjectProxy::MaxValue() {
  UpdateLayout();
  float max_value = 0.0f;
  accessibility_object_.MaxValueForRange(&max_value);
  return max_value;
}

int WebAXObjectProxy::StepValue() {
  UpdateLayout();
  float step_value = 0.0f;
  accessibility_object_.StepValueForRange(&step_value);
  return step_value;
}

std::string WebAXObjectProxy::ValueDescription() {
  UpdateLayout();
  std::string value_description =
      GetAXNodeData().GetStringAttribute(ax::mojom::StringAttribute::kValue);
  return value_description.insert(0, "AXValueDescription: ");
}

int WebAXObjectProxy::ChildrenCount() {
  UpdateLayout();
  int count = 1;  // Root object always has only one child, the WebView.
  if (!IsRoot())
    count = accessibility_object_.ChildCount();
  return count;
}

bool WebAXObjectProxy::SelectionIsBackward() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return is_selection_backward;
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionAnchorObject() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (anchor_object.IsNull())
    return v8::Null(blink::MainThreadIsolate());

  return factory_->GetOrCreate(anchor_object);
}

int WebAXObjectProxy::SelectionAnchorOffset() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (anchor_offset < 0)
    return -1;

  return anchor_offset;
}

std::string WebAXObjectProxy::SelectionAnchorAffinity() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return anchor_affinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                               : "downstream";
}

v8::Local<v8::Value> WebAXObjectProxy::SelectionFocusObject() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (focus_object.IsNull())
    return v8::Null(blink::MainThreadIsolate());

  return factory_->GetOrCreate(focus_object);
}

int WebAXObjectProxy::SelectionFocusOffset() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  if (focus_offset < 0)
    return -1;

  return focus_offset;
}

std::string WebAXObjectProxy::SelectionFocusAffinity() {
  UpdateLayout();

  bool is_selection_backward = false;
  blink::WebAXObject anchor_object;
  int anchor_offset = -1;
  ax::mojom::TextAffinity anchor_affinity;
  blink::WebAXObject focus_object;
  int focus_offset = -1;
  ax::mojom::TextAffinity focus_affinity;
  accessibility_object_.Selection(is_selection_backward, anchor_object,
                                  anchor_offset, anchor_affinity, focus_object,
                                  focus_offset, focus_affinity);
  return focus_affinity == ax::mojom::TextAffinity::kUpstream ? "upstream"
                                                              : "downstream";
}

bool WebAXObjectProxy::IsAtomic() {
  UpdateLayout();
  return accessibility_object_.LiveRegionAtomic();
}

bool WebAXObjectProxy::IsAutofillAvailable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kAutofillAvailable);
}

bool WebAXObjectProxy::IsBusy() {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .bool_attributes[blink::WebAXBoolAttribute::kAriaBusy];
}

std::string WebAXObjectProxy::Restriction() {
  UpdateLayout();
  blink::WebAXRestriction web_ax_restriction =
      static_cast<blink::WebAXRestriction>(GetAXNodeData().GetRestriction());
  switch (web_ax_restriction) {
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
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kRequired);
}

bool WebAXObjectProxy::IsEditableRoot() {
  UpdateLayout();
  return accessibility_object_.IsEditableRoot();
}

bool WebAXObjectProxy::IsEditable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kEditable);
}

bool WebAXObjectProxy::IsRichlyEditable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kRichlyEditable);
}

bool WebAXObjectProxy::IsFocused() {
  UpdateLayout();
  return accessibility_object_.IsFocused();
}

bool WebAXObjectProxy::IsFocusable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kFocusable);
}

bool WebAXObjectProxy::IsModal() {
  UpdateLayout();
  return accessibility_object_.IsModal();
}

bool WebAXObjectProxy::IsSelected() {
  UpdateLayout();
  return GetAXNodeData().GetBoolAttribute(ax::mojom::BoolAttribute::kSelected);
}

bool WebAXObjectProxy::IsSelectable() {
  UpdateLayout();
  ui::AXNodeData node_data = GetAXNodeData();
  // It's selectable if it has the attribute, whether it's true or false.
  return node_data.HasBoolAttribute(ax::mojom::BoolAttribute::kSelected) &&
         node_data.GetRestriction() != ax::mojom::Restriction::kDisabled;
}

bool WebAXObjectProxy::IsMultiLine() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kMultiline);
}

bool WebAXObjectProxy::IsMultiSelectable() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kMultiselectable);
}

bool WebAXObjectProxy::IsSelectedOptionActive() {
  UpdateLayout();
  return accessibility_object_.IsSelectedOptionActive();
}

bool WebAXObjectProxy::IsExpanded() {
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kExpanded);
}

std::string WebAXObjectProxy::Checked() {
  UpdateLayout();
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
  UpdateLayout();
  return GetAXNodeData().HasState(ax::mojom::State::kCollapsed);
}

bool WebAXObjectProxy::IsVisible() {
  UpdateLayout();
  return !GetAXNodeData().HasState(ax::mojom::State::kInvisible);
}

bool WebAXObjectProxy::IsVisited() {
  UpdateLayout();
  return accessibility_object_.IsVisited();
}

bool WebAXObjectProxy::IsOffScreen() {
  UpdateLayout();
  return accessibility_object_.IsOffScreen();
}

bool WebAXObjectProxy::IsValid() {
  UpdateLayout();
  return !accessibility_object_.IsDetached();
}

bool WebAXObjectProxy::IsReadOnly() {
  UpdateLayout();
  return GetAXNodeData().GetRestriction() == ax::mojom::Restriction::kReadOnly;
}

bool WebAXObjectProxy::IsIgnored() {
  UpdateLayout();
  return accessibility_object_.AccessibilityIsIgnored();
}

v8::Local<v8::Object> WebAXObjectProxy::ActiveDescendant() {
  UpdateLayout();
  blink::WebAXObject element = accessibility_object_.AriaActiveDescendant();
  return factory_->GetOrCreate(element);
}

unsigned int WebAXObjectProxy::BackgroundColor() {
  UpdateLayout();
  return accessibility_object_.BackgroundColor();
}

unsigned int WebAXObjectProxy::Color() {
  UpdateLayout();
  unsigned int color = accessibility_object_.GetColor();
  // Remove the alpha because it's always 1 and thus not informative.
  return color & 0xFFFFFF;
}

// For input elements of type color.
unsigned int WebAXObjectProxy::ColorValue() {
  UpdateLayout();
  return accessibility_object_.ColorValue();
}

std::string WebAXObjectProxy::FontFamily() {
  UpdateLayout();
  std::string font_family(accessibility_object_.FontFamily().Utf8());
  return font_family.insert(0, "AXFontFamily: ");
}

float WebAXObjectProxy::FontSize() {
  UpdateLayout();
  return accessibility_object_.FontSize();
}

std::string WebAXObjectProxy::Autocomplete() {
  UpdateLayout();
  return accessibility_object_.AutoComplete().Utf8();
}

std::string WebAXObjectProxy::Current() {
  UpdateLayout();
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
  UpdateLayout();
  switch (GetAXNodeData().GetHasPopup()) {
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
  UpdateLayout();
  switch (accessibility_object_.InvalidState()) {
    case ax::mojom::InvalidState::kFalse:
      return "false";
    case ax::mojom::InvalidState::kTrue:
      return "true";
    case ax::mojom::InvalidState::kOther:
      return "other";
    default:
      return std::string();
  }
}

std::string WebAXObjectProxy::KeyShortcuts() {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .string_attributes[blink::WebAXStringAttribute::kAriaKeyShortcuts]
      .Utf8();
}

int32_t WebAXObjectProxy::AriaColumnCount() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaColumnCount);
}

uint32_t WebAXObjectProxy::AriaColumnIndex() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellColumnIndex);
}

uint32_t WebAXObjectProxy::AriaColumnSpan() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellColumnSpan);
}

int32_t WebAXObjectProxy::AriaRowCount() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaRowCount);
}

uint32_t WebAXObjectProxy::AriaRowIndex() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellRowIndex);
}

uint32_t WebAXObjectProxy::AriaRowSpan() {
  UpdateLayout();
  return GetAXNodeData().GetIntAttribute(
      ax::mojom::IntAttribute::kAriaCellRowSpan);
}

std::string WebAXObjectProxy::Live() {
  UpdateLayout();
  return accessibility_object_.LiveRegionStatus().Utf8();
}

std::string WebAXObjectProxy::Orientation() {
  UpdateLayout();
  ui::AXNodeData node_data = GetAXNodeData();
  if (node_data.HasState(ax::mojom::State::kVertical))
    return "AXOrientation: AXVerticalOrientation";
  else if (node_data.HasState(ax::mojom::State::kHorizontal))
    return "AXOrientation: AXHorizontalOrientation";
  return std::string();
}

std::string WebAXObjectProxy::Relevant() {
  UpdateLayout();
  return accessibility_object_.LiveRegionRelevant().Utf8();
}

std::string WebAXObjectProxy::RoleDescription() {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  return attribute_adapter
      .string_attributes[blink::WebAXStringAttribute::kAriaRoleDescription]
      .Utf8();
}

std::string WebAXObjectProxy::Sort() {
  UpdateLayout();
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

std::string WebAXObjectProxy::Url() {
  UpdateLayout();
  return accessibility_object_.Url().GetString().Utf8();
}

int WebAXObjectProxy::HierarchicalLevel() {
  UpdateLayout();
  return accessibility_object_.HierarchicalLevel();
}

int WebAXObjectProxy::PosInSet() {
  UpdateLayout();
  return accessibility_object_.PosInSet();
}

int WebAXObjectProxy::SetSize() {
  UpdateLayout();
  return accessibility_object_.SetSize();
}

int WebAXObjectProxy::ClickPointX() {
  UpdateLayout();
  gfx::RectF bounds = BoundsForObject(accessibility_object_);
  return bounds.x() + bounds.width() / 2;
}

int WebAXObjectProxy::ClickPointY() {
  UpdateLayout();
  gfx::RectF bounds = BoundsForObject(accessibility_object_);
  return bounds.y() + bounds.height() / 2;
}

int32_t WebAXObjectProxy::RowCount() {
  UpdateLayout();
  return static_cast<int32_t>(accessibility_object_.RowCount());
}

int32_t WebAXObjectProxy::RowHeadersCount() {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

int32_t WebAXObjectProxy::ColumnCount() {
  UpdateLayout();
  return static_cast<int32_t>(accessibility_object_.ColumnCount());
}

int32_t WebAXObjectProxy::ColumnHeadersCount() {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  return static_cast<int32_t>(headers.size());
}

bool WebAXObjectProxy::IsClickable() {
  UpdateLayout();
  return accessibility_object_.IsClickable();
}

v8::Local<v8::Object> WebAXObjectProxy::AriaActiveDescendantElement() {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebAXObject element =
      attribute_adapter.object_attributes
          [blink::WebAXObjectAttribute::kAriaActiveDescendant];
  return factory_->GetOrCreate(element);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaControlsElementAtIndex(
    unsigned index) {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::kAriaControls];
  size_t element_count = elements.size();
  if (index >= element_count)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaDetailsElementAtIndex(
    unsigned index) {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::kAriaDetails];
  size_t element_count = elements.size();
  if (index >= element_count)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaErrorMessageElement() {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebAXObject element =
      attribute_adapter
          .object_attributes[blink::WebAXObjectAttribute::kAriaErrorMessage];
  return factory_->GetOrCreate(element);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaFlowToElementAtIndex(
    unsigned index) {
  UpdateLayout();
  SparseAttributeAdapter attribute_adapter;
  accessibility_object_.GetSparseAXAttributes(attribute_adapter);
  blink::WebVector<blink::WebAXObject> elements =
      attribute_adapter.object_vector_attributes
          [blink::WebAXObjectVectorAttribute::kAriaFlowTo];
  size_t element_count = elements.size();
  if (index >= element_count)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::AriaOwnsElementAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> elements;
  accessibility_object_.AriaOwns(elements);
  size_t element_count = elements.size();
  if (index >= element_count)
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(elements[index]);
}

std::string WebAXObjectProxy::AllAttributes() {
  UpdateLayout();
  return GetAttributes(accessibility_object_);
}

std::string WebAXObjectProxy::AttributesOfChildren() {
  UpdateLayout();
  AttributesCollector collector;
  unsigned size = accessibility_object_.ChildCount();
  for (unsigned i = 0; i < size; ++i)
    collector.CollectAttributes(accessibility_object_.ChildAt(i));
  return collector.attributes();
}

std::string WebAXObjectProxy::BoundsForRange(int start, int end) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return std::string();

  if (!accessibility_object_.MaybeUpdateLayoutAndCheckValidity())
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
  return GetChildAtIndex(index);
}

v8::Local<v8::Object> WebAXObjectProxy::ElementAtPoint(int x, int y) {
  UpdateLayout();
  gfx::Point point(x, y);
  blink::WebAXObject obj = accessibility_object_.HitTest(point);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::RowHeaderAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.RowHeaders(headers);
  size_t header_count = headers.size();
  if (index >= header_count)
    return {};

  return factory_->GetOrCreate(headers[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::ColumnHeaderAtIndex(unsigned index) {
  UpdateLayout();
  blink::WebVector<blink::WebAXObject> headers;
  accessibility_object_.ColumnHeaders(headers);
  size_t header_count = headers.size();
  if (index >= header_count)
    return {};

  return factory_->GetOrCreate(headers[index]);
}

std::string WebAXObjectProxy::RowIndexRange() {
  UpdateLayout();
  unsigned row_index = accessibility_object_.CellRowIndex();
  unsigned row_span = accessibility_object_.CellRowSpan();
  return base::StringPrintf("{%d, %d}", row_index, row_span);
}

std::string WebAXObjectProxy::ColumnIndexRange() {
  UpdateLayout();
  unsigned column_index = accessibility_object_.CellColumnIndex();
  unsigned column_span = accessibility_object_.CellColumnSpan();
  return base::StringPrintf("{%d, %d}", column_index, column_span);
}

v8::Local<v8::Object> WebAXObjectProxy::CellForColumnAndRow(int column,
                                                            int row) {
  UpdateLayout();
  blink::WebAXObject obj =
      accessibility_object_.CellForColumnAndRow(column, row);
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

void WebAXObjectProxy::SetSelectedTextRange(int selection_start, int length) {
  UpdateLayout();
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

  UpdateLayout();
  return accessibility_object_.SetSelection(
      web_ax_anchor->accessibility_object_, anchor_offset,
      web_ax_focus->accessibility_object_, focus_offset);
}

bool WebAXObjectProxy::IsAttributeSettable(const std::string& attribute) {
  UpdateLayout();
  bool settable = false;
  if (attribute == "AXValue")
    settable = accessibility_object_.CanSetValueAttribute();
  return settable;
}

bool WebAXObjectProxy::IsPressActionSupported() {
  UpdateLayout();
  return accessibility_object_.CanPress();
}

v8::Local<v8::Object> WebAXObjectProxy::ParentElement() {
  UpdateLayout();
  blink::WebAXObject parent_object = accessibility_object_.ParentObject();
  return factory_->GetOrCreate(parent_object);
}

void WebAXObjectProxy::Increment() {
  UpdateLayout();
  accessibility_object_.Increment();
}

void WebAXObjectProxy::Decrement() {
  UpdateLayout();
  accessibility_object_.Decrement();
}

void WebAXObjectProxy::ShowMenu() {
  accessibility_object_.ShowContextMenu();
}

void WebAXObjectProxy::Press() {
  UpdateLayout();
  accessibility_object_.Click();
}

bool WebAXObjectProxy::SetValue(const std::string& value) {
  UpdateLayout();
  if (GetAXNodeData().GetRestriction() != ax::mojom::Restriction::kNone ||
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
  UpdateLayout();
  accessibility_object_.Focus();
}

void WebAXObjectProxy::ScrollToMakeVisible() {
  UpdateLayout();
  accessibility_object_.ScrollToMakeVisible();
}

void WebAXObjectProxy::ScrollToMakeVisibleWithSubFocus(int x,
                                                       int y,
                                                       int width,
                                                       int height) {
  UpdateLayout();
  accessibility_object_.ScrollToMakeVisibleWithSubFocus(
      blink::WebRect(x, y, width, height));
}

void WebAXObjectProxy::ScrollToGlobalPoint(int x, int y) {
  UpdateLayout();
  accessibility_object_.ScrollToGlobalPoint(gfx::Point(x, y));
}

int WebAXObjectProxy::ScrollX() {
  UpdateLayout();
  return accessibility_object_.GetScrollOffset().x();
}

int WebAXObjectProxy::ScrollY() {
  UpdateLayout();
  return accessibility_object_.GetScrollOffset().y();
}

std::string WebAXObjectProxy::ToString() {
  UpdateLayout();
  return accessibility_object_.ToString().Utf8();
}

float WebAXObjectProxy::BoundsX() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).x();
}

float WebAXObjectProxy::BoundsY() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).y();
}

float WebAXObjectProxy::BoundsWidth() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).width();
}

float WebAXObjectProxy::BoundsHeight() {
  UpdateLayout();
  return BoundsForObject(accessibility_object_).height();
}

int WebAXObjectProxy::WordStart(int character_index) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_start;
}

int WebAXObjectProxy::WordEnd(int character_index) {
  UpdateLayout();
  if (accessibility_object_.Role() != ax::mojom::Role::kStaticText)
    return -1;

  int word_start = 0, word_end = 0;
  GetBoundariesForOneWord(accessibility_object_, character_index, word_start,
                          word_end);
  return word_end;
}

v8::Local<v8::Object> WebAXObjectProxy::NextOnLine() {
  UpdateLayout();
  blink::WebAXObject obj = accessibility_object_.NextOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

v8::Local<v8::Object> WebAXObjectProxy::PreviousOnLine() {
  UpdateLayout();
  blink::WebAXObject obj = accessibility_object_.PreviousOnLine();
  if (obj.IsNull())
    return v8::Local<v8::Object>();

  return factory_->GetOrCreate(obj);
}

std::string WebAXObjectProxy::MisspellingAtIndex(int index) {
  UpdateLayout();
  if (index < 0 || index >= MisspellingsCount())
    return std::string();
  return GetMisspellings(accessibility_object_)[index];
}

std::string WebAXObjectProxy::Name() {
  UpdateLayout();
  return accessibility_object_.GetName().Utf8();
}

std::string WebAXObjectProxy::NameFrom() {
  UpdateLayout();
  ax::mojom::NameFrom name_from = ax::mojom::NameFrom::kUninitialized;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  switch (name_from) {
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
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  return static_cast<int>(name_objects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::NameElementAtIndex(unsigned index) {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  if (index >= name_objects.size())
    return {};
  return factory_->GetOrCreate(name_objects[index]);
}

std::string WebAXObjectProxy::Description() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  return accessibility_object_
      .Description(name_from, description_from, description_objects)
      .Utf8();
}

std::string WebAXObjectProxy::DescriptionFrom() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from =
      ax::mojom::DescriptionFrom::kUninitialized;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  switch (description_from) {
    case ax::mojom::DescriptionFrom::kUninitialized:
    case ax::mojom::DescriptionFrom::kNone:
      return "";
    case ax::mojom::DescriptionFrom::kAttribute:
      return "attribute";
    case ax::mojom::DescriptionFrom::kContents:
      return "contents";
    case ax::mojom::DescriptionFrom::kRelatedElement:
      return "relatedElement";
    case ax::mojom::DescriptionFrom::kTitle:
      return "title";
  }

  NOTREACHED();
  return std::string();
}

std::string WebAXObjectProxy::Placeholder() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  return accessibility_object_.Placeholder(name_from).Utf8();
}

int WebAXObjectProxy::MisspellingsCount() {
  UpdateLayout();
  return GetMisspellings(accessibility_object_).size();
}

int WebAXObjectProxy::DescriptionElementCount() {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  return static_cast<int>(description_objects.size());
}

v8::Local<v8::Object> WebAXObjectProxy::DescriptionElementAtIndex(
    unsigned index) {
  UpdateLayout();
  ax::mojom::NameFrom name_from;
  blink::WebVector<blink::WebAXObject> name_objects;
  accessibility_object_.GetName(name_from, name_objects);
  ax::mojom::DescriptionFrom description_from;
  blink::WebVector<blink::WebAXObject> description_objects;
  accessibility_object_.Description(name_from, description_from,
                                    description_objects);
  if (index >= description_objects.size())
    return v8::Local<v8::Object>();
  return factory_->GetOrCreate(description_objects[index]);
}

v8::Local<v8::Object> WebAXObjectProxy::OffsetContainer() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return factory_->GetOrCreate(container);
}

float WebAXObjectProxy::BoundsInContainerX() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.x();
}

float WebAXObjectProxy::BoundsInContainerY() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.y();
}

float WebAXObjectProxy::BoundsInContainerWidth() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.width();
}

float WebAXObjectProxy::BoundsInContainerHeight() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
  SkMatrix44 matrix;
  accessibility_object_.GetRelativeBounds(container, bounds, matrix);
  return bounds.height();
}

bool WebAXObjectProxy::HasNonIdentityTransform() {
  UpdateLayout();
  blink::WebAXObject container;
  gfx::RectF bounds;
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

WebAXObjectProxyList::WebAXObjectProxyList() = default;

WebAXObjectProxyList::~WebAXObjectProxyList() {
  Clear();
}

void WebAXObjectProxyList::Clear() {
  v8::Isolate* isolate = blink::MainThreadIsolate();
  v8::HandleScope handle_scope(isolate);

  for (auto& persistent : elements_) {
    auto local = v8::Local<v8::Object>::New(isolate, persistent);

    WebAXObjectProxy* proxy = nullptr;
    bool ok = gin::ConvertFromV8(isolate, local, &proxy);
    DCHECK(ok);

    // Because the v8::Persistent in this container uses
    // CopyablePersistentObject traits, it will not leak the Persistent objects
    // on destruction. However, blink may be keeping a reference to the |proxy|.
    // We Reset() it to drop the callback in the proxy object now that its not
    // in the proxy list.
    proxy->Reset();
  }

  elements_.clear();
}

v8::Local<v8::Object> WebAXObjectProxyList::GetOrCreate(
    const blink::WebAXObject& object) {
  if (object.IsNull())
    return v8::Local<v8::Object>();

  v8::Isolate* isolate = blink::MainThreadIsolate();

  for (const auto& persistent : elements_) {
    auto local = v8::Local<v8::Object>::New(isolate, persistent);

    WebAXObjectProxy* proxy = nullptr;
    bool ok = gin::ConvertFromV8(isolate, local, &proxy);
    DCHECK(ok);

    if (proxy->IsEqualToObject(object))
      return local;
  }

  v8::Local<v8::Value> value_handle =
      gin::CreateHandle(isolate, new WebAXObjectProxy(object, this)).ToV8();
  v8::Local<v8::Object> handle;
  if (value_handle.IsEmpty() ||
      !value_handle->ToObject(isolate->GetCurrentContext()).ToLocal(&handle)) {
    return {};
  }

  elements_.emplace_back(isolate, handle);
  return handle;
}

}  // namespace content
