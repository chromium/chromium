/*
 * Copyright (C) 1997 Martin Jones (mjones@kde.org)
 *           (C) 1997 Torben Weis (weis@kde.org)
 *           (C) 1998 Waldo Bastian (bastian@kde.org)
 *           (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008, 2010, 2011 Apple Inc. All rights
 * reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_table_element.h"

#include "third_party/blink/renderer/core/css/css_identifier_value.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_inherited_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_lists_node_data.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_table_caption_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_rows_collection.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/weborigin/referrer.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"

namespace blink {

using namespace HTMLNames;

inline HTMLTableElement::HTMLTableElement(Document& document)
    : HTMLElement(tableTag, document),
      border_attr_(false),
      border_color_attr_(false),
      frame_attr_(false),
      rules_attr_(kUnsetRules),
      padding_(1) {}

// An explicit empty destructor should be in html_table_element.cc, because
// if an implicit destructor is used or an empty destructor is defined in
// html_table_element.h, when including html_table_element.h, msvc tries to
// expand the destructor and causes a compile error because of lack of
// CSSPropertyValueSet definition.
HTMLTableElement::~HTMLTableElement() = default;

DEFINE_NODE_FACTORY(HTMLTableElement)

HTMLTableCaptionElement* HTMLTableElement::caption() const {
  return Traversal<HTMLTableCaptionElement>::FirstChild(*this);
}

void HTMLTableElement::setCaption(HTMLTableCaptionElement* new_caption,
                                  ExceptionState& exception_state) {
  deleteCaption();
  if (new_caption)
    InsertBefore(new_caption, firstChild(), exception_state);
}

HTMLTableSectionElement* HTMLTableElement::tHead() const {
  return ToHTMLTableSectionElement(
      Traversal<HTMLElement>::FirstChild(*this, HasHTMLTagName(theadTag)));
}

void HTMLTableElement::setTHead(HTMLTableSectionElement* new_head,
                                ExceptionState& exception_state) {
  if (new_head && !new_head->HasTagName(theadTag)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Not a thead element.");
    return;
  }

  deleteTHead();
  if (!new_head)
    return;

  HTMLElement* child;
  for (child = Traversal<HTMLElement>::FirstChild(*this); child;
       child = Traversal<HTMLElement>::NextSibling(*child)) {
    if (!child->HasTagName(captionTag) && !child->HasTagName(colgroupTag))
      break;
  }

  InsertBefore(new_head, child, exception_state);
}

HTMLTableSectionElement* HTMLTableElement::tFoot() const {
  return ToHTMLTableSectionElement(
      Traversal<HTMLElement>::FirstChild(*this, HasHTMLTagName(tfootTag)));
}

void HTMLTableElement::setTFoot(HTMLTableSectionElement* new_foot,
                                ExceptionState& exception_state) {
  if (new_foot && !new_foot->HasTagName(tfootTag)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kHierarchyRequestError,
                                      "Not a tfoot element.");
    return;
  }

  deleteTFoot();

  if (new_foot)
    AppendChild(new_foot, exception_state);
}

HTMLTableSectionElement* HTMLTableElement::createTHead() {
  if (HTMLTableSectionElement* existing_head = tHead())
    return existing_head;
  HTMLTableSectionElement* head =
      HTMLTableSectionElement::Create(theadTag, GetDocument());
  setTHead(head, IGNORE_EXCEPTION_FOR_TESTING);
  return head;
}

void HTMLTableElement::deleteTHead() {
  RemoveChild(tHead(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::createTFoot() {
  if (HTMLTableSectionElement* existing_foot = tFoot())
    return existing_foot;
  HTMLTableSectionElement* foot =
      HTMLTableSectionElement::Create(tfootTag, GetDocument());
  setTFoot(foot, IGNORE_EXCEPTION_FOR_TESTING);
  return foot;
}

void HTMLTableElement::deleteTFoot() {
  RemoveChild(tFoot(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::createTBody() {
  HTMLTableSectionElement* body =
      HTMLTableSectionElement::Create(tbodyTag, GetDocument());
  Node* reference_element = LastBody() ? LastBody()->nextSibling() : nullptr;

  InsertBefore(body, reference_element);
  return body;
}

HTMLTableCaptionElement* HTMLTableElement::createCaption() {
  if (HTMLTableCaptionElement* existing_caption = caption())
    return existing_caption;
  HTMLTableCaptionElement* caption =
      HTMLTableCaptionElement::Create(GetDocument());
  setCaption(caption, IGNORE_EXCEPTION_FOR_TESTING);
  return caption;
}

void HTMLTableElement::deleteCaption() {
  RemoveChild(caption(), IGNORE_EXCEPTION_FOR_TESTING);
}

HTMLTableSectionElement* HTMLTableElement::LastBody() const {
  return ToHTMLTableSectionElement(
      Traversal<HTMLElement>::LastChild(*this, HasHTMLTagName(tbodyTag)));
}

HTMLTableRowElement* HTMLTableElement::insertRow(
    int index,
    ExceptionState& exception_state) {
  if (index < -1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) + ") is less than -1.");
    return nullptr;
  }

  HTMLTableRowElement* last_row = nullptr;
  HTMLTableRowElement* row = nullptr;
  if (index == -1) {
    last_row = HTMLTableRowsCollection::LastRow(*this);
  } else {
    for (int i = 0; i <= index; ++i) {
      row = HTMLTableRowsCollection::RowAfter(*this, last_row);
      if (!row) {
        if (i != index) {
          exception_state.ThrowDOMException(
              DOMExceptionCode::kIndexSizeError,
              "The index provided (" + String::Number(index) +
                  ") is greater than the number of rows in the table (" +
                  String::Number(i) + ").");
          return nullptr;
        }
        break;
      }
      last_row = row;
    }
  }

  ContainerNode* parent;
  if (last_row) {
    parent = row ? row->parentNode() : last_row->parentNode();
  } else {
    parent = LastBody();
    if (!parent) {
      HTMLTableSectionElement* new_body =
          HTMLTableSectionElement::Create(tbodyTag, GetDocument());
      HTMLTableRowElement* new_row = HTMLTableRowElement::Create(GetDocument());
      new_body->AppendChild(new_row, exception_state);
      AppendChild(new_body, exception_state);
      return new_row;
    }
  }

  HTMLTableRowElement* new_row = HTMLTableRowElement::Create(GetDocument());
  parent->InsertBefore(new_row, row, exception_state);
  return new_row;
}

void HTMLTableElement::deleteRow(int index, ExceptionState& exception_state) {
  if (index < -1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) + ") is less than -1.");
    return;
  }

  HTMLTableRowElement* row = nullptr;
  int i = 0;
  if (index == -1) {
    row = HTMLTableRowsCollection::LastRow(*this);
    if (!row)
      return;
  } else {
    for (i = 0; i <= index; ++i) {
      row = HTMLTableRowsCollection::RowAfter(*this, row);
      if (!row)
        break;
    }
  }
  if (!row) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kIndexSizeError,
        "The index provided (" + String::Number(index) +
            ") is greater than the number of rows in the table (" +
            String::Number(i) + ").");
    return;
  }
  row->remove(exception_state);
}

void HTMLTableElement::SetNeedsTableStyleRecalc() const {
  Element* element = ElementTraversal::Next(*this, this);
  while (element) {
    element->SetNeedsStyleRecalc(
        kLocalStyleChange,
        StyleChangeReasonForTracing::FromAttribute(rulesAttr));
    if (IsHTMLTableCellElement(*element))
      element = ElementTraversal::NextSkippingChildren(*element, this);
    else
      element = ElementTraversal::Next(*element, this);
  }
}

static bool GetBordersFromFrameAttributeValue(const AtomicString& value,
                                              bool& border_top,
                                              bool& border_right,
                                              bool& border_bottom,
                                              bool& border_left) {
  border_top = false;
  border_right = false;
  border_bottom = false;
  border_left = false;

  if (DeprecatedEqualIgnoringCase(value, "above"))
    border_top = true;
  else if (DeprecatedEqualIgnoringCase(value, "below"))
    border_bottom = true;
  else if (DeprecatedEqualIgnoringCase(value, "hsides"))
    border_top = border_bottom = true;
  else if (DeprecatedEqualIgnoringCase(value, "vsides"))
    border_left = border_right = true;
  else if (DeprecatedEqualIgnoringCase(value, "lhs"))
    border_left = true;
  else if (DeprecatedEqualIgnoringCase(value, "rhs"))
    border_right = true;
  else if (DeprecatedEqualIgnoringCase(value, "box") ||
           DeprecatedEqualIgnoringCase(value, "border"))
    border_top = border_bottom = border_left = border_right = true;
  else if (!DeprecatedEqualIgnoringCase(value, "void"))
    return false;
  return true;
}

void HTMLTableElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == borderAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyBorderWidth, ParseBorderWidthAttribute(value),
        CSSPrimitiveValue::UnitType::kPixels);
  } else if (name == bordercolorAttr) {
    if (!value.IsEmpty())
      AddHTMLColorToStyle(style, CSSPropertyBorderColor, value);
  } else if (name == bgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
  } else if (name == backgroundAttr) {
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (!url.IsEmpty()) {
      UseCounter::Count(
          GetDocument(),
          WebFeature::kHTMLTableElementPresentationAttributeBackground);
      CSSImageValue* image_value =
          CSSImageValue::Create(url, GetDocument().CompleteURL(url),
                                Referrer(GetDocument().OutgoingReferrer(),
                                         GetDocument().GetReferrerPolicy()));
      style->SetProperty(
          CSSPropertyValue(GetCSSPropertyBackgroundImage(), *image_value));
    }
  } else if (name == valignAttr) {
    if (!value.IsEmpty())
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyVerticalAlign,
                                              value);
  } else if (name == cellspacingAttr) {
    if (!value.IsEmpty()) {
      AddHTMLLengthToStyle(style, CSSPropertyBorderSpacing, value,
                           kDontAllowPercentageValues);
    }
  } else if (name == alignAttr) {
    if (!value.IsEmpty()) {
      if (DeprecatedEqualIgnoringCase(value, "center")) {
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyMarginInlineStart, CSSValueAuto);
        AddPropertyToPresentationAttributeStyle(
            style, CSSPropertyMarginInlineEnd, CSSValueAuto);
      } else {
        AddPropertyToPresentationAttributeStyle(style, CSSPropertyFloat, value);
      }
    }
  } else if (name == rulesAttr) {
    // The presence of a valid rules attribute causes border collapsing to be
    // enabled.
    if (rules_attr_ != kUnsetRules)
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyBorderCollapse,
                                              CSSValueCollapse);
  } else if (name == frameAttr) {
    bool border_top;
    bool border_right;
    bool border_bottom;
    bool border_left;
    if (GetBordersFromFrameAttributeValue(value, border_top, border_right,
                                          border_bottom, border_left)) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyBorderWidth,
                                              CSSValueThin);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderTopStyle,
          border_top ? CSSValueSolid : CSSValueHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderBottomStyle,
          border_bottom ? CSSValueSolid : CSSValueHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderLeftStyle,
          border_left ? CSSValueSolid : CSSValueHidden);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyBorderRightStyle,
          border_right ? CSSValueSolid : CSSValueHidden);
    }
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

bool HTMLTableElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == bgcolorAttr ||
      name == backgroundAttr || name == valignAttr || name == vspaceAttr ||
      name == hspaceAttr || name == alignAttr || name == cellspacingAttr ||
      name == borderAttr || name == bordercolorAttr || name == frameAttr ||
      name == rulesAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLTableElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  CellBorders borders_before = GetCellBorders();
  unsigned short old_padding = padding_;

  if (name == borderAttr) {
    // FIXME: This attribute is a mess.
    border_attr_ = ParseBorderWidthAttribute(params.new_value);
  } else if (name == bordercolorAttr) {
    border_color_attr_ = !params.new_value.IsEmpty();
  } else if (name == frameAttr) {
    // FIXME: This attribute is a mess.
    bool border_top;
    bool border_right;
    bool border_bottom;
    bool border_left;
    frame_attr_ = GetBordersFromFrameAttributeValue(
        params.new_value, border_top, border_right, border_bottom, border_left);
  } else if (name == rulesAttr) {
    rules_attr_ = kUnsetRules;
    if (DeprecatedEqualIgnoringCase(params.new_value, "none"))
      rules_attr_ = kNoneRules;
    else if (DeprecatedEqualIgnoringCase(params.new_value, "groups"))
      rules_attr_ = kGroupsRules;
    else if (DeprecatedEqualIgnoringCase(params.new_value, "rows"))
      rules_attr_ = kRowsRules;
    else if (DeprecatedEqualIgnoringCase(params.new_value, "cols"))
      rules_attr_ = kColsRules;
    else if (DeprecatedEqualIgnoringCase(params.new_value, "all"))
      rules_attr_ = kAllRules;
  } else if (params.name == cellpaddingAttr) {
    if (!params.new_value.IsEmpty())
      padding_ = std::max(0, params.new_value.ToInt());
    else
      padding_ = 1;
  } else if (params.name == colsAttr) {
    // ###
  } else {
    HTMLElement::ParseAttribute(params);
  }

  if (borders_before != GetCellBorders() || old_padding != padding_) {
    shared_cell_style_ = nullptr;
    SetNeedsTableStyleRecalc();
  }
}

static CSSPropertyValueSet* CreateBorderStyle(CSSValueID value) {
  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kHTMLQuirksMode);
  style->SetProperty(CSSPropertyBorderTopStyle, value);
  style->SetProperty(CSSPropertyBorderBottomStyle, value);
  style->SetProperty(CSSPropertyBorderLeftStyle, value);
  style->SetProperty(CSSPropertyBorderRightStyle, value);
  return style;
}

const CSSPropertyValueSet*
HTMLTableElement::AdditionalPresentationAttributeStyle() {
  if (frame_attr_)
    return nullptr;

  if (!border_attr_ && !border_color_attr_) {
    // Setting the border to 'hidden' allows it to win over any border
    // set on the table's cells during border-conflict resolution.
    if (rules_attr_ != kUnsetRules) {
      DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, solid_border_style,
                          (CreateBorderStyle(CSSValueHidden)));
      return solid_border_style;
    }
    return nullptr;
  }

  if (border_color_attr_) {
    DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, solid_border_style,
                        (CreateBorderStyle(CSSValueSolid)));
    return solid_border_style;
  }
  DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, outset_border_style,
                      (CreateBorderStyle(CSSValueOutset)));
  return outset_border_style;
}

HTMLTableElement::CellBorders HTMLTableElement::GetCellBorders() const {
  switch (rules_attr_) {
    case kNoneRules:
    case kGroupsRules:
      return kNoBorders;
    case kAllRules:
      return kSolidBorders;
    case kColsRules:
      return kSolidBordersColsOnly;
    case kRowsRules:
      return kSolidBordersRowsOnly;
    case kUnsetRules:
      if (!border_attr_)
        return kNoBorders;
      if (border_color_attr_)
        return kSolidBorders;
      return kInsetBorders;
  }
  NOTREACHED();
  return kNoBorders;
}

CSSPropertyValueSet* HTMLTableElement::CreateSharedCellStyle() {
  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kHTMLQuirksMode);

  switch (GetCellBorders()) {
    case kSolidBordersColsOnly:
      style->SetProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
      style->SetProperty(CSSPropertyBorderRightWidth, CSSValueThin);
      style->SetProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
      style->SetProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
      style->SetProperty(CSSPropertyBorderColor, *CSSInheritedValue::Create());
      break;
    case kSolidBordersRowsOnly:
      style->SetProperty(CSSPropertyBorderTopWidth, CSSValueThin);
      style->SetProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
      style->SetProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
      style->SetProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
      style->SetProperty(CSSPropertyBorderColor, *CSSInheritedValue::Create());
      break;
    case kSolidBorders:
      style->SetProperty(
          CSSPropertyBorderWidth,
          *CSSPrimitiveValue::Create(1, CSSPrimitiveValue::UnitType::kPixels));
      style->SetProperty(CSSPropertyBorderStyle,
                         *CSSIdentifierValue::Create(CSSValueSolid));
      style->SetProperty(CSSPropertyBorderColor, *CSSInheritedValue::Create());
      break;
    case kInsetBorders:
      style->SetProperty(
          CSSPropertyBorderWidth,
          *CSSPrimitiveValue::Create(1, CSSPrimitiveValue::UnitType::kPixels));
      style->SetProperty(CSSPropertyBorderStyle,
                         *CSSIdentifierValue::Create(CSSValueInset));
      style->SetProperty(CSSPropertyBorderColor, *CSSInheritedValue::Create());
      break;
    case kNoBorders:
      // If 'rules=none' then allow any borders set at cell level to take
      // effect.
      break;
  }

  if (padding_)
    style->SetProperty(CSSPropertyPadding,
                       *CSSPrimitiveValue::Create(
                           padding_, CSSPrimitiveValue::UnitType::kPixels));

  return style;
}

const CSSPropertyValueSet* HTMLTableElement::AdditionalCellStyle() {
  if (!shared_cell_style_)
    shared_cell_style_ = CreateSharedCellStyle();
  return shared_cell_style_.Get();
}

static CSSPropertyValueSet* CreateGroupBorderStyle(int rows) {
  MutableCSSPropertyValueSet* style =
      MutableCSSPropertyValueSet::Create(kHTMLQuirksMode);
  if (rows) {
    style->SetProperty(CSSPropertyBorderTopWidth, CSSValueThin);
    style->SetProperty(CSSPropertyBorderBottomWidth, CSSValueThin);
    style->SetProperty(CSSPropertyBorderTopStyle, CSSValueSolid);
    style->SetProperty(CSSPropertyBorderBottomStyle, CSSValueSolid);
  } else {
    style->SetProperty(CSSPropertyBorderLeftWidth, CSSValueThin);
    style->SetProperty(CSSPropertyBorderRightWidth, CSSValueThin);
    style->SetProperty(CSSPropertyBorderLeftStyle, CSSValueSolid);
    style->SetProperty(CSSPropertyBorderRightStyle, CSSValueSolid);
  }
  return style;
}

const CSSPropertyValueSet* HTMLTableElement::AdditionalGroupStyle(bool rows) {
  if (rules_attr_ != kGroupsRules)
    return nullptr;

  if (rows) {
    DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, row_border_style,
                        (CreateGroupBorderStyle(true)));
    return row_border_style;
  }
  DEFINE_STATIC_LOCAL(Persistent<CSSPropertyValueSet>, column_border_style,
                      (CreateGroupBorderStyle(false)));
  return column_border_style;
}

bool HTMLTableElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == backgroundAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLTableElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == backgroundAttr || HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLTableElement::SubResourceAttributeName() const {
  return backgroundAttr;
}

HTMLTableRowsCollection* HTMLTableElement::rows() {
  return EnsureCachedCollection<HTMLTableRowsCollection>(kTableRows);
}

HTMLCollection* HTMLTableElement::tBodies() {
  return EnsureCachedCollection<HTMLCollection>(kTableTBodies);
}

const AtomicString& HTMLTableElement::Rules() const {
  return getAttribute(rulesAttr);
}

const AtomicString& HTMLTableElement::Summary() const {
  return getAttribute(summaryAttr);
}

void HTMLTableElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(shared_cell_style_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
