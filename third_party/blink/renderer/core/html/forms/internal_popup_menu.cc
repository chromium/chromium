// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/forms/internal_popup_menu.h"

#include "base/containers/span.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/css/css_font_selector.h"
#include "third_party/blink/renderer/core/css/css_value_id_mappings.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css/properties/longhands.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css/style_request.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/events/scoped_event_queue.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/exported/web_view_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/web_frame_widget_impl.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/html/forms/chooser_resource_loader.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/layout/custom_scrollbar.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page_popup.h"
#include "third_party/blink/renderer/core/scroll/scrollable_area.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/font_selector_client.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/geometry/rect.h"

namespace blink {

namespace {

// TODO crbug.com/516675 Add stretch to serialization

const char* FontStyleToString(FontSelectionValue slope) {
  if (slope == kItalicSlopeValue) {
    return "italic";
  }
  return "normal";
}

const char* TextTransformToString(ETextTransform transform) {
  return getValueName(PlatformEnumToCSSValueID(transform));
}

const char* TextAlignToString(ETextAlign align) {
  return getValueName(PlatformEnumToCSSValueID(align));
}

const String SerializeComputedStyleForProperty(const ComputedStyle& style,
                                               CSSPropertyID id) {
  const CSSProperty& property = CSSProperty::Get(id);
  const CSSValue* value = property.CSSValueFromComputedStyle(
      style, nullptr, false, CSSValuePhase::kResolvedValue);
  return String::Format("%s : %s;\n", property.GetPropertyName(),
                        value->CssText().Utf8().c_str());
}

const String SerializeColorScheme(const ComputedStyle& style) {
  // Only serialize known color-scheme values to make sure we do not allow
  // injections via <custom-ident> into the popup.
  StringBuilder buffer;
  bool first_added = false;
  for (const AtomicString& ident : style.ColorScheme()) {
    if (ident == AtomicString("only") || ident == AtomicString("light") ||
        ident == AtomicString("dark")) {
      if (first_added)
        buffer.Append(" ");
      else
        first_added = true;
      buffer.Append(ident);
    }
  }
  if (!first_added)
    return String("normal");
  return buffer.ToString();
}

ScrollbarPart ScrollbarPartFromPseudoId(PseudoId id) {
  switch (id) {
    case kPseudoIdScrollbar:
      return kScrollbarBGPart;
    case kPseudoIdScrollbarThumb:
      return kThumbPart;
    case kPseudoIdScrollbarTrack:
    case kPseudoIdScrollbarTrackPiece:
      return kBackTrackPart;
    default:
      break;
  }
  return kNoPart;
}

const ComputedStyle* StyleForHoveredScrollbarPart(HTMLSelectElement& element,
                                                  const ComputedStyle* style,
                                                  Scrollbar* scrollbar,
                                                  PseudoId target_id) {
  ScrollbarPart part = ScrollbarPartFromPseudoId(target_id);
  if (part == kNoPart)
    return nullptr;
  scrollbar->SetHoveredPart(part);
  const ComputedStyle* part_style = element.UncachedStyleForPseudoElement(
      StyleRequest(target_id, To<CustomScrollbar>(scrollbar), part, style));
  return part_style;
}

}  // anonymous namespace

class PopupMenuCSSFontSelector : public CSSFontSelector,
                                 private FontSelectorClient {
 public:
  PopupMenuCSSFontSelector(Document&, CSSFontSelector*);
  ~PopupMenuCSSFontSelector() override;

  // We don't override willUseFontData() for now because the old PopupListBox
  // only worked with fonts loaded when opening the popup.
  const FontData* GetFontData(const FontDescription&,
                              const FontFamily&) override;

  void Trace(Visitor*) const override;

 private:
  void FontsNeedUpdate(FontSelector*, FontInvalidationReason) override;

  Member<CSSFontSelector> owner_font_selector_;
};

PopupMenuCSSFontSelector::PopupMenuCSSFontSelector(
    Document& document,
    CSSFontSelector* owner_font_selector)
    : CSSFontSelector(document), owner_font_selector_(owner_font_selector) {
  owner_font_selector_->RegisterForInvalidationCallbacks(this);
}

PopupMenuCSSFontSelector::~PopupMenuCSSFontSelector() = default;

const FontData* PopupMenuCSSFontSelector::GetFontData(
    const FontDescription& description,
    const FontFamily& font_family) {
  return owner_font_selector_->GetFontData(description, font_family);
}

void PopupMenuCSSFontSelector::FontsNeedUpdate(FontSelector* font_selector,
                                               FontInvalidationReason reason) {
  DispatchInvalidationCallbacks(reason);
}

void PopupMenuCSSFontSelector::Trace(Visitor* visitor) const {
  visitor->Trace(owner_font_selector_);
  CSSFontSelector::Trace(visitor);
  FontSelectorClient::Trace(visitor);
}

// ----------------------------------------------------------------

class InternalPopupMenu::ItemIterationContext {
  STACK_ALLOCATED();

 public:
  ItemIterationContext(const ComputedStyle& style, SegmentedBuffer& buffer)
      : base_style_(style),
        background_color_(
            style.VisitedDependentColor(GetCSSPropertyBackgroundColor())),
        buffer_(buffer) {}

  void SerializeBaseStyle() {
    DCHECK(!is_in_group_);
    PagePopupClient::AddString("baseStyle: {", buffer_);
    if (!BaseStyle().ColorSchemeForced()) {
      AddProperty("backgroundColor", background_color_.SerializeAsCSSColor(),
                  buffer_);
      AddProperty("color",
                  BaseStyle()
                      .VisitedDependentColor(GetCSSPropertyColor())
                      .SerializeAsCSSColor(),
                  buffer_);
    }
    AddProperty("textTransform",
                String(TextTransformToString(BaseStyle().TextTransform())),
                buffer_);
    AddProperty("textAlign",
                String(TextAlignToString(BaseStyle().GetTextAlign(false))),
                buffer_);
    AddProperty("fontSize", BaseFont().ComputedPixelSize(), buffer_);
    AddProperty("fontStyle", String(FontStyleToString(BaseFont().Style())),
                buffer_);
    AddProperty("fontVariant",
                BaseFont().VariantCaps() == FontDescription::kSmallCaps
                    ? String("small-caps")
                    : String(),
                buffer_);

    AddProperty(
        "fontFamily",
        ComputedStyleUtils::ValueForFontFamily(BaseFont().Family())->CssText(),
        buffer_);

    PagePopupClient::AddString("},\n", buffer_);
  }

  Color BackgroundColor() const {
    return is_in_group_ ? group_style_->VisitedDependentColor(
                              GetCSSPropertyBackgroundColor())
                        : background_color_;
  }
  // Do not use baseStyle() for background-color, use backgroundColor()
  // instead.
  const ComputedStyle& BaseStyle() {
    return is_in_group_ ? *group_style_ : base_style_;
  }
  const FontDescription& BaseFont() {
    return is_in_group_ ? group_style_->GetFontDescription()
                        : base_style_.GetFontDescription();
  }
  void StartGroupChildren(const ComputedStyle& group_style) {
    DCHECK(!is_in_group_);
    PagePopupClient::AddString("children: [", buffer_);
    is_in_group_ = true;
    group_style_ = &group_style;
  }
  void FinishGroupIfNecessary() {
    if (!is_in_group_)
      return;
    PagePopupClient::AddString("],},\n", buffer_);
    is_in_group_ = false;
    group_style_ = nullptr;
  }

  const ComputedStyle& base_style_;
  Color background_color_;
  const ComputedStyle* group_style_;

  unsigned list_index_ = 0;
  bool is_in_group_ = false;
  SegmentedBuffer& buffer_;
};

// ----------------------------------------------------------------

InternalPopupMenu::InternalPopupMenu(ChromeClient* chrome_client,
                                     HTMLSelectElement& owner_element)
    : chrome_client_(chrome_client),
      owner_element_(owner_element),
      popup_(nullptr),
      needs_update_(false) {}

InternalPopupMenu::~InternalPopupMenu() {
  DCHECK(!popup_);
}

void InternalPopupMenu::Trace(Visitor* visitor) const {
  visitor->Trace(chrome_client_);
  visitor->Trace(owner_element_);
  PopupMenu::Trace(visitor);
}

void InternalPopupMenu::WriteDocument(SegmentedBuffer& data) {
  HTMLSelectElement& owner_element = *owner_element_;
  // When writing the document, we ensure the ComputedStyle of the select
  // element's items (see AddElementStyle). This requires a style-clean tree.
  // See Element::EnsureComputedStyle for further explanation.
  DCHECK(!owner_element.GetDocument().NeedsLayoutTreeUpdate());
  gfx::Rect anchor_rect_in_screen = chrome_client_->LocalRootToScreenDIPs(
      owner_element.VisibleBoundsInLocalRoot(),
      owner_element.GetDocument().View());

  float scale_factor = chrome_client_->WindowToViewportScalar(
      owner_element.GetDocument().GetFrame(), 1.f);
  PagePopupClient::AddString("<!DOCTYPE html><head><meta charset='UTF-8'>",
                             data);

  const ComputedStyle& owner_style = owner_element.ComputedStyleRef();

  // Add the color-scheme of the <select> element to the popup as a color-scheme
  // meta.
  PagePopupClient::AddString("<meta name='color-scheme' content='only ", data);
  PagePopupClient::AddString(owner_style.DarkColorScheme() ? "dark" : "light",
                             data);
  PagePopupClient::AddString("'><style>\n", data);

  LayoutObject* owner_layout = owner_element.GetLayoutObject();

  std::pair<PseudoId, const String> targets[] = {
      {kPseudoIdScrollbar, "select::-webkit-scrollbar"},
      {kPseudoIdScrollbarThumb, "select::-webkit-scrollbar-thumb"},
      {kPseudoIdScrollbarTrack, "select::-webkit-scrollbar-track"},
      {kPseudoIdScrollbarTrackPiece, "select::-webkit-scrollbar-track-piece"},
      {kPseudoIdScrollbarCorner, "select::-webkit-scrollbar-corner"}};

  Scrollbar* temp_scrollbar = nullptr;
  const LayoutBox* box = owner_element.InnerElement().GetLayoutBox();
  if (box && box->GetScrollableArea()) {
    if (ScrollableArea* scrollable = box->GetScrollableArea()) {
      temp_scrollbar = MakeGarbageCollected<CustomScrollbar>(
          scrollable, kVerticalScrollbar, box);
    }
  }
  for (auto target : targets) {
    if (const ComputedStyle* style =
            owner_layout->GetCachedPseudoElementStyle(target.first)) {
      AppendOwnerElementPseudoStyles(target.second, data, *style);
    }
    // For Pseudo-class styles, Style should be calculated via that status.
    if (temp_scrollbar) {
      const ComputedStyle* part_style = StyleForHoveredScrollbarPart(
          owner_element, owner_element.GetComputedStyle(), temp_scrollbar,
          target.first);
      if (part_style) {
        AppendOwnerElementPseudoStyles(target.second + ":hover", data,
                                       *part_style);
      }
    }
  }
  if (temp_scrollbar)
    temp_scrollbar->DisconnectFromScrollableArea();

  data.Append(ChooserResourceLoader::GetPickerCommonStyleSheet());
  data.Append(ChooserResourceLoader::GetListPickerStyleSheet());
  if (taller_options_) {
    int padding = static_cast<int>(roundf(4 * scale_factor));
    int min_height = static_cast<int>(roundf(24 * scale_factor));
    PagePopupClient::AddString(String::Format("option, optgroup {"
                                              "padding-top: %dpx;"
                                              "}\n"
                                              "option {"
                                              "padding-bottom: %dpx;"
                                              "min-block-size: %dpx;"
                                              "display: flex;"
                                              "align-items: center;"
                                              "}\n",
                                              padding, padding, min_height),
                               data);
    // Sets the min target size of <option> to 24x24 CSS pixels to meet
    // Accessibility standards.
    if (RuntimeEnabledFeatures::SelectOptionAccessibilityTargetSizeEnabled()) {
      PagePopupClient::AddString(
          String::Format("option {"
                         "display: block;"
                         "align-content: center;"
                         "min-inline-size: %dpx;"
                         "min-block-size: %dpx;"
                         "box-sizing: border-box;"
                         "}\n",
                         min_height, std::max(24, min_height)),
          data);
    }
  }

  PagePopupClient::AddString(
      "</style></head><body><div id=main>Loading...</div><script>\n"
      "window.dialogArguments = {\n",
      data);
  AddProperty("selectedIndex", owner_element.SelectedListIndex(), data);
  ItemIterationContext context(owner_style, data);
  context.SerializeBaseStyle();
  PagePopupClient::AddString("children: [\n", data);
  const HeapVector<Member<HTMLElement>>& items = owner_element.GetListItems();
  for (; context.list_index_ < items.size(); ++context.list_index_) {
    Element& child = *items[context.list_index_];
    if (!IsA<HTMLOptGroupElement>(child.parentNode()))
      context.FinishGroupIfNecessary();
    if (auto* option = DynamicTo<HTMLOptionElement>(child))
      AddOption(context, *option);
    else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(child))
      AddOptGroup(context, *optgroup);
    else if (auto* hr = DynamicTo<HTMLHRElement>(child))
      AddSeparator(context, *hr);
  }
  context.FinishGroupIfNecessary();
  PagePopupClient::AddString("],\n", data);

  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  AddProperty("zoomFactor", 1, data);
  AddProperty("scaleFactor", scale_factor, data);
  bool is_rtl = !owner_style.IsLeftToRightDirection();
  AddProperty("isRTL", is_rtl, data);
  AddProperty("paddingStart",
              is_rtl ? owner_element.ClientPaddingRight().ToDouble()
                     : owner_element.ClientPaddingLeft().ToDouble(),
              data);
  PagePopupClient::AddString("};\n", data);
  data.Append(ChooserResourceLoader::GetPickerCommonJS());
  data.Append(ChooserResourceLoader::GetListPickerJS());

  PagePopupClient::AddString("</script></body>\n", data);
}

void InternalPopupMenu::AddElementStyle(ItemIterationContext& context,
                                        HTMLElement& element) {
  const ComputedStyle* style = owner_element_->ItemComputedStyle(element);
  DCHECK(style);
  SegmentedBuffer& data = context.buffer_;
  // TODO(tkent): We generate unnecessary "style: {\n},\n" even if no
  // additional style.
  PagePopupClient::AddString("style: {\n", data);
  if (style->UsedVisibility() == EVisibility::kHidden) {
    AddProperty("visibility", String("hidden"), data);
  }
  if (style->Display() == EDisplay::kNone) {
    AddProperty("display", String("none"), data);
  }
  const ComputedStyle& base_style = context.BaseStyle();
  if (base_style.Direction() != style->Direction()) {
    AddProperty(
        "direction",
        String(style->Direction() == TextDirection::kRtl ? "rtl" : "ltr"),
        data);
  }
  if (IsOverride(style->GetUnicodeBidi()))
    AddProperty("unicodeBidi", String("bidi-override"), data);

  if (!base_style.ColorSchemeForced()) {
    bool color_applied = false;
    Color foreground_color =
        style->VisitedDependentColor(GetCSSPropertyColor());
    if (base_style.VisitedDependentColor(GetCSSPropertyColor()) !=
        foreground_color) {
      AddProperty("color", foreground_color.SerializeAsCSSColor(), data);
      color_applied = true;
    }
    Color background_color =
        style->VisitedDependentColor(GetCSSPropertyBackgroundColor());
    if (background_color != Color::kTransparent &&
        (context.BackgroundColor() != background_color)) {
      AddProperty("backgroundColor", background_color.SerializeAsCSSColor(),
                  data);
      color_applied = true;
    }
    if (color_applied)
      AddProperty("colorScheme", SerializeColorScheme(*style), data);
  }

  const FontDescription& base_font = context.BaseFont();
  const FontDescription& font_description =
      style->GetFont().GetFontDescription();
  if (base_font.ComputedPixelSize() != font_description.ComputedPixelSize()) {
    // We don't use FontDescription::specifiedSize() because this element
    // might have its own zoom level.
    AddProperty("fontSize", font_description.ComputedPixelSize(), data);
  }
  // Our UA stylesheet has font-weight:normal for OPTION.
  if (kNormalWeightValue != font_description.Weight()) {
    AddProperty("fontWeight", font_description.Weight().ToString(), data);
  }
  if (base_font.Family() != font_description.Family()) {
    AddProperty(
        "fontFamily",
        ComputedStyleUtils::ValueForFontFamily(font_description.Family())
            ->CssText(),
        data);
  }
  if (base_font.Style() != font_description.Style()) {
    AddProperty("fontStyle",
                String(FontStyleToString(font_description.Style())), data);
  }

  if (base_font.VariantCaps() != font_description.VariantCaps() &&
      font_description.VariantCaps() == FontDescription::kSmallCaps)
    AddProperty("fontVariant", String("small-caps"), data);

  if (base_style.TextTransform() != style->TextTransform()) {
    AddProperty("textTransform",
                String(TextTransformToString(style->TextTransform())), data);
  }
  if (base_style.GetTextAlign(false) != style->GetTextAlign(false)) {
    AddProperty("textAlign",
                String(TextAlignToString(style->GetTextAlign(false))), data);
  }

  PagePopupClient::AddString("},\n", data);
}

void InternalPopupMenu::AddOption(ItemIterationContext& context,
                                  HTMLOptionElement& element) {
  SegmentedBuffer& data = context.buffer_;
  PagePopupClient::AddString("{", data);
  AddProperty("label", element.DisplayLabel(), data);
  AddProperty("value", context.list_index_, data);
  if (!element.title().empty())
    AddProperty("title", element.title(), data);
  const AtomicString& aria_label =
      element.FastGetAttribute(html_names::kAriaLabelAttr);
  if (!aria_label.empty())
    AddProperty("ariaLabel", aria_label, data);
  if (element.IsDisabledFormControl())
    AddProperty("disabled", true, data);
  AddElementStyle(context, element);
  PagePopupClient::AddString("},", data);
}

void InternalPopupMenu::AddOptGroup(ItemIterationContext& context,
                                    HTMLOptGroupElement& element) {
  SegmentedBuffer& data = context.buffer_;
  PagePopupClient::AddString("{\n", data);
  PagePopupClient::AddString("type: \"optgroup\",\n", data);
  AddProperty("label", element.GroupLabelText(), data);
  AddProperty("title", element.title(), data);
  AddProperty("ariaLabel", element.FastGetAttribute(html_names::kAriaLabelAttr),
              data);
  AddProperty("disabled", element.IsDisabledFormControl(), data);
  AddElementStyle(context, element);
  context.StartGroupChildren(*owner_element_->ItemComputedStyle(element));
  // We should call ItemIterationContext::finishGroupIfNecessary() later.
}

void InternalPopupMenu::AddSeparator(ItemIterationContext& context,
                                     HTMLHRElement& element) {
  SegmentedBuffer& data = context.buffer_;
  PagePopupClient::AddString("{\n", data);
  PagePopupClient::AddString("type: \"separator\",\n", data);
  AddProperty("title", element.title(), data);
  AddProperty("ariaLabel", element.FastGetAttribute(html_names::kAriaLabelAttr),
              data);
  AddProperty("disabled", element.IsDisabledFormControl(), data);
  AddElementStyle(context, element);
  PagePopupClient::AddString("},\n", data);
}

void InternalPopupMenu::AppendOwnerElementPseudoStyles(
    const String& target,
    SegmentedBuffer& data,
    const ComputedStyle& style) {
  PagePopupClient::AddString(target + "{ \n", data);

  const CSSPropertyID serialize_targets[] = {
      CSSPropertyID::kDisplay,        CSSPropertyID::kBackgroundColor,
      CSSPropertyID::kWidth,          CSSPropertyID::kBorderBottom,
      CSSPropertyID::kBorderLeft,     CSSPropertyID::kBorderRight,
      CSSPropertyID::kBorderTop,      CSSPropertyID::kBorderRadius,
      CSSPropertyID::kBackgroundClip, CSSPropertyID::kBoxShadow};

  for (CSSPropertyID id : serialize_targets) {
    PagePopupClient::AddString(SerializeComputedStyleForProperty(style, id),
                               data);
  }

  PagePopupClient::AddString("}\n", data);
}

CSSFontSelector* InternalPopupMenu::CreateCSSFontSelector(
    Document& popup_document) {
  Document& owner_document = OwnerElement().GetDocument();
  return MakeGarbageCollected<PopupMenuCSSFontSelector>(
      popup_document, owner_document.GetStyleEngine().GetFontSelector());
}

void InternalPopupMenu::SetValueAndClosePopup(int num_value,
                                              const String& string_value) {
  DCHECK(popup_);
  DCHECK(owner_element_);
  if (!string_value.empty()) {
    bool success;
    int list_index = string_value.ToInt(&success);
    DCHECK(success);

    EventQueueScope scope;
    owner_element_->SelectOptionByPopup(list_index);
    if (popup_)
      chrome_client_->ClosePagePopup(popup_);
    // 'change' event is dispatched here.  For compatbility with
    // Angular 1.2, we need to dispatch a change event before
    // mouseup/click events.
  } else {
    if (popup_)
      chrome_client_->ClosePagePopup(popup_);
  }
  // We dispatch events on the owner element to match the legacy behavior.
  // Other browsers dispatch click events before and after showing the popup.
  if (owner_element_) {
    WebMouseEvent event;
    event.SetFrameScale(1);
    PhysicalRect bounding_box = owner_element_->BoundingBox();
    event.SetPositionInWidget(bounding_box.X(), bounding_box.Y());
    event.SetTimeStamp(base::TimeTicks::Now());
    Element* owner = &OwnerElement();
    if (LocalFrame* frame = owner->GetDocument().GetFrame()) {
      frame->GetEventHandler().HandleTargetedMouseEvent(
          owner, event, event_type_names::kMouseup, Vector<WebMouseEvent>(),
          Vector<WebMouseEvent>());
      frame->GetEventHandler().HandleTargetedMouseEvent(
          owner, event, event_type_names::kClick, Vector<WebMouseEvent>(),
          Vector<WebMouseEvent>());
    }
  }
}

void InternalPopupMenu::SetValue(const String& value) {
  DCHECK(owner_element_);
  bool success;
  int list_index = value.ToInt(&success);
  DCHECK(success);
  owner_element_->ProvisionalSelectionChanged(list_index);
}

void InternalPopupMenu::DidClosePopup() {
  // Clearing popup_ first to prevent from trying to close the popup again.
  popup_ = nullptr;
  if (owner_element_)
    owner_element_->PopupDidHide();
}

Element& InternalPopupMenu::OwnerElement() {
  return *owner_element_;
}

ChromeClient& InternalPopupMenu::GetChromeClient() {
  return *chrome_client_;
}

Locale& InternalPopupMenu::GetLocale() {
  return Locale::DefaultLocale();
}

void InternalPopupMenu::CancelPopup() {
  if (popup_)
    chrome_client_->ClosePagePopup(popup_);
  if (owner_element_)
    owner_element_->PopupDidCancel();
}

void InternalPopupMenu::Dispose() {
  if (popup_)
    chrome_client_->ClosePagePopup(popup_);
}

void InternalPopupMenu::Show(PopupMenu::ShowEventType type) {
  DCHECK(!popup_);
  taller_options_ =
      type == PopupMenu::kTouch ||
      RuntimeEnabledFeatures::ForceTallerSelectPopupEnabled() ||
      RuntimeEnabledFeatures::SelectOptionAccessibilityTargetSizeEnabled();
  popup_ = chrome_client_->OpenPagePopup(this);
}

void InternalPopupMenu::Hide() {
  CancelPopup();
}

void InternalPopupMenu::UpdateFromElement(UpdateReason) {
  needs_update_ = true;
}

AXObject* InternalPopupMenu::PopupRootAXObject() const {
  return popup_ ? popup_->RootAXObject(owner_element_) : nullptr;
}

void InternalPopupMenu::Update(bool force_update) {
  if (!popup_ || !owner_element_ || (!needs_update_ && !force_update))
    return;
  // disconnectClient() might have been called.
  if (!owner_element_)
    return;
  needs_update_ = false;

  if (!gfx::Rect(gfx::Point(), OwnerElement().GetDocument().View()->Size())
           .Intersects(OwnerElement().PixelSnappedBoundingBox())) {
    Hide();
    return;
  }

  SegmentedBuffer data;
  PagePopupClient::AddString("window.updateData = {\n", data);
  PagePopupClient::AddString("type: \"update\",\n", data);
  ItemIterationContext context(*owner_element_->GetComputedStyle(), data);
  context.SerializeBaseStyle();
  PagePopupClient::AddString("children: [", data);
  const HeapVector<Member<HTMLElement>>& items = owner_element_->GetListItems();
  for (; context.list_index_ < items.size(); ++context.list_index_) {
    Element& child = *items[context.list_index_];
    if (!IsA<HTMLOptGroupElement>(child.parentNode()))
      context.FinishGroupIfNecessary();
    if (auto* option = DynamicTo<HTMLOptionElement>(child))
      AddOption(context, *option);
    else if (auto* optgroup = DynamicTo<HTMLOptGroupElement>(child))
      AddOptGroup(context, *optgroup);
    else if (auto* hr = DynamicTo<HTMLHRElement>(child))
      AddSeparator(context, *hr);
  }
  context.FinishGroupIfNecessary();
  PagePopupClient::AddString("],\n", data);
  gfx::Rect anchor_rect_in_screen = chrome_client_->LocalRootToScreenDIPs(
      owner_element_->VisibleBoundsInLocalRoot(),
      OwnerElement().GetDocument().View());
  AddProperty("anchorRectInScreen", anchor_rect_in_screen, data);
  PagePopupClient::AddString("}\n", data);
  Vector<char> flatten_data = std::move(data).CopyAs<Vector<char>>();
  popup_->PostMessageToPopup(
      String::FromUTF8(base::as_string_view(flatten_data)));
}

void InternalPopupMenu::DisconnectClient() {
  owner_element_ = nullptr;
  // Cannot be done during finalization, so instead done when the
  // layout object is destroyed and disconnected.
  Dispose();
}

void InternalPopupMenu::SetMenuListOptionsBoundsInAXTree(
    WTF::Vector<gfx::Rect>& options_bounds,
    gfx::Point popup_origin) {
  WebFrameWidgetImpl* widget =
      WebLocalFrameImpl::FromFrame(owner_element_->GetDocument().GetFrame())
          ->LocalRootFrameWidget();
  if (!widget) {
    return;
  }

  // Convert popup origin point from screen coordinates to blink coordinates.
  gfx::Rect widget_view_rect = widget->ViewRect();
  popup_origin.Offset(-widget_view_rect.x(), -widget_view_rect.y());
  popup_origin = widget->DIPsToRoundedBlinkSpace(popup_origin);

  // Factor in the scroll offset of the select's window.
  LocalDOMWindow* window = owner_element_->GetDocument().domWindow();
  const float page_zoom_factor =
      owner_element_->GetDocument().GetFrame()->LayoutZoomFactor();
  popup_origin.Offset(window->scrollX() * page_zoom_factor,
                      window->scrollY() * page_zoom_factor);

  // We need to make sure we take into account any iframes. Since OOPIF and
  // srcdoc iframes aren't allowed to access the root viewport, we need to
  // iterate through the frame owner's parent nodes and accumulate the offsets.
  Frame* frame = owner_element_->GetDocument().GetFrame();
  while (frame->Owner()) {
    if (auto* frame_view = frame->View()) {
        gfx::Point frame_point = frame_view->Location();
        popup_origin.Offset(-frame_point.x(), -frame_point.y());
    }
    frame = frame->Parent();
  }

  for (auto& option_bounds : options_bounds) {
    option_bounds.Offset(popup_origin.x(), popup_origin.y());
  }

  AXObjectCache* cache = owner_element_->GetDocument().ExistingAXObjectCache();
  if (cache) {
    cache->SetMenuListOptionsBounds(owner_element_, options_bounds);
  }
}

}  // namespace blink
