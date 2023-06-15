/*
 * Copyright (C) 2008, 2009, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/modules/accessibility/ax_object.h"

#include <algorithm>
#include <ostream>

#include "base/auto_reset.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/mojom/frame/user_activation_notification_type.mojom-blink.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment_engine.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/frame/visual_viewport.h"
#include "third_party/blink/renderer/core/fullscreen/fullscreen.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/portal/html_portal_element.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/scroll/scroll_into_view_util.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#if DCHECK_IS_ON()
#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"
#endif
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

namespace {

#if defined(AX_FAIL_FAST_BUILD)
// TODO(accessibility) Move this out of DEBUG by having a new enum in
// ax_enums.mojom, and a matching ToString() in ax_enum_utils, as well as move
// out duplicate code of String IgnoredReasonName(AXIgnoredReason reason) in
// inspector_type_builder_helper.cc.
String IgnoredReasonName(AXIgnoredReason reason) {
  switch (reason) {
    case kAXActiveFullscreenElement:
      return "activeFullscreenElement";
    case kAXActiveModalDialog:
      return "activeModalDialog";
    case kAXAriaModalDialog:
      return "activeAriaModalDialog";
    case kAXAriaHiddenElement:
      return "ariaHiddenElement";
    case kAXAriaHiddenSubtree:
      return "ariaHiddenSubtree";
    case kAXEmptyAlt:
      return "emptyAlt";
    case kAXEmptyText:
      return "emptyText";
    case kAXInertElement:
      return "inertElement";
    case kAXInertSubtree:
      return "inertSubtree";
    case kAXLabelContainer:
      return "labelContainer";
    case kAXLabelFor:
      return "labelFor";
    case kAXNotRendered:
      return "notRendered";
    case kAXNotVisible:
      return "notVisible";
    case kAXPresentational:
      return "presentationalRole";
    case kAXProbablyPresentational:
      return "probablyPresentational";
    case kAXUninteresting:
      return "uninteresting";
  }
  NOTREACHED();
  return "";
}

String GetIgnoredReasonsDebugString(AXObject::IgnoredReasons& reasons) {
  if (reasons.size() == 0)
    return "";
  String string_builder = "(";
  for (wtf_size_t count = 0; count < reasons.size(); count++) {
    if (count > 0)
      string_builder = string_builder + ',';
    string_builder = string_builder + IgnoredReasonName(reasons[count].reason);
  }
  string_builder = string_builder + ")";
  return string_builder;
}

#endif

String GetNodeString(Node* node) {
  if (node->IsTextNode()) {
    String string_builder = "\"";
    string_builder = string_builder + node->nodeValue();
    string_builder = string_builder + "\"";
    return string_builder;
  }

  Element* element = DynamicTo<Element>(node);
  if (!element) {
    return To<Document>(node)->IsLoadCompleted() ? "#document"
                                                 : "#document (loading)";
  }

  String string_builder = "<";

  string_builder = string_builder + element->tagName().LowerASCII();
  // Cannot safely get @class from SVG elements.
  if (!element->IsSVGElement() &&
      element->FastHasAttribute(html_names::kClassAttr)) {
    string_builder = string_builder + "." +
                     element->FastGetAttribute(html_names::kClassAttr);
  }
  if (element->FastHasAttribute(html_names::kIdAttr)) {
    string_builder =
        string_builder + "#" + element->FastGetAttribute(html_names::kIdAttr);
  }
  return string_builder + ">";
}

#if DCHECK_IS_ON()
bool IsValidRole(ax::mojom::blink::Role role) {
  // Check for illegal roles that should not be assigned in Blink.
  switch (role) {
    case ax::mojom::blink::Role::kCaret:
    case ax::mojom::blink::Role::kClient:
    case ax::mojom::blink::Role::kColumn:
    case ax::mojom::blink::Role::kDesktop:
    case ax::mojom::blink::Role::kKeyboard:
    case ax::mojom::blink::Role::kImeCandidate:
    case ax::mojom::blink::Role::kListGrid:
    case ax::mojom::blink::Role::kPane:
    case ax::mojom::blink::Role::kPdfActionableHighlight:
    case ax::mojom::blink::Role::kPdfRoot:
    case ax::mojom::blink::Role::kTableHeaderContainer:
    case ax::mojom::blink::Role::kTitleBar:
    case ax::mojom::blink::Role::kUnknown:
    case ax::mojom::blink::Role::kWebView:
    case ax::mojom::blink::Role::kWindow:
      return false;
    default:
      return true;
  }
}
#endif

using RoleHashTraits =
    EnumHashTraits<ax::mojom::blink::Role, ax::mojom::blink::Role::kUnknown>;

constexpr wtf_size_t kNumRoles =
    static_cast<wtf_size_t>(ax::mojom::blink::Role::kMaxValue) + 1;

using ARIARoleMap = HashMap<String,
                            ax::mojom::blink::Role,
                            CaseFoldingHashTraits<String>,
                            RoleHashTraits>;

struct RoleEntry {
  const char* role_name;
  ax::mojom::blink::Role role;
};

// Mapping of ARIA role name to internal role name.
// This is used for the following:
// 1. Map from an ARIA role to the internal role when building tree.
// 2. Map from an internal role to an ARIA role name, for debugging, the
//    xml-roles object attribute and element.computedRole.
const RoleEntry kAriaRoles[] = {
    {"alert", ax::mojom::blink::Role::kAlert},
    {"alertdialog", ax::mojom::blink::Role::kAlertDialog},
    {"application", ax::mojom::blink::Role::kApplication},
    {"article", ax::mojom::blink::Role::kArticle},
    {"banner", ax::mojom::blink::Role::kBanner},
    {"blockquote", ax::mojom::blink::Role::kBlockquote},
    {"button", ax::mojom::blink::Role::kButton},
    {"caption", ax::mojom::blink::Role::kCaption},
    {"cell", ax::mojom::blink::Role::kCell},
    {"code", ax::mojom::blink::Role::kCode},
    {"checkbox", ax::mojom::blink::Role::kCheckBox},
    {"columnheader", ax::mojom::blink::Role::kColumnHeader},
    {"combobox", ax::mojom::blink::Role::kComboBoxGrouping},
    {"comment", ax::mojom::blink::Role::kComment},
    {"complementary", ax::mojom::blink::Role::kComplementary},
    {"contentinfo", ax::mojom::blink::Role::kContentInfo},
    {"definition", ax::mojom::blink::Role::kDefinition},
    {"deletion", ax::mojom::blink::Role::kContentDeletion},
    {"dialog", ax::mojom::blink::Role::kDialog},
    {"directory", ax::mojom::blink::Role::kDirectory},
    // -------------------------------------------------
    // DPub Roles:
    // www.w3.org/TR/dpub-aam-1.0/#mapping_role_table
    {"doc-abstract", ax::mojom::blink::Role::kDocAbstract},
    {"doc-acknowledgments", ax::mojom::blink::Role::kDocAcknowledgments},
    {"doc-afterword", ax::mojom::blink::Role::kDocAfterword},
    {"doc-appendix", ax::mojom::blink::Role::kDocAppendix},
    {"doc-backlink", ax::mojom::blink::Role::kDocBackLink},
    // Deprecated in DPUB-ARIA 1.1. Use a listitem inside of a doc-bibliography.
    {"doc-biblioentry", ax::mojom::blink::Role::kDocBiblioEntry},
    {"doc-bibliography", ax::mojom::blink::Role::kDocBibliography},
    {"doc-biblioref", ax::mojom::blink::Role::kDocBiblioRef},
    {"doc-chapter", ax::mojom::blink::Role::kDocChapter},
    {"doc-colophon", ax::mojom::blink::Role::kDocColophon},
    {"doc-conclusion", ax::mojom::blink::Role::kDocConclusion},
    {"doc-cover", ax::mojom::blink::Role::kDocCover},
    {"doc-credit", ax::mojom::blink::Role::kDocCredit},
    {"doc-credits", ax::mojom::blink::Role::kDocCredits},
    {"doc-dedication", ax::mojom::blink::Role::kDocDedication},
    // Deprecated in DPUB-ARIA 1.1. Use a listitem inside of a doc-endnotes.
    {"doc-endnote", ax::mojom::blink::Role::kDocEndnote},
    {"doc-endnotes", ax::mojom::blink::Role::kDocEndnotes},
    {"doc-epigraph", ax::mojom::blink::Role::kDocEpigraph},
    {"doc-epilogue", ax::mojom::blink::Role::kDocEpilogue},
    {"doc-errata", ax::mojom::blink::Role::kDocErrata},
    {"doc-example", ax::mojom::blink::Role::kDocExample},
    {"doc-footnote", ax::mojom::blink::Role::kDocFootnote},
    {"doc-foreword", ax::mojom::blink::Role::kDocForeword},
    {"doc-glossary", ax::mojom::blink::Role::kDocGlossary},
    {"doc-glossref", ax::mojom::blink::Role::kDocGlossRef},
    {"doc-index", ax::mojom::blink::Role::kDocIndex},
    {"doc-introduction", ax::mojom::blink::Role::kDocIntroduction},
    {"doc-noteref", ax::mojom::blink::Role::kDocNoteRef},
    {"doc-notice", ax::mojom::blink::Role::kDocNotice},
    {"doc-pagebreak", ax::mojom::blink::Role::kDocPageBreak},
    {"doc-pagefooter", ax::mojom::blink::Role::kDocPageFooter},
    {"doc-pageheader", ax::mojom::blink::Role::kDocPageHeader},
    {"doc-pagelist", ax::mojom::blink::Role::kDocPageList},
    {"doc-part", ax::mojom::blink::Role::kDocPart},
    {"doc-preface", ax::mojom::blink::Role::kDocPreface},
    {"doc-prologue", ax::mojom::blink::Role::kDocPrologue},
    {"doc-pullquote", ax::mojom::blink::Role::kDocPullquote},
    {"doc-qna", ax::mojom::blink::Role::kDocQna},
    {"doc-subtitle", ax::mojom::blink::Role::kDocSubtitle},
    {"doc-tip", ax::mojom::blink::Role::kDocTip},
    {"doc-toc", ax::mojom::blink::Role::kDocToc},
    // End DPub roles.
    // -------------------------------------------------
    {"document", ax::mojom::blink::Role::kDocument},
    {"emphasis", ax::mojom::blink::Role::kEmphasis},
    {"feed", ax::mojom::blink::Role::kFeed},
    {"figure", ax::mojom::blink::Role::kFigure},
    {"form", ax::mojom::blink::Role::kForm},
    {"generic", ax::mojom::blink::Role::kGenericContainer},
    // -------------------------------------------------
    // ARIA Graphics module roles:
    // https://rawgit.com/w3c/graphics-aam/master/
    {"graphics-document", ax::mojom::blink::Role::kGraphicsDocument},
    {"graphics-object", ax::mojom::blink::Role::kGraphicsObject},
    {"graphics-symbol", ax::mojom::blink::Role::kGraphicsSymbol},
    // End ARIA Graphics module roles.
    // -------------------------------------------------
    {"grid", ax::mojom::blink::Role::kGrid},
    {"gridcell", ax::mojom::blink::Role::kCell},
    {"group", ax::mojom::blink::Role::kGroup},
    {"heading", ax::mojom::blink::Role::kHeading},
    {"img", ax::mojom::blink::Role::kImage},
    {"insertion", ax::mojom::blink::Role::kContentInsertion},
    {"link", ax::mojom::blink::Role::kLink},
    {"list", ax::mojom::blink::Role::kList},
    {"listbox", ax::mojom::blink::Role::kListBox},
    {"listitem", ax::mojom::blink::Role::kListItem},
    {"log", ax::mojom::blink::Role::kLog},
    {"main", ax::mojom::blink::Role::kMain},
    {"marquee", ax::mojom::blink::Role::kMarquee},
    {"math", ax::mojom::blink::Role::kMath},
    {"menu", ax::mojom::blink::Role::kMenu},
    {"menubar", ax::mojom::blink::Role::kMenuBar},
    {"menuitem", ax::mojom::blink::Role::kMenuItem},
    {"menuitemcheckbox", ax::mojom::blink::Role::kMenuItemCheckBox},
    {"menuitemradio", ax::mojom::blink::Role::kMenuItemRadio},
    {"mark", ax::mojom::blink::Role::kMark},
    {"meter", ax::mojom::blink::Role::kMeter},
    {"navigation", ax::mojom::blink::Role::kNavigation},
    // role="presentation" is the same as role="none".
    {"presentation", ax::mojom::blink::Role::kNone},
    // role="none" is listed after role="presentation", so that it is the
    // canonical name in devtools and tests.
    {"none", ax::mojom::blink::Role::kNone},
    {"note", ax::mojom::blink::Role::kNote},
    {"option", ax::mojom::blink::Role::kListBoxOption},
    {"paragraph", ax::mojom::blink::Role::kParagraph},
    {"progressbar", ax::mojom::blink::Role::kProgressIndicator},
    {"radio", ax::mojom::blink::Role::kRadioButton},
    {"radiogroup", ax::mojom::blink::Role::kRadioGroup},
    {"region", ax::mojom::blink::Role::kRegion},
    {"row", ax::mojom::blink::Role::kRow},
    {"rowgroup", ax::mojom::blink::Role::kRowGroup},
    {"rowheader", ax::mojom::blink::Role::kRowHeader},
    {"scrollbar", ax::mojom::blink::Role::kScrollBar},
    {"search", ax::mojom::blink::Role::kSearch},
    {"searchbox", ax::mojom::blink::Role::kSearchBox},
    {"separator", ax::mojom::blink::Role::kSplitter},
    {"slider", ax::mojom::blink::Role::kSlider},
    {"spinbutton", ax::mojom::blink::Role::kSpinButton},
    {"status", ax::mojom::blink::Role::kStatus},
    {"strong", ax::mojom::blink::Role::kStrong},
    {"subscript", ax::mojom::blink::Role::kSubscript},
    {"suggestion", ax::mojom::blink::Role::kSuggestion},
    {"superscript", ax::mojom::blink::Role::kSuperscript},
    {"switch", ax::mojom::blink::Role::kSwitch},
    {"tab", ax::mojom::blink::Role::kTab},
    {"table", ax::mojom::blink::Role::kTable},
    {"tablist", ax::mojom::blink::Role::kTabList},
    {"tabpanel", ax::mojom::blink::Role::kTabPanel},
    {"term", ax::mojom::blink::Role::kTerm},
    {"textbox", ax::mojom::blink::Role::kTextField},
    {"time", ax::mojom::blink::Role::kTime},
    {"timer", ax::mojom::blink::Role::kTimer},
    {"toolbar", ax::mojom::blink::Role::kToolbar},
    {"tooltip", ax::mojom::blink::Role::kTooltip},
    {"tree", ax::mojom::blink::Role::kTree},
    {"treegrid", ax::mojom::blink::Role::kTreeGrid},
    {"treeitem", ax::mojom::blink::Role::kTreeItem}};

// More friendly names for debugging. These are roles which don't map from
// the ARIA role name to the internal role when building the tree, but when
// debugging, we want to show the ARIA role name, since it is close in meaning.
const RoleEntry kReverseRoles[] = {
    {"banner", ax::mojom::blink::Role::kHeader},
    {"button", ax::mojom::blink::Role::kToggleButton},
    {"button", ax::mojom::blink::Role::kPopUpButton},
    {"contentinfo", ax::mojom::blink::Role::kFooter},
    {"menuitem", ax::mojom::blink::Role::kMenuListOption},
    {"combobox", ax::mojom::blink::Role::kComboBoxMenuButton},
    {"combobox", ax::mojom::blink::Role::kComboBoxSelect},
    {"combobox", ax::mojom::blink::Role::kTextFieldWithComboBox}};

static ARIARoleMap* CreateARIARoleMap() {
  ARIARoleMap* role_map = new ARIARoleMap;

  for (auto aria_role : kAriaRoles)
    role_map->Set(String(aria_role.role_name), aria_role.role);

  return role_map;
}

// The role name vector contains only ARIA roles, and no internal roles.
static Vector<AtomicString>* CreateARIARoleNameVector() {
  Vector<AtomicString>* role_name_vector = new Vector<AtomicString>(kNumRoles);
  role_name_vector->Fill(g_null_atom, kNumRoles);

  for (auto aria_role : kAriaRoles) {
    (*role_name_vector)[static_cast<wtf_size_t>(aria_role.role)] =
        AtomicString(aria_role.role_name);
  }

  for (auto reverse_role : kReverseRoles) {
    (*role_name_vector)[static_cast<wtf_size_t>(reverse_role.role)] =
        AtomicString(reverse_role.role_name);
  }

  return role_name_vector;
}

void AddIntListAttributeFromObjects(ax::mojom::blink::IntListAttribute attr,
                                    const AXObject::AXObjectVector& objects,
                                    ui::AXNodeData* node_data) {
  DCHECK(node_data);
  std::vector<int32_t> ids;
  for (const auto& obj : objects) {
    if (!obj->AccessibilityIsIgnored())
      ids.push_back(obj->AXObjectID());
  }
  if (!ids.empty())
    node_data->AddIntListAttribute(attr, ids);
}

// Max length for attributes such as aria-label.
static constexpr uint32_t kMaxStringAttributeLength = 10000;
// Max length for a static text name.
// Length of War and Peace (http://www.gutenberg.org/files/2600/2600-0.txt).
static constexpr uint32_t kMaxStaticTextLength = 3227574;

void TruncateAndAddStringAttribute(
    ui::AXNodeData* dst,
    ax::mojom::blink::StringAttribute attribute,
    const String& value,
    uint32_t max_len = kMaxStringAttributeLength) {
  if (value.empty())
    return;
  std::string value_utf8 = value.Utf8(kStrictUTF8Conversion);
  if (value_utf8.size() > max_len) {
    std::string truncated;
    base::TruncateUTF8ToByteSize(value_utf8, max_len, &truncated);
    dst->AddStringAttribute(attribute, truncated);
  } else {
    dst->AddStringAttribute(attribute, value_utf8);
  }
}

void AddIntListAttributeFromOffsetVector(
    ax::mojom::blink::IntListAttribute attr,
    const Vector<int> offsets,
    ui::AXNodeData* node_data) {
  std::vector<int32_t> offset_values;
  for (int offset : offsets)
    offset_values.push_back(static_cast<int32_t>(offset));
  DCHECK(node_data);
  if (!offset_values.empty())
    node_data->AddIntListAttribute(attr, offset_values);
}

}  // namespace

int32_t ToAXMarkerType(DocumentMarker::MarkerType marker_type) {
  ax::mojom::blink::MarkerType result;
  switch (marker_type) {
    case DocumentMarker::kSpelling:
      result = ax::mojom::blink::MarkerType::kSpelling;
      break;
    case DocumentMarker::kGrammar:
      result = ax::mojom::blink::MarkerType::kGrammar;
      break;
    case DocumentMarker::kTextFragment:
    case DocumentMarker::kTextMatch:
      result = ax::mojom::blink::MarkerType::kTextMatch;
      break;
    case DocumentMarker::kActiveSuggestion:
      result = ax::mojom::blink::MarkerType::kActiveSuggestion;
      break;
    case DocumentMarker::kSuggestion:
      result = ax::mojom::blink::MarkerType::kSuggestion;
      break;
    case DocumentMarker::kCustomHighlight:
      result = ax::mojom::blink::MarkerType::kHighlight;
      break;
    default:
      result = ax::mojom::blink::MarkerType::kNone;
      break;
  }

  return static_cast<int32_t>(result);
}

int32_t ToAXHighlightType(const AtomicString& highlight_type) {
  DEFINE_STATIC_LOCAL(const AtomicString, type_highlight, ("highlight"));
  DEFINE_STATIC_LOCAL(const AtomicString, type_spelling_error,
                      ("spelling-error"));
  DEFINE_STATIC_LOCAL(const AtomicString, type_grammar_error,
                      ("grammar-error"));
  ax::mojom::blink::HighlightType result =
      ax::mojom::blink::HighlightType::kNone;
  if (highlight_type == type_highlight)
    result = ax::mojom::blink::HighlightType::kHighlight;
  else if (highlight_type == type_spelling_error)
    result = ax::mojom::blink::HighlightType::kSpellingError;
  else if (highlight_type == type_grammar_error)
    result = ax::mojom::blink::HighlightType::kGrammarError;

  // Check that |highlight_type| is one of the static AtomicStrings defined
  // above or "none", so if there are more HighlightTypes added, they should
  // also be taken into account in this function.
  DCHECK(result != ax::mojom::blink::HighlightType::kNone ||
         highlight_type == "none");
  return static_cast<int32_t>(result);
}

const AXObject* FindAncestorWithAriaHidden(const AXObject* start) {
  for (const AXObject* object = start; object && !object->IsWebArea();
       object = object->ParentObject()) {
    if (object->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden))
      return object;
  }

  return nullptr;
}

// static
unsigned AXObject::number_of_live_ax_objects_ = 0;

AXObject::AXObject(AXObjectCacheImpl& ax_object_cache)
    : id_(0),
      parent_(nullptr),
      role_(ax::mojom::blink::Role::kUnknown),
      explicit_container_id_(0),
      last_modification_count_(-1),
      cached_is_ignored_(false),
      cached_is_ignored_but_included_in_tree_(false),
      cached_is_inert_(false),
      cached_is_aria_hidden_(false),
      cached_is_descendant_of_disabled_node_(false),
      cached_can_set_focus_attribute_(false),
      cached_live_region_root_(nullptr),
      cached_aria_column_index_(0),
      cached_aria_row_index_(0),
      ax_object_cache_(&ax_object_cache) {
  ++number_of_live_ax_objects_;
}

AXObject::~AXObject() {
  DCHECK(IsDetached());
  --number_of_live_ax_objects_;
}

void AXObject::SetAncestorsHaveDirtyDescendants() const {
  if (!RuntimeEnabledFeatures::AccessibilityEagerAXTreeUpdateEnabled()) {
    return;
  }
  for (auto* obj = CachedParentObject(); obj; obj = obj->CachedParentObject()) {
    DCHECK(!obj->IsDetached());
    // We need to to continue setting bits through AX objects for which
    // LastKnownIsIncludedInTreeValue is false, since those objects are omitted
    // from the generated tree. However, don't set the bit on unincluded
    // objects, during the clearing phase in
    // AXObjectCacheImpl::UpdateTreeIfNeededOnce(), only included nodes are
    // visited.
    if (!obj->LastKnownIsIncludedInTreeValue()) {
      continue;
    }
    if (obj->has_dirty_descendants_) {
      break;
    }
    obj->has_dirty_descendants_ = true;
  }
#if DCHECK_IS_ON()
  // Walk up the tree looking for dirty bits that failed to be set. If any
  // are found, this is a bug.
  if (!AXObjectCache().UpdatingTree()) {
    bool fail = false;
    for (auto* obj = CachedParentObject(); obj;
         obj = obj->CachedParentObject()) {
      if (obj->LastKnownIsIncludedInTreeValue() &&
          !obj->has_dirty_descendants_) {
        fail = true;
        break;
      }
    }
    if (fail) {
      LOG(ERROR) << "Failed to set dirty bits on some objects in the ancestor"
                    "chain. Bits set: ";
      for (auto* obj = this; obj; obj = obj->CachedParentObject()) {
        LOG(ERROR) << "* has_dirty_descendants_: "
                   << obj->has_dirty_descendants_
                   << " object: " << obj->ToString(true, true);
      }
      DCHECK(false);
    }
  }
#endif
}

void AXObject::Init(AXObject* parent) {
#if DCHECK_IS_ON()
  DCHECK(!parent_) << "Should not already have a cached parent:"
                   << "\n* Child = " << GetNode() << " / " << GetLayoutObject()
                   << "\n* Parent = " << parent_->ToString(true, true)
                   << "\n* Equal to passed-in parent? " << (parent == parent_);
  DCHECK(!is_initializing_);
  base::AutoReset<bool> reentrancy_protector(&is_initializing_, true);
#endif  // DCHECK_IS_ON()
  // The role must be determined immediately.
  // Note: in order to avoid reentrancy, the role computation cannot use the
  // ParentObject(), although it can use the DOM parent.
  role_ = DetermineAccessibilityRole();
#if DCHECK_IS_ON()
  DCHECK(IsValidRole(role_)) << "Illegal " << role_ << " for\n"
                             << GetNode() << '\n'
                             << GetLayoutObject();

  HTMLOptGroupElement* optgroup = DynamicTo<HTMLOptGroupElement>(GetNode());
  if (optgroup && optgroup->OwnerSelectElement()) {
    // We do not currently create accessible objects for an <optgroup> inside of
    // a <select size=1>.
    // TODO(accessibility) Remove this once we refactor HTML <select> to use
    // the shadow DOM and AXNodeObject instead of AXMenuList* classes.
    DCHECK(!optgroup->OwnerSelectElement()->UsesMenuList());
  }
#endif  // DCHECK_IS_ON()

  // Determine the parent as soon as possible.
  // Every AXObject must have a parent unless it's the root.
  SetParent(parent);
  DCHECK(parent_ || IsRoot())
      << "The following node should have a parent: " << GetNode();

  // The parent cannot have children. This object must be destroyed.
  DCHECK(!parent_ || parent_->CanHaveChildren())
      << "Tried to set a parent that cannot have children:"
      << "\n* Parent = " << parent_->ToString(true, true)
      << "\n* Child = " << ToString(true, true);

  // This is one after the role_ is computed, because the role is used to
  // determine whether an AXObject can have children.
  children_dirty_ = CanHaveChildren();

  UpdateCachedAttributeValuesIfNeeded(false);

  DCHECK(GetDocument()) << "All AXObjects must have a document: "
                        << ToString(true, true);

  // Set the dirty bit for the root AX object when created. For all other
  // objects, this is set by a descendant needing to be updated, and
  // AXObjectCacheImpl::UpdateTreeIfNeeded will therefore process an object
  // if its parent has has_dirty_descendants_ set. The root, however, has no
  // parent, so there is no parent to mark in order to cause the root to update
  // itself. Therefore this bit serves a second purpose of determining
  // whether AXObjectCacheImpl::UpdateTreeIfNeeded needs to update the root
  // object.
  if (IsRoot()) {
    has_dirty_descendants_ = true;
  }
}

void AXObject::Detach() {
  // Prevents LastKnown*() methods from returning the wrong values.
  cached_is_ignored_ = true;
  cached_is_ignored_but_included_in_tree_ = false;

  if (IsDetached()) {
    // Only mock objects can end up being detached twice, because their owner
    // may have needed to detach them when they were detached, but couldn't
    // remove them from the object cache yet.
    DCHECK(IsMockObject()) << "Object detached twice: " << RoleValue();
    return;
  }

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(ax_object_cache_);
  // AXInlineTextBox objects are the only objects that are safe to remove during
  // serialization. This occurs when a the serializer reaches a static text
  // object and its ignored state changes. Ignored static text boxes should not
  // have any inline textbox children, and they are removed by ClearChildren().
  SANITIZER_CHECK(!ax_object_cache_->IsFrozen() || IsAXInlineTextBox())
      << "Do not detach children while the tree is frozen, in order to avoid "
         "an object detaching itself in the middle of computing its own "
         "accessibility properties.";
  SANITIZER_CHECK(!is_adding_children_) << ToString(true, true);
#endif

#if !defined(NDEBUG)
  // Facilitates debugging of detached objects by providing info on what it was.
  if (!ax_object_cache_->HasBeenDisposed()) {
    detached_object_debug_info_ = ToString(true, true);
  }
#endif

  // Clear any children and call DetachFromParent() on them so that
  // no children are left with dangling pointers to their parent.
  ClearChildren();

  parent_ = nullptr;
  ax_object_cache_ = nullptr;
  children_dirty_ = false;
  has_dirty_descendants_ = false;
  id_ = 0;
}

bool AXObject::IsDetached() const {
  return !ax_object_cache_;
}

bool AXObject::IsRoot() const {
  return GetNode() && GetNode() == &AXObjectCache().GetDocument();
}

void AXObject::SetParent(AXObject* new_parent) const {
// TODO(crbug.com/1353205): Re-enable DCHECK for all platforms.
#if DCHECK_IS_ON() && !BUILDFLAG(IS_CHROMEOS_ASH)
  if (!new_parent && !IsRoot()) {
    std::ostringstream message;
    message << "Parent cannot be null, except at the root."
            << "\nParent: " << ToString(true, true)
            << "\nParent chain from DOM, starting at |this|:";
    int count = 0;
    for (Node* node = GetNode(); node;
         node = GetParentNodeForComputeParent(AXObjectCache(), node)) {
      message << "\n"
              << (++count) << ". " << node
              << "\n  LayoutObject=" << node->GetLayoutObject();
      if (AXObject* obj = AXObjectCache().Get(node))
        message << "\n  " << obj->ToString(true, true);
    }
    NOTREACHED() << message.str();
  }

  if (new_parent) {
    DCHECK(!new_parent->IsDetached())
        << "Cannot set parent to a detached object:"
        << "\n* Child: " << ToString(true, true)
        << "\n* New parent: " << new_parent->ToString(true, true);

    DCHECK(!IsAXInlineTextBox() ||
           ui::CanHaveInlineTextBoxChildren(new_parent->RoleValue()))
        << "Unexpected parent of inline text box: " << new_parent->RoleValue();
  }

  // Check to ensure that if the parent is changing from a previous parent,
  // that |this| is not still a child of that one.
  // This is similar to the IsParentUnignoredOf() check in
  // BlinkAXTreeSource, but closer to where the problem would occur.
  if (parent_ && new_parent != parent_ && !parent_->NeedsToUpdateChildren() &&
      !parent_->IsDetached()) {
    for (const auto& child : parent_->ChildrenIncludingIgnored()) {
      DCHECK(child != this) << "Previous parent still has |this| child:\n"
                            << ToString(true, true) << " should be a child of "
                            << new_parent->ToString(true, true) << " not of "
                            << parent_->ToString(true, true);
    }
    // TODO(accessibility) This should not be reached unless this method is
    // called on an AXObject of role kRootWebArea or when the parent's
    // children are dirty, aka parent_->NeedsToUpdateChildren());
    // Ideally we will also ensure |this| is in the parent's children now, so
    // that ClearChildren() can later find the child to detach from the parent.
  }

#endif
  parent_ = new_parent;
  SetAncestorsHaveDirtyDescendants();
}

bool AXObject::IsMissingParent() const {
  if (!parent_) {
    // Do not attempt to repair the ParentObject() of a validation message
    // object, because hidden ones are purposely kept around without being in
    // the tree, and without a parent, for potential later reuse.
    // TODO(accessibility) This is ugly. Consider destroying validation message
    // objects between uses instead. See GetOrCreateValidationMessageObject().
    return !IsRoot() && !IsValidationMessage();
  }

  if (parent_->IsDetached())
    return true;

  return false;
}

void AXObject::RepairMissingParent() const {
  DCHECK(IsMissingParent());
  DCHECK(!AXObjectCache().HasBeenDisposed());

  SetParent(ComputeParent());

  SANITIZER_CHECK(!parent_ ||
                  parent_->RoleValue() != ax::mojom::blink::Role::kIframe ||
                  RoleValue() == ax::mojom::blink::Role::kDocument)
      << "An iframe can only have a document child."
      << "\n* Child = " << ToString(true, true)
      << "\n* Parent =  " << parent_->ToString(true, true);
}

// In many cases, ComputeParent() is not called, because the parent adding
// the parent adding the child will pass itself into AXObjectCacheImpl.
// ComputeParent() is still necessary because some parts of the code,
// especially web tests, result in AXObjects being created in the middle of
// the tree before their parents are created.
// TODO(accessibility) Consider forcing all ax objects to be created from
// the top down, eliminating the need for ComputeParent().
AXObject* AXObject::ComputeParent() const {
  AXObject* ax_parent = ComputeParentOrNull();

  CHECK(!ax_parent || !ax_parent->IsDetached())
      << "Computed parent should never be detached:"
      << "\n* Child: " << GetNode()
      << "\n* Parent: " << ax_parent->ToString(true, true);

  return ax_parent;
}

// Same as ComputeParent, but without the extra check for valid parent in the
// end. This is for use in RestoreParentOrPrune.
AXObject* AXObject::ComputeParentOrNull() const {
#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!IsDetached());

  SANITIZER_CHECK(!IsMockObject())
      << "A mock object must have a parent, and cannot exist without one. "
         "The parent is set when the object is constructed.";

  SANITIZER_CHECK(GetNode() || GetLayoutObject() || IsVirtualObject())
      << "Can't compute parent on AXObjects without a backing Node "
         "LayoutObject, "
         " or AccessibleNode. Objects without those must set the "
         "parent in Init(), |this| = "
      << RoleValue();
#endif

  AXObject* ax_parent = nullptr;
  if (IsAXInlineTextBox()) {
    NOTREACHED()
        << "AXInlineTextBox box tried to compute a new parent, but they are "
           "not allowed to exist even temporarily without a parent, as their "
           "existence depends on the parent text object. Parent text = "
        << (AXObjectCache().SafeGet(GetNode())
                ? AXObjectCache().SafeGet(GetNode())->ToString(true, true)
                : "");
  } else if (AXObjectCache().IsAriaOwned(this)) {
    ax_parent = AXObjectCache().ValidatedAriaOwner(this);
  } else if (IsVirtualObject()) {
    ax_parent =
        ComputeAccessibleNodeParent(AXObjectCache(), *GetAccessibleNode());
  }
  if (!ax_parent) {
    ax_parent = ComputeNonARIAParent(AXObjectCache(), GetNode());
  }

  return ax_parent;
}

// static
Node* AXObject::GetParentNodeForComputeParent(AXObjectCacheImpl& cache,
                                              Node* node) {
  if (!node) {
    return nullptr;
  }

  DCHECK(node->isConnected())
      << "Should not call with disconnected node: " << node;

  // A document's parent should be the page popup owner, if any, otherwise null.
  if (auto* document = DynamicTo<Document>(node)) {
    LocalFrame* frame = document->GetFrame();
    DCHECK(frame);
    Node* popup_owner = frame->PagePopupOwner();
    if (!popup_owner) {
      return nullptr;
    }
    // TODO(accessibility) Remove this rule once we stop using AXMenuList*.
    if (IsA<HTMLSelectElement>(popup_owner) &&
        AXObjectCacheImpl::ShouldCreateAXMenuListFor(
            popup_owner->GetLayoutObject())) {
      return nullptr;
    }
    return popup_owner;
  }

  // Use LayoutTreeBuilderTraversal::Parent(), which handles pseudo content.
  // This can return nullptr for a node that is never visited by
  // LayoutTreeBuilderTraversal's child traversal. For example, while an element
  // can be appended as a <textarea>'s child, it is never visited by
  // LayoutTreeBuilderTraversal's child traversal. Therefore, returning null in
  // this case is appropriate, because that child content is not attached to any
  // parent as far as rendering or accessibility are concerned.
  // Whenever null is returned from this function, then a parent cannot be
  // computed, and when a parent is not provided or computed, the accessible
  // object will not be created.
  Node* parent = LayoutTreeBuilderTraversal::Parent(*node);
  if (!parent) {
    return nullptr;
  }

  // Descendants of pseudo elements must only be created by walking the tree via
  // AXNodeObject::AddChildren(), which already knows the parent. Therefore, the
  // parent must not be computed. This helps avoid situations with certain
  // elements where there is asymmetry between what considers this a child vs
  // what the this considers its parent. An example of this kind of situation is
  // a ::first-letter within a ::before.
  if (node->GetLayoutObject() && node->GetLayoutObject()->Parent() &&
      node->GetLayoutObject()->Parent()->IsPseudoElement()) {
    return nullptr;
  }

  HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(parent);
  if (map_element) {
    // For a <map>, return the <img> associated with it. This is necessary
    // because the AX tree is flat, adding image map children as children of the
    // <img>, whereas in the DOM they are actually children of the <map>.
    // Therefore, if a node is a DOM child of a map, its AX parent is the image.
    // This code double checks that the image actually uses the map.
    HTMLImageElement* image_element = map_element->ImageElement();
    return AXObject::GetMapForImage(image_element) == map_element
               ? image_element
               : nullptr;
  }

  return CanComputeAsNaturalParent(parent) ? parent : nullptr;
}

// static
bool AXObject::CanComputeAsNaturalParent(Node* node) {
  if (IsA<Document>(node)) {
    return true;
  }

  DCHECK(IsA<Element>(node)) << "Expected element: " << node;

  // When the flag to use AXMenuList in on, a menu list is only allowed to
  // parent an AXMenuListPopup, which is added as a child on creation. No other
  // children are allowed, and false is returned for anything else where the
  // parent would be AXMenuList.
  if (AXObjectCacheImpl::ShouldCreateAXMenuListFor(node->GetLayoutObject())) {
    return false;
  }

  // An image cannot be the natural DOM parent of another AXObject, it can only
  // have <area> children, which are from another part of the DOM tree.
  if (IsA<HTMLImageElement>(node)) {
    return false;
  }

  return CanHaveChildren(*To<Element>(node));
}

// static
bool AXObject::CanHaveChildren(Element& element) {
  // Image map parent-child relationships work as follows:
  // - The image is the parent
  // - The DOM children of the associated <map> are the children
  // This is accomplished by having GetParentNodeForComputeParent() return the
  // <img> instead of the <map> for the map's children.
  if (IsA<HTMLMapElement>(element)) {
    return false;
  }

  // Placeholder gets exposed as an attribute on the input accessibility node,
  // so there's no need to add its text children. Placeholder text is a separate
  // node that gets removed when it disappears, so this will only be present if
  // the placeholder is visible.
  if (Element* host = element.OwnerShadowHost()) {
    if (auto* ancestor_input = DynamicTo<TextControlElement>(host)) {
      if (ancestor_input->PlaceholderElement() == &element) {
        // |element| is a placeholder.
        return false;
      }
    }
  }

  if (IsA<HTMLBRElement>(element)) {
    // Normally, a <br> is allowed to have a single inline text box child.
    // However, a <br> element that has DOM children can occur only if a script
    // adds the children, and Blink will not render those children. This is an
    // obscure edge case that should only occur during fuzzing, but to maintain
    // tree consistency and prevent DCHECKs, AXObjects for <br> elements are not
    // allowed to have children if there are any DOM children at all.
    return !element.hasChildren();
  }

  if (IsA<HTMLHRElement>(element)) {
    return false;
  }

  if (auto* input = DynamicTo<HTMLInputElement>(&element)) {
    // False for checkbox, radio and range.
    return !input->IsCheckable() && input->type() != input_type_names::kRange;
  }

  if (IsA<HTMLOptionElement>(element)) {
    return false;
  }

  if (IsA<HTMLProgressElement>(element)) {
    return false;
  }

  return true;
}

// static
AXObject* AXObject::ComputeAccessibleNodeParent(
    AXObjectCacheImpl& cache,
    AccessibleNode& accessible_node) {
  if (AccessibleNode* parent_accessible_node = accessible_node.GetParent()) {
    if (AXObject* parent = cache.Get(parent_accessible_node))
      return parent;

    // Compute grandparent first, since constructing parent AXObject for
    // |accessible_node| requires grandparent to be provided.
    AXObject* grandparent_object =
        AXObject::ComputeAccessibleNodeParent(cache, *parent_accessible_node);

    if (grandparent_object)
      return cache.GetOrCreate(parent_accessible_node, grandparent_object);
  }

  return nullptr;
}

// static
HTMLMapElement* AXObject::GetMapForImage(Node* image) {
  if (!IsA<HTMLImageElement>(image))
    return nullptr;

  LayoutImage* layout_image = DynamicTo<LayoutImage>(image->GetLayoutObject());
  if (!layout_image)
    return nullptr;

  HTMLMapElement* map_element = layout_image->ImageMap();
  if (!map_element)
    return nullptr;

  // Don't allow images that are actually children of a map, as this could lead
  // to an infinite loop, where the descendant image points to the ancestor map,
  // yet the descendant image is being returned here as an ancestor.
  if (Traversal<HTMLMapElement>::FirstAncestor(*image))
    return nullptr;

  // The image has an associated <map> and does not have a <map> ancestor.
  return map_element;
}

// static
AXObject* AXObject::ComputeNonARIAParent(AXObjectCacheImpl& cache,
                                         Node* current_node) {
  if (!current_node) {
    return nullptr;
  }

  // For <option> in <select size=1>, return the popup.
  if (AXObjectCacheImpl::UseAXMenuList()) {
    if (auto* option = DynamicTo<HTMLOptionElement>(current_node)) {
      if (AXObject* ax_select =
              AXMenuListOption::ComputeParentAXMenuPopupFor(cache, option)) {
        return ax_select;
      }
    }
  }

  Node* parent_node = GetParentNodeForComputeParent(cache, current_node);

  // Will not create an object if no valid parent node is found. This occurs
  // when a DOM child isn't visited by LayoutTreeBuilderTraversal, such as an
  // element child of a <textarea>, which only supports plain text.
  return cache.GetOrCreate(parent_node);
}

#if DCHECK_IS_ON()
void AXObject::EnsureCorrectParentComputation() {
  if (!parent_)
    return;

  DCHECK(!parent_->IsDetached());

  DCHECK(parent_->CanHaveChildren());

  // Don't check the computed parent if the cached parent is a mock object.
  // It is expected that a computed parent could never be a mock object,
  // which has no backing DOM node or layout object, and therefore cannot be
  // found by traversing DOM/layout ancestors.
  if (parent_->IsMockObject())
    return;

  // Cannot compute a parent for an object that has no backing node or layout
  // object to start from.
  if (!GetNode() || !GetLayoutObject())
    return;

  // Don't check the computed parent if the cached parent is an image:
  // <area> children's location in the DOM and HTML hierarchy does not match.
  // TODO(aleventhal) Try to remove this rule, it may be unnecessary now.
  if (parent_->RoleValue() == ax::mojom::blink::Role::kImage)
    return;

  // TODO(aleventhal) Different in test fast/css/first-letter-removed-added.html
  // when run with --force-renderer-accessibility.
  if (GetNode() && GetNode()->IsPseudoElement())
    return;

  // Verify that the algorithm in ComputeParent() provides same results as
  // parents that init their children with themselves as the parent.
  // Inconsistency indicates a problem could potentially exist where a child's
  // parent does not include the child in its children.
  AXObject* computed_parent = ComputeParent();

  DCHECK(computed_parent) << "Computed parent was null for " << this
                          << ", expected " << parent_;
  DCHECK_EQ(computed_parent, parent_)
      << "\n**** ComputeParent should have provided the same result as "
         "the known parent.\n**** Computed parent layout object was "
      << computed_parent->GetLayoutObject()
      << "\n**** Actual parent's layout object was "
      << parent_->GetLayoutObject() << "\n**** Child was " << this;
}

void AXObject::ShowAXTreeForThis() const {
  DLOG(INFO) << "\n"
             << TreeToStringWithMarkedObjectHelper(AXObjectCache().Root(),
                                                   this);
}

#endif

const AtomicString& AXObject::GetAOMPropertyOrARIAAttribute(
    AOMStringProperty property) const {
  Element* element = GetElement();
  if (!element)
    return g_null_atom;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

Element* AXObject::GetAOMPropertyOrARIAAttribute(
    AOMRelationProperty property) const {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

bool AXObject::HasAOMProperty(AOMRelationListProperty property,
                              HeapVector<Member<Element>>& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetProperty(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(
    AOMRelationListProperty property,
    HeapVector<Member<Element>>& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMBooleanProperty property,
                                             bool& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::AOMPropertyOrARIAAttributeIsTrue(
    AOMBooleanProperty property) const {
  bool result;
  if (HasAOMPropertyOrARIAAttribute(property, result))
    return result;
  return false;
}

bool AXObject::AOMPropertyOrARIAAttributeIsFalse(
    AOMBooleanProperty property) const {
  bool result;
  if (HasAOMPropertyOrARIAAttribute(property, result))
    return !result;
  return false;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMUIntProperty property,
                                             uint32_t& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMIntProperty property,
                                             int32_t& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMFloatProperty property,
                                             float& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMStringProperty property,
                                             AtomicString& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  result = AccessibleNode::GetPropertyOrARIAAttribute(element, property);
  return !result.IsNull();
}

AccessibleNode* AXObject::GetAccessibleNode() const {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  return element->ExistingAccessibleNode();
}

void AXObject::Serialize(ui::AXNodeData* node_data,
                         ui::AXMode accessibility_mode) {
  // Reduce redundant ancestor chain walking for display lock computations.
  auto memoization_scope =
      DisplayLockUtilities::CreateLockCheckMemoizationScope();

  node_data->role = ComputeFinalRoleForSerialization();
  node_data->id = AXObjectID();

  PreSerializationConsistencyCheck();

  // Serialize a few things that we need even for ignored nodes.
  bool is_focusable = CanSetFocusAttribute();
  if (is_focusable)
    node_data->AddState(ax::mojom::blink::State::kFocusable);

  bool is_visible = IsVisible();
  if (!is_visible)
    node_data->AddState(ax::mojom::blink::State::kInvisible);

  if (is_visible || is_focusable) {
    // If the author applied the ARIA "textbox" role on something that is not
    // (currently) editable, this may be a read-only rich-text object. Or it
    // might just be bad authoring. Either way, we want to expose its
    // descendants, especially the interactive ones which might gain focus.
    bool is_non_atomic_textfield_root = IsARIATextField();

    // Preserve continuity in subtrees of richly editable content by including
    // richlyEditable state even if ignored.
    if (IsEditable()) {
      node_data->AddState(ax::mojom::blink::State::kEditable);
      if (!is_non_atomic_textfield_root)
        is_non_atomic_textfield_root = IsEditableRoot();

      if (IsRichlyEditable())
        node_data->AddState(ax::mojom::blink::State::kRichlyEditable);
    }
    if (is_non_atomic_textfield_root) {
      node_data->AddBoolAttribute(
          ax::mojom::blink::BoolAttribute::kNonAtomicTextFieldRoot, true);
    }
  }

  if (accessibility_mode.has_mode(ui::AXMode::kHTML))
    SerializeHTMLTagAndClass(node_data);  // Used for test readability.

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader))
    SerializeColorAttributes(node_data);  // Blends using all nodes' values.

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader) ||
      accessibility_mode.has_mode(ui::AXMode::kPDF)) {
    SerializeLangAttribute(node_data);  // Propagates using all nodes' values.
  }

  // Always try to serialize child tree ids.
  SerializeChildTreeID(node_data);

  if (!accessibility_mode.has_mode(ui::AXMode::kPDF))
    SerializeBoundingBoxAttributes(*node_data);

  // Return early. The following attributes are unnecessary for ignored nodes.
  // Exception: focusable ignored nodes are fully serialized, so that reasonable
  // verbalizations can be made if they actually receive focus.
  if (AccessibilityIsIgnored()) {
    node_data->AddState(ax::mojom::blink::State::kIgnored);
    // Early return for ignored, unfocusable nodes, avoiding unnecessary work.
    if (!is_focusable &&
        !RuntimeEnabledFeatures::AccessibilityExposeIgnoredNodesEnabled()) {
      // The name is important for exposing the selection around ignored nodes.
      // TODO(accessibility) Remove this and still pass this
      // content_browsertest:
      // All/DumpAccessibilityTreeTest.AccessibilityIgnoredSelection/blink
      if (RoleValue() == ax::mojom::blink::Role::kStaticText)
        SerializeNameAndDescriptionAttributes(accessibility_mode, node_data);
      return;
    }
  }

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader))
    SerializeScreenReaderAttributes(node_data);

  SerializeUnignoredAttributes(node_data, accessibility_mode);

  if (accessibility_mode.has_mode(ui::AXMode::kPDF)) {
    SerializeNameAndDescriptionAttributes(accessibility_mode, node_data);
    // Return early. None of the following attributes are needed for PDFs.
    return;
  }

  SerializeNameAndDescriptionAttributes(accessibility_mode, node_data);

  if (!accessibility_mode.has_mode(ui::AXMode::kScreenReader))
    return;

  if (LiveRegionRoot())
    SerializeLiveRegionAttributes(node_data);

  SerializeOtherScreenReaderAttributes(node_data);

  // Return early. The following attributes are unnecessary for ignored nodes.
  // Exception: focusable ignored nodes are fully serialized, so that reasonable
  // verbalizations can be made if they actually receive focus.
  if (AccessibilityIsIgnored() &&
      !node_data->HasState(ax::mojom::blink::State::kFocusable)) {
    return;
  }
}

void AXObject::SerializeBoundingBoxAttributes(ui::AXNodeData& dst) const {
  bool clips_children = false;
  PopulateAXRelativeBounds(dst.relative_bounds, &clips_children);
  if (clips_children) {
    dst.AddBoolAttribute(ax::mojom::blink::BoolAttribute::kClipsChildren, true);
  }

  if (IsLineBreakingObject()) {
    dst.AddBoolAttribute(ax::mojom::blink::BoolAttribute::kIsLineBreakingObject,
                         true);
  }
  AXObjectCache().SetCachedBoundingBox(AXObjectID(), dst.relative_bounds);
}

static bool AXShouldIncludePageScaleFactorInRoot() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

void AXObject::PopulateAXRelativeBounds(ui::AXRelativeBounds& bounds,
                                        bool* clips_children) const {
  AXObject* offset_container;
  gfx::RectF bounds_in_container;
  gfx::Transform container_transform;
  GetRelativeBounds(&offset_container, bounds_in_container, container_transform,
                    clips_children);
  bounds.bounds = bounds_in_container;
  if (offset_container && !offset_container->IsDetached())
    bounds.offset_container_id = offset_container->AXObjectID();

  if (AXShouldIncludePageScaleFactorInRoot() && IsRoot()) {
    const Page* page = GetDocument()->GetPage();
    container_transform.Scale(page->PageScaleFactor(), page->PageScaleFactor());
    container_transform.Translate(
        -page->GetVisualViewport().VisibleRect().origin().OffsetFromOrigin());
  }

  if (!container_transform.IsIdentity())
    bounds.transform = std::make_unique<gfx::Transform>(container_transform);
}

void AXObject::MarkAllImageAXObjectsDirty() {
  if (RoleValue() == ax::mojom::blink::Role::kImage) {
    AXObjectCache().MarkAXObjectDirtyWithCleanLayoutAndEvent(
        this, ax::mojom::blink::EventFrom::kNone,
        ax::mojom::Action::kAnnotatePageImages);
  }

  for (auto& child : UnignoredChildren())
    child->MarkAllImageAXObjectsDirty();
}

void AXObject::SerializeActionAttributes(ui::AXNodeData* node_data) {
  if (CanSetValueAttribute())
    node_data->AddAction(ax::mojom::blink::Action::kSetValue);
  if (IsSlider()) {
    node_data->AddAction(ax::mojom::blink::Action::kDecrement);
    node_data->AddAction(ax::mojom::blink::Action::kIncrement);
  }
  if (IsUserScrollable()) {
    node_data->AddAction(ax::mojom::blink::Action::kScrollUp);
    node_data->AddAction(ax::mojom::blink::Action::kScrollDown);
    node_data->AddAction(ax::mojom::blink::Action::kScrollLeft);
    node_data->AddAction(ax::mojom::blink::Action::kScrollRight);
    node_data->AddAction(ax::mojom::blink::Action::kScrollForward);
    node_data->AddAction(ax::mojom::blink::Action::kScrollBackward);
  }
}

void AXObject::SerializeChildTreeID(ui::AXNodeData* node_data) {
  // If this is an HTMLFrameOwnerElement (such as an iframe), we may need
  // to embed the ID of the child frame.
  if (!IsChildTreeOwner()) {
    // TODO(crbug.com/1342603) Determine why these are firing in the wild and,
    // once fixed, turn into a DCHECK.
    SANITIZER_CHECK(!IsFrame(GetNode()))
        << "If this is an iframe, it should also be a child tree owner: "
        << ToString(true, true);
    return;
  }

  // Do not attach hidden child trees.
  if (!IsVisible()) {
    return;
  }

  auto* html_frame_owner_element = To<HTMLFrameOwnerElement>(GetElement());

  Frame* child_frame = html_frame_owner_element->ContentFrame();
  if (!child_frame) {
    // TODO(crbug.com/1342603) Determine why these are firing in the wild and,
    // once fixed, turn into a DCHECK.
    SANITIZER_CHECK(IsDisabled()) << ToString(true, true);
    return;
  }

  absl::optional<base::UnguessableToken> child_token =
      child_frame->GetEmbeddingToken();
  if (!child_token)
    return;  // No child token means that the connection isn't ready yet.

  DCHECK_EQ(ChildCountIncludingIgnored(), 0)
      << "Children won't exist until the trees are stitched together in the "
         "browser process. A failure means that a child node was incorrectly "
         "considered relevant by AXObjectCacheImpl."
      << "\n* Parent: " << ToString(true)
      << "\n* Frame owner: " << IsA<HTMLFrameOwnerElement>(GetNode())
      << "\n* Element src: " << GetAttribute(html_names::kSrcAttr)
      << "\n* First child: " << FirstChildIncludingIgnored()->ToString(true);

  ui::AXTreeID child_tree_id = ui::AXTreeID::FromToken(child_token.value());
  node_data->AddChildTreeId(child_tree_id);
}

void AXObject::SerializeChooserPopupAttributes(ui::AXNodeData* node_data) {
  AXObject* chooser_popup = ChooserPopup();
  if (!chooser_popup)
    return;

  int32_t chooser_popup_id = chooser_popup->AXObjectID();
  auto controls_ids = node_data->GetIntListAttribute(
      ax::mojom::blink::IntListAttribute::kControlsIds);
  controls_ids.push_back(chooser_popup_id);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kControlsIds, controls_ids);
}

void AXObject::SerializeColorAttributes(ui::AXNodeData* node_data) {
  // Text attributes.
  if (RGBA32 bg_color = BackgroundColor()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kBackgroundColor,
                               bg_color);
  }

  if (RGBA32 color = GetColor())
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kColor, color);
}

void AXObject::SerializeElementAttributes(ui::AXNodeData* node_data) {
  Element* element = GetElement();
  if (!element)
    return;

  if (const AtomicString& class_name = element->GetClassAttribute()) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kClassName, class_name);
  }

  // Expose StringAttribute::kRole, which is used for the xml-roles object
  // attribute. Prefer the raw ARIA role attribute value, otherwise, the ARIA
  // equivalent role is used, if it is a role that is exposed in xml-roles.
  const AtomicString& role_str =
      GetRoleAttributeStringForObjectAttribute(node_data);
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kRole, role_str);
}

void AXObject::SerializeHTMLTagAndClass(ui::AXNodeData* node_data) {
  Element* element = GetElement();
  if (!element) {
    if (IsA<Document>(GetNode())) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kHtmlTag, "#document");
    }
    return;
  }

  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kHtmlTag,
                                element->tagName().LowerASCII());

  if (const AtomicString& class_name = element->GetClassAttribute()) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kClassName, class_name);
  }
}

void AXObject::SerializeHTMLAttributes(ui::AXNodeData* node_data) {
  Element* element = GetElement();
  DCHECK(element);
  for (const Attribute& attr : element->Attributes()) {
    std::string name = attr.LocalName().LowerASCII().Utf8();
    if (name == "class") {  // class already in kClassName
      continue;
    }
    std::string value = attr.Value().Utf8();
    node_data->html_attributes.push_back(std::make_pair(name, value));
  }

// TODO(nektar): Turn off kHTMLAccessibilityMode for automation and Mac
// and remove ifdef.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  if (node_data->role == ax::mojom::blink::Role::kMath ||
      node_data->role == ax::mojom::blink::Role::kMathMLMath) {
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kInnerHtml,
                                  element->innerHTML(), kMaxStaticTextLength);
  }
#endif
}

void AXObject::SerializeInlineTextBoxAttributes(
    ui::AXNodeData* node_data) const {
  DCHECK_EQ(ax::mojom::blink::Role::kInlineTextBox, node_data->role);

  Vector<int> character_offsets;
  TextCharacterOffsets(character_offsets);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kCharacterOffsets, character_offsets,
      node_data);

  Vector<int> word_starts;
  Vector<int> word_ends;
  GetWordBoundaries(word_starts, word_ends);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordStarts, word_starts, node_data);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordEnds, word_ends, node_data);
}

void AXObject::SerializeLangAttribute(ui::AXNodeData* node_data) {
  AXObject* parent = ParentObject();
  if (Language().length()) {
    // TODO(chrishall): should we still trim redundant languages off here?
    if (!parent || parent->Language() != Language()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kLanguage, Language());
    }
  }
}

void AXObject::SerializeListAttributes(ui::AXNodeData* node_data) {
  if (SetSize()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kSetSize,
                               SetSize());
  }

  if (PosInSet()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kPosInSet,
                               PosInSet());
  }
}

void AXObject::SerializeListMarkerAttributes(ui::AXNodeData* node_data) const {
  DCHECK_EQ(ax::mojom::blink::Role::kListMarker, node_data->role);

  Vector<int> word_starts;
  Vector<int> word_ends;
  GetWordBoundaries(word_starts, word_ends);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordStarts, word_starts, node_data);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordEnds, word_ends, node_data);
}

void AXObject::SerializeLiveRegionAttributes(ui::AXNodeData* node_data) const {
  DCHECK(LiveRegionRoot());

  node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kLiveAtomic,
                              LiveRegionAtomic());
  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kLiveStatus,
                                LiveRegionStatus());
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kLiveRelevant,
      LiveRegionRelevant());
  // If we are not at the root of an atomic live region.
  if (ContainerLiveRegionAtomic() && !LiveRegionRoot()->IsDetached() &&
      !LiveRegionAtomic()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kMemberOfId,
                               LiveRegionRoot()->AXObjectID());
  }
  node_data->AddBoolAttribute(
      ax::mojom::blink::BoolAttribute::kContainerLiveAtomic,
      ContainerLiveRegionAtomic());
  node_data->AddBoolAttribute(
      ax::mojom::blink::BoolAttribute::kContainerLiveBusy,
      ContainerLiveRegionBusy());
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kContainerLiveStatus,
      ContainerLiveRegionStatus());
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kContainerLiveRelevant,
      ContainerLiveRegionRelevant());
}

void AXObject::SerializeNameAndDescriptionAttributes(
    ui::AXMode accessibility_mode,
    ui::AXNodeData* node_data) const {
  ax::mojom::blink::NameFrom name_from;
  AXObjectVector name_objects;
  String name = GetName(name_from, &name_objects);
  if (name_from == ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty) {
    node_data->AddStringAttribute(ax::mojom::blink::StringAttribute::kName,
                                  std::string());
    node_data->SetNameFrom(
        ax::mojom::blink::NameFrom::kAttributeExplicitlyEmpty);
  } else if (!name.empty()) {
    int max_length = node_data->role == ax::mojom::blink::Role::kStaticText
                         ? kMaxStaticTextLength
                         : kMaxStringAttributeLength;
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kName, name, max_length);
    node_data->SetNameFrom(name_from);
    AddIntListAttributeFromObjects(
        ax::mojom::blink::IntListAttribute::kLabelledbyIds, name_objects,
        node_data);
  }

  ax::mojom::blink::DescriptionFrom description_from;
  AXObjectVector description_objects;
  String description =
      Description(name_from, description_from, &description_objects);
  if (!description.empty()) {
    DCHECK(description_from != ax::mojom::blink::DescriptionFrom::kNone);
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kDescription,
        description);
    node_data->SetDescriptionFrom(description_from);
    AddIntListAttributeFromObjects(
        ax::mojom::blink::IntListAttribute::kDescribedbyIds,
        description_objects, node_data);
  }

  String title = Title(name_from);
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kTooltip, title);

  if (!accessibility_mode.has_mode(ui::AXMode::kScreenReader))
    return;

  String placeholder = Placeholder(name_from);
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kPlaceholder, placeholder);
}

void AXObject::SerializeScreenReaderAttributes(ui::AXNodeData* node_data) {
  if (ui::IsText(RoleValue())) {
    // Don't serialize these attributes on text, where it is uninteresting.
    return;
  }
  String display_style;
  if (Node* node = GetNode(); node && !node->IsDocumentNode()) {
    if (const ComputedStyle* computed_style = node->GetComputedStyle()) {
      display_style = CSSProperty::Get(CSSPropertyID::kDisplay)
                          .CSSValueFromComputedStyle(
                              *computed_style, /* layout_object */ nullptr,
                              /* allow_visited_style */ false)
                          ->CssText();
      if (!display_style.empty()) {
        TruncateAndAddStringAttribute(
            node_data, ax::mojom::blink::StringAttribute::kDisplay,
            display_style);
      }
    }

    // Whether it has ARIA attributes at all.
    if (HasAriaAttribute()) {
      node_data->AddBoolAttribute(
          ax::mojom::blink::BoolAttribute::kHasAriaAttribute, true);
    }
  }

  if (KeyboardShortcut().length() &&
      !node_data->HasStringAttribute(
          ax::mojom::blink::StringAttribute::kKeyShortcuts)) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kKeyShortcuts,
        KeyboardShortcut());
  }

  if (AXObject* active_descendant = ActiveDescendant()) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kActivedescendantId,
        active_descendant->AXObjectID());
  }
}

String AXObject::KeyboardShortcut() const {
  const AtomicString& access_key = AccessKey();
  if (access_key.IsNull())
    return String();

  DEFINE_STATIC_LOCAL(String, modifier_string, ());
  if (modifier_string.IsNull()) {
    unsigned modifiers = KeyboardEventManager::kAccessKeyModifiers;
    // Follow the same order as Mozilla MSAA implementation:
    // Ctrl+Alt+Shift+Meta+key. MSDN states that keyboard shortcut strings
    // should not be localized and defines the separator as "+".
    StringBuilder modifier_string_builder;
    if (modifiers & WebInputEvent::kControlKey)
      modifier_string_builder.Append("Ctrl+");
    if (modifiers & WebInputEvent::kAltKey)
      modifier_string_builder.Append("Alt+");
    if (modifiers & WebInputEvent::kShiftKey)
      modifier_string_builder.Append("Shift+");
    if (modifiers & WebInputEvent::kMetaKey)
      modifier_string_builder.Append("Win+");
    modifier_string = modifier_string_builder.ToString();
  }

  return String(modifier_string + access_key);
}

void AXObject::SerializeOtherScreenReaderAttributes(
    ui::AXNodeData* node_data) const {
  DCHECK_NE(node_data->role, ax::mojom::blink::Role::kUnknown);
  DCHECK_NE(node_data->role, ax::mojom::blink::Role::kNone);

  if (IsA<Document>(GetNode())) {
    if (!IsLoaded()) {
      node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy, true);
    }
    if (AXObject* parent = ParentObject()) {
      DCHECK(parent->ChooserPopup() == this)
          << "ChooserPopup missing for: " << parent->ToString(true);
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kPopupForId,
                                 parent->AXObjectID());
    }
  }

  if (node_data->role == ax::mojom::blink::Role::kColorWell) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kColorValue,
                               ColorValue());
  }

  if (node_data->role == ax::mojom::blink::Role::kLink) {
    AXObject* target = InPageLinkTarget();
    if (target) {
      int32_t target_id = target->AXObjectID();
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kInPageLinkTargetId, target_id);
    }

    // `ax::mojom::blink::StringAttribute::kLinkTarget` is only valid on <a> and
    // <area> elements. <area> elements should link to something in order to be
    // considered, see `AXImageMap::Role()`.
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kLinkTarget,
        EffectiveTarget());
  }

  if (node_data->role == ax::mojom::blink::Role::kRadioButton) {
    AddIntListAttributeFromObjects(
        ax::mojom::blink::IntListAttribute::kRadioGroupIds,
        RadioButtonsInGroup(), node_data);
  }

  if (GetAriaCurrentState() != ax::mojom::blink::AriaCurrentState::kNone) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kAriaCurrentState,
        static_cast<int32_t>(GetAriaCurrentState()));
  }

  if (GetInvalidState() != ax::mojom::blink::InvalidState::kNone)
    node_data->SetInvalidState(GetInvalidState());

  if (CheckedState() != ax::mojom::blink::CheckedState::kNone) {
    node_data->SetCheckedState(CheckedState());
  }

  if (node_data->role == ax::mojom::blink::Role::kInlineTextBox) {
    SerializeInlineTextBoxAttributes(node_data);
  }

  if (node_data->role == ax::mojom::blink::Role::kListMarker) {
    SerializeListMarkerAttributes(node_data);
  }

  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kAccessKey, AccessKey());

  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kAutoComplete,
      AutoComplete());

  if (Action() != ax::mojom::blink::DefaultActionVerb::kNone) {
    node_data->SetDefaultActionVerb(Action());
  }

  if (NextOnLine() && !NextOnLine()->IsDetached()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kNextOnLineId,
                               NextOnLine()->AXObjectID());
  }

  if (PreviousOnLine() && !PreviousOnLine()->IsDetached()) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kPreviousOnLineId,
        PreviousOnLine()->AXObjectID());
  }

  if (ErrorMessage() && !ErrorMessage()->IsDetached()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kErrormessageId,
                               ErrorMessage()->AXObjectID());
  }

  if (ui::SupportsHierarchicalLevel(node_data->role) && HierarchicalLevel()) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kHierarchicalLevel,
        HierarchicalLevel());
  }

  if (CanvasHasFallbackContent()) {
    node_data->AddBoolAttribute(
        ax::mojom::blink::BoolAttribute::kCanvasHasFallback, true);
  }

  if (IsRangeValueSupported()) {
    float value;
    if (ValueForRange(&value)) {
      node_data->AddFloatAttribute(
          ax::mojom::blink::FloatAttribute::kValueForRange, value);
    }

    float max_value;
    if (MaxValueForRange(&max_value)) {
      node_data->AddFloatAttribute(
          ax::mojom::blink::FloatAttribute::kMaxValueForRange, max_value);
    }

    float min_value;
    if (MinValueForRange(&min_value)) {
      node_data->AddFloatAttribute(
          ax::mojom::blink::FloatAttribute::kMinValueForRange, min_value);
    }

    float step_value;
    if (StepValueForRange(&step_value)) {
      node_data->AddFloatAttribute(
          ax::mojom::blink::FloatAttribute::kStepValueForRange, step_value);
    }
  }

  if (ui::IsDialog(node_data->role)) {
    node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kModal,
                                IsModal());
  }

  // aria-dropeffect is deprecated in WAI-ARIA 1.1.
  Vector<ax::mojom::blink::Dropeffect> dropeffects;
  Dropeffects(dropeffects);
  if (!dropeffects.empty()) {
    for (auto&& dropeffect : dropeffects) {
      node_data->AddDropeffect(dropeffect);
    }
  }
}

void AXObject::SerializeScrollAttributes(ui::AXNodeData* node_data) {
  // Only mark as scrollable if user has actual scrollbars to use.
  node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kScrollable,
                              IsUserScrollable());
  // Provide x,y scroll info if scrollable in any way (programmatically or via
  // user).
  gfx::Point scroll_offset = GetScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollX,
                             scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollY,
                             scroll_offset.y());

  gfx::Point min_scroll_offset = MinimumScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollXMin,
                             min_scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollYMin,
                             min_scroll_offset.y());

  gfx::Point max_scroll_offset = MaximumScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollXMax,
                             max_scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollYMax,
                             max_scroll_offset.y());
}

void AXObject::SerializeSparseAttributes(ui::AXNodeData* node_data) {
  if (IsVirtualObject()) {
    AccessibleNode* accessible_node = GetAccessibleNode();
    if (accessible_node) {
      AXNodeDataAOMPropertyClient property_client(*ax_object_cache_,
                                                  *node_data);
      accessible_node->GetAllAOMProperties(&property_client);
    }
  }

  Element* element = GetElement();
  if (!element)
    return;

  AXSparseAttributeSetterMap& setter_map = GetAXSparseAttributeSetterMap();
  AttributeCollection attributes = element->AttributesWithoutUpdate();
  HashSet<QualifiedName> set_attributes;
  for (const Attribute& attr : attributes) {
    set_attributes.insert(attr.GetName());
    AXSparseSetterFunc callback;
    auto it = setter_map.find(attr.GetName());
    if (it == setter_map.end())
      continue;
    it->value.Run(this, node_data, attr.Value());
  }

  if (!element->DidAttachInternals())
    return;
  const auto& internals_attributes =
      element->EnsureElementInternals().GetAttributes();
  for (const QualifiedName& attr : internals_attributes.Keys()) {
    auto it = setter_map.find(attr);
    if (set_attributes.Contains(attr) || it == setter_map.end())
      continue;
    it->value.Run(this, node_data, internals_attributes.at(attr));
  }
}

void AXObject::SerializeStyleAttributes(ui::AXNodeData* node_data) {
  // Only serialize font family if there is one, and it is different from the
  // parent. Use the value from computed style first since that is a fast lookup
  // and comparison, and serialize the user-friendly name at points in the tree
  // where the font family changes between parent/child.
  const AtomicString& computed_family = ComputedFontFamily();
  if (computed_family.length()) {
    AXObject* parent = ParentObjectUnignored();
    if (!parent || parent->ComputedFontFamily() != computed_family) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kFontFamily,
          FontFamilyForSerialization());
    }
  }

  // Font size is in pixels.
  if (FontSize()) {
    node_data->AddFloatAttribute(ax::mojom::blink::FloatAttribute::kFontSize,
                                 FontSize());
  }

  if (FontWeight()) {
    node_data->AddFloatAttribute(ax::mojom::blink::FloatAttribute::kFontWeight,
                                 FontWeight());
  }

  if (RoleValue() == ax::mojom::blink::Role::kListItem &&
      GetListStyle() != ax::mojom::blink::ListStyle::kNone) {
    node_data->SetListStyle(GetListStyle());
  }

  if (GetTextDirection() != ax::mojom::blink::WritingDirection::kNone) {
    node_data->SetTextDirection(GetTextDirection());
  }

  if (GetTextPosition() != ax::mojom::blink::TextPosition::kNone) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kTextPosition,
                               static_cast<int32_t>(GetTextPosition()));
  }

  int32_t text_style = 0;
  ax::mojom::blink::TextDecorationStyle text_overline_style;
  ax::mojom::blink::TextDecorationStyle text_strikethrough_style;
  ax::mojom::blink::TextDecorationStyle text_underline_style;
  GetTextStyleAndTextDecorationStyle(&text_style, &text_overline_style,
                                     &text_strikethrough_style,
                                     &text_underline_style);
  if (text_style) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kTextStyle,
                               text_style);
  }

  if (text_overline_style != ax::mojom::blink::TextDecorationStyle::kNone) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTextOverlineStyle,
        static_cast<int32_t>(text_overline_style));
  }

  if (text_strikethrough_style !=
      ax::mojom::blink::TextDecorationStyle::kNone) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTextStrikethroughStyle,
        static_cast<int32_t>(text_strikethrough_style));
  }

  if (text_underline_style != ax::mojom::blink::TextDecorationStyle::kNone) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTextUnderlineStyle,
        static_cast<int32_t>(text_underline_style));
  }
}

void AXObject::SerializeTableAttributes(ui::AXNodeData* node_data) {
  if (ui::IsTableLike(RoleValue())) {
    int aria_colcount = AriaColumnCount();
    if (aria_colcount) {
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kAriaColumnCount, aria_colcount);
    }
    int aria_rowcount = AriaRowCount();
    if (aria_rowcount) {
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kAriaRowCount,
                                 aria_rowcount);
    }
  }

  if (ui::IsTableRow(RoleValue())) {
    AXObject* header = HeaderObject();
    if (header && !header->IsDetached()) {
      // TODO(accessibility): these should be computed by ui::AXTableInfo and
      // removed here.
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kTableRowHeaderId,
          header->AXObjectID());
    }
  }

  if (ui::IsCellOrTableHeader(RoleValue())) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTableCellColumnSpan, ColumnSpan());
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTableCellRowSpan, RowSpan());
  }

  if (ui::IsCellOrTableHeader(RoleValue()) || ui::IsTableRow(RoleValue())) {
    // aria-rowindex and aria-colindex are supported on cells, headers and
    // rows.
    int aria_rowindex = AriaRowIndex();
    if (aria_rowindex) {
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kAriaCellRowIndex, aria_rowindex);
    }

    int aria_colindex = AriaColumnIndex();
    if (aria_colindex) {
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kAriaCellColumnIndex, aria_colindex);
    }
  }

  if (ui::IsTableHeader(RoleValue()) &&
      GetSortDirection() != ax::mojom::blink::SortDirection::kNone) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kSortDirection,
                               static_cast<int32_t>(GetSortDirection()));
  }
}

// Attributes that don't need to be serialized on ignored nodes.
void AXObject::SerializeUnignoredAttributes(ui::AXNodeData* node_data,
                                            ui::AXMode accessibility_mode) {
  AccessibilityExpanded expanded = IsExpanded();
  if (expanded) {
    if (expanded == kExpandedCollapsed)
      node_data->AddState(ax::mojom::blink::State::kCollapsed);
    else if (expanded == kExpandedExpanded)
      node_data->AddState(ax::mojom::blink::State::kExpanded);
  }

  ax::mojom::blink::Role role = RoleValue();
  if (HasPopup() != ax::mojom::blink::HasPopup::kFalse) {
    node_data->SetHasPopup(HasPopup());
  } else if (role == ax::mojom::blink::Role::kPopUpButton ||
             role == ax::mojom::blink::Role::kComboBoxSelect) {
    node_data->SetHasPopup(ax::mojom::blink::HasPopup::kMenu);
  } else if (ui::IsComboBox(role)) {
    node_data->SetHasPopup(ax::mojom::blink::HasPopup::kListbox);
  }

  if (IsPopup() != ax::mojom::blink::IsPopup::kNone) {
    node_data->SetIsPopup(IsPopup());
  }

  if (IsAutofillAvailable())
    node_data->AddState(ax::mojom::blink::State::kAutofillAvailable);

  if (IsDefault())
    node_data->AddState(ax::mojom::blink::State::kDefault);

  // aria-grabbed is deprecated in WAI-ARIA 1.1.
  if (IsGrabbed() != kGrabbedStateUndefined) {
    node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kGrabbed,
                                IsGrabbed() == kGrabbedStateTrue);
  }

  if (IsHovered())
    node_data->AddState(ax::mojom::blink::State::kHovered);

  if (IsLinked())
    node_data->AddState(ax::mojom::blink::State::kLinked);

  if (IsMultiline())
    node_data->AddState(ax::mojom::blink::State::kMultiline);

  if (IsMultiSelectable())
    node_data->AddState(ax::mojom::blink::State::kMultiselectable);

  if (IsPasswordField())
    node_data->AddState(ax::mojom::blink::State::kProtected);

  if (IsRequired())
    node_data->AddState(ax::mojom::blink::State::kRequired);

  if (IsSelected() != blink::kSelectedStateUndefined) {
    node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kSelected,
                                IsSelected() == blink::kSelectedStateTrue);
    node_data->AddBoolAttribute(
        ax::mojom::blink::BoolAttribute::kSelectedFromFocus,
        IsSelectedFromFocus());
  }

  if (IsNotUserSelectable()) {
    node_data->AddBoolAttribute(
        ax::mojom::blink::BoolAttribute::kNotUserSelectableStyle, true);
  }

  if (IsVisited())
    node_data->AddState(ax::mojom::blink::State::kVisited);

  if (Orientation() == kAccessibilityOrientationVertical)
    node_data->AddState(ax::mojom::blink::State::kVertical);
  else if (Orientation() == blink::kAccessibilityOrientationHorizontal)
    node_data->AddState(ax::mojom::blink::State::kHorizontal);

  if (GetTextAlign() != ax::mojom::blink::TextAlign::kNone) {
    node_data->SetTextAlign(GetTextAlign());
  }

  if (GetTextIndent() != 0.0f) {
    node_data->AddFloatAttribute(ax::mojom::blink::FloatAttribute::kTextIndent,
                                 GetTextIndent());
  }

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader) ||
      accessibility_mode.has_mode(ui::AXMode::kPDF)) {
    // The DOMNodeID from Blink. Currently only populated when using
    // the accessibility tree for PDF exporting. Warning, this is totally
    // unrelated to the accessibility node ID, or the ID attribute for an
    // HTML element - it's an ID used to uniquely identify nodes in Blink.
    int dom_node_id = GetDOMNodeId();
    if (dom_node_id) {
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kDOMNodeId,
                                 dom_node_id);
    }

    // Heading level.
    if (ui::IsHeading(role) && HeadingLevel()) {
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kHierarchicalLevel, HeadingLevel());
    }

    SerializeListAttributes(node_data);
    SerializeTableAttributes(node_data);
  }

  if (accessibility_mode.has_mode(ui::AXMode::kPDF)) {
    // Return early. None of the following attributes are needed for PDFs.
    return;
  }

  switch (Restriction()) {
    case AXRestriction::kRestrictionReadOnly:
      node_data->SetRestriction(ax::mojom::blink::Restriction::kReadOnly);
      break;
    case AXRestriction::kRestrictionDisabled:
      node_data->SetRestriction(ax::mojom::blink::Restriction::kDisabled);
      break;
    case AXRestriction::kRestrictionNone:
      SerializeActionAttributes(node_data);
      break;
  }

  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kUrl, Url().GetString());

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader)) {
    SerializeMarkerAttributes(node_data);
    SerializeStyleAttributes(node_data);
  }

  SerializeSparseAttributes(node_data);

  if (Element* element = GetElement()) {
    // Do not send the value attribute for non-atomic text fields in order to
    // improve the performance of the cross-process communication with the
    // browser process, and since it can be easily computed in that process.
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kValue,
                                  GetValueForControl());

    if (IsA<HTMLInputElement>(element)) {
      String type = element->getAttribute(html_names::kTypeAttr);
      if (type.empty()) {
        type = "text";
      }
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kInputType, type);
    }

    if (IsAtomicTextField()) {
      // Selection offsets are only used for plain text controls, (input of a
      // text field type, and textarea). Rich editable areas, such as
      // contenteditables, use AXTreeData.
      //
      // TODO(nektar): Remove kTextSelStart and kTextSelEnd from the renderer.
      const auto ax_selection =
          AXSelection::FromCurrentSelection(ToTextControl(*element));
      int start = ax_selection.Base().IsTextPosition()
                      ? ax_selection.Base().TextOffset()
                      : ax_selection.Base().ChildIndex();
      int end = ax_selection.Extent().IsTextPosition()
                    ? ax_selection.Extent().TextOffset()
                    : ax_selection.Extent().ChildIndex();
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kTextSelStart,
                                 start);
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kTextSelEnd,
                                 end);
    }
  }

  SerializeComputedDetailsRelation(node_data);
  // Try to get an aria-controls listbox for an <input role="combobox">.
  if (!node_data->HasIntListAttribute(
          ax::mojom::blink::IntListAttribute::kControlsIds)) {
    if (AXObject* listbox = GetControlsListboxForTextfieldCombobox()) {
      node_data->AddIntListAttribute(
          ax::mojom::blink::IntListAttribute::kControlsIds,
          {static_cast<int32_t>(listbox->AXObjectID())});
    }
  }

  if (IsScrollableContainer())
    SerializeScrollAttributes(node_data);

  SerializeChooserPopupAttributes(node_data);

  if (GetElement()) {
    SerializeElementAttributes(node_data);
    if (accessibility_mode.has_mode(ui::AXMode::kHTML)) {
      SerializeHTMLAttributes(node_data);
    }
  }
}

void AXObject::SerializeComputedDetailsRelation(
    ui::AXNodeData* node_data) const {
  // aria-details was used -- it may have set a relation, unless the attribute
  // value did not point to valid elements (e.g aria-details=""). Whether it
  // actually set the relation or not, the author's intent in using the
  // aria-details attribute is understood to mean that no automatic relation
  // should be set.
  if (HasAttribute(html_names::kAriaDetailsAttr)) {
    return;
  }

  // Add details relation to <figure>, pointing at <figcaption>.
  if (node_data->role == ax::mojom::blink::Role::kFigure) {
    AXObject* fig_caption = GetChildFigcaption();
    if (fig_caption) {
      std::vector<int32_t> ids;
      ids.push_back(GetChildFigcaption()->AXObjectID());
      node_data->AddIntListAttribute(
          ax::mojom::blink::IntListAttribute::kDetailsIds, ids);
      return;
    }
  }

  // Add aria-details for a popover invoker.
  // TODO(https://crbug.com/1426607) Support this for non-plain hint popovers.
  if (AXObject* popover = GetTargetPopoverForInvoker()) {
    node_data->AddIntListAttribute(
        ax::mojom::blink::IntListAttribute::kDetailsIds,
        {static_cast<int32_t>(popover->AXObjectID())});
  }
}

bool AXObject::IsPlainContent() const {
  if (!ui::IsPlainContentElement(role_)) {
    return false;
  }
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (!child->IsPlainContent()) {
      return false;
    }
  }
  return true;
}

// Popover invoking elements should have details relationships with their
// target popover, when that popover is a) open, and b) not the next element
// in the DOM (depth first search order).
AXObject* AXObject::GetTargetPopoverForInvoker() const {
  auto* form_element = DynamicTo<HTMLFormControlElement>(GetElement());
  if (!form_element) {
    return nullptr;
  }
  HTMLElement* target_popover = form_element->popoverTargetElement().popover;
  if (!target_popover || !target_popover->popoverOpen()) {
    return nullptr;
  }
  if (ElementTraversal::NextSkippingChildren(*form_element) == target_popover) {
    // The next element is already the popover.
    return nullptr;
  }
  return AXObjectCache().GetOrCreate(target_popover);
}

// Try to get an aria-controls for an <input role="combobox">, because it
// helps identify focusable options in the listbox using activedescendant
// detection, even though the focus is on the textbox and not on the listbox
// ancestor.
AXObject* AXObject::GetControlsListboxForTextfieldCombobox() {
  // Only perform work for textfields.
  if (!ui::IsTextField(RoleValue()))
    return nullptr;

  // Object is ignored for some reason, most likely hidden.
  if (AccessibilityIsIgnored()) {
    return nullptr;
  }

  // Authors used to be told to use aria-owns to point from the textfield to the
  // listbox. However, the aria-owns  on a textfield must be ignored for its
  // normal purpose because a textfield cannot have children. This code allows
  // the textfield's invalid aria-owns to be remapped to aria-controls.
  DCHECK(GetElement());
  HeapVector<Member<Element>> owned_elements;
  Vector<String> ids;
  AXObject* listbox_candidate = nullptr;
  if (ElementsFromAttribute(GetElement(), owned_elements,
                            html_names::kAriaOwnsAttr, ids) &&
      owned_elements.size() > 0) {
    DCHECK(owned_elements[0]);
    listbox_candidate = AXObjectCache().GetOrCreate(owned_elements[0]);
  }

  // Combobox grouping <div role="combobox"><input><div role="listbox"></div>.
  if (!listbox_candidate && RoleValue() == ax::mojom::blink::Role::kTextField &&
      ParentObject()->RoleValue() ==
          ax::mojom::blink::Role::kComboBoxGrouping) {
    listbox_candidate = UnignoredNextSibling();
  }

  // Heuristic: try the next sibling, but we are very strict about this in
  // order to avoid false positives such as an <input> followed by a
  // <select>.
  if (!listbox_candidate &&
      RoleValue() == ax::mojom::blink::Role::kTextFieldWithComboBox) {
    // Require an aria-activedescendant on the <input>.
    if (!GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant))
      return nullptr;
    listbox_candidate = UnignoredNextSibling();
    if (!listbox_candidate)
      return nullptr;
    // Require that the next sibling is not a <select>.
    if (IsA<HTMLSelectElement>(listbox_candidate->GetNode()))
      return nullptr;
    // Require an ARIA role on the next sibling.
    if (listbox_candidate->AriaRoleAttribute() !=
        ax::mojom::blink::Role::kListBox) {
      return nullptr;
    }
    // Naming a listbox within a composite combobox widget is not part of a
    // known/used pattern. If it has a name, it's an indicator that it's
    // probably a separate listbox widget.
    if (!listbox_candidate->ComputedName().empty())
      return nullptr;
  }

  if (!listbox_candidate ||
      listbox_candidate->RoleValue() != ax::mojom::blink::Role::kListBox) {
    return nullptr;
  }

  return listbox_candidate;
}

const AtomicString& AXObject::GetRoleAttributeStringForObjectAttribute(
    ui::AXNodeData* node_data) {
  // All ARIA roles are exposed in xml-roles.
  if (const AtomicString& role_str =
          GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole)) {
    return role_str;
  }

  ax::mojom::blink::Role landmark_role = node_data->role;
  if (landmark_role == ax::mojom::blink::Role::kFooter) {
    // - Treat <footer> as "contentinfo" in xml-roles object attribute.
    landmark_role = ax::mojom::blink::Role::kContentInfo;
  } else if (landmark_role == ax::mojom::blink::Role::kHeader) {
    // - Treat <header> as "banner" in xml-roles object attribute.
    landmark_role = ax::mojom::blink::Role::kBanner;
  } else if (!ui::IsLandmark(node_data->role)) {
    // Landmarks are the only roles exposed in xml-roles, matching Firefox.
    return g_null_atom;
  }
  return ARIARoleName(landmark_role);
}

void AXObject::SerializeMarkerAttributes(ui::AXNodeData* node_data) const {
  // Implemented in subclasses.
}

bool AXObject::IsAXNodeObject() const {
  return false;
}

bool AXObject::IsAXLayoutObject() const {
  return false;
}

bool AXObject::IsAXInlineTextBox() const {
  return false;
}

bool AXObject::IsList() const {
  return ui::IsList(RoleValue());
}

bool AXObject::IsAXListBox() const {
  return false;
}

bool AXObject::IsAXListBoxOption() const {
  return false;
}

bool AXObject::IsMenuList() const {
  return false;
}

bool AXObject::IsMenuListOption() const {
  return false;
}

bool AXObject::IsMenuListPopup() const {
  return false;
}

bool AXObject::IsMockObject() const {
  return false;
}

bool AXObject::IsProgressIndicator() const {
  return false;
}

bool AXObject::IsAXRadioInput() const {
  return false;
}

bool AXObject::IsSlider() const {
  return false;
}

bool AXObject::IsValidationMessage() const {
  return false;
}

bool AXObject::IsVirtualObject() const {
  return false;
}

ax::mojom::blink::Role AXObject::ComputeFinalRoleForSerialization() const {
  // An SVG with no accessible children should be exposed as an image rather
  // than a document. See https://github.com/w3c/svg-aam/issues/12.
  // We do this check here for performance purposes: When
  // AXLayoutObject::RoleFromLayoutObjectOrNode is called, that node's
  // accessible children have not been calculated. Rather than force calculation
  // there, wait until we have the full tree.
  if (role_ == ax::mojom::blink::Role::kSvgRoot && !UnignoredChildCount()) {
    return ax::mojom::blink::Role::kImage;
  }

  // DPUB ARIA 1.1 deprecated doc-biblioentry and doc-endnote, but it's still
  // possible to create these internal roles / platform mappings with a listitem
  // (native or ARIA) inside of a doc-bibliography or doc-endnotes section.
  if (role_ == ax::mojom::blink::Role::kListItem) {
    AXObject* ancestor = CachedParentObject();
    if (ancestor && ancestor->RoleValue() == ax::mojom::blink::Role::kList) {
      // Go up to the root, or next list, checking to see if the list item is
      // inside an endnote or bibliography section. If it is, remap the role.
      // The remapping does not occur for list items multiple levels deep.
      while (true) {
        ancestor = ancestor->CachedParentObject();
        if (!ancestor)
          break;
        ax::mojom::blink::Role ancestor_role = ancestor->RoleValue();
        if (ancestor_role == ax::mojom::blink::Role::kList)
          break;
        if (ancestor_role == ax::mojom::blink::Role::kDocBibliography)
          return ax::mojom::blink::Role::kDocBiblioEntry;
        if (ancestor_role == ax::mojom::blink::Role::kDocEndnotes)
          return ax::mojom::blink::Role::kDocEndnote;
      }
    }
  }

  if (role_ == ax::mojom::blink::Role::kHeader) {
    if (IsDescendantOfLandmarkDisallowedElement()) {
      return ax::mojom::blink::Role::kHeaderAsNonLandmark;
    }
  }

  if (role_ == ax::mojom::blink::Role::kFooter) {
    if (IsDescendantOfLandmarkDisallowedElement()) {
      return ax::mojom::blink::Role::kFooterAsNonLandmark;
    }
  }

  // An <aside> element should not be considered a landmark region
  // if it is a child of a landmark disallowed element, UNLESS it has
  // an accessible name.
  if (role_ == ax::mojom::blink::Role::kComplementary) {
    if (IsDescendantOfLandmarkDisallowedElement() &&
        !IsNameFromAuthorAttribute()) {
      return ax::mojom::blink::Role::kGenericContainer;
    }
  }

  // TODO(accessibility): Consider moving the image vs. image map role logic
  // here. Currently it is implemented in AXPlatformNode subclasses and thus
  // not available to the InspectorAccessibilityAgent.
  return role_;
}

ax::mojom::blink::Role AXObject::RoleValue() const {
  return role_;
}

bool AXObject::IsARIATextField() const {
  if (IsAtomicTextField())
    return false;  // Native role supercedes the ARIA one.
  return AriaRoleAttribute() == ax::mojom::blink::Role::kTextField ||
         AriaRoleAttribute() == ax::mojom::blink::Role::kSearchBox ||
         AriaRoleAttribute() == ax::mojom::blink::Role::kTextFieldWithComboBox;
}

bool AXObject::IsButton() const {
  return ui::IsButton(RoleValue());
}

bool AXObject::IsCanvas() const {
  return RoleValue() == ax::mojom::blink::Role::kCanvas;
}

bool AXObject::IsColorWell() const {
  return RoleValue() == ax::mojom::blink::Role::kColorWell;
}

bool AXObject::IsControl() const {
  return ui::IsControl(RoleValue());
}

bool AXObject::IsDefault() const {
  return false;
}

bool AXObject::IsFieldset() const {
  return false;
}

bool AXObject::IsHeading() const {
  return ui::IsHeading(RoleValue());
}

bool AXObject::IsImage() const {
  // Canvas is not currently included so that it is not exposed unless there is
  // a label, fallback content or something to make it accessible. This decision
  // may be revisited at a later date.
  return ui::IsImage(RoleValue()) &&
         RoleValue() != ax::mojom::blink::Role::kCanvas;
}

bool AXObject::IsInputImage() const {
  return false;
}

bool AXObject::IsLink() const {
  return ui::IsLink(RoleValue());
}

bool AXObject::IsImageMapLink() const {
  return false;
}

bool AXObject::IsMenu() const {
  return RoleValue() == ax::mojom::blink::Role::kMenu;
}

bool AXObject::IsCheckable() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kRadioButton:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kToggleButton:
      return true;
    case ax::mojom::blink::Role::kTreeItem:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuListOption:
      return AriaCheckedIsPresent();
    default:
      return false;
  }
}

// Why this is here instead of AXNodeObject:
// Because an AXMenuListOption (<option>) can
// have an ARIA role of menuitemcheckbox/menuitemradio
// yet does not inherit from AXNodeObject
ax::mojom::blink::CheckedState AXObject::CheckedState() const {
  if (!IsCheckable())
    return ax::mojom::blink::CheckedState::kNone;

  // Try ARIA checked/pressed state
  const ax::mojom::blink::Role role = RoleValue();
  const auto prop = role == ax::mojom::blink::Role::kToggleButton
                        ? AOMStringProperty::kPressed
                        : AOMStringProperty::kChecked;
  const AtomicString& checked_attribute = GetAOMPropertyOrARIAAttribute(prop);
  if (checked_attribute) {
    if (EqualIgnoringASCIICase(checked_attribute, "mixed")) {
      // Only checkable role that doesn't support mixed is the switch.
      if (role != ax::mojom::blink::Role::kSwitch)
        return ax::mojom::blink::CheckedState::kMixed;
    }

    // Anything other than "false" should be treated as "true".
    return EqualIgnoringASCIICase(checked_attribute, "false")
               ? ax::mojom::blink::CheckedState::kFalse
               : ax::mojom::blink::CheckedState::kTrue;
  }

  // Native checked state
  if (role != ax::mojom::blink::Role::kToggleButton) {
    const Node* node = GetNode();
    if (!node)
      return ax::mojom::blink::CheckedState::kNone;

    // Expose native checkbox mixed state as accessibility mixed state. However,
    // do not expose native radio mixed state as accessibility mixed state.
    // This would confuse the JAWS screen reader, which reports a mixed radio as
    // both checked and partially checked, but a native mixed native radio
    // button simply means no radio buttons have been checked in the group yet.
    if (IsNativeCheckboxInMixedState(node))
      return ax::mojom::blink::CheckedState::kMixed;

    auto* html_input_element = DynamicTo<HTMLInputElement>(node);
    if (html_input_element && html_input_element->ShouldAppearChecked()) {
      return ax::mojom::blink::CheckedState::kTrue;
    }
  }

  return ax::mojom::blink::CheckedState::kFalse;
}

String AXObject::GetValueForControl() const {
  return String();
}

String AXObject::SlowGetValueForControlIncludingContentEditable() const {
  return String();
}

bool AXObject::IsNativeCheckboxInMixedState(const Node* node) {
  const auto* input = DynamicTo<HTMLInputElement>(node);
  if (!input)
    return false;

  const auto inputType = input->type();
  if (inputType != input_type_names::kCheckbox)
    return false;
  return input->ShouldAppearIndeterminate();
}

bool AXObject::IsMenuRelated() const {
  return ui::IsMenuRelated(RoleValue());
}

bool AXObject::IsMeter() const {
  return RoleValue() == ax::mojom::blink::Role::kMeter;
}

bool AXObject::IsNativeImage() const {
  return false;
}

bool AXObject::IsNativeSpinButton() const {
  return false;
}

bool AXObject::IsAtomicTextField() const {
  return blink::IsTextControl(GetNode());
}

bool AXObject::IsNonAtomicTextField() const {
  // Consivably, an <input type=text> or a <textarea> might also have the
  // contenteditable attribute applied. In such cases, the <input> or <textarea>
  // tags should supercede.
  if (IsAtomicTextField())
    return false;
  return HasContentEditableAttributeSet() || IsARIATextField();
}

bool AXObject::IsPasswordField() const {
  auto* input_element = DynamicTo<HTMLInputElement>(GetNode());
  return input_element && input_element->type() == input_type_names::kPassword;
}

bool AXObject::IsPasswordFieldAndShouldHideValue() const {
  if (!IsPasswordField())
    return false;
  const Settings* settings = GetDocument()->GetSettings();
  return settings && !settings->GetAccessibilityPasswordValuesEnabled();
}

bool AXObject::IsPresentational() const {
  return ui::IsPresentational(RoleValue());
}

bool AXObject::IsTextObject() const {
  // Objects with |ax::mojom::blink::Role::kLineBreak| are HTML <br> elements
  // and are not backed by DOM text nodes. We can't mark them as text objects
  // for that reason.
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kInlineTextBox:
    case ax::mojom::blink::Role::kStaticText:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsRangeValueSupported() const {
  if (RoleValue() == ax::mojom::blink::Role::kSplitter) {
    // According to the ARIA spec, role="separator" acts as a splitter only
    // when focusable, and supports a range only in that case.
    return CanSetFocusAttribute();
  }
  return ui::IsRangeValueSupported(RoleValue());
}

bool AXObject::IsScrollbar() const {
  return RoleValue() == ax::mojom::blink::Role::kScrollBar;
}

bool AXObject::IsNativeSlider() const {
  return false;
}

bool AXObject::IsSpinButton() const {
  return RoleValue() == ax::mojom::blink::Role::kSpinButton;
}

bool AXObject::IsTabItem() const {
  return RoleValue() == ax::mojom::blink::Role::kTab;
}

bool AXObject::IsTextField() const {
  if (IsDetached())
    return false;
  return IsAtomicTextField() || IsNonAtomicTextField();
}

bool AXObject::IsAutofillAvailable() const {
  return false;
}

bool AXObject::IsClickable() const {
  return ui::IsClickable(RoleValue());
}

AccessibilityExpanded AXObject::IsExpanded() const {
  return kExpandedUndefined;
}

bool AXObject::IsFocused() const {
  return false;
}

AccessibilityGrabbedState AXObject::IsGrabbed() const {
  return kGrabbedStateUndefined;
}

bool AXObject::IsHovered() const {
  return false;
}

bool AXObject::IsLineBreakingObject() const {
  // Not all AXObjects have an associated node or layout object. They could be
  // virtual accessibility nodes, for example.
  //
  // We assume that most images on the Web are inline.
  return !IsImage() && ui::IsStructure(RoleValue());
}

bool AXObject::IsLinked() const {
  return false;
}

bool AXObject::IsLoaded() const {
  return false;
}

bool AXObject::IsMultiSelectable() const {
  return false;
}

bool AXObject::IsOffScreen() const {
  return false;
}

bool AXObject::IsRequired() const {
  return false;
}

AccessibilitySelectedState AXObject::IsSelected() const {
  return kSelectedStateUndefined;
}

bool AXObject::IsSelectedFromFocusSupported() const {
  return false;
}

bool AXObject::IsSelectedFromFocus() const {
  return false;
}

bool AXObject::IsSelectedOptionActive() const {
  return false;
}

bool AXObject::IsNotUserSelectable() const {
  return false;
}

bool AXObject::IsVisited() const {
  return false;
}

bool AXObject::AccessibilityIsIgnored() const {
  UpdateCachedAttributeValuesIfNeeded();
#if defined(AX_FAIL_FAST_BUILD)
  if (!cached_is_ignored_ && IsDetached()) {
    NOTREACHED()
        << "A detached node cannot be ignored: " << ToString(true)
        << "\nThe Detach() method sets cached_is_ignored_ to true, but "
           "something has recomputed it.";
  }
  if (!cached_is_ignored_ && IsA<Document>(GetNode()) && CachedParentObject() &&
      CachedParentObject()->IsMenuList()) {
    NOTREACHED() << "The menulist popup's document must be ignored.";
  }
#endif
  return cached_is_ignored_;
}

bool AXObject::AccessibilityIsIgnoredButIncludedInTree() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_ignored_but_included_in_tree_;
}

// AccessibilityIsIncludedInTree should be true for all nodes that should be
// included in the tree, even if they are ignored
bool AXObject::AccessibilityIsIncludedInTree() const {
  return !AccessibilityIsIgnored() || AccessibilityIsIgnoredButIncludedInTree();
}

void AXObject::UpdateCachedAttributeValuesIfNeeded(
    bool notify_parent_of_ignored_changes) const {
  if (IsDetached()) {
    cached_is_ignored_ = true;
    cached_is_ignored_but_included_in_tree_ = false;
    return;
  }

  AXObjectCacheImpl& cache = AXObjectCache();

  if (cache.ModificationCount() == last_modification_count_)
    return;

  last_modification_count_ = cache.ModificationCount();

#if DCHECK_IS_ON()  // Required in order to get Lifecycle().ToString()
  DCHECK(!is_computing_role_)
      << "Updating cached values while computing a role is dangerous as it "
         "can lead to code that uses the AXObject before it is ready.";
  DCHECK(!is_updating_cached_values_)
      << "Reentering UpdateCachedAttributeValuesIfNeeded() on same node: "
      << GetNode();

  base::AutoReset<bool> reentrancy_protector(&is_updating_cached_values_, true);

  DCHECK(!GetDocument() || GetDocument()->Lifecycle().GetState() >=
                               DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle "
      << GetDocument()->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (IsMissingParent())
    RepairMissingParent();

  const ComputedStyle* style = GetComputedStyle();

  cached_is_hidden_via_style_ = ComputeIsHiddenViaStyle(style);

  // Decisions in what subtree descendants are included (each descendant's
  // cached children_) depends on the ARIA hidden state. When it changes,
  // the entire subtree needs to recompute descendants.
  // In addition, the below computations for is_ignored_but_included_in_tree is
  // dependent on having the correct new cached value.
  bool is_inert = ComputeIsInertViaStyle(style);
  bool is_aria_hidden = ComputeIsAriaHidden();
  if (cached_is_inert_ != is_inert ||
      cached_is_aria_hidden_ != is_aria_hidden) {
    // Update children if not already dirty (e.g. during Init() time.
    SetNeedsToUpdateChildren();
    cached_is_inert_ = is_inert;
    cached_is_aria_hidden_ = is_aria_hidden;
  }
  cached_is_descendant_of_disabled_node_ = ComputeIsDescendantOfDisabledNode();

  bool is_ignored = ComputeAccessibilityIsIgnored();
  bool is_ignored_but_included_in_tree =
      is_ignored && ComputeAccessibilityIsIgnoredButIncludedInTree();
  bool is_included_in_tree = !is_ignored || is_ignored_but_included_in_tree;
  bool included_in_tree_changed =
      is_included_in_tree != LastKnownIsIncludedInTreeValue();
  bool notify_included_in_tree_changed = false;
  if (included_in_tree_changed) {
    // If the inclusion bit is changing, we need to repair the
    // has_dirty_descendants, because it is only set on included nodes.
    if (is_included_in_tree) {
      // This is being inserted in the hierarchy as an included node: if the
      // parent has dirty descendants copy that bit to this as well, so as not
      // to interrupt the chain of descendant updates.
      if (AXObject* unignored_parent = ParentObjectUnignored()) {
        if (unignored_parent->HasDirtyDescendants()) {
          has_dirty_descendants_ = true;
        }
      }
    } else {
      // The has dirty descendant bits will only be cleared on included
      // nodes, so it should not be set on nodes that becomes unincluded.
      has_dirty_descendants_ = false;
    }

    // If the child's "included in tree" state changes, we will be notifying the
    // parent to recompute its children.
    // Exceptions:
    // - Caller passes in |notify_parent_of_ignored_changes = false| -- this
    //   occurs when this is a new child, or when a parent is in the middle of
    //   adding this child, and doing this would be redundant.
    // - Inline text boxes: their "included in tree" state is entirely dependent
    //   on their static text parent.
    if (notify_parent_of_ignored_changes &&
        RoleValue() != ax::mojom::blink::Role::kInlineTextBox) {
      notify_included_in_tree_changed = true;
    }
  }

  // Presence of inline text children depends on ignored state.
  if (is_ignored != LastKnownIsIgnoredValue() &&
      ui::CanHaveInlineTextBoxChildren(RoleValue())) {
    // Update children if not already dirty (e.g. during Init() time.
    SetNeedsToUpdateChildren();
  }

  // Call children changed on included ancestor.
  // This must be called before cached_is_ignored_* are updated, otherwise a
  // performance optimization depending on LastKnownIsIncludedInTreeValue()
  // may misfire.
  if (notify_included_in_tree_changed) {
    if (AXObject* parent = CachedParentObject()) {
      // Defers a ChildrenChanged() on the first included ancestor.
      // Must defer it, otherwise it can cause reentry into
      // UpdateCachedAttributeValuesIfNeeded() on |this|.
      // ParentObjectUnignored()->SetNeedsToUpdateChildren();
      AXObjectCache().ChildrenChangedOnAncestorOf(const_cast<AXObject*>(this));
    }
  }

  cached_is_ignored_ = is_ignored;
  cached_is_ignored_but_included_in_tree_ = is_ignored_but_included_in_tree;
  // Compute live region root, which can be from any ARIA live value, including
  // "off", or from an automatic ARIA live value, e.g. from role="status".
  // TODO(dmazzoni): remove this const_cast.
  AtomicString aria_live;
  if (GetNode() && IsA<Document>(GetNode())) {
    // The document root is never a live region root.
    cached_live_region_root_ = nullptr;
  } else if (RoleValue() == ax::mojom::blink::Role::kInlineTextBox) {
    // Inline text boxes do not need live region properties.
    cached_live_region_root_ = nullptr;
  } else if (parent_) {
    // Is a live region root if this or an ancestor is a live region.
    cached_live_region_root_ = IsLiveRegionRoot() ? const_cast<AXObject*>(this)
                                                  : parent_->LiveRegionRoot();
  }
  cached_aria_column_index_ = ComputeAriaColumnIndex();
  cached_aria_row_index_ = ComputeAriaRowIndex();

  if (GetLayoutObject() && GetLayoutObject()->IsText()) {
    cached_local_bounding_box_rect_for_accessibility_ =
        GetLayoutObject()->LocalBoundingBoxRectForAccessibility();
  }
}

bool AXObject::ComputeAccessibilityIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return ShouldIgnoreForHiddenOrInert(ignored_reasons);
}

bool AXObject::ShouldIgnoreForHiddenOrInert(
    IgnoredReasons* ignored_reasons) const {
  DCHECK(AXObjectCache().ModificationCount() == last_modification_count_)
      << "Hidden values must be computed before ignored.";

  // All nodes must have an unignored parent within their tree under
  // the root node of the web area, so force that node to always be unignored.
  if (IsA<Document>(GetNode())) {
    return false;
  }

  if (cached_is_aria_hidden_) {
    // Keep keyboard focusable elements that are aria-hidden in tree, so that
    // they can still fire events such as focus and value changes.
    if (!IsKeyboardFocusable()) {
      if (ignored_reasons)
        ComputeIsAriaHidden(ignored_reasons);
      return true;
    }
  }

  if (cached_is_inert_) {
    if (ignored_reasons) {
      ComputeIsInert(ignored_reasons);
    }
    return true;
  }

  // aria-hidden=false is meant to override visibility as the determinant in
  // AX hierarchy inclusion, but only for the element it is specified, and not
  // the entire subtree. See https://w3c.github.io/aria/#aria-hidden.
  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden)) {
    return false;
  }

  if (cached_is_hidden_via_style_) {
    if (ignored_reasons) {
      ignored_reasons->push_back(
          IgnoredReason(GetLayoutObject() ? kAXNotVisible : kAXNotRendered));
    }
    return true;
  }

  // Hide nodes that are whitespace or are occluded by CSS alt text.
  if (!GetLayoutObject() && GetNode() && !IsA<HTMLAreaElement>(GetNode()) &&
      !DisplayLockUtilities::IsDisplayLockedPreventingPaint(GetNode()) &&
      (!GetElement() || !GetElement()->HasDisplayContentsStyle())) {
    if (ignored_reasons) {
      ignored_reasons->push_back(IgnoredReason(kAXNotRendered));
    }
    return true;
  }

  return false;
}

// Note: do not rely on the value of this inside of display:none.
// In practice, it does not matter because nodes in display:none subtrees are
// marked ignored either way.
bool AXObject::IsInert() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_inert_;
}

bool AXObject::ComputeIsInertViaStyle(const ComputedStyle* style,
                                      IgnoredReasons* ignored_reasons) const {
  // TODO(szager): This method is n^2 -- it recurses into itself via
  // ComputeIsInert(), and InertRoot() does as well.
  if (style) {
    if (style->IsInert()) {
      if (ignored_reasons) {
        const AXObject* ax_inert_root = InertRoot();
        if (ax_inert_root == this) {
          ignored_reasons->push_back(IgnoredReason(kAXInertElement));
          return true;
        }
        if (ax_inert_root) {
          ignored_reasons->push_back(
              IgnoredReason(kAXInertSubtree, ax_inert_root));
          return true;
        }
        // If there is no inert root, inertness must have been set by a modal
        // dialog or a fullscreen element (see AdjustStyleForInert).
        Document& document = GetNode()->GetDocument();
        if (HTMLDialogElement* dialog = document.ActiveModalDialog()) {
          if (AXObject* dialog_object = AXObjectCache().GetOrCreate(dialog)) {
            ignored_reasons->push_back(
                IgnoredReason(kAXActiveModalDialog, dialog_object));
            return true;
          }
        } else if (Element* fullscreen =
                       Fullscreen::FullscreenElementFrom(document)) {
          if (AXObject* fullscreen_object =
                  AXObjectCache().GetOrCreate(fullscreen)) {
            ignored_reasons->push_back(
                IgnoredReason(kAXActiveFullscreenElement, fullscreen_object));
            return true;
          }
        }
        ignored_reasons->push_back(IgnoredReason(kAXInertElement));
      }
      return true;
    } else if (IsBlockedByAriaModalDialog(ignored_reasons)) {
      if (ignored_reasons)
        ignored_reasons->push_back(IgnoredReason(kAXAriaModalDialog));
      return true;
    } else if (const LocalFrame* frame = GetNode()->GetDocument().GetFrame()) {
      // Inert frames don't expose the inertness to the style of their contents,
      // but accessibility should consider them inert anyways.
      if (frame->IsInert()) {
        if (ignored_reasons)
          ignored_reasons->push_back(IgnoredReason(kAXInertSubtree));
        return true;
      }
    }
    return false;
  }

  // Either GetNode() is null, or it's locked by content-visibility, or we
  // failed to obtain a ComputedStyle. Make a guess iterating the ancestors.
  if (const AXObject* ax_inert_root = InertRoot()) {
    if (ignored_reasons) {
      if (ax_inert_root == this) {
        ignored_reasons->push_back(IgnoredReason(kAXInertElement));
      } else {
        ignored_reasons->push_back(
            IgnoredReason(kAXInertSubtree, ax_inert_root));
      }
    }
    return true;
  } else if (IsBlockedByAriaModalDialog(ignored_reasons)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXAriaModalDialog));
    return true;
  } else if (GetNode()) {
    if (const LocalFrame* frame = GetNode()->GetDocument().GetFrame()) {
      // Inert frames don't expose the inertness to the style of their contents,
      // but accessibility should consider them inert anyways.
      if (frame->IsInert()) {
        if (ignored_reasons)
          ignored_reasons->push_back(IgnoredReason(kAXInertSubtree));
        return true;
      }
    }
  }

  AXObject* parent = ParentObject();
  if (parent && parent->IsInert()) {
    if (ignored_reasons)
      parent->ComputeIsInert(ignored_reasons);
    return true;
  }

  return false;
}

bool AXObject::ComputeIsInert(IgnoredReasons* ignored_reasons) const {
  return ComputeIsInertViaStyle(GetComputedStyle(), ignored_reasons);
}

bool AXObject::IsAriaHidden() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_aria_hidden_;
}

bool AXObject::ComputeIsAriaHidden(IgnoredReasons* ignored_reasons) const {
  // The root node of a document or popup document cannot be aria-hidden.
  if (IsA<Document>(GetNode())) {
    return false;
  }

  // aria-hidden:true works a bit like display:none.
  // * aria-hidden=true affects entire subtree.
  // * aria-hidden=false cannot override aria-hidden=true on an ancestor.
  //   It can only affect elements that are styled as hidden, and only when
  //   there is no aria-hidden=true in the ancestor chain.
  // Therefore aria-hidden=true must be checked on every ancestor.
  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden)) {
    if (ignored_reasons)
      ignored_reasons->push_back(IgnoredReason(kAXAriaHiddenElement));
    return true;
  }

  if (AXObject* parent = ParentObject()) {
    if (parent->IsAriaHidden()) {
      if (ignored_reasons) {
        ignored_reasons->push_back(
            IgnoredReason(kAXAriaHiddenSubtree, AriaHiddenRoot()));
      }
      return true;
    }
  }

  return false;
}

bool AXObject::IsModal() const {
  if (RoleValue() != ax::mojom::blink::Role::kDialog &&
      RoleValue() != ax::mojom::blink::Role::kAlertDialog)
    return false;

  bool modal = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kModal, modal)) {
    return modal;
  }

  if (GetNode() && IsA<HTMLDialogElement>(*GetNode()))
    return To<Element>(GetNode())->IsInTopLayer();

  return false;
}

bool AXObject::IsBlockedByAriaModalDialog(
    IgnoredReasons* ignored_reasons) const {
  AXObject* active_aria_modal_dialog =
      AXObjectCache().GetActiveAriaModalDialog();

  // On platforms that don't require manual pruning of the accessibility tree,
  // the active aria modal dialog should never be set, so has no effect.
  if (!active_aria_modal_dialog)
    return false;

  if (this == active_aria_modal_dialog ||
      IsDescendantOf(*active_aria_modal_dialog))
    return false;

  if (ignored_reasons) {
    ignored_reasons->push_back(
        IgnoredReason(kAXAriaModalDialog, active_aria_modal_dialog));
  }
  return true;
}

bool AXObject::IsVisible() const {
  // TODO(accessibility) Consider exposing inert objects as visible, since they
  // are visible. It should be fine, since the objexcts are ignored.
  return !IsAriaHidden() && !IsInert() && !IsHiddenViaStyle();
}

const AXObject* AXObject::AriaHiddenRoot() const {
  return IsAriaHidden() ? FindAncestorWithAriaHidden(this) : nullptr;
}

const AXObject* AXObject::InertRoot() const {
  const AXObject* object = this;
  if (!RuntimeEnabledFeatures::InertAttributeEnabled())
    return nullptr;

  while (object && !object->IsAXNodeObject())
    object = object->ParentObject();

  DCHECK(object);

  Node* node = object->GetNode();
  if (!node)
    return nullptr;
  auto* element = DynamicTo<Element>(node);
  if (!element)
    element = FlatTreeTraversal::ParentElement(*node);

  while (element) {
    if (element->IsInertRoot())
      return AXObjectCache().GetOrCreate(element);
    element = FlatTreeTraversal::ParentElement(*element);
  }

  return nullptr;
}

bool AXObject::DispatchEventToAOMEventListeners(Event& event) {
  HeapVector<Member<AccessibleNode>> event_path;
  for (AXObject* ancestor = this; ancestor;
       ancestor = ancestor->ParentObject()) {
    AccessibleNode* ancestor_accessible_node = ancestor->GetAccessibleNode();
    if (!ancestor_accessible_node)
      continue;

    if (!ancestor_accessible_node->HasEventListeners(event.type()))
      continue;

    event_path.push_back(ancestor_accessible_node);
  }

  // Short-circuit: if there are no AccessibleNodes attached anywhere
  // in the ancestry of this node, exit.
  if (!event_path.size())
    return false;

  // Check if the user has granted permission for this domain to use
  // AOM event listeners yet. This may trigger an infobar, but we shouldn't
  // block, so whatever decision the user makes will apply to the next
  // event received after that.
  //
  // Note that we only ask the user about this permission the first
  // time an event is received that actually would have triggered an
  // event listener. However, if the user grants this permission, it
  // persists for this origin from then on.
  if (!AXObjectCache().CanCallAOMEventListeners()) {
    AXObjectCache().RequestAOMEventListenerPermission();
    return false;
  }

  // Since we now know the AOM is being used in this document, get the
  // AccessibleNode for the target element and create it if necessary -
  // otherwise we wouldn't be able to set the event target. However note
  // that if it didn't previously exist it won't be part of the event path.
  AccessibleNode* target = GetAccessibleNode();
  if (!target) {
    if (Element* element = GetElement())
      target = element->accessibleNode();
  }
  if (!target)
    return false;
  event.SetTarget(target);

  // Capturing phase.
  event.SetEventPhase(Event::PhaseType::kCapturingPhase);
  for (int i = static_cast<int>(event_path.size()) - 1; i >= 0; i--) {
    // Don't call capturing event listeners on the target. Note that
    // the target may not necessarily be in the event path which is why
    // we check here.
    if (event_path[i] == target)
      break;

    event.SetCurrentTarget(event_path[i]);
    event_path[i]->FireEventListeners(event);
    if (event.PropagationStopped())
      return true;
  }

  // Targeting phase.
  event.SetEventPhase(Event::PhaseType::kAtTarget);
  event.SetCurrentTarget(event_path[0]);
  event_path[0]->FireEventListeners(event);
  if (event.PropagationStopped())
    return true;

  // Bubbling phase.
  event.SetEventPhase(Event::PhaseType::kBubblingPhase);
  for (wtf_size_t i = 1; i < event_path.size(); i++) {
    event.SetCurrentTarget(event_path[i]);
    event_path[i]->FireEventListeners(event);
    if (event.PropagationStopped())
      return true;
  }

  if (event.defaultPrevented())
    return true;

  return false;
}

bool AXObject::IsDescendantOfDisabledNode() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_descendant_of_disabled_node_;
}

bool AXObject::ComputeIsDescendantOfDisabledNode() const {
  if (IsA<Document>(GetNode()))
    return false;

  bool disabled = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled, disabled))
    return disabled;

  if (AXObject* parent = ParentObject()) {
    return parent->IsDescendantOfDisabledNode() || parent->IsDisabled();
  }

  return false;
}

bool AXObject::IsExcludedByFormControlsFilter() const {
  AXObjectCacheImpl& cache = AXObjectCache();
  const ui::AXMode& mode = cache.GetAXMode();

  bool filter_to_form_controls =
      mode.HasExperimentalFlags(ui::AXMode::kExperimentalFormControls);

  if (!filter_to_form_controls) {
    return false;
  }

  // Filter out elements hidden via style.
  if (IsHiddenViaStyle()) {
    return true;
  }

  // Keep control elements.
  if (IsControl()) {
    return false;
  }

  // Keep any relevant contextual labels on form controls.
  // TODO (aldietz): this check could have further nuance to filter out
  // irrelevant text. Potential future adjustments include: Trim out text nodes
  // with length > 40 (or some threshold), as these are likely to be prose. Trim
  // out text nodes that would end up as siblings of other text in the reduced
  // tree.
  if (RoleValue() == ax::mojom::blink::Role::kStaticText) {
    return false;
  }

  // Keep generic container shadow DOM nodes inside text controls like input
  // elements.
  if (RoleValue() == ax::mojom::blink::Role::kGenericContainer &&
      EnclosingTextControl(GetNode())) {
    return false;
  }

  // Keep focusable elements to avoid breaking focus events.
  if (CanSetFocusAttribute()) {
    return false;
  }

  // Keep elements with rich text editing.
  // This is an O(1) check that will return true for matching elements and
  // avoid the O(n) IsEditable() check below.
  // It is unlikely that password managers will need elements within
  // the content editable, but if we do then consider adding a check
  // for IsEditable(). IsEditable() is O(n) where n is the number of
  // ancestors so it should only be added if necessary.
  // We may also consider caching IsEditable value so that the
  // HasContentEditableAttributeSet call can potentially be folded into a single
  // IsEditable call. See crbug/1420757.
  if (HasContentEditableAttributeSet()) {
    return false;
  }

  return true;
}

bool AXObject::ComputeAccessibilityIsIgnoredButIncludedInTree() const {
  if (RuntimeEnabledFeatures::AccessibilityExposeIgnoredNodesEnabled())
    return true;

  // If an inline text box is ignored, it is never included in the tree.
  if (IsAXInlineTextBox()) {
    return false;
  }

  if (AXObjectCache().IsAriaOwned(this) || HasARIAOwns(GetElement())) {
    // Always include an aria-owned object. It must be a child of the
    // element with aria-owns.
    return true;
  }

  const Node* node = GetNode();

  if (!node) {
    if (GetLayoutObject()) {
      // All AXObjects created for anonymous layout objects are included.
      // See IsLayoutObjectRelevantForAccessibility() in
      // ax_object_cache_impl.cc.
      // - Visible content, such as text, images and quotes (can't have
      // children).
      // - Any containers inside of pseudo-elements.
      DCHECK(GetLayoutObject()->IsAnonymous())
          << "Object has layout object but no node and is not anonymous: "
          << GetLayoutObject();
    } else {
      // Include ignored mock objects, virtual objects and inline text boxes.
      DCHECK(IsMockObject() || IsVirtualObject())
          << "Nodeless, layout-less object found with role " << RoleValue();
    }
    // By including all of these objects in the tree, it is ensured that
    // ClearChildren() will be able to find these children and detach them
    // from their parent.
    return true;
  }

  // Labels are sometimes marked ignored, to prevent duplication when the AT
  // reads the label and the control it labels (see
  // AXNodeObject::IsRedundantLabel), but we will need them to calculate the
  // name of the control.
  if (IsA<HTMLLabelElement>(node)) {
    return true;
  }

  // Custom elements and their children are included in the tree.
  // <slot>s and their children are included in the tree.
  // Also children of <label> elements, for accname calculation purposes.
  // This checks to see whether this is a child of one of those.
  if (Node* parent_node = LayoutTreeBuilderTraversal::Parent(*node)) {
    if (parent_node->IsCustomElement() ||
        ToHTMLSlotElementIfSupportsAssignmentOrNull(parent_node)) {
      return true;
    }
    // <span>s are ignored because they are considered uninteresting. Do not add
    // them back inside labels.
    if (IsA<HTMLLabelElement>(parent_node) && !IsA<HTMLSpanElement>(node)) {
      return true;
    }
    // Simplify AXNodeObject::AddImageMapChildren() -- it will only need to deal
    // with included children.
    if (IsA<HTMLMapElement>(parent_node)) {
      return true;
    }
    // Necessary to calculate the accessible description of a ruby node.
    if (IsA<HTMLRTElement>(parent_node)) {
      return true;
    }
  }

  if (IsExcludedByFormControlsFilter()) {
    return false;
  }

  // Allow the browser side ax tree to access "visibility: [hidden|collapse]"
  // and "display: none" nodes. This is useful for APIs that return the node
  // referenced by aria-labeledby and aria-describedby.
  // The conditions are oversimplified, we will include more nodes than
  // strictly necessary for aria-labelledby and aria-describedby but we
  // avoid performing very complicated checks that could impact performance.

  // We identify nodes in display none subtrees, or nodes that are display
  // locked, because they lack a layout object.
  if (!GetLayoutObject()) {
    // Datalists and options inside them will never a layout object. They
    // match the condition above, but we don't need them for accessible
    // naming nor have any other use in the accessibility tree, so we exclude
    // them specifically. What's more, including them breaks the browser test
    // SelectToSpeakKeystrokeSelectionTest.textFieldWithComboBoxSimple.
    // Selection and position code takes into account ignored nodes, and it
    // looks like including ignored nodes for datalists and options is totally
    // unexpected, making selections misbehave.
    if (!IsA<HTMLDataListElement>(node) && !IsA<HTMLOptionElement>(node))
      return true;

  } else {  // GetLayoutObject() != null
    // We identify hidden or collapsed nodes by their associated style values.
    if (GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible)
      return true;

    // Allow the browser side ax tree to access "aria-hidden" nodes.
    // This is useful for APIs that return the node referenced by
    // aria-labeledby and aria-describedby.
    // Exception: iframes, in order to stop exposing aria-hidden iframes, where
    // there is no possibility for the content within to know it's aria-hidden.
    if (IsAriaHidden()) {
      return !IsChildTreeOwner();
    }
  }

  if (const Element* owner = node->OwnerShadowHost()) {
    // The ignored state of media controls can change without a layout update.
    // Keep them in the tree at all times so that the serializer isn't
    // accidentally working with unincluded nodes, which is not allowed.
    if (IsA<HTMLMediaElement>(owner))
      return true;

    // Do not include ignored descendants of an <input type="search"> or
    // <input type="number"> because they interfere with AXPosition code that
    // assumes a plain input field structure. Specifically, due to the ignored
    // node at the end of textfield, end of editable text position will get
    // adjusted to past text field or caret moved events will not be emitted for
    // the final offset because the associated tree position. In some cases
    // platform accessibility code will instead incorrectly emit a caret moved
    // event for the AXPosition which follows the input.
    if (IsA<HTMLInputElement>(owner) &&
        (DynamicTo<HTMLInputElement>(owner)->type() ==
             input_type_names::kSearch ||
         DynamicTo<HTMLInputElement>(owner)->type() ==
             input_type_names::kNumber)) {
      return false;
    }
  }

  // Portals don't directly expose their contents as the contents are not
  // focusable, but they use them to compute a default accessible name.
  if (GetDocument()->GetPage() && GetDocument()->GetPage()->InsidePortal())
    return true;

  Element* element = GetElement();
  if (!element)
    return false;

  // Custom elements and their children are included in the tree.
  if (element->IsCustomElement())
    return true;

  // <slot>s and their children are included in the tree.
  // Detailed explanation:
  // <slot> elements are placeholders marking locations in a shadow tree where
  // users of a web component can insert their own custom nodes. Inserted nodes
  // (also known as distributed nodes) become children of their respective slots
  // in the accessibility tree. In other words, the accessibility tree mirrors
  // the flattened DOM tree or the layout tree, not the original DOM tree.
  // Distributed nodes still maintain their parent relations and computed style
  // information with their original location in the DOM. Therefore, we need to
  // ensure that in the accessibility tree no remnant information from the
  // unflattened DOM tree remains, such as the cached parent.
  if (ToHTMLSlotElementIfSupportsAssignmentOrNull(element))
    return true;

  // Include all pseudo element content. Any anonymous subtree is included
  // from above, in the condition where there is no node.
  if (element->IsPseudoElement())
    return true;

  // Include all parents of ::before/::after/::marker pseudo elements to help
  // ClearChildren() find all children, and assist naming computation.
  // It is unnecessary to include a rule for other types of pseudo elements:
  // Specifically, ::first-letter/::backdrop are not visited by
  // LayoutTreeBuilderTraversal, and cannot be in the tree, therefore do not add
  // a special rule to include their parents.
  if (element->GetPseudoElement(kPseudoIdBefore) ||
      element->GetPseudoElement(kPseudoIdAfter) ||
      element->GetPseudoElement(kPseudoIdMarker)) {
    return true;
  }

  // Use a flag to control whether or not the <html> element is included
  // in the accessibility tree. Either way it's always marked as "ignored",
  // but eventually we want to always include it in the tree to simplify
  // some logic.
  if (IsA<HTMLHtmlElement>(element))
    return RuntimeEnabledFeatures::AccessibilityExposeHTMLElementEnabled();

  // Keep the internal accessibility tree consistent for videos which lack
  // a player and also inner text.
  if (RoleValue() == ax::mojom::blink::Role::kVideo ||
      RoleValue() == ax::mojom::blink::Role::kAudio) {
    return true;
  }

  // Always pass through Line Breaking objects, this is necessary to
  // detect paragraph edges, which are defined as hard-line breaks.
  if (IsLineBreakingObject())
    return true;

  // Ruby annotations (i.e. <rt> elements) need to be included because they are
  // used for calculating an accessible description for the ruby. We explicitly
  // exclude from the tree any <rp> elements, even though they also have the
  // kRubyAnnotation role, because such elements provide fallback content for
  // browsers that do not support ruby. Hence, their contents should not be
  // included in the accessible description, unless another condition in this
  // method decides to keep them in the tree for some reason.
  if (IsA<HTMLRTElement>(element))
    return true;

  // Preserve SVG grouping elements.
  if (IsA<SVGGElement>(element))
    return true;

  // Keep table-related elements in the tree, because it's too easy for them
  // to in and out of being ignored based on their ancestry, as their role
  // can depend on several levels up in the hierarchy.
  if (IsA<HTMLTableElement>(element) || IsA<HTMLTableSectionElement>(element) ||
      IsA<HTMLTableRowElement>(element) || IsA<HTMLTableCellElement>(element)) {
    return true;
  }

  // Ensure clean teardown of AXMenuList.
  if (auto* option = DynamicTo<HTMLOptionElement>(element)) {
    if (option->OwnerSelectElement())
      return true;
  }

  // Preserve nodes with language attributes.
  if (HasAttribute(html_names::kLangAttr))
    return true;

  return false;
}

const AXObject* AXObject::GetAtomicTextFieldAncestor(
    int max_levels_to_check) const {
  if (IsAtomicTextField())
    return this;

  if (max_levels_to_check == 0)
    return nullptr;

  if (AXObject* parent = ParentObject())
    return parent->GetAtomicTextFieldAncestor(max_levels_to_check - 1);

  return nullptr;
}

const AXObject* AXObject::DatetimeAncestor() const {
  ShadowRoot* shadow_root = GetNode()->ContainingShadowRoot();
  if (!shadow_root || shadow_root->GetType() != ShadowRootType::kUserAgent) {
    return nullptr;
  }
  auto* input = DynamicTo<HTMLInputElement>(&shadow_root->host());
  if (!input) {
    return nullptr;
  }
  if (input->type() != input_type_names::kDatetimeLocal &&
      input->type() != input_type_names::kDatetime &&
      input->type() != input_type_names::kDate &&
      input->type() != input_type_names::kTime &&
      input->type() != input_type_names::kMonth &&
      input->type() != input_type_names::kWeek) {
    return nullptr;
  }
  return AXObjectCache().GetOrCreate(input);
}

bool AXObject::LastKnownIsIgnoredValue() const {
  DCHECK(cached_is_ignored_ || !IsDetached())
      << "A detached object should always indicate that it is ignored so that "
         "it won't ever accidentally be included in the tree.";
  return cached_is_ignored_;
}

bool AXObject::LastKnownIsIgnoredButIncludedInTreeValue() const {
  DCHECK(!cached_is_ignored_but_included_in_tree_ || !IsDetached())
      << "A detached object should never be included in the tree.";
  return cached_is_ignored_but_included_in_tree_;
}

bool AXObject::LastKnownIsIncludedInTreeValue() const {
  return !LastKnownIsIgnoredValue() ||
         LastKnownIsIgnoredButIncludedInTreeValue();
}

ax::mojom::blink::Role AXObject::DetermineAccessibilityRole() {
#if DCHECK_IS_ON()
  base::AutoReset<bool> reentrancy_protector(&is_computing_role_, true);
#endif
  DCHECK(!IsDetached());

  return NativeRoleIgnoringAria();
}

bool AXObject::CanSetValueAttribute() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kColorWell:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kDateTime:
    case ax::mojom::blink::Role::kInputTime:
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kSearchBox:
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSpinButton:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kTextField:
    case ax::mojom::blink::Role::kTextFieldWithComboBox:
      return Restriction() == kRestrictionNone;
    default:
      return false;
  }
}

bool AXObject::IsFocusableStyleUsingBestAvailableState() const {
  auto* element = GetElement();
  DCHECK(element);

  // If this element's layout tree does not need an update, it means that we can
  // rely on Element's IsFocusableStyle directly, which is the best available
  // source of information.
  // Note that we also allow this to be used if we're in a style recalc, since
  // we might get here through layout object attachment. In that case, the dirty
  // bits may not have been cleared yet, but all relevant style and layout tree
  // should be up to date. Note that this quirk can be fixed by deferring AX
  // tree updates to happen after the layout tree attachment has finished.
  if (GetDocument()->InStyleRecalc() ||
      !GetDocument()->NeedsLayoutTreeUpdateForNodeIncludingDisplayLocked(
          *element)) {
    return element->IsFocusableStyle();
  }

  // The best available source of information is now the AX tree, so use that to
  // figure out whether we have focusable style.
  return element->IsBaseElementFocusableStyle();
}

bool AXObject::CanSetFocusAttribute() const {
  // If we are detached or have no document, then we can't set focus on the
  // object. Note that this early out is necessary since we access the cache and
  // the document below.
  if (IsDetached() || !GetDocument())
    return false;

  AXObjectCacheImpl& cache = AXObjectCache();
  auto* document = GetDocument();

  if (document->StyleVersion() != focus_attribute_style_version_ ||
      document->DomTreeVersion() != focus_attribute_dom_tree_version_ ||
      cache.ModificationCount() != focus_attribute_cache_modification_count_) {
    focus_attribute_style_version_ = document->StyleVersion();
    focus_attribute_dom_tree_version_ = document->DomTreeVersion();
    focus_attribute_cache_modification_count_ = cache.ModificationCount();

    cached_can_set_focus_attribute_ = ComputeCanSetFocusAttribute();
  } else {
    DCHECK_EQ(cached_can_set_focus_attribute_, ComputeCanSetFocusAttribute());
  }
  return cached_can_set_focus_attribute_;
}

// This does not use Element::IsFocusable(), as that can sometimes recalculate
// styles because of IsFocusableStyle() check, resetting the document lifecycle.
bool AXObject::ComputeCanSetFocusAttribute() const {
  DCHECK(!IsDetached());
  DCHECK(GetDocument());

  // Objects within a portal are not focusable.
  // Note that they are ignored but can be included in the tree.
  bool inside_portal =
      GetDocument()->GetPage() && GetDocument()->GetPage()->InsidePortal();
  if (inside_portal)
    return false;

  // The portal itself is focusable. Portals are treated as buttons in platform
  // APIs, hiding their subtree.
  if (RoleValue() == ax::mojom::blink::Role::kPortal)
    return true;

  // Display-locked nodes that have content-visibility: hidden are not exposed
  // to accessibility in any way, so they are not focusable. Note that for
  // content-visibility: auto cases, `ShouldIgnoreNodeDueToDisplayLock()` would
  // return false, since we're not ignoring the element in that case.
  if (GetNode() &&
      DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
          *GetNode(), DisplayLockActivationReason::kAccessibility)) {
    return false;
  }

  // Focusable: web area -- this is the only focusable non-element. Web areas
  // inside portals are not focusable though (portal contents cannot get focus).
  if (IsWebArea())
    return true;

  // NOT focusable: objects with no DOM node, e.g. extra layout blocks inserted
  // as filler, or objects where the node is not an element, such as a text
  // node or an HTML comment.
  Element* elem = GetElement();
  if (!elem)
    return false;

  // NOT focusable: inert elements. Note we can't just call IsInert() here
  // because UpdateCachedAttributeValuesIfNeeded() can end up calling
  // CanSetFocusAttribute() again, which will then try to return
  // cached_can_set_focus_attribute_, but we haven't set it yet.
  bool are_cached_attributes_up_to_date =
      AXObjectCache().ModificationCount() == last_modification_count_;
  if (are_cached_attributes_up_to_date ? cached_is_inert_ : ComputeIsInert())
    return false;

  // NOT focusable: child tree owners (it's the content area that will be marked
  // focusable in the a11y tree).
  if (IsChildTreeOwner())
    return false;

  // NOT focusable: disabled form controls.
  if (IsDisabledFormControl(elem))
    return false;

  // Option elements do not receive DOM focus but they do receive a11y focus,
  // unless they are part of a <datalist>, in which case they can be displayed
  // by the browser process, but not the renderer.
  // TODO(crbug.com/1399852) Address gaps in datalist a11y.
  if (auto* option = DynamicTo<HTMLOptionElement>(elem))
    return !option->OwnerDataListElement();

  // NOT focusable: hidden elements.
  // TODO(aleventhal) Consider caching visibility when it's safe to compute.
  if (!IsA<HTMLAreaElement>(elem) && !IsFocusableStyleUsingBestAvailableState())
    return false;

  // Focusable: element supports focus.
  if (elem->SupportsFocus())
    return true;

  // TODO(accessibility) Focusable: scrollable with the keyboard.
  // Keyboard-focusable scroll containers feature:
  // https://www.chromestatus.com/feature/5231964663578624
  // When adding here, remove similar check from ::SupportsNameFromContents().
  // if (RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
  //     IsUserScrollable()) {
  //   return true;
  // }

  // NOT focusable: everything else.
  return false;
}

// We can't use `Element::IsKeyboardFocusable()` since the downstream
// `Element::IsFocusableStyle()` call will reset the document lifecycle.
bool AXObject::IsKeyboardFocusable() const {
  if (!CanSetFocusAttribute())
    return false;

  Element* element = GetElement();
  DCHECK(element) << "Cannot be focusable without an element: "
                  << ToString(true, true);
  // TODO(jarhar) Scrollable containers should return true here if
  // `RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled()`
  // is true.
  return element->tabIndex() >= 0 || IsRootEditableElement(*element);
}

bool AXObject::CanSetSelectedAttribute() const {
  // Sub-widget elements can be selected if not disabled (native or ARIA)
  return IsSubWidget() && Restriction() != kRestrictionDisabled;
}

bool AXObject::IsSubWidget() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kCell:
    case ax::mojom::blink::Role::kColumnHeader:
    case ax::mojom::blink::Role::kRowHeader:
    case ax::mojom::blink::Role::kColumn:
    case ax::mojom::blink::Role::kRow: {
      // If it has an explicit ARIA role, it's a subwidget.
      //
      // Reasoning:
      // Static table cells are not selectable, but ARIA grid cells
      // and rows definitely are according to the spec. To support
      // ARIA 1.0, it's sufficient to just check if there's any
      // ARIA role at all, because if so then it must be a grid-related
      // role so it must be selectable.
      //
      // TODO(accessibility): an ARIA 1.1+ role of "cell", or a role of "row"
      // inside an ARIA 1.1 role of "table", should not be selectable. We may
      // need to create separate role enums for grid cells vs table cells
      // to implement this.
      if (AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown)
        return true;

      // Otherwise it's only a subwidget if it's in a grid or treegrid,
      // not in a table.
      AncestorsIterator ancestor = base::ranges::find_if(
          UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
          &AXObject::IsTableLikeRole);
      return ancestor.current_ &&
             (ancestor.current_->RoleValue() == ax::mojom::blink::Role::kGrid ||
              ancestor.current_->RoleValue() ==
                  ax::mojom::blink::Role::kTreeGrid);
    }
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kTab:
    case ax::mojom::blink::Role::kTreeItem:
      return true;
    default:
      return false;
  }
}

bool AXObject::SupportsARIASetSizeAndPosInSet() const {
  if (RoleValue() == ax::mojom::blink::Role::kRow) {
    AncestorsIterator ancestor = base::ranges::find_if(
        UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
        &AXObject::IsTableLikeRole);
    return ancestor.current_ &&
           ancestor.current_->RoleValue() == ax::mojom::blink::Role::kTreeGrid;
  }
  return ui::IsSetLike(RoleValue()) || ui::IsItemLike(RoleValue());
}

bool AXObject::IsProhibited(ax::mojom::blink::StringAttribute attribute) const {
  // ARIA 1.2 prohibits aria-roledescription on the "generic" role.
  if (attribute == ax::mojom::blink::StringAttribute::kRoleDescription)
    return RoleValue() == ax::mojom::blink::Role::kGenericContainer;
  return false;
}

bool AXObject::IsProhibited(ax::mojom::blink::IntAttribute attribute) const {
  // ARIA 1.2 prohibits exposure of aria-errormessage when aria-invalid is
  // false.
  if (attribute == ax::mojom::blink::IntAttribute::kErrormessageId)
    return GetInvalidState() == ax::mojom::blink::InvalidState::kFalse;
  return false;
}

// Simplify whitespace, but preserve a single leading and trailing whitespace
// character if it's present.
String AXObject::SimplifyName(const String& str) const {
  if (str.empty())
    return "";

  // Do not simplify name for text, unless it is pseudo content.
  // TODO(accessibility) There seems to be relatively little value for the
  // special pseudo content rule, and that the null check for node can
  // probably be removed without harm.
  if (GetNode() && ui::IsText(RoleValue()))
    return str;

  bool has_before_space = IsHTMLSpace<UChar>(str[0]);
  bool has_after_space = IsHTMLSpace<UChar>(str[str.length() - 1]);
  String simplified = str.SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  if (!has_before_space && !has_after_space)
    return simplified;

  // Preserve a trailing and/or leading space.
  StringBuilder result;
  if (has_before_space)
    result.Append(' ');
  result.Append(simplified);
  if (has_after_space)
    result.Append(' ');
  return result.ToString();
}

String AXObject::ComputedName() const {
  ax::mojom::blink::NameFrom name_from;
  AXObjectVector name_objects;
  return GetName(name_from, &name_objects);
}

String AXObject::GetName(ax::mojom::blink::NameFrom& name_from,
                         AXObject::AXObjectVector* name_objects) const {
  HeapHashSet<Member<const AXObject>> visited;
  AXRelatedObjectVector related_objects;

  // Initialize |name_from|, as TextAlternative() might never set it in some
  // cases.
  name_from = ax::mojom::blink::NameFrom::kNone;
  String text = TextAlternative(false, nullptr, visited, name_from,
                                &related_objects, nullptr);

  if (name_objects) {
    name_objects->clear();
    for (NameSourceRelatedObject* related_object : related_objects)
      name_objects->push_back(related_object->object);
  }

  return SimplifyName(text);
}

String AXObject::GetName(NameSources* name_sources) const {
  AXObjectSet visited;
  ax::mojom::blink::NameFrom tmp_name_from;
  AXRelatedObjectVector tmp_related_objects;
  String text = TextAlternative(false, nullptr, visited, tmp_name_from,
                                &tmp_related_objects, name_sources);
  return SimplifyName(text);
}

String AXObject::RecursiveTextAlternative(
    const AXObject& ax_obj,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited) {
  ax::mojom::blink::NameFrom tmp_name_from = ax::mojom::blink::NameFrom::kNone;
  return RecursiveTextAlternative(ax_obj, aria_label_or_description_root,
                                  visited, tmp_name_from);
}

String AXObject::RecursiveTextAlternative(
    const AXObject& ax_obj,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from) {
  if (visited.Contains(&ax_obj) && !aria_label_or_description_root)
    return String();

  return ax_obj.TextAlternative(true, aria_label_or_description_root, visited,
                                name_from, nullptr, nullptr);
}

const ComputedStyle* AXObject::GetComputedStyle() const {
  Node* node = GetNode();
  if (!node)
    return nullptr;

#if DCHECK_IS_ON()
  DCHECK(GetDocument());
  DCHECK(GetDocument()->Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle "
      << GetDocument()->Lifecycle().ToString();
#endif

  // content-visibility:hidden or content-visibility: auto.
  if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(node))
    return nullptr;

  // For elements with layout objects we can get their style directly.
  if (GetLayoutObject())
    return GetLayoutObject()->Style();

  return node->GetComputedStyle();
}

// There are 4 ways to use CSS to hide something:
// * "display: none" is "destroy rendering state and don't do anything in the
//   subtree"
// * "visibility: [hidden|collapse]" are "don't visually show things, but still
//   keep all of the rendering up to date"
// * "content-visibility: hidden" is "don't show anything, skip all of the
//   work, but don't destroy the work that was already there"
// * "content-visibility: auto" is "paint when it's scrolled into the viewport,
//   but its layout information is not updated when it isn't"
bool AXObject::ComputeIsHiddenViaStyle(const ComputedStyle* style) const {
  // The the parent element of text is hidden, then the text is hidden too.
  // This helps provide more consistent results in edge cases, e.g. text inside
  // of a <canvas> or display:none content.
  if (RoleValue() == ax::mojom::blink::Role::kStaticText) {
    // TODO(accessibility) All text objects should have a parent, and therefore
    // the extra null check should be unnecessary.
    DCHECK(ParentObject());
    if (ParentObject() && ParentObject()->IsHiddenViaStyle()) {
      return true;
    }
  }

  if (style) {
    if (GetLayoutObject())
      return style->Visibility() != EVisibility::kVisible;

    // TODO(crbug.com/1286465): It's not consistent to only check
    // IsEnsuredInDisplayNone() on layoutless elements.
    return GetNode()->IsElementNode() &&
           (style->IsEnsuredInDisplayNone() ||
            style->Visibility() != EVisibility::kVisible);
  }

  Node* node = GetNode();
  if (!node)
    return false;

  // content-visibility:hidden or content-visibility: auto.
  if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(node)) {
    // Ensure contents of head, style and script are not exposed when
    // display-locked --the only time they are ever exposed is if author
    // explicitly makes them visible.
    DCHECK(!Traversal<SVGStyleElement>::FirstAncestorOrSelf(*node)) << node;
    DCHECK(!Traversal<HTMLHeadElement>::FirstAncestorOrSelf(*node)) << node;
    DCHECK(!Traversal<HTMLStyleElement>::FirstAncestorOrSelf(*node)) << node;
    DCHECK(!Traversal<HTMLScriptElement>::FirstAncestorOrSelf(*node)) << node;

    // content-visibility: hidden subtrees are always hidden.
    // content-visibility: auto subtrees are treated as visible, as we must
    // make a guess since computed style is not available.
    return DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
        *node, DisplayLockActivationReason::kAccessibility);
  }

  return node->IsElementNode();
}

bool AXObject::IsHiddenViaStyle() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_hidden_via_style_;
}

// Return true if this should be removed from accessible name computations.
// We must take into account if we are traversing an aria-labelledby or
// describedby relation, because those can use hidden subtrees. When the target
// node of the aria-labelledby or describedby relation is hidden, we contribute
// all its children, because there is no way to know if they are explicitly
// hidden or they inherited the hidden value. See:
// https://github.com/w3c/accname/issues/57
bool AXObject::IsHiddenForTextAlternativeCalculation(
    const AXObject* aria_label_or_description_root) const {
  auto* node = GetNode();
  if (!node)
    return false;

  // Display-locked elements are available for text/name resolution.
  if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(node))
    return false;

  Document* document = GetDocument();
  if (!document || !document->GetFrame())
    return false;

  // Do not contribute <noscript> to text alternative of an ancestor.
  if (IsA<HTMLNoScriptElement>(node))
    return true;

  // Always contribute SVG <title> despite it having a hidden style by default.
  if (IsA<SVGTitleElement>(node))
    return false;

  // Always contribute SVG <desc> despite it having a hidden style by default.
  if (IsA<SVGDescElement>(node))
    return false;

  // Always contribute text nodes, because they don't have display-related
  // properties of their own, only their parents do. Parents should have been
  // checked for their contribution earlier in the process.
  if (IsA<Text>(node))
    return false;

  // Markers do not contribute to the accessible name.
  // TODO(accessibility): Chrome has never included markers, but that's
  // actually undefined behavior. We will have to revisit after this is
  // settled, see: https://github.com/w3c/accname/issues/76
  if (node->IsMarkerPseudoElement())
    return true;

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  // When traversing an aria-labelledby relation where the targeted node is
  // hidden, we must contribute its children. There is no way to know if they
  // are explicitly hidden or they inherited the hidden value, so we resort to
  // contributing them all. See also: https://github.com/w3c/accname/issues/57
  if (aria_label_or_description_root &&
      !aria_label_or_description_root->IsVisible()) {
    return false;
  }

  // aria-hidden nodes are generally excluded, with the exception:
  // when computing name/description through an aria-labelledby/describedby
  // relation, if the target of the relation is hidden it will expose the entire
  // subtree, including aria-hidden=true nodes. The exception was accounted in
  // the previous if block, so we are safe to hide any node with
  // aria-hidden=true at this point.
  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden)) {
    // We only hide aria-hidden text if the node does not support focus as a
    // bad authoring correction.
    if (!CanSetFocusAttribute())
      return true;
  } else {
    // When IsAriaHidden() returns false, we only know the node is not in an
    // aria-hidden="true" subtree. We need to check for the case where
    // aria-hidden="false" specifically.
    if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
      return false;
  }

  return IsHiddenViaStyle();
}

String AXObject::AriaTextAlternative(
    bool recursive,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from,
    AXRelatedObjectVector* related_objects,
    NameSources* name_sources,
    bool* found_text_alternative) const {
  String text_alternative;
  bool already_visited = visited.Contains(this);
  visited.insert(this);

  // Slots are elements that cannot be named.
  if (IsA<HTMLSlotElement>(GetNode())) {
    *found_text_alternative = false;
    return String();
  }

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (IsHiddenForTextAlternativeCalculation(aria_label_or_description_root)) {
    *found_text_alternative = true;
    return String();
  }

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!aria_label_or_description_root && !already_visited) {
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;

    // Check ARIA attributes.
    const QualifiedName& attr =
        HasAttribute(html_names::kAriaLabeledbyAttr) &&
                !HasAttribute(html_names::kAriaLabelledbyAttr)
            ? html_names::kAriaLabeledbyAttr
            : html_names::kAriaLabelledbyAttr;

    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, attr));
      name_sources->back().type = name_from;
    }

    Element* element = GetElement();
    if (element) {
      HeapVector<Member<Element>> elements_from_attribute;
      Vector<String> ids;
      ElementsFromAttribute(element, elements_from_attribute, attr, ids);

      const AtomicString& aria_labelledby = GetAttribute(attr);

      if (!aria_labelledby.IsNull()) {
        if (name_sources)
          name_sources->back().attribute_value = aria_labelledby;

        // Operate on a copy of |visited| so that if |name_sources| is not
        // null, the set of visited objects is preserved unmodified for future
        // calculations.
        AXObjectSet visited_copy = visited;
        text_alternative = TextFromElements(
            true, visited_copy, elements_from_attribute, related_objects);
        if (!ids.empty())
          AXObjectCache().UpdateReverseTextRelations(this, ids);
        if (!text_alternative.IsNull()) {
          if (name_sources) {
            NameSource& source = name_sources->back();
            source.type = name_from;
            source.related_objects = *related_objects;
            source.text = text_alternative;
            *found_text_alternative = true;
          } else {
            *found_text_alternative = true;
            return text_alternative;
          }
        } else if (name_sources) {
          name_sources->back().invalid = true;
        }
      }
    }
  }

  // Step 2C from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  name_from = ax::mojom::blink::NameFrom::kAttribute;
  if (name_sources) {
    name_sources->push_back(
        NameSource(*found_text_alternative, html_names::kAriaLabelAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& aria_label =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLabel);
  if (!aria_label.empty()) {
    text_alternative = aria_label;

    if (name_sources) {
      NameSource& source = name_sources->back();
      source.text = text_alternative;
      source.attribute_value = aria_label;
      *found_text_alternative = true;
    } else {
      *found_text_alternative = true;
      return text_alternative;
    }
  }

  return text_alternative;
}

String AXObject::TextFromElements(
    bool in_aria_labelledby_traversal,
    AXObjectSet& visited,
    HeapVector<Member<Element>>& elements,
    AXRelatedObjectVector* related_objects) const {
  StringBuilder accumulated_text;
  bool found_valid_element = false;
  AXRelatedObjectVector local_related_objects;

  for (const auto& element : elements) {
    AXObject* ax_element = AXObjectCache().GetOrCreate(element);
    if (ax_element) {
      found_valid_element = true;
      AXObject* aria_labelled_by_node = nullptr;
      if (in_aria_labelledby_traversal)
        aria_labelled_by_node = ax_element;
      String result =
          RecursiveTextAlternative(*ax_element, aria_labelled_by_node, visited);
      visited.insert(ax_element);
      local_related_objects.push_back(
          MakeGarbageCollected<NameSourceRelatedObject>(ax_element, result));
      if (!result.empty()) {
        if (!accumulated_text.empty())
          accumulated_text.Append(' ');
        accumulated_text.Append(result);
      }
    }
  }
  if (!found_valid_element)
    return String();
  if (related_objects)
    *related_objects = local_related_objects;
  return accumulated_text.ToString();
}

// static
void AXObject::TokenVectorFromAttribute(Element* element,
                                        Vector<String>& tokens,
                                        const QualifiedName& attribute) {
  if (!element)
    return;

  String attribute_value = element->FastGetAttribute(attribute).GetString();
  if (attribute_value.empty())
    return;

  attribute_value = attribute_value.SimplifyWhiteSpace();
  attribute_value.Split(' ', tokens);
}

// static
bool AXObject::ElementsFromAttribute(Element* from,
                                     HeapVector<Member<Element>>& elements,
                                     const QualifiedName& attribute,
                                     Vector<String>& ids) {
  if (!from)
    return false;

  // We compute the attr-associated elements, which are either explicitly set
  // element references set via the IDL, or computed from the content attribute.
  TokenVectorFromAttribute(from, ids, attribute);
  HeapVector<Member<Element>>* attr_associated_elements =
      from->GetElementArrayAttribute(attribute);
  if (!attr_associated_elements)
    return ids.size();

  for (const auto& element : *attr_associated_elements)
    elements.push_back(element);

  return ids.size();
}

// static
bool AXObject::AriaLabelledbyElementVector(
    Element* from,
    HeapVector<Member<Element>>& elements,
    Vector<String>& ids) {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  if (ElementsFromAttribute(from, elements, html_names::kAriaLabelledbyAttr,
                            ids) &&
      elements.size() > 0) {
    return true;
  }

  return ElementsFromAttribute(from, elements, html_names::kAriaLabeledbyAttr,
                               ids) &&
         elements.size() > 0;
}

// static
bool AXObject::IsNameFromAriaAttribute(Element* element) {
  // TODO(accessibility) Make this work for virtual nodes.

  if (!element)
    return false;

  HeapVector<Member<Element>> elements_from_attribute;
  Vector<String> ids;
  if (AriaLabelledbyElementVector(element, elements_from_attribute, ids))
    return true;

  const AtomicString& aria_label = AccessibleNode::GetPropertyOrARIAAttribute(
      element, AOMStringProperty::kLabel);
  if (!aria_label.empty())
    return true;

  return false;
}

bool AXObject::IsNameFromAuthorAttribute() const {
  return IsNameFromAriaAttribute(GetElement()) ||
         HasAttribute(html_names::kTitleAttr);
}

AXObject* AXObject::InPageLinkTarget() const {
  return nullptr;
}

const AtomicString& AXObject::EffectiveTarget() const {
  return g_null_atom;
}

AccessibilityOrientation AXObject::Orientation() const {
  // In ARIA 1.1, the default value for aria-orientation changed from
  // horizontal to undefined.
  return kAccessibilityOrientationUndefined;
}

AXObject* AXObject::GetChildFigcaption() const { return nullptr; }

bool AXObject::IsDescendantOfLandmarkDisallowedElement() const {
  return false;
}

void AXObject::LoadInlineTextBoxes() {}

void AXObject::ForceAddInlineTextBoxChildren() {}

AXObject* AXObject::NextOnLine() const {
  return nullptr;
}

AXObject* AXObject::PreviousOnLine() const {
  return nullptr;
}

absl::optional<const DocumentMarker::MarkerType>
AXObject::GetAriaSpellingOrGrammarMarker() const {
  AtomicString aria_invalid_value;
  const AncestorsIterator iter = std::find_if(
      UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
      [&aria_invalid_value](const AXObject& ancestor) {
        return ancestor.HasAOMPropertyOrARIAAttribute(
                   AOMStringProperty::kInvalid, aria_invalid_value) ||
               ancestor.IsLineBreakingObject();
      });

  if (iter == UnignoredAncestorsEnd())
    return absl::nullopt;
  if (EqualIgnoringASCIICase(aria_invalid_value, "spelling"))
    return DocumentMarker::kSpelling;
  if (EqualIgnoringASCIICase(aria_invalid_value, "grammar"))
    return DocumentMarker::kGrammar;
  return absl::nullopt;
}

void AXObject::TextCharacterOffsets(Vector<int>&) const {}

void AXObject::GetWordBoundaries(Vector<int>& word_starts,
                                 Vector<int>& word_ends) const {}

int AXObject::TextLength() const {
  if (IsAtomicTextField())
    return GetValueForControl().length();
  return 0;
}

int AXObject::TextOffsetInFormattingContext(int offset) const {
  DCHECK_GE(offset, 0);
  return offset;
}

int AXObject::TextOffsetInContainer(int offset) const {
  DCHECK_GE(offset, 0);
  return offset;
}

ax::mojom::blink::DefaultActionVerb AXObject::Action() const {
  Element* action_element = ActionElement();

  if (!action_element)
    return ax::mojom::blink::DefaultActionVerb::kNone;

  // TODO(dmazzoni): Ensure that combo box text field is handled here.
  if (IsTextField())
    return ax::mojom::blink::DefaultActionVerb::kActivate;

  if (IsCheckable()) {
    return CheckedState() != ax::mojom::blink::CheckedState::kTrue
               ? ax::mojom::blink::DefaultActionVerb::kCheck
               : ax::mojom::blink::DefaultActionVerb::kUncheck;
  }

  switch (RoleValue()) {
    case ax::mojom::blink::Role::kButton:
    case ax::mojom::blink::Role::kDisclosureTriangle:
    case ax::mojom::blink::Role::kToggleButton:
      return ax::mojom::blink::DefaultActionVerb::kPress;
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuListOption:
      return ax::mojom::blink::DefaultActionVerb::kSelect;
    case ax::mojom::blink::Role::kLink:
      return ax::mojom::blink::DefaultActionVerb::kJump;
    case ax::mojom::blink::Role::kComboBoxMenuButton:
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kPopUpButton:
      return ax::mojom::blink::DefaultActionVerb::kOpen;
    default:
      if (action_element == GetNode())
        return ax::mojom::blink::DefaultActionVerb::kClick;
      return ax::mojom::blink::DefaultActionVerb::kClickAncestor;
  }
}

bool AXObject::AriaPressedIsPresent() const {
  AtomicString result;
  return HasAOMPropertyOrARIAAttribute(AOMStringProperty::kPressed, result);
}

bool AXObject::AriaCheckedIsPresent() const {
  AtomicString result;
  return HasAOMPropertyOrARIAAttribute(AOMStringProperty::kChecked, result);
}

bool AXObject::SupportsARIAExpanded() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kApplication:
    case ax::mojom::blink::Role::kButton:
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kColumnHeader:
    case ax::mojom::blink::Role::kComboBoxGrouping:
    case ax::mojom::blink::Role::kComboBoxMenuButton:
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kDisclosureTriangle:
    case ax::mojom::blink::Role::kListBox:
    case ax::mojom::blink::Role::kLink:
    case ax::mojom::blink::Role::kPopUpButton:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kRow:
    case ax::mojom::blink::Role::kRowHeader:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTab:
    case ax::mojom::blink::Role::kTextFieldWithComboBox:
    case ax::mojom::blink::Role::kToggleButton:
    case ax::mojom::blink::Role::kTreeItem:
      return true;
    case ax::mojom::blink::Role::kCell:
      // TODO(Accessibility): aria-expanded is supported on grid cells but not
      // on cells inside a static table. Consider creating separate internal
      // roles so that we can easily distinguish these two types. See also
      // IsSubWidget().
      return true;
    default:
      return false;
  }
}

bool DoesUndoRolePresentation(const AtomicString& name) {
  // This is the list of global ARIA properties that force
  // role="presentation"/"none" to be exposed, and does not contain ARIA
  // properties who's global status is being deprecated.
  // clang-format off
  DEFINE_STATIC_LOCAL(
      HashSet<AtomicString>, aria_global_properties,
      ({
        AtomicString("ARIA-ATOMIC"),
        AtomicString("ARIA-BRAILLEROLEDESCRIPTION"),
        AtomicString("ARIA-BUSY"),
        AtomicString("ARIA-CONTROLS"),
        AtomicString("ARIA-CURRENT"),
        AtomicString("ARIA-DESCRIBEDBY"),
        AtomicString("ARIA-DESCRIPTION"),
        AtomicString("ARIA-DETAILS"),
        AtomicString("ARIA-DROPEFFECT"),
        AtomicString("ARIA-FLOWTO"),
        AtomicString("ARIA-GRABBED"),
        AtomicString("ARIA-HIDDEN"),  // For aria-hidden=false.
        AtomicString("ARIA-KEYSHORTCUTS"),
        AtomicString("ARIA-LIVE"),
        AtomicString("ARIA-OWNS"),
        AtomicString("ARIA-RELEVANT"),
        AtomicString("ARIA-ROLEDESCRIPTION")
      }));
  // clang-format on

  return aria_global_properties.Contains(name);
}

bool AXObject::HasAriaAttribute(bool does_undo_role_presentation) const {
  auto* element = GetElement();
  if (!element)
    return false;

  // A role is considered an ARIA attribute.
  if (!does_undo_role_presentation &&
      AriaRoleAttribute() != ax::mojom::blink::Role::kUnknown) {
    return true;
  }

  // Check for any attribute that begins with "aria-".
  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    auto name = attr.GetName().LocalNameUpper();
    if (name.StartsWith("ARIA-")) {
      if (!does_undo_role_presentation || DoesUndoRolePresentation(name))
        return true;
    }
  }

  return false;
}

int AXObject::IndexInParent() const {
  DCHECK(AccessibilityIsIncludedInTree())
      << "IndexInParent is only valid when a node is included in the tree";
  AXObject* ax_parent_included = ParentObjectIncludedInTree();
  if (!ax_parent_included)
    return 0;

  const AXObjectVector& siblings =
      ax_parent_included->ChildrenIncludingIgnored();

  wtf_size_t index = siblings.Find(this);

  DCHECK_NE(index, kNotFound)
      << "Could not find child in parent:"
      << "\nChild: " << ToString(true)
      << "\nParent: " << ax_parent_included->ToString(true)
      << "  #children=" << siblings.size();
  return (index == kNotFound) ? 0 : static_cast<int>(index);
}

bool AXObject::IsLiveRegionRoot() const {
  const AtomicString& live_region = LiveRegionStatus();
  return !live_region.empty();
}

bool AXObject::IsActiveLiveRegionRoot() const {
  const AtomicString& live_region = LiveRegionStatus();
  return !live_region.empty() && !EqualIgnoringASCIICase(live_region, "off");
}

const AtomicString& AXObject::LiveRegionStatus() const {
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_assertive,
                      ("assertive"));
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_polite,
                      ("polite"));
  DEFINE_STATIC_LOCAL(const AtomicString, live_region_status_off, ("off"));

  const AtomicString& live_region_status =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kLive);
  // These roles have implicit live region status.
  if (live_region_status.empty()) {
    switch (RoleValue()) {
      case ax::mojom::blink::Role::kAlert:
        return live_region_status_assertive;
      case ax::mojom::blink::Role::kLog:
      case ax::mojom::blink::Role::kStatus:
        return live_region_status_polite;
      case ax::mojom::blink::Role::kTimer:
      case ax::mojom::blink::Role::kMarquee:
        return live_region_status_off;
      default:
        break;
    }
  }

  return live_region_status;
}

const AtomicString& AXObject::LiveRegionRelevant() const {
  DEFINE_STATIC_LOCAL(const AtomicString, default_live_region_relevant,
                      ("additions text"));
  const AtomicString& relevant =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRelevant);

  // Default aria-relevant = "additions text".
  if (relevant.empty())
    return default_live_region_relevant;

  return relevant;
}

bool AXObject::IsDisabled() const {
  // <embed> or <object> with unsupported plugin, or more iframes than allowed.
  if (IsChildTreeOwner()) {
    if (IsAriaHidden()) {
      return true;
    }
    auto* html_frame_owner_element = To<HTMLFrameOwnerElement>(GetElement());
    return !html_frame_owner_element->ContentFrame();
  }

  Node* node = GetNode();
  if (node) {
    // Check for HTML form control with the disabled attribute.
    if (GetElement() && GetElement()->IsDisabledFormControl()) {
      return true;
    }
    // This is for complex pickers, such as a date picker.
    if (AXObject::IsControl()) {
      Element* owner_shadow_host = node->OwnerShadowHost();
      if (owner_shadow_host &&
          owner_shadow_host->FastHasAttribute(html_names::kDisabledAttr)) {
        return true;
      }
    }
  }
  // Check aria-disabled. According to ARIA in HTML section 3.1, aria-disabled
  // attribute does NOT override the native HTML disabled attribute.
  // https://www.w3.org/TR/html-aria/
  if (AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kDisabled))
    return true;

  // A focusable object with a disabled container.
  return cached_is_descendant_of_disabled_node_ && CanSetFocusAttribute();
}

AXRestriction AXObject::Restriction() const {
  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  if (IsDisabled())
    return kRestrictionDisabled;

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kRestrictionReadOnly : kRestrictionNone;
  }

  // This is a node that is not readonly and not disabled.
  return kRestrictionNone;
}

ax::mojom::blink::Role AXObject::AriaRoleAttribute() const {
  return ax::mojom::blink::Role::kUnknown;
}

ax::mojom::blink::Role AXObject::RawAriaRole() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsNull() || aria_role.empty())
    return ax::mojom::blink::Role::kUnknown;
  return AriaRoleStringToRoleEnum(aria_role);
}

ax::mojom::blink::Role AXObject::DetermineAriaRoleAttribute() const {
  ax::mojom::blink::Role role = RawAriaRole();

  if (role == ax::mojom::blink::Role::kRegion && !IsNameFromAuthorAttribute() &&
      !HasAttribute(html_names::kAriaRoledescriptionAttr)) {
    // Nameless ARIA regions fall back on the native element's role.
    // We only check aria-label/aria-labelledby because those are the only
    // allowed ways to name an ARIA region.
    // TODO(accessibility) The aria-roledescription logic is required, otherwise
    // ChromeVox will ignore the aria-roledescription. It only speaks the role
    // description on certain roles, and ignores it on the generic role.
    // See also https://github.com/w3c/aria/issues/1463.
    return ax::mojom::blink::Role::kUnknown;
  }

  // ARIA states if an item can get focus, it should not be presentational.
  // It also states user agents should ignore the presentational role if
  // the element has global ARIA states and properties.
  if (ui::IsPresentational(role)) {
    if (IsFrame(GetNode()))
      return ax::mojom::blink::Role::kIframePresentational;
    if ((GetElement() && GetElement()->SupportsFocus()) ||
        HasAriaAttribute(true /* does_undo_role_presentation */)) {
      // Must be exposed with a role if focusable or has a global ARIA property
      // that is allowed in this context. See
      // https://w3c.github.io/aria/#presentation for more information about the
      // conditions upon which elements with role="none"/"presentation" must be
      // included in the tree. Return Role::kUnknown, so that the native HTML
      // role is used instead.
      return ax::mojom::blink::Role::kUnknown;
    }
  }

  if (role == ax::mojom::blink::Role::kButton)
    role = ButtonRoleType();

  // Distinguish between different uses of the "combobox" role:
  //
  // ax::mojom::blink::Role::kComboBoxGrouping:
  //   <div role="combobox"><input></div>
  // ax::mojom::blink::Role::kTextFieldWithComboBox:
  //   <input role="combobox">
  // ax::mojom::blink::Role::kComboBoxMenuButton:
  //   <div tabindex=0 role="combobox">Select</div>
  if (role == ax::mojom::blink::Role::kComboBoxGrouping) {
    if (IsAtomicTextField())
      role = ax::mojom::blink::Role::kTextFieldWithComboBox;
    else if (GetElement() && GetElement()->SupportsFocus())
      role = ax::mojom::blink::Role::kComboBoxMenuButton;
  }

  return role;
}

ax::mojom::blink::HasPopup AXObject::HasPopup() const {
  return ax::mojom::blink::HasPopup::kFalse;
}

ax::mojom::blink::IsPopup AXObject::IsPopup() const {
  return ax::mojom::blink::IsPopup::kNone;
}

bool AXObject::IsEditable() const {
  const Node* node = GetNode();
  if (IsDetached() || !node)
    return false;
#if DCHECK_IS_ON()  // Required in order to get Lifecycle().ToString()
  DCHECK(GetDocument());
  DCHECK_GE(GetDocument()->Lifecycle().GetState(),
            DocumentLifecycle::kStyleClean)
      << "Unclean document style at lifecycle state "
      << GetDocument()->Lifecycle().ToString();
#endif  // DCHECK_IS_ON()

  if (blink::IsEditable(*node))
    return true;

  // For the purposes of accessibility, atomic text fields  i.e. input and
  // textarea are editable because the user can potentially enter text in them.
  if (IsAtomicTextField())
    return true;

  return false;
}

bool AXObject::IsEditableRoot() const {
  return false;
}

bool AXObject::HasContentEditableAttributeSet() const {
  return false;
}

bool AXObject::IsMultiline() const {
  if (IsDetached() || !GetNode() || !IsTextField())
    return false;

  // While the specs don't specify that we can't do <input aria-multiline=true>,
  // it is in direct contradiction to the `HTMLInputElement` which is always
  // single line. Ensure that we can't make an input report that it's multiline
  // by returning early.
  if (IsA<HTMLInputElement>(*GetNode()))
    return false;

  bool is_multiline = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kMultiline,
                                    is_multiline)) {
    return is_multiline;
  }

  return IsA<HTMLTextAreaElement>(*GetNode()) ||
         HasContentEditableAttributeSet();
}

bool AXObject::IsRichlyEditable() const {
  const Node* node = GetNode();
  if (IsDetached() || !node)
    return false;

  return node->IsRichlyEditableForAccessibility();
}

AXObject* AXObject::LiveRegionRoot() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_;
}

bool AXObject::LiveRegionAtomic() const {
  bool atomic = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kAtomic, atomic))
    return atomic;

  // ARIA roles "alert" and "status" should have an implicit aria-atomic value
  // of true.
  return RoleValue() == ax::mojom::blink::Role::kAlert ||
         RoleValue() == ax::mojom::blink::Role::kStatus;
}

const AtomicString& AXObject::ContainerLiveRegionStatus() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ ? cached_live_region_root_->LiveRegionStatus()
                                  : g_null_atom;
}

const AtomicString& AXObject::ContainerLiveRegionRelevant() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_
             ? cached_live_region_root_->LiveRegionRelevant()
             : g_null_atom;
}

bool AXObject::ContainerLiveRegionAtomic() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ &&
         cached_live_region_root_->LiveRegionAtomic();
}

bool AXObject::ContainerLiveRegionBusy() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_ &&
         cached_live_region_root_->AOMPropertyOrARIAAttributeIsTrue(
             AOMBooleanProperty::kBusy);
}

AXObject* AXObject::ElementAccessibilityHitTest(const gfx::Point& point) const {
  // Check if there are any mock elements that need to be handled.
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (child->IsMockObject() &&
        child->GetBoundsInFrameCoordinates().Contains(LayoutPoint(point)))
      return child->ElementAccessibilityHitTest(point);
  }

  return const_cast<AXObject*>(this);
}

AXObject::AncestorsIterator AXObject::UnignoredAncestorsBegin() const {
  AXObject* parent = ParentObjectUnignored();
  if (parent)
    return AXObject::AncestorsIterator(*parent);
  return UnignoredAncestorsEnd();
}

AXObject::AncestorsIterator AXObject::UnignoredAncestorsEnd() const {
  return AXObject::AncestorsIterator();
}

int AXObject::ChildCountIncludingIgnored() const {
  return static_cast<int>(ChildrenIncludingIgnored().size());
}

AXObject* AXObject::ChildAtIncludingIgnored(int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LE(index, ChildCountIncludingIgnored());
  if (index >= ChildCountIncludingIgnored())
    return nullptr;
  return ChildrenIncludingIgnored()[index];
}

const AXObject::AXObjectVector& AXObject::ChildrenIncludingIgnored() const {
  DCHECK(!IsDetached());
  return const_cast<AXObject*>(this)->ChildrenIncludingIgnored();
}

const AXObject::AXObjectVector& AXObject::ChildrenIncludingIgnored() {
  UpdateChildrenIfNecessary();
  return children_;
}

const AXObject::AXObjectVector AXObject::UnignoredChildren() const {
  return const_cast<AXObject*>(this)->UnignoredChildren();
}

const AXObject::AXObjectVector AXObject::UnignoredChildren() {
  UpdateChildrenIfNecessary();

  if (!AccessibilityIsIncludedInTree()) {
    NOTREACHED() << "We don't support finding the unignored children of "
                    "objects excluded from the accessibility tree: "
                 << ToString(true, true);
    return {};
  }

  // Capture only descendants that are not accessibility ignored, and that are
  // one level deeper than the current object after flattening any accessibility
  // ignored descendants.
  //
  // For example :
  // ++A
  // ++++B
  // ++++C IGNORED
  // ++++++F
  // ++++D
  // ++++++G
  // ++++E IGNORED
  // ++++++H IGNORED
  // ++++++++J
  // ++++++I
  //
  // Objects [B, F, D, I, J] will be returned, since after flattening all
  // ignored objects ,those are the ones that are one level deep.

  AXObjectVector unignored_children;
  AXObject* child = FirstChildIncludingIgnored();
  while (child && child != this) {
    if (child->AccessibilityIsIgnored()) {
      child = child->NextInPreOrderIncludingIgnored(this);
      continue;
    }

    unignored_children.push_back(child);
    for (; child != this; child = child->ParentObjectIncludedInTree()) {
      if (AXObject* sibling = child->NextSiblingIncludingIgnored()) {
        child = sibling;
        break;
      }
    }
  }

  return unignored_children;
}

AXObject* AXObject::FirstChildIncludingIgnored() const {
  return ChildCountIncludingIgnored() ? *ChildrenIncludingIgnored().begin()
                                      : nullptr;
}

AXObject* AXObject::LastChildIncludingIgnored() const {
  DCHECK(!IsDetached());
  return ChildCountIncludingIgnored() ? *(ChildrenIncludingIgnored().end() - 1)
                                      : nullptr;
}

AXObject* AXObject::DeepestFirstChildIncludingIgnored() const {
  if (IsDetached()) {
    NOTREACHED();
    return nullptr;
  }
  if (!ChildCountIncludingIgnored())
    return nullptr;

  AXObject* deepest_child = FirstChildIncludingIgnored();
  while (deepest_child->ChildCountIncludingIgnored())
    deepest_child = deepest_child->FirstChildIncludingIgnored();

  return deepest_child;
}

AXObject* AXObject::DeepestLastChildIncludingIgnored() const {
  if (IsDetached()) {
    NOTREACHED();
    return nullptr;
  }
  if (!ChildCountIncludingIgnored())
    return nullptr;

  AXObject* deepest_child = LastChildIncludingIgnored();
  while (deepest_child->ChildCountIncludingIgnored())
    deepest_child = deepest_child->LastChildIncludingIgnored();

  return deepest_child;
}

bool AXObject::IsAncestorOf(const AXObject& descendant) const {
  return descendant.IsDescendantOf(*this);
}

bool AXObject::IsDescendantOf(const AXObject& ancestor) const {
  const AXObject* parent = ParentObject();
  while (parent && parent != &ancestor)
    parent = parent->ParentObject();
  return !!parent;
}

AXObject* AXObject::NextSiblingIncludingIgnored() const {
  if (!AccessibilityIsIncludedInTree()) {
    NOTREACHED() << "We don't support iterating children of objects excluded "
                    "from the accessibility tree: "
                 << ToString(true, true);
    return nullptr;
  }

  const AXObject* parent_in_tree = ParentObjectIncludedInTree();
  if (!parent_in_tree)
    return nullptr;

  const int index_in_parent = IndexInParent();
  if (index_in_parent < parent_in_tree->ChildCountIncludingIgnored() - 1)
    return parent_in_tree->ChildAtIncludingIgnored(index_in_parent + 1);
  return nullptr;
}

AXObject* AXObject::PreviousSiblingIncludingIgnored() const {
  if (!AccessibilityIsIncludedInTree()) {
    NOTREACHED() << "We don't support iterating children of objects excluded "
                    "from the accessibility tree: "
                 << ToString(true, true);
    return nullptr;
  }

  const AXObject* parent_in_tree = ParentObjectIncludedInTree();
  if (!parent_in_tree)
    return nullptr;

  const int index_in_parent = IndexInParent();
  if (index_in_parent > 0)
    return parent_in_tree->ChildAtIncludingIgnored(index_in_parent - 1);
  return nullptr;
}

AXObject* AXObject::NextInPreOrderIncludingIgnored(
    const AXObject* within) const {
  if (!AccessibilityIsIncludedInTree()) {
    // TODO(crbug.com/1421052): Make sure this no longer fires then turn the
    // above into CHECK(AccessibilityIsIncludedInTree());
    DUMP_WILL_BE_NOTREACHED_NORETURN()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << ToString(true, true);
    return nullptr;
  }

  if (ChildCountIncludingIgnored())
    return FirstChildIncludingIgnored();

  if (within == this)
    return nullptr;

  const AXObject* current = this;
  AXObject* next = current->NextSiblingIncludingIgnored();
  for (; !next; next = current->NextSiblingIncludingIgnored()) {
    current = current->ParentObjectIncludedInTree();
    if (!current || within == current)
      return nullptr;
  }
  return next;
}

AXObject* AXObject::PreviousInPreOrderIncludingIgnored(
    const AXObject* within) const {
  if (!AccessibilityIsIncludedInTree()) {
    NOTREACHED() << "We don't support iterating children of objects excluded "
                    "from the accessibility tree: "
                 << ToString(true, true);
    return nullptr;
  }
  if (within == this)
    return nullptr;

  if (AXObject* sibling = PreviousSiblingIncludingIgnored()) {
    if (sibling->ChildCountIncludingIgnored())
      return sibling->DeepestLastChildIncludingIgnored();
    return sibling;
  }

  return ParentObjectIncludedInTree();
}

AXObject* AXObject::PreviousInPostOrderIncludingIgnored(
    const AXObject* within) const {
  if (!AccessibilityIsIncludedInTree()) {
    NOTREACHED() << "We don't support iterating children of objects excluded "
                    "from the accessibility tree: "
                 << ToString(true, true);
    return nullptr;
  }

  if (ChildCountIncludingIgnored())
    return LastChildIncludingIgnored();

  if (within == this)
    return nullptr;

  const AXObject* current = this;
  AXObject* previous = current->PreviousSiblingIncludingIgnored();
  for (; !previous; previous = current->PreviousSiblingIncludingIgnored()) {
    current = current->ParentObjectIncludedInTree();
    if (!current || within == current)
      return nullptr;
  }
  return previous;
}

int AXObject::UnignoredChildCount() const {
  return static_cast<int>(UnignoredChildren().size());
}

AXObject* AXObject::UnignoredChildAt(int index) const {
  const AXObjectVector unignored_children = UnignoredChildren();
  if (index < 0 || index >= static_cast<int>(unignored_children.size()))
    return nullptr;
  return unignored_children[index];
}

AXObject* AXObject::UnignoredNextSibling() const {
  if (AccessibilityIsIgnored()) {
    // TODO(crbug.com/1407397): Make sure this no longer fires then turn this
    // block into CHECK(!AccessibilityIsIgnored());
    DUMP_WILL_BE_NOTREACHED_NORETURN()
        << "We don't support finding unignored siblings for ignored "
           "objects because it is not clear whether to search for the "
           "sibling in the unignored tree or in the whole tree: "
        << ToString(true, true);
    return nullptr;
  }

  // Find the next sibling for the same unignored parent object,
  // flattening accessibility ignored objects.
  //
  // For example :
  // ++A
  // ++++B
  // ++++C IGNORED
  // ++++++E
  // ++++D
  // Objects [B, E, D] will be siblings since C is ignored.

  const AXObject* unignored_parent = ParentObjectUnignored();
  const AXObject* current_obj = this;
  while (current_obj) {
    AXObject* sibling = current_obj->NextSiblingIncludingIgnored();
    if (sibling) {
      // If we found an ignored sibling, walk in next pre-order
      // until an unignored object is found, flattening the ignored object.
      while (sibling && sibling->AccessibilityIsIgnored()) {
        sibling = sibling->NextInPreOrderIncludingIgnored(unignored_parent);
      }
      return sibling;
    }

    // If a sibling has not been found, try again with the parent object,
    // until the unignored parent is reached.
    current_obj = current_obj->ParentObjectIncludedInTree();
    if (!current_obj || !current_obj->AccessibilityIsIgnored())
      return nullptr;
  }
  return nullptr;
}

AXObject* AXObject::UnignoredPreviousSibling() const {
  if (AccessibilityIsIgnored()) {
    NOTREACHED() << "We don't support finding unignored siblings for ignored "
                    "objects because it is not clear whether to search for the "
                    "sibling in the unignored tree or in the whole tree: "
                 << ToString(true, true);
    return nullptr;
  }

  // Find the previous sibling for the same unignored parent object,
  // flattening accessibility ignored objects.
  //
  // For example :
  // ++A
  // ++++B
  // ++++C IGNORED
  // ++++++E
  // ++++D
  // Objects [B, E, D] will be siblings since C is ignored.

  const AXObject* current_obj = this;
  while (current_obj) {
    AXObject* sibling = current_obj->PreviousSiblingIncludingIgnored();
    if (sibling) {
      const AXObject* unignored_parent = ParentObjectUnignored();
      // If we found an ignored sibling, walk in previous post-order
      // until an unignored object is found, flattening the ignored object.
      while (sibling && sibling->AccessibilityIsIgnored()) {
        sibling =
            sibling->PreviousInPostOrderIncludingIgnored(unignored_parent);
      }
      return sibling;
    }

    // If a sibling has not been found, try again with the parent object,
    // until the unignored parent is reached.
    current_obj = current_obj->ParentObjectIncludedInTree();
    if (!current_obj || !current_obj->AccessibilityIsIgnored())
      return nullptr;
  }
  return nullptr;
}

AXObject* AXObject::UnignoredNextInPreOrder() const {
  AXObject* next = NextInPreOrderIncludingIgnored();
  while (next && next->AccessibilityIsIgnored()) {
    next = next->NextInPreOrderIncludingIgnored();
  }
  return next;
}

AXObject* AXObject::UnignoredPreviousInPreOrder() const {
  AXObject* previous = PreviousInPreOrderIncludingIgnored();
  while (previous && previous->AccessibilityIsIgnored()) {
    previous = previous->PreviousInPreOrderIncludingIgnored();
  }
  return previous;
}

AXObject* AXObject::ParentObject() const {
  if (IsDetached())
    return nullptr;

  // This can happen when an object in the middle of the tree is suddenly
  // detached, but the children still exist. One example of this is when
  // a <select size="1"> changes to <select size="2">, where the
  // Role::kMenuListPopup is detached.
  if (IsMissingParent())
    RepairMissingParent();

  return parent_;
}

AXObject* AXObject::ParentObjectUnignored() const {
  AXObject* parent;
  for (parent = ParentObject(); parent && parent->AccessibilityIsIgnored();
       parent = parent->ParentObject()) {
  }

  return parent;
}

AXObject* AXObject::ParentObjectIncludedInTree() const {
  AXObject* parent;
  for (parent = ParentObject();
       parent && !parent->AccessibilityIsIncludedInTree();
       parent = parent->ParentObject()) {
  }

  return parent;
}

Element* AXObject::GetClosestElement() const {
  Element* element = GetElement();
  // Certain AXObjects, such as those created from layout tree traversal,
  // have null values for `AXObject::GetNode()` and `AXObject::GetElement()`.
  // Just look for the closest parent that can handle this request.
  if (!element) {
    for (AXObject* parent = ParentObject(); parent;
         parent = parent->ParentObject()) {
      if (parent) {
        return parent->GetElement();
      }
    }
  }

  return element;
}

// Container widgets are those that a user tabs into and arrows around
// sub-widgets
bool AXObject::IsContainerWidget() const {
  return ui::IsContainerWithSelectableChildren(RoleValue());
}

AXObject* AXObject::ContainerWidget() const {
  AXObject* ancestor = ParentObjectUnignored();
  while (ancestor && !ancestor->IsContainerWidget())
    ancestor = ancestor->ParentObjectUnignored();

  return ancestor;
}

AXObject* AXObject::ContainerListMarkerIncludingIgnored() const {
  AXObject* ancestor = ParentObject();
  while (ancestor && (!ancestor->GetLayoutObject() ||
                      !ancestor->GetLayoutObject()->IsListMarker())) {
    ancestor = ancestor->ParentObject();
  }

  return ancestor;
}

// Determine which traversal approach is used to get children of an object.
bool AXObject::ShouldUseLayoutObjectTraversalForChildren() const {
  // There are two types of traversal used to find AXObjects:
  // 1. LayoutTreeBuilderTraversal, which takes FlatTreeTraversal and adds
  // pseudo elements on top of that. This is the usual case. However, while this
  // can add pseudo elements it cannot add important content descendants such as
  // text and images. For this, LayoutObject traversal (#2) is required.
  // 2. LayoutObject traversal, which just uses the children of a LayoutObject.

  // Therefore, if the object is a pseudo element or pseudo element descendant,
  // use LayoutObject traversal (#2) to find the children.
  if (GetNode() && GetNode()->IsPseudoElement())
    return true;

  // If no node, this is an anonymous layout object. The only way this can be
  // reached is inside a pseudo element subtree.
  if (!GetNode() && GetLayoutObject()) {
    DCHECK(GetLayoutObject()->IsAnonymous());
    DCHECK(AXObjectCacheImpl::IsRelevantPseudoElementDescendant(
        *GetLayoutObject()));
    return true;
  }

  return false;
}

void AXObject::UpdateChildrenIfNecessary() {
#if DCHECK_IS_ON()
  DCHECK(GetDocument()) << ToString(true, true);
  DCHECK(GetDocument()->IsActive());
  DCHECK(!GetDocument()->IsDetached());
  DCHECK(GetDocument()->GetPage());
  DCHECK(GetDocument()->View());
  DCHECK(!AXObjectCache().HasBeenDisposed());
#endif

  if (!NeedsToUpdateChildren())
    return;

#if DCHECK_IS_ON()
  // Ensure there are no unexpected, preexisting children, before we add more.
  if (IsMenuList()) {
    // AXMenuList is special and keeps its popup child, even when cleared.
    DCHECK_LE(children_.size(), 1U);
  } else {
    // Ensure children have been correctly cleared.
    DCHECK_EQ(children_.size(), 0U)
        << "\nChildren should have been cleared in SetNeedsToUpdateChildren(): "
        << GetNode() << "  with " << children_.size() << " children";
  }
#endif

  UpdateCachedAttributeValuesIfNeeded();

  AddChildren();
}

bool AXObject::NeedsToUpdateChildren() const {
  DCHECK(!children_dirty_ || CanHaveChildren())
      << "Needs to update children but cannot have children: " << GetNode()
      << " " << GetLayoutObject();
  return children_dirty_;
}

void AXObject::SetNeedsToUpdateChildren() const {
  DCHECK(!IsDetached()) << "Cannot update children on a detached node: "
                        << ToString(true, true);
  DCHECK(!AXObjectCache().HasBeenDisposed());
  if (children_dirty_ || !CanHaveChildren())
    return;
  children_dirty_ = true;
  ClearChildren();
  SetAncestorsHaveDirtyDescendants();
}

// static
bool AXObject::CanSafelyUseFlatTreeTraversalNow(Document& document) {
  return !document.IsFlatTreeTraversalForbidden() &&
         !document.GetSlotAssignmentEngine().HasPendingSlotAssignmentRecalc();
}

void AXObject::ClearChildren() const {
  DCHECK(!IsDetached());

  // No need for additional work here when clearing the entire cache at once.
  if (AXObjectCache().HasBeenDisposed()) {
    children_.clear();
    return;
  }

  // Detach all weak pointers from immediate children to their parents.
  // First check to make sure the child's parent wasn't already reassigned.
  // In addition, the immediate children are different from children_, and are
  // the objects where the parent_ points to this. For example:
  // Parent (this)
  //   Child not included in tree  (immediate child)
  //     Child included in tree (an item in |children_|)
  // These situations only occur for children that were backed by a DOM node.
  // Therefore, in addition to looping through |children_|, we must also loop
  // through any unincluded children associated with any DOM children;
  // TODO(accessibility) Try to remove ugly second loop when we transition to
  // AccessibilityExposeIgnoredNodes().

  // Loop through AXObject children.

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!is_adding_children_)
      << "Should not attempt to simultaneously add and clear children on: "
      << ToString(true, true);
  SANITIZER_CHECK(!is_computing_text_from_descendants_)
      << "Should not attempt to simultaneously compute text from descendants "
         "and clear children on: "
      << ToString(true, true);
#endif

  // Detach included children from their parent (this).
  for (const auto& child : children_) {
    // AXInlineTextBoxes depend on their parent's static text as well is the
    // parent's ignored state. Therefore, if something changed in a parent
    // static text causing its children to be cleared, remove any
    // AXInlineTextBox children from the cache rather than just detaching from
    // the parent, so they are not leaked. If the static text needs
    // AXInlineTextBoxes again in the future, it will create them based on the
    // AbstractInlineTextBoxes present at that time. Other types of objects do
    // not need this treatment --they are removed based on signals from Blink.
    if (child->IsAXInlineTextBox() && !AXObjectCache().HasBeenDisposed()) {
      AXObjectCache().Remove(child, /* notify_parent */ false);
      continue;
    }
    // Check parent first, as the child might be several levels down if there
    // are unincluded nodes in between, in which case the cached parent will
    // also be a descendant (unlike children_, parent_ does not skip levels).
    // Another case where the parent is not the same is when the child has been
    // reparented using aria-owns.
    if (child->CachedParentObject() == this)
      child->DetachFromParent();
  }

  children_.clear();

  Node* node = GetNode();
  if (!node)
    return;

  if (!CanSafelyUseFlatTreeTraversalNow(*GetDocument())) {
    // Cannot use layout tree builder traversal now, will have to rely on
    // RepairParent() at a later point.
    return;
  }

  // <slot> content is always included in the tree, so there is no need to
  // iterate through the nodes. This also protects us against slot use "after
  // poison", where attempts to access assigned nodes triggers a DCHECK.

  // Detailed explanation:
  // <slot> elements are placeholders marking locations in a shadow tree where
  // users of a web component can insert their own custom nodes. Inserted nodes
  // (also known as distributed nodes) become children of their respective slots
  // in the accessibility tree. In other words, the accessibility tree mirrors
  // the flattened DOM tree or the layout tree, not the original DOM tree.
  // Distributed nodes still maintain their parent relations and computed style
  // information with their original location in the DOM. Therefore, we need to
  // ensure that in the accessibility tree no remnant information from the
  // unflattened DOM tree remains, such as the cached parent.

  // TODO(crbug.com/1209216): Figure out why removing this causes a
  // use-after-poison and possibly replace it with a better check.
  HTMLSlotElement* slot = ToHTMLSlotElementIfSupportsAssignmentOrNull(node);
  if (slot)
    return;

  Node* map = GetMapForImage(node);
  if (map) {
    node = map;
  }

  // Detach unincluded children from their parent (this).
  // These are children that were not cleared from first loop, as well as
  // children that will be included once the parent next updates its children.
  for (Node* child_node = LayoutTreeBuilderTraversal::FirstChild(*node);
       child_node;
       child_node = LayoutTreeBuilderTraversal::NextSibling(*child_node)) {
    // Get the child object that should be detached from this parent.
    // Do not invalidate from layout, because it may be unsafe to check layout
    // at this time. However, do allow invalidations if an object changes its
    // display locking (content-visibility: auto) status, as this may be the
    // only chance to do that, and it's safe to do now.
    AXObject* ax_child_from_node = AXObjectCache().SafeGet(child_node, true);
    if (ax_child_from_node &&
        ax_child_from_node->CachedParentObject() == this) {
      if (map) {
        // Children (and other descendants, recursively) of a <map> need to be
        // fully removed, because they may no longer have a valid AX parent if
        // the image is removed. See HTMLMapElement and HTMLImageElement-related
        // code in AXObject::GetParentNodeForComputeParent.
        // Since this code only runs when |map| is set, and therefore
        // |node| is an image outside the map, this only needs to happen for
        // the map descendants, not the image descendants.
        AXObjectCache().RemoveSubtreeWithFlatTraversal(
            child_node,
            /* remove_root */ true, /* notify_parent */ false);
      } else {
        ax_child_from_node->DetachFromParent();
      }
    }
  }
}

void AXObject::ChildrenChangedWithCleanLayout() {
  DCHECK(!IsDetached()) << "Don't call on detached node: "
                        << ToString(true, true);

  // When children changed on a <map> that means we need to forward the
  // children changed to the <img> that parents the <area> elements.
  // TODO(accessibility) Consider treating <img usemap> as aria-owns so that
  // we get implementation "for free" vai relation cache, etc.
  if (HTMLMapElement* map_element = DynamicTo<HTMLMapElement>(GetNode())) {
    HTMLImageElement* image_element = map_element->ImageElement();
    if (image_element) {
      AXObject* ax_image = AXObjectCache().Get(image_element);
      if (ax_image) {
        ax_image->ChildrenChangedWithCleanLayout();
        return;
      }
    }
  }

  // Always invalidate |children_| even if it was invalidated before, because
  // now layout is clean.
  SetNeedsToUpdateChildren();

  // Between the time that AXObjectCacheImpl::ChildrenChanged() determines
  // which included parent to use and now, it's possible that the parent will
  // no longer be ignored. This is rare, but is covered by this test:
  // external/wpt/accessibility/crashtests/delayed-ignored-change.html/
  // In this case, first ancestor that's still included in the tree will used.
  if (!LastKnownIsIncludedInTreeValue()) {
    if (AXObject* ax_parent = CachedParentObject()) {
      ax_parent->ChildrenChangedWithCleanLayout();
      return;
    }
  }

  // TODO(accessibility) Move this up.
  if (!CanHaveChildren()) {
    return;
  }

  DCHECK(!IsDetached()) << "None of the above should be able to detach |this|: "
                        << ToString(true, true);

  AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);

  // Special case: when the children of a layout inline are changed, it can
  // cause whitespace redundancy in the parent object to change as well.
  if (IsA<LayoutInline>(GetLayoutObject())) {
    if (AXObject* ax_parent = CachedParentObject()) {
      if (LayoutBlockFlow* layout_block_flow =
              DynamicTo<LayoutBlockFlow>(ax_parent->GetLayoutObject())) {
        ax_parent->ChildrenChangedWithCleanLayout();
      }
    }
  }
}

Node* AXObject::GetNode() const {
  return nullptr;
}

LayoutObject* AXObject::GetLayoutObject() const {
  return nullptr;
}

Element* AXObject::GetElement() const {
  return DynamicTo<Element>(GetNode());
}

AXObject* AXObject::RootScroller() const {
  Node* global_root_scroller = GetDocument()
                                   ->GetPage()
                                   ->GlobalRootScrollerController()
                                   .GlobalRootScroller();
  if (!global_root_scroller)
    return nullptr;

  // Only return the root scroller if it's part of the same document.
  if (global_root_scroller->GetDocument() != GetDocument())
    return nullptr;

  return AXObjectCache().GetOrCreate(global_root_scroller);
}

LocalFrameView* AXObject::DocumentFrameView() const {
  if (Document* document = GetDocument())
    return document->View();
  return nullptr;
}

AtomicString AXObject::Language() const {
  // This method is used when the style engine is either not available on this
  // object, e.g. for canvas fallback content, or is unable to determine the
  // document's language. We use the following signals to detect the element's
  // language, in decreasing priority:
  // 1. The [language of a node] as defined in HTML, if known.
  // 2. The list of languages the browser sends in the [Accept-Language] header.
  // 3. The browser's default language.

  const AtomicString& lang = GetAttribute(html_names::kLangAttr);
  if (!lang.empty())
    return lang;

  // Only fallback for the root node, propagating this value down the tree is
  // handled browser side within AXNode::GetLanguage.
  //
  // TODO(chrishall): Consider moving this to AXNodeObject or AXLayoutObject as
  // the web area node is currently an AXLayoutObject.
  if (IsWebArea()) {
    const Document* document = GetDocument();
    if (document) {
      // Fall back to the first content language specified in the meta tag.
      // This is not part of what the HTML5 Standard suggests but it still
      // appears to be necessary.
      if (document->ContentLanguage()) {
        const String content_languages = document->ContentLanguage();
        Vector<String> languages;
        content_languages.Split(',', languages);
        if (!languages.empty())
          return AtomicString(languages[0].StripWhiteSpace());
      }

      if (document->GetPage()) {
        // Use the first accept language preference if present.
        const String accept_languages =
            document->GetPage()->GetChromeClient().AcceptLanguages();
        Vector<String> languages;
        accept_languages.Split(',', languages);
        if (!languages.empty())
          return AtomicString(languages[0].StripWhiteSpace());
      }
    }

    // As a last resort, return the default language of the browser's UI.
    AtomicString default_language = DefaultLanguage();
    return default_language;
  }

  return g_null_atom;
}

//
// Scrollable containers.
//

bool AXObject::IsScrollableContainer() const {
  return !!GetScrollableAreaIfScrollable();
}

bool AXObject::IsUserScrollable() const {
  // TODO(accessibility) Actually expose correct info on whether a doc is
  // is scrollable or not. Unfortunately IsScrollableContainer() always returns
  // true anyway. For now, just expose as scrollable unless overflow is hidden.
  if (IsWebArea()) {
    if (!GetScrollableAreaIfScrollable() || !GetLayoutObject())
      return false;

    const ComputedStyle* style = GetLayoutObject()->Style();
    if (!style)
      return false;

    return style->ScrollsOverflowY() || style->ScrollsOverflowX();
  }

  return GetLayoutObject() && GetLayoutObject()->IsBox() &&
         To<LayoutBox>(GetLayoutObject())->IsUserScrollable();
}

gfx::Point AXObject::GetScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return gfx::Point();
  // TODO(crbug.com/1274078): Should this be converted to scroll position, or
  // should the result type be gfx::Vector2d?
  return gfx::PointAtOffsetFromOrigin(area->ScrollOffsetInt());
}

gfx::Point AXObject::MinimumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return gfx::Point();
  // TODO(crbug.com/1274078): Should this be converted to scroll position, or
  // should the result type be gfx::Vector2d?
  return gfx::PointAtOffsetFromOrigin(area->MinimumScrollOffsetInt());
}

gfx::Point AXObject::MaximumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return gfx::Point();
  // TODO(crbug.com/1274078): Should this be converted to scroll position, or
  // should the result type be gfx::Vector2d?
  return gfx::PointAtOffsetFromOrigin(area->MaximumScrollOffsetInt());
}

void AXObject::SetScrollOffset(const gfx::Point& offset) const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return;

  // TODO(bokan): This should potentially be a UserScroll.
  area->SetScrollOffset(ScrollOffset(offset.OffsetFromOrigin()),
                        mojom::blink::ScrollType::kProgrammatic);
}

void AXObject::Scroll(ax::mojom::blink::Action scroll_action) const {
  AXObject* offset_container = nullptr;
  gfx::RectF bounds;
  gfx::Transform container_transform;
  GetRelativeBounds(&offset_container, bounds, container_transform);
  if (bounds.IsEmpty())
    return;

  gfx::Point initial = GetScrollOffset();
  gfx::Point min = MinimumScrollOffset();
  gfx::Point max = MaximumScrollOffset();

  // TODO(anastasi): This 4/5ths came from the Android implementation, revisit
  // to find the appropriate modifier to keep enough context onscreen after
  // scrolling.
  int page_x = std::max(base::ClampRound<int>(bounds.width() * 4 / 5), 1);
  int page_y = std::max(base::ClampRound<int>(bounds.height() * 4 / 5), 1);

  // Forward/backward defaults to down/up unless it can only be scrolled
  // horizontally.
  if (scroll_action == ax::mojom::blink::Action::kScrollForward) {
    scroll_action = max.y() > min.y() ? ax::mojom::blink::Action::kScrollDown
                                      : ax::mojom::blink::Action::kScrollRight;
  } else if (scroll_action == ax::mojom::blink::Action::kScrollBackward) {
    scroll_action = max.y() > min.y() ? ax::mojom::blink::Action::kScrollUp
                                      : ax::mojom::blink::Action::kScrollLeft;
  }

  int x = initial.x();
  int y = initial.y();
  switch (scroll_action) {
    case ax::mojom::blink::Action::kScrollUp:
      if (initial.y() == min.y())
        return;
      y = std::max(initial.y() - page_y, min.y());
      break;
    case ax::mojom::blink::Action::kScrollDown:
      if (initial.y() == max.y())
        return;
      y = std::min(initial.y() + page_y, max.y());
      break;
    case ax::mojom::blink::Action::kScrollLeft:
      if (initial.x() == min.x())
        return;
      x = std::max(initial.x() - page_x, min.x());
      break;
    case ax::mojom::blink::Action::kScrollRight:
      if (initial.x() == max.x())
        return;
      x = std::min(initial.x() + page_x, max.x());
      break;
    default:
      NOTREACHED();
  }

  SetScrollOffset(gfx::Point(x, y));

  if (!RuntimeEnabledFeatures::
          SynthesizedKeyboardEventsForAccessibilityActionsEnabled())
    return;

  // There are no keys that produce scroll left/right, so we shouldn't
  // synthesize any keyboard events for these actions.
  if (scroll_action == ax::mojom::blink::Action::kScrollLeft ||
      scroll_action == ax::mojom::blink::Action::kScrollRight)
    return;

  LocalDOMWindow* local_dom_window = GetDocument()->domWindow();
  DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kRawKeyDown,
                        scroll_action);
  DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kKeyUp,
                        scroll_action);
}

bool AXObject::IsTableLikeRole() const {
  return ui::IsTableLike(RoleValue()) ||
         RoleValue() == ax::mojom::blink::Role::kLayoutTable;
}

bool AXObject::IsTableRowLikeRole() const {
  return ui::IsTableRow(RoleValue()) ||
         RoleValue() == ax::mojom::blink::Role::kLayoutTableRow;
}

bool AXObject::IsTableCellLikeRole() const {
  return ui::IsCellOrTableHeader(RoleValue()) ||
         RoleValue() == ax::mojom::blink::Role::kLayoutTableCell;
}

unsigned AXObject::ColumnCount() const {
  if (!IsTableLikeRole())
    return 0;

  unsigned max_column_count = 0;
  for (const auto& row : TableRowChildren()) {
    unsigned column_count = row->TableCellChildren().size();
    max_column_count = std::max(column_count, max_column_count);
  }

  return max_column_count;
}

unsigned AXObject::RowCount() const {
  if (!IsTableLikeRole())
    return 0;

  return TableRowChildren().size();
}

void AXObject::ColumnHeaders(AXObjectVector& headers) const {
  if (!IsTableLikeRole())
    return;

  for (const auto& row : TableRowChildren()) {
    for (const auto& cell : row->TableCellChildren()) {
      if (cell->RoleValue() == ax::mojom::blink::Role::kColumnHeader)
        headers.push_back(cell);
    }
  }
}

void AXObject::RowHeaders(AXObjectVector& headers) const {
  if (!IsTableLikeRole())
    return;

  for (const auto& row : TableRowChildren()) {
    for (const auto& cell : row->TableCellChildren()) {
      if (cell->RoleValue() == ax::mojom::blink::Role::kRowHeader)
        headers.push_back(cell);
    }
  }
}

AXObject* AXObject::CellForColumnAndRow(unsigned target_column_index,
                                        unsigned target_row_index) const {
  if (!IsTableLikeRole())
    return nullptr;

  // Note that this code is only triggered if this is not a LayoutNGTable,
  // i.e. it's an ARIA grid/table.
  //
  // TODO(dmazzoni): delete this code or rename it "for testing only"
  // since it's only needed for Blink web tests and not for production.
  unsigned row_index = 0;
  for (const auto& row : TableRowChildren()) {
    unsigned column_index = 0;
    for (const auto& cell : row->TableCellChildren()) {
      if (target_column_index == column_index && target_row_index == row_index)
        return cell;
      column_index++;
    }
    row_index++;
  }

  return nullptr;
}

int AXObject::AriaColumnCount() const {
  if (!IsTableLikeRole())
    return 0;

  int32_t col_count;
  if (!HasAOMPropertyOrARIAAttribute(AOMIntProperty::kColCount, col_count))
    return 0;

  if (col_count > static_cast<int>(ColumnCount()))
    return col_count;

  // Spec says that if all of the columns are present in the DOM, it
  // is not necessary to set this attribute as the user agent can
  // automatically calculate the total number of columns.
  // It returns 0 in order not to set this attribute.
  if (col_count == static_cast<int>(ColumnCount()) || col_count != -1)
    return 0;

  return -1;
}

int AXObject::AriaRowCount() const {
  if (!IsTableLikeRole())
    return 0;

  int32_t row_count;
  if (!HasAOMPropertyOrARIAAttribute(AOMIntProperty::kRowCount, row_count))
    return 0;

  if (row_count > static_cast<int>(RowCount()))
    return row_count;

  // Spec says that if all of the rows are present in the DOM, it is
  // not necessary to set this attribute as the user agent can
  // automatically calculate the total number of rows.
  // It returns 0 in order not to set this attribute.
  if (row_count == static_cast<int>(RowCount()) || row_count != -1)
    return 0;

  // In the spec, -1 explicitly means an unknown number of rows.
  return -1;
}

unsigned AXObject::ColumnIndex() const {
  return 0;
}

unsigned AXObject::RowIndex() const {
  return 0;
}

unsigned AXObject::ColumnSpan() const {
  return IsTableCellLikeRole() ? 1 : 0;
}

unsigned AXObject::RowSpan() const {
  return IsTableCellLikeRole() ? 1 : 0;
}

unsigned AXObject::AriaColumnIndex() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_aria_column_index_;
}

unsigned AXObject::AriaRowIndex() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_aria_row_index_;
}

unsigned AXObject::ComputeAriaColumnIndex() const {
  // Return the ARIA column index if it has been set. Otherwise return a default
  // value of 0.
  uint32_t col_index = 0;
  HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kColIndex, col_index);
  return col_index;
}

unsigned AXObject::ComputeAriaRowIndex() const {
  // Return the ARIA row index if it has been set. Otherwise return a default
  // value of 0.
  uint32_t row_index = 0;
  HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kRowIndex, row_index);
  return row_index;
}

AXObject::AXObjectVector AXObject::TableRowChildren() const {
  AXObjectVector result;
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (child->IsTableRowLikeRole())
      result.push_back(child);
    else if (child->RoleValue() == ax::mojom::blink::Role::kRowGroup)
      result.AppendVector(child->TableRowChildren());
  }
  return result;
}

AXObject::AXObjectVector AXObject::TableCellChildren() const {
  AXObjectVector result;
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (child->IsTableCellLikeRole())
      result.push_back(child);
    else if (child->RoleValue() == ax::mojom::blink::Role::kGenericContainer)
      result.AppendVector(child->TableCellChildren());
  }
  return result;
}

const AXObject* AXObject::TableRowParent() const {
  const AXObject* row = ParentObjectUnignored();
  while (row && !row->IsTableRowLikeRole() &&
         row->RoleValue() == ax::mojom::blink::Role::kGenericContainer)
    row = row->ParentObjectUnignored();
  return row;
}

const AXObject* AXObject::TableParent() const {
  const AXObject* table = ParentObjectUnignored();
  while (table && !table->IsTableLikeRole() &&
         table->RoleValue() == ax::mojom::blink::Role::kGenericContainer)
    table = table->ParentObjectUnignored();
  return table;
}

int AXObject::GetDOMNodeId() const {
  Node* node = GetNode();
  if (node)
    return DOMNodeIds::IdForNode(node);
  return 0;
}

void AXObject::GetRelativeBounds(AXObject** out_container,
                                 gfx::RectF& out_bounds_in_container,
                                 gfx::Transform& out_container_transform,
                                 bool* clips_children) const {
  *out_container = nullptr;
  out_bounds_in_container = gfx::RectF();
  out_container_transform.MakeIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AXObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = gfx::RectF(explicit_element_rect_);
      return;
    }
  }

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object)
    return;

  if (layout_object->IsFixedPositioned() ||
      layout_object->IsStickyPositioned()) {
    AXObjectCache().AddToFixedOrStickyNodeList(this);
  }

  if (clips_children) {
    if (IsA<Document>(GetNode())) {
      *clips_children = true;
    } else {
      *clips_children = layout_object->HasNonVisibleOverflow();
    }
  }

  if (IsA<Document>(GetNode())) {
    if (LocalFrameView* view = layout_object->GetFrame()->View()) {
      out_bounds_in_container.set_size(gfx::SizeF(view->Size()));

      // If it's a popup, account for the popup window's offset.
      if (view->GetPage()->GetChromeClient().IsPopup()) {
        gfx::Rect frame_rect = view->FrameToScreen(view->FrameRect());
        LocalFrameView* root_view =
            AXObjectCache().GetDocument().GetFrame()->View();
        gfx::Rect root_frame_rect =
            root_view->FrameToScreen(root_view->FrameRect());

        // Screen coordinates are in DIP without device scale factor applied.
        // Accessibility expects device scale factor applied here which is
        // unapplied at the destination AXTree.
        float scale_factor =
            view->GetPage()->GetChromeClient().WindowToViewportScalar(
                layout_object->GetFrame(), 1.0f);
        out_bounds_in_container.set_origin(
            gfx::PointF(scale_factor * (frame_rect.x() - root_frame_rect.x()),
                        scale_factor * (frame_rect.y() - root_frame_rect.y())));
      }
    }
    return;
  }

  // First compute the container. The container must be an ancestor in the
  // accessibility tree, and its LayoutObject must be an ancestor in the layout
  // tree. Get the first such ancestor that's either scrollable or has a paint
  // layer.
  AXObject* container = ParentObjectUnignored();
  LayoutObject* container_layout_object = nullptr;
  if (layout_object->IsFixedPositioned()) {
    // If it's a fixed position element, the container should simply be the
    // root web area.
    container = AXObjectCache().GetOrCreate(GetDocument());
  } else {
    while (container) {
      container_layout_object = container->GetLayoutObject();
      if (container_layout_object && container_layout_object->IsBox() &&
          layout_object->IsDescendantOf(container_layout_object)) {
        if (container->IsScrollableContainer() ||
            container_layout_object->HasLayer()) {
          if (layout_object->IsAbsolutePositioned()) {
            // If it's absolutely positioned, the container must be the
            // nearest positioned container, or the root.
            if (IsA<LayoutView>(layout_object)) {
              break;
            }
            if (container_layout_object->IsPositioned())
              break;
          } else {
            break;
          }
        }
      }

      container = container->ParentObjectUnignored();
    }
  }

  if (!container)
    return;
  *out_container = container;
  out_bounds_in_container =
      layout_object->LocalBoundingBoxRectForAccessibility();

  // Frames need to take their border and padding into account so the
  // child element's computed position will be correct.
  if (layout_object->IsBox() && layout_object->GetNode() &&
      layout_object->GetNode()->IsFrameOwnerElement()) {
    out_bounds_in_container =
        gfx::RectF(To<LayoutBox>(layout_object)->PhysicalContentBoxRect());
  }

  // If the container has a scroll offset, subtract that out because we want our
  // bounds to be relative to the *unscrolled* position of the container object.
  if (auto* scrollable_area = container->GetScrollableAreaIfScrollable())
    out_bounds_in_container.Offset(scrollable_area->GetScrollOffset());

  // Compute the transform between the container's coordinate space and this
  // object.
  gfx::Transform transform = layout_object->LocalToAncestorTransform(
      To<LayoutBoxModelObject>(container_layout_object));

  // If the transform is just a simple translation, apply that to the
  // bounding box, but if it's a non-trivial transformation like a rotation,
  // scaling, etc. then return the full matrix instead.
  if (transform.IsIdentityOr2dTranslation()) {
    out_bounds_in_container.Offset(transform.To2dTranslation());
  } else {
    out_container_transform = transform;
  }
}

gfx::RectF AXObject::LocalBoundingBoxRectForAccessibility() {
  if (!GetLayoutObject())
    return gfx::RectF();
  DCHECK(GetLayoutObject()->IsText());
  UpdateCachedAttributeValuesIfNeeded();
  return cached_local_bounding_box_rect_for_accessibility_;
}

LayoutRect AXObject::GetBoundsInFrameCoordinates() const {
  AXObject* container = nullptr;
  gfx::RectF bounds;
  gfx::Transform transform;
  GetRelativeBounds(&container, bounds, transform);
  gfx::RectF computed_bounds(0, 0, bounds.width(), bounds.height());
  while (container && container != this) {
    computed_bounds.Offset(bounds.x(), bounds.y());
    if (!container->IsWebArea()) {
      computed_bounds.Offset(-container->GetScrollOffset().x(),
                             -container->GetScrollOffset().y());
    }
    computed_bounds = transform.MapRect(computed_bounds);
    container->GetRelativeBounds(&container, bounds, transform);
  }
  return LayoutRect(computed_bounds);
}

//
// Modify or take an action on an object.
//

bool AXObject::PerformAction(const ui::AXActionData& action_data) {
  Document* document = GetDocument();
  if (!document) {
    return false;
  }
  AXObjectCacheImpl& cache = AXObjectCache();
  Node* node = GetNode();
  if (!node) {
    node = GetClosestElement();
  }

  // In most cases, UpdateAllLifecyclePhasesExceptPaint() is enough, but if
  // the action is part of a display locked node, that will not update the node
  // because it's not part of the layout update cycle yet. In that case, calling
  // UpdateStyleAndLayoutTreeForNode() is also necessary.
  document->UpdateStyleAndLayoutTreeForNode(
      node, DocumentUpdateReason::kAccessibility);
  document->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);

  // Updating style and layout for the node can cause it to gain layout,
  // detaching an AXNodeObject to make room for an AXLayoutObject.
  if (IsDetached()) {
    AXObject* new_object = cache.GetOrCreate(node);
    return new_object ? new_object->PerformAction(action_data) : false;
  }

  switch (action_data.action) {
    case ax::mojom::blink::Action::kBlur:
      return OnNativeBlurAction();
    case ax::mojom::blink::Action::kClearAccessibilityFocus:
      return InternalClearAccessibilityFocusAction();
    case ax::mojom::blink::Action::kCollapse:
      return RequestCollapseAction();
    case ax::mojom::blink::Action::kDecrement:
      return RequestDecrementAction();
    case ax::mojom::blink::Action::kDoDefault:
      return RequestClickAction();
    case ax::mojom::blink::Action::kExpand:
      return RequestExpandAction();
    case ax::mojom::blink::Action::kFocus:
      return RequestFocusAction();
    case ax::mojom::blink::Action::kIncrement:
      return RequestIncrementAction();
    case ax::mojom::blink::Action::kScrollToPoint:
      return RequestScrollToGlobalPointAction(action_data.target_point);
    case ax::mojom::blink::Action::kSetAccessibilityFocus:
      return InternalSetAccessibilityFocusAction();
    case ax::mojom::blink::Action::kSetScrollOffset:
      SetScrollOffset(action_data.target_point);
      return true;
    case ax::mojom::blink::Action::kSetSequentialFocusNavigationStartingPoint:
      return RequestSetSequentialFocusNavigationStartingPointAction();
    case ax::mojom::blink::Action::kSetValue:
      return RequestSetValueAction(
          WTF::String::FromUTF8(action_data.value.c_str()));
    case ax::mojom::blink::Action::kShowContextMenu:
      return RequestShowContextMenuAction();
    case ax::mojom::blink::Action::kScrollToMakeVisible:
      return RequestScrollToMakeVisibleAction();
    case ax::mojom::blink::Action::kScrollBackward:
    case ax::mojom::blink::Action::kScrollDown:
    case ax::mojom::blink::Action::kScrollForward:
    case ax::mojom::blink::Action::kScrollLeft:
    case ax::mojom::blink::Action::kScrollRight:
    case ax::mojom::blink::Action::kScrollUp:
      Scroll(action_data.action);
      return true;
    case ax::mojom::blink::Action::kAnnotatePageImages:
    case ax::mojom::blink::Action::kCustomAction:
    case ax::mojom::blink::Action::kGetImageData:
    case ax::mojom::blink::Action::kGetTextLocation:
    case ax::mojom::blink::Action::kHideTooltip:
    case ax::mojom::blink::Action::kHitTest:
    case ax::mojom::blink::Action::kInternalInvalidateTree:
    case ax::mojom::blink::Action::kLoadInlineTextBoxes:
    case ax::mojom::blink::Action::kNone:
    case ax::mojom::blink::Action::kReplaceSelectedText:
    case ax::mojom::blink::Action::kSetSelection:
    case ax::mojom::blink::Action::kShowTooltip:
    case ax::mojom::blink::Action::kSignalEndOfTest:
    case ax::mojom::blink::Action::kResumeMedia:
    case ax::mojom::blink::Action::kStartDuckingMedia:
    case ax::mojom::blink::Action::kStopDuckingMedia:
    case ax::mojom::blink::Action::kSuspendMedia:
    case ax::mojom::blink::Action::kLongClick:
    case ax::mojom::blink::Action::kScrollToPositionAtRowColumn:
      return false;
  }
}

bool AXObject::RequestDecrementAction() {
  Event* event =
      Event::CreateCancelable(event_type_names::kAccessibledecrement);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeDecrementAction();
}

bool AXObject::RequestClickAction() {
  Event* event = Event::CreateCancelable(event_type_names::kAccessibleclick);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeClickAction();
}

bool AXObject::OnNativeClickAction() {
  Document* document = GetDocument();
  if (!document)
    return false;

  LocalFrame::NotifyUserActivation(
      document->GetFrame(),
      mojom::blink::UserActivationNotificationType::kInteraction);

  if (IsTextField())
    return OnNativeFocusAction();

  Element* element = GetClosestElement();

  if (element) {
    // Always set the sequential focus navigation starting point.
    // Even if this element isn't focusable, if you press "Tab" it will
    // start the search from this element.
    GetDocument()->SetSequentialFocusNavigationStartingPoint(element);

    // Explicitly focus the element if it's focusable but not currently
    // the focused element, to be consistent with
    // EventHandler::HandleMousePressEvent.
    if (element->IsMouseFocusable() && !element->IsFocusedElementInDocument()) {
      Page* const page = GetDocument()->GetPage();
      if (page) {
        page->GetFocusController().SetFocusedElement(
            element, GetDocument()->GetFrame(),
            FocusParams(SelectionBehaviorOnFocus::kNone,
                        mojom::blink::FocusType::kMouse, nullptr));
      }
    }

    // For most elements, AccessKeyAction triggers sending a simulated
    // click, including simulating the mousedown, mouseup, and click events.
    element->AccessKeyAction(SimulatedClickCreationScope::kFromAccessibility);
    return true;
  }

  if (CanSetFocusAttribute())
    return OnNativeFocusAction();

  return false;
}

bool AXObject::RequestFocusAction() {
  Event* event = Event::CreateCancelable(event_type_names::kAccessiblefocus);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeFocusAction();
}

bool AXObject::RequestIncrementAction() {
  Event* event =
      Event::CreateCancelable(event_type_names::kAccessibleincrement);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeIncrementAction();
}

bool AXObject::RequestScrollToGlobalPointAction(const gfx::Point& point) {
  return OnNativeScrollToGlobalPointAction(point);
}

bool AXObject::RequestScrollToMakeVisibleAction() {
  Event* event =
      Event::CreateCancelable(event_type_names::kAccessiblescrollintoview);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeScrollToMakeVisibleAction();
}

bool AXObject::RequestScrollToMakeVisibleWithSubFocusAction(
    const gfx::Rect& subfocus,
    blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
    blink::mojom::blink::ScrollAlignment vertical_scroll_alignment) {
  Document* document = GetDocument();
  if (!document) {
    return false;
  }
  AXObjectCacheImpl& cache = AXObjectCache();
  Node* node = GetNode();
  if (!node) {
    node = GetClosestElement();
  }

  // In most cases, UpdateAllLifecyclePhasesExceptPaint() is enough, but if
  // focus is is moving to a display locked node, that will not update the node
  // because it's not part of the layout update cycle yet. In that case, calling
  // UpdateStyleAndLayoutTreeForNode() is also necessary.
  document->UpdateStyleAndLayoutTreeForNode(
      node, DocumentUpdateReason::kAccessibility);
  document->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);

  // Updating style and layout for the node can cause it to gain layout,
  // detaching an AXNodeObject to make room for an AXLayoutObject.
  if (IsDetached()) {
    AXObject* new_object = cache.GetOrCreate(node);
    return new_object
               ? new_object->OnNativeScrollToMakeVisibleWithSubFocusAction(
                     subfocus, horizontal_scroll_alignment,
                     vertical_scroll_alignment)
               : false;
  }

  return OnNativeScrollToMakeVisibleWithSubFocusAction(
      subfocus, horizontal_scroll_alignment, vertical_scroll_alignment);
}

bool AXObject::RequestSetSelectedAction(bool selected) {
  return OnNativeSetSelectedAction(selected);
}

bool AXObject::RequestSetSequentialFocusNavigationStartingPointAction() {
  return OnNativeSetSequentialFocusNavigationStartingPointAction();
}

bool AXObject::RequestSetValueAction(const String& value) {
  return OnNativeSetValueAction(value);
}

bool AXObject::RequestShowContextMenuAction() {
  Event* event =
      Event::CreateCancelable(event_type_names::kAccessiblecontextmenu);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeShowContextMenuAction();
}

bool AXObject::RequestExpandAction() {
  if (ui::SupportsArrowKeysForExpandCollapse(RoleValue())) {
    return OnNativeKeyboardAction(ax::mojom::blink::Action::kExpand);
  }
  return RequestClickAction();
}

bool AXObject::RequestCollapseAction() {
  if (ui::SupportsArrowKeysForExpandCollapse(RoleValue())) {
    return OnNativeKeyboardAction(ax::mojom::blink::Action::kCollapse);
  }
  return RequestClickAction();
}

bool AXObject::OnNativeKeyboardAction(const ax::mojom::Action action) {
  LocalDOMWindow* local_dom_window = GetDocument()->domWindow();

  DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kRawKeyDown,
                        action);
  DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kKeyUp, action);

  return true;
}

bool AXObject::InternalSetAccessibilityFocusAction() {
  return false;
}

bool AXObject::InternalClearAccessibilityFocusAction() {
  return false;
}

LayoutObject* AXObject::GetLayoutObjectForNativeScrollAction() const {
  Node* node = GetNode();
  if (!node || !node->isConnected())
    return nullptr;

  // Node might not have a LayoutObject due to the fact that it is in a locked
  // subtree. Force the update to create the LayoutObject (and update position
  // information) for this node.
  GetDocument()->UpdateStyleAndLayoutForNode(
      node, DocumentUpdateReason::kDisplayLock);
  return node->GetLayoutObject();
}

void AXObject::DispatchKeyboardEvent(LocalDOMWindow* local_dom_window,
                                     WebInputEvent::Type type,
                                     ax::mojom::blink::Action action) const {
  blink::WebKeyboardEvent key(type,
                              blink::WebInputEvent::Modifiers::kNoModifiers,
                              base::TimeTicks::Now());
  switch (action) {
    case ax::mojom::blink::Action::kExpand:
      DCHECK(ui::SupportsArrowKeysForExpandCollapse(RoleValue()));
      key.dom_key = ui::DomKey::ARROW_RIGHT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_RIGHT);
      key.native_key_code = key.windows_key_code = blink::VKEY_RIGHT;
      break;
    case ax::mojom::blink::Action::kCollapse:
      DCHECK(ui::SupportsArrowKeysForExpandCollapse(RoleValue()));
      key.dom_key = ui::DomKey::ARROW_LEFT;
      key.dom_code = static_cast<int>(ui::DomCode::ARROW_LEFT);
      key.native_key_code = key.windows_key_code = blink::VKEY_LEFT;
      break;
    case ax::mojom::blink::Action::kShowContextMenu:
      key.dom_key = ui::DomKey::CONTEXT_MENU;
      key.dom_code = static_cast<int>(ui::DomCode::CONTEXT_MENU);
      key.native_key_code = key.windows_key_code = blink::VKEY_APPS;
      break;
    case ax::mojom::blink::Action::kScrollUp:
      key.dom_key = ui::DomKey::PAGE_UP;
      key.dom_code = static_cast<int>(ui::DomCode::PAGE_UP);
      key.native_key_code = key.windows_key_code = blink::VKEY_PRIOR;
      break;
    case ax::mojom::blink::Action::kScrollDown:
      key.dom_key = ui::DomKey::PAGE_DOWN;
      key.dom_code = static_cast<int>(ui::DomCode::PAGE_DOWN);
      key.native_key_code = key.windows_key_code = blink::VKEY_NEXT;
      break;
    default:
      NOTREACHED();
  }
  GetNode()->DispatchEvent(
      *blink::KeyboardEvent::Create(key, local_dom_window, true));
}

bool AXObject::OnNativeScrollToMakeVisibleAction() const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;
  PhysicalRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  scroll_into_view_util::ScrollRectToVisible(
      *layout_object, target_rect,
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::blink::Event::kLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToMakeVisibleWithSubFocusAction(
    const gfx::Rect& rect,
    blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
    blink::mojom::blink::ScrollAlignment vertical_scroll_alignment) const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;

  PhysicalRect target_rect =
      layout_object->LocalToAbsoluteRect(PhysicalRect(rect));
  scroll_into_view_util::ScrollRectToVisible(
      *layout_object, target_rect,
      ScrollAlignment::CreateScrollIntoViewParams(
          horizontal_scroll_alignment, vertical_scroll_alignment,
          mojom::blink::ScrollType::kProgrammatic,
          false /* make_visible_in_visual_viewport */,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::blink::Event::kLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToGlobalPointAction(
    const gfx::Point& global_point) const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;

  PhysicalRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  target_rect.Move(-PhysicalOffset(global_point));
  scroll_into_view_util::ScrollRectToVisible(
      *layout_object, target_rect,
      ScrollAlignment::CreateScrollIntoViewParams(
          ScrollAlignment::LeftAlways(), ScrollAlignment::TopAlways(),
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::blink::Event::kLocationChanged);
  return true;
}

bool AXObject::OnNativeSetSequentialFocusNavigationStartingPointAction() {
  // Call it on the nearest ancestor that overrides this with a specific
  // implementation.
  if (ParentObject()) {
    return ParentObject()
        ->OnNativeSetSequentialFocusNavigationStartingPointAction();
  }
  return false;
}

bool AXObject::OnNativeDecrementAction() {
  return false;
}

bool AXObject::OnNativeBlurAction() {
  return false;
}

bool AXObject::OnNativeFocusAction() {
  return false;
}

bool AXObject::OnNativeIncrementAction() {
  return false;
}

bool AXObject::OnNativeSetValueAction(const String&) {
  return false;
}

bool AXObject::OnNativeSetSelectedAction(bool) {
  return false;
}

bool AXObject::OnNativeShowContextMenuAction() {
  Element* element = GetElement();
  if (!element)
    element = ParentObject() ? ParentObject()->GetElement() : nullptr;
  if (!element)
    return false;

  Document* document = GetDocument();
  if (!document || !document->GetFrame())
    return false;

  LocalDOMWindow* local_dom_window = GetDocument()->domWindow();
  if (RuntimeEnabledFeatures::
          SynthesizedKeyboardEventsForAccessibilityActionsEnabled()) {
    // To make less evident that the events are synthesized, we have to emit
    // them in this order: 1) keydown. 2) contextmenu. 3) keyup.
    DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kRawKeyDown,
                          ax::mojom::blink::Action::kShowContextMenu);
  }

  ContextMenuAllowedScope scope;
  WebInputEventResult result =
      document->GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
          element, kMenuSourceKeyboard);

  // The node may have ceased to exist due to the event handler actions, so we
  // check its detached state. We also check the result of the contextMenu
  // event: if it was consumed by the system, executing the default action, we
  // don't synthesize the keyup event because it would not be produced normally;
  // the system context menu captures it and never reaches the DOM.
  if (!IsDetached() && result != WebInputEventResult::kHandledSystem &&
      RuntimeEnabledFeatures::
          SynthesizedKeyboardEventsForAccessibilityActionsEnabled()) {
    DispatchKeyboardEvent(local_dom_window, WebInputEvent::Type::kKeyUp,
                          ax::mojom::blink::Action::kShowContextMenu);
  }

  return true;
}

// static
bool AXObject::IsARIAControl(ax::mojom::blink::Role aria_role) {
  return IsARIAInput(aria_role) ||
         aria_role == ax::mojom::blink::Role::kButton ||
         aria_role == ax::mojom::blink::Role::kComboBoxMenuButton ||
         aria_role == ax::mojom::blink::Role::kSlider;
}

// static
bool AXObject::IsARIAInput(ax::mojom::blink::Role aria_role) {
  return aria_role == ax::mojom::blink::Role::kRadioButton ||
         aria_role == ax::mojom::blink::Role::kCheckBox ||
         aria_role == ax::mojom::blink::Role::kTextField ||
         aria_role == ax::mojom::blink::Role::kSwitch ||
         aria_role == ax::mojom::blink::Role::kSearchBox ||
         aria_role == ax::mojom::blink::Role::kTextFieldWithComboBox;
}

// static
bool AXObject::IsFrame(const Node* node) {
  auto* frame_owner = DynamicTo<HTMLFrameOwnerElement>(node);
  if (!frame_owner)
    return false;
  switch (frame_owner->OwnerType()) {
    case FrameOwnerElementType::kIframe:
    case FrameOwnerElementType::kFrame:
    case FrameOwnerElementType::kFencedframe:
      return true;
    case FrameOwnerElementType::kObject:
    case FrameOwnerElementType::kEmbed:
    case FrameOwnerElementType::kPortal:
    case FrameOwnerElementType::kNone:
      return false;
  }
}

// static
bool AXObject::HasARIAOwns(Element* element) {
  if (!element)
    return false;

  // A LayoutObject is not required, because an invisible object can still
  // use aria-owns to point to visible children.

  const AtomicString& aria_owns =
      element->FastGetAttribute(html_names::kAriaOwnsAttr);

  // TODO(accessibility): do we need to check !AriaOwnsElements.empty() ? Is
  // that fundamentally different from HasExplicitlySetAttrAssociatedElements()?
  // And is an element even necessary in the case of virtual nodes?
  return !aria_owns.empty() || element->HasExplicitlySetAttrAssociatedElements(
                                   html_names::kAriaOwnsAttr);
}

ax::mojom::blink::Role AXObject::AriaRoleStringToRoleEnum(const String& value) {
  DCHECK(!value.empty());

  static const ARIARoleMap* role_map = CreateARIARoleMap();

  Vector<String> role_vector;
  value.Split(' ', role_vector);
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kUnknown;
  for (const auto& child : role_vector) {
    auto it = role_map->find(child);
    if (it != role_map->end())
      return it->value;
  }

  return role;
}

bool AXObject::SupportsNameFromContents(bool recursive) const {
  // ARIA 1.1, section 5.2.7.5.
  bool result = false;

  switch (RoleValue()) {
    // ----- NameFrom: contents -------------------------
    // Get their own name from contents, or contribute to ancestors
    case ax::mojom::blink::Role::kButton:
    case ax::mojom::blink::Role::kCell:
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kColumnHeader:
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kDocBackLink:
    case ax::mojom::blink::Role::kDocBiblioRef:
    case ax::mojom::blink::Role::kDocNoteRef:
    case ax::mojom::blink::Role::kDocGlossRef:
    case ax::mojom::blink::Role::kDisclosureTriangle:
    case ax::mojom::blink::Role::kHeading:
    case ax::mojom::blink::Role::kLayoutTableCell:
    case ax::mojom::blink::Role::kLineBreak:
    case ax::mojom::blink::Role::kLink:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kMath:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kMenuListOption:
    case ax::mojom::blink::Role::kPopUpButton:
    case ax::mojom::blink::Role::kPortal:
    case ax::mojom::blink::Role::kRadioButton:
    case ax::mojom::blink::Role::kRowHeader:
    case ax::mojom::blink::Role::kStaticText:
    case ax::mojom::blink::Role::kSwitch:
    case ax::mojom::blink::Role::kTab:
    case ax::mojom::blink::Role::kToggleButton:
    case ax::mojom::blink::Role::kTreeItem:
    case ax::mojom::blink::Role::kTooltip:
      result = true;
      break;

    // ----- No name from contents -------------------------
    // These never have or contribute a name from contents, as they are
    // containers for many subobjects. Superset of nameFrom:author ARIA roles.
    case ax::mojom::blink::Role::kAlert:
    case ax::mojom::blink::Role::kAlertDialog:
    case ax::mojom::blink::Role::kApplication:
    case ax::mojom::blink::Role::kAudio:
    case ax::mojom::blink::Role::kArticle:
    case ax::mojom::blink::Role::kBanner:
    case ax::mojom::blink::Role::kBlockquote:
    case ax::mojom::blink::Role::kColorWell:
    case ax::mojom::blink::Role::kComboBoxMenuButton:  // Only value from
                                                       // content.
    case ax::mojom::blink::Role::kComboBoxGrouping:
    case ax::mojom::blink::Role::kComment:
    case ax::mojom::blink::Role::kComplementary:
    case ax::mojom::blink::Role::kContentInfo:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kDateTime:
    case ax::mojom::blink::Role::kDialog:
    case ax::mojom::blink::Role::kDirectory:
    case ax::mojom::blink::Role::kDocCover:
    case ax::mojom::blink::Role::kDocBiblioEntry:
    case ax::mojom::blink::Role::kDocEndnote:
    case ax::mojom::blink::Role::kDocFootnote:
    case ax::mojom::blink::Role::kDocPageBreak:
    case ax::mojom::blink::Role::kDocPageFooter:
    case ax::mojom::blink::Role::kDocPageHeader:
    case ax::mojom::blink::Role::kDocAbstract:
    case ax::mojom::blink::Role::kDocAcknowledgments:
    case ax::mojom::blink::Role::kDocAfterword:
    case ax::mojom::blink::Role::kDocAppendix:
    case ax::mojom::blink::Role::kDocBibliography:
    case ax::mojom::blink::Role::kDocChapter:
    case ax::mojom::blink::Role::kDocColophon:
    case ax::mojom::blink::Role::kDocConclusion:
    case ax::mojom::blink::Role::kDocCredit:
    case ax::mojom::blink::Role::kDocCredits:
    case ax::mojom::blink::Role::kDocDedication:
    case ax::mojom::blink::Role::kDocEndnotes:
    case ax::mojom::blink::Role::kDocEpigraph:
    case ax::mojom::blink::Role::kDocEpilogue:
    case ax::mojom::blink::Role::kDocErrata:
    case ax::mojom::blink::Role::kDocExample:
    case ax::mojom::blink::Role::kDocForeword:
    case ax::mojom::blink::Role::kDocGlossary:
    case ax::mojom::blink::Role::kDocIndex:
    case ax::mojom::blink::Role::kDocIntroduction:
    case ax::mojom::blink::Role::kDocNotice:
    case ax::mojom::blink::Role::kDocPageList:
    case ax::mojom::blink::Role::kDocPart:
    case ax::mojom::blink::Role::kDocPreface:
    case ax::mojom::blink::Role::kDocPrologue:
    case ax::mojom::blink::Role::kDocPullquote:
    case ax::mojom::blink::Role::kDocQna:
    case ax::mojom::blink::Role::kDocSubtitle:
    case ax::mojom::blink::Role::kDocTip:
    case ax::mojom::blink::Role::kDocToc:
    case ax::mojom::blink::Role::kDocument:
    case ax::mojom::blink::Role::kEmbeddedObject:
    case ax::mojom::blink::Role::kFeed:
    case ax::mojom::blink::Role::kFigure:
    case ax::mojom::blink::Role::kForm:
    case ax::mojom::blink::Role::kGraphicsDocument:
    case ax::mojom::blink::Role::kGraphicsObject:
    case ax::mojom::blink::Role::kGraphicsSymbol:
    case ax::mojom::blink::Role::kGrid:
    case ax::mojom::blink::Role::kGroup:
    case ax::mojom::blink::Role::kHeader:
    case ax::mojom::blink::Role::kIframePresentational:
    case ax::mojom::blink::Role::kIframe:
    case ax::mojom::blink::Role::kImage:
    case ax::mojom::blink::Role::kInputTime:
    case ax::mojom::blink::Role::kListBox:
    case ax::mojom::blink::Role::kLog:
    case ax::mojom::blink::Role::kMain:
    case ax::mojom::blink::Role::kMarquee:
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
    case ax::mojom::blink::Role::kMenu:
    case ax::mojom::blink::Role::kMenuBar:
    case ax::mojom::blink::Role::kMeter:
    case ax::mojom::blink::Role::kNavigation:
    case ax::mojom::blink::Role::kNote:
    case ax::mojom::blink::Role::kPluginObject:
    case ax::mojom::blink::Role::kProgressIndicator:
    case ax::mojom::blink::Role::kRadioGroup:
    case ax::mojom::blink::Role::kRowGroup:
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kScrollView:
    case ax::mojom::blink::Role::kSearch:
    case ax::mojom::blink::Role::kSearchBox:
    case ax::mojom::blink::Role::kSplitter:
    case ax::mojom::blink::Role::kSlider:
    case ax::mojom::blink::Role::kSpinButton:
    case ax::mojom::blink::Role::kStatus:
    case ax::mojom::blink::Role::kSuggestion:
    case ax::mojom::blink::Role::kSvgRoot:
    case ax::mojom::blink::Role::kTable:
    case ax::mojom::blink::Role::kTabList:
    case ax::mojom::blink::Role::kTabPanel:
    case ax::mojom::blink::Role::kTerm:
    case ax::mojom::blink::Role::kTextField:
    case ax::mojom::blink::Role::kTextFieldWithComboBox:
    case ax::mojom::blink::Role::kTimer:
    case ax::mojom::blink::Role::kToolbar:
    case ax::mojom::blink::Role::kTree:
    case ax::mojom::blink::Role::kTreeGrid:
    case ax::mojom::blink::Role::kVideo:
      result = false;
      break;

    // ----- role="row" -------
    // Spec says we should always expose the name on rows,
    // but for performance reasons we only do it
    // if the row is focusable or is the active descendant of a grid/treegrid.
    case ax::mojom::blink::Role::kRow: {
      if (CanSetFocusAttribute())
        return true;
      AXObject* ancestor = ParentObjectUnignored();
      while (ancestor) {
        if (ancestor->GetAOMPropertyOrARIAAttribute(
                AOMRelationProperty::kActiveDescendant)) {
          return true;
        }
        if (ancestor->RoleValue() !=
                ax::mojom::blink::Role::kGenericContainer &&
            ancestor->RoleValue() != ax::mojom::blink::Role::kNone &&
            ancestor->RoleValue() != ax::mojom::blink::Role::kRowGroup) {
          // Not inside a grid or a treegrid, or reached the top of one.
          return false;
        }
        ancestor = ancestor->ParentObjectUnignored();
      }
      return false;
    }

    // ----- Conditional: contribute to ancestor only, unless focusable -------
    // Some objects can contribute their contents to ancestor names, but
    // only have their own name if they are focusable
    case ax::mojom::blink::Role::kGenericContainer:
      // The <body> and <html> element can pass information up to the the root
      // for a portal name.
      if (IsA<HTMLBodyElement>(GetNode()) ||
          GetNode() == GetDocument()->documentElement()) {
        return recursive && GetDocument()->GetPage() &&
               GetDocument()->GetPage()->InsidePortal();
      }
      [[fallthrough]];
    case ax::mojom::blink::Role::kAbbr:
    case ax::mojom::blink::Role::kCanvas:
    case ax::mojom::blink::Role::kCaption:
    case ax::mojom::blink::Role::kCode:
    case ax::mojom::blink::Role::kContentDeletion:
    case ax::mojom::blink::Role::kContentInsertion:
    case ax::mojom::blink::Role::kDefinition:
    case ax::mojom::blink::Role::kDescriptionListDetail:
    case ax::mojom::blink::Role::kDescriptionList:
    case ax::mojom::blink::Role::kDescriptionListTerm:
    case ax::mojom::blink::Role::kDetails:
    case ax::mojom::blink::Role::kEmphasis:
    case ax::mojom::blink::Role::kFigcaption:
    case ax::mojom::blink::Role::kFooter:
    case ax::mojom::blink::Role::kFooterAsNonLandmark:
    case ax::mojom::blink::Role::kHeaderAsNonLandmark:
    case ax::mojom::blink::Role::kInlineTextBox:
    case ax::mojom::blink::Role::kLabelText:
    case ax::mojom::blink::Role::kLayoutTable:
    case ax::mojom::blink::Role::kLayoutTableRow:
    case ax::mojom::blink::Role::kLegend:
    case ax::mojom::blink::Role::kList:
    case ax::mojom::blink::Role::kListItem:
    case ax::mojom::blink::Role::kListMarker:
    case ax::mojom::blink::Role::kMark:
    case ax::mojom::blink::Role::kNone:
    case ax::mojom::blink::Role::kParagraph:
    case ax::mojom::blink::Role::kPre:
    case ax::mojom::blink::Role::kRegion:
    case ax::mojom::blink::Role::kRuby:
    case ax::mojom::blink::Role::kSection:
    case ax::mojom::blink::Role::kStrong:
    case ax::mojom::blink::Role::kSubscript:
    case ax::mojom::blink::Role::kSuperscript:
    case ax::mojom::blink::Role::kTime:
      if (recursive) {
        // Use contents if part of a recursive name computation.
        result = true;
      } else {
        // Use contents if tabbable, so that there is a name in the case
        // where the author mistakenly forgot to provide one.
        // Exceptions:
        // 1.Elements with contenteditable, where using the contents as a name
        //   would cause them to be double-announced.
        // 2.Containers with aria-activedescendant, where the focus is being
        //   forwarded somewhere else.
        // TODO(accessibility) Scrollables are currently allowed here in order
        // to keep the current behavior. In the future, this can be removed
        // because this code will be handled in IsFocusable(), once
        // KeyboardFocusableScrollersEnabled is permanently enabled.
        // Note: this uses the same scrollable check that element.cc uses.
        result = false;
        if (!IsEditable() && !GetAOMPropertyOrARIAAttribute(
                                 AOMRelationProperty::kActiveDescendant)) {
          if (RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
              IsUserScrollable()) {
            return true;
          }
          if (!GetElement() || !GetDocument())
            return false;
          int tab_index = GetElement()->tabIndex();
          bool is_focused = GetElement() == GetDocument()->FocusedElement();
          bool is_in_tab_order_or_focused = tab_index >= 0 || is_focused;
          // Don't repair name from contents to focusable elements unless
          // tabbable or focused, because providing a repaired accessible name
          // often leads to redundant verbalizations.
          return is_in_tab_order_or_focused && CanSetFocusAttribute();
        }
      }
      break;

    case ax::mojom::blink::Role::kRubyAnnotation:
      // Ruby annotations are removed from accessible names and instead used
      // as a description of the parent Role::kRuby object. The benefit is that
      // announcement of the description can be toggled on/off per user choice.
      // In this way, ruby annotations are treated like other annotations, e.g.
      // <mark aria-description="annotation">base text</mark>.
      // In order to achieve the above:
      // * When recursive is true:
      //   Return false, so that the ruby annotation text does not contribute to
      //   the name of the parent Role::kRuby, since it will also be in the
      //   description of that object.
      // * When recursive is false:
      //   Return true, so that text is generated for the object. This text will
      //   be assigned as the description of he parent Role::kRuby object.
      return !recursive;

    // A root web area normally only computes its name from the document title,
    // but a root web area inside a portal's main frame should compute its name
    // from its contents. This name is used by the portal element that hosts
    // this portal.
    case ax::mojom::blink::Role::kRootWebArea: {
      DCHECK(GetNode());
      const Document& document = GetNode()->GetDocument();
      bool is_portal_main_frame =
          document.GetFrame() && document.GetFrame()->IsMainFrame() &&
          !document.GetFrame()->IsFencedFrameRoot() && document.GetPage() &&
          document.GetPage()->InsidePortal();
      return is_portal_main_frame;
    }

    case ax::mojom::blink::Role::kCaret:
    case ax::mojom::blink::Role::kClient:
    case ax::mojom::blink::Role::kColumn:
    case ax::mojom::blink::Role::kDesktop:
    case ax::mojom::blink::Role::kKeyboard:
    case ax::mojom::blink::Role::kImeCandidate:
    case ax::mojom::blink::Role::kListGrid:
    case ax::mojom::blink::Role::kPane:
    case ax::mojom::blink::Role::kPdfActionableHighlight:
    case ax::mojom::blink::Role::kPdfRoot:
    case ax::mojom::blink::Role::kTableHeaderContainer:
    case ax::mojom::blink::Role::kTitleBar:
    case ax::mojom::blink::Role::kUnknown:
    case ax::mojom::blink::Role::kWebView:
    case ax::mojom::blink::Role::kWindow:
      NOTREACHED() << "Role shouldn't occur in Blink: " << ToString(true, true);
      break;
  }

  return result;
}

bool AXObject::SupportsARIAReadOnly() const {
  if (ui::IsReadOnlySupported(RoleValue()))
    return true;

  if (ui::IsCellOrTableHeader(RoleValue())) {
    // For cells and row/column headers, readonly is supported within a grid.
    AncestorsIterator ancestor = base::ranges::find_if(
        UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
        &AXObject::IsTableLikeRole);
    return ancestor.current_ &&
           (ancestor.current_->RoleValue() == ax::mojom::blink::Role::kGrid ||
            ancestor.current_->RoleValue() ==
                ax::mojom::blink::Role::kTreeGrid);
  }

  return false;
}

ax::mojom::blink::Role AXObject::ButtonRoleType() const {
  // If aria-pressed is present, then it should be exposed as a toggle button.
  // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
  if (AriaPressedIsPresent())
    return ax::mojom::blink::Role::kToggleButton;

  // If aria-haspopup is present and is not "dialog", expose as a popup button,
  // which is exposed in MSAA/IA2 with a role of button menu. Note that this is
  // not done for dialog because screen readers use the button menu role as a
  // tip to turn off the virtual buffer mode.
  // Here is the GitHub issue -- ARIA WG is working to update the spec to match.
  if (HasPopup() != ax::mojom::blink::HasPopup::kFalse &&
      HasPopup() != ax::mojom::blink::HasPopup::kDialog) {
    return ax::mojom::blink::Role::kPopUpButton;
  }

  return ax::mojom::blink::Role::kButton;
}

// static
const AtomicString& AXObject::ARIARoleName(ax::mojom::blink::Role role) {
  static const Vector<AtomicString>* aria_role_name_vector =
      CreateARIARoleNameVector();

  return aria_role_name_vector->at(static_cast<wtf_size_t>(role));
}

const String AXObject::InternalRoleName(ax::mojom::blink::Role role) {
  std::ostringstream role_name;
  role_name << role;
  // Convert from std::ostringstream to std::string, while removing "k" prefix.
  // For example, kStaticText becomes StaticText.
  // Many conversions, but this isn't used in performance-sensitive code.
  std::string role_name_std = role_name.str().substr(1, std::string::npos);
  String role_name_wtf_string = role_name_std.c_str();
  return role_name_wtf_string;
}

// static
const String AXObject::RoleName(ax::mojom::blink::Role role,
                                bool* is_internal) {
  if (is_internal)
    *is_internal = false;
  if (const auto& role_name = ARIARoleName(role))
    return role_name.GetString();

  if (is_internal)
    *is_internal = true;

  return InternalRoleName(role);
}

// static
const AXObject* AXObject::LowestCommonAncestor(const AXObject& first,
                                               const AXObject& second,
                                               int* index_in_ancestor1,
                                               int* index_in_ancestor2) {
  *index_in_ancestor1 = -1;
  *index_in_ancestor2 = -1;

  if (first.IsDetached() || second.IsDetached())
    return nullptr;

  if (first == second)
    return &first;

  HeapVector<Member<const AXObject>> ancestors1;
  ancestors1.push_back(&first);
  while (ancestors1.back())
    ancestors1.push_back(ancestors1.back()->ParentObjectIncludedInTree());

  HeapVector<Member<const AXObject>> ancestors2;
  ancestors2.push_back(&second);
  while (ancestors2.back())
    ancestors2.push_back(ancestors2.back()->ParentObjectIncludedInTree());

  const AXObject* common_ancestor = nullptr;
  while (!ancestors1.empty() && !ancestors2.empty() &&
         ancestors1.back() == ancestors2.back()) {
    common_ancestor = ancestors1.back();
    ancestors1.pop_back();
    ancestors2.pop_back();
  }

  if (common_ancestor) {
    if (!ancestors1.empty())
      *index_in_ancestor1 = ancestors1.back()->IndexInParent();
    if (!ancestors2.empty())
      *index_in_ancestor2 = ancestors2.back()->IndexInParent();
  }

  return common_ancestor;
}

void AXObject::PreSerializationConsistencyCheck() {
#if defined(AX_FAIL_FAST_BUILD)
  DCHECK(!IsDetached()) << "Do not serialize detached nodes: "
                        << ToString(true, true);
  DCHECK(AccessibilityIsIncludedInTree())
      << "Do not serialize unincluded nodes: " << ToString(true, true);
  SANITIZER_CHECK(!IsDetached());
  // Extra checks that only occur during serialization.
  SANITIZER_CHECK_EQ(IsAriaHidden(), !!FindAncestorWithAriaHidden(this))
      << "IsAriaHidden() doesn't match existence of an aria-hidden ancestor: "
      << ToString(true);
#endif
}

String AXObject::ToString(bool verbose, bool cached_values_only) const {
  // Build a friendly name for debugging the object.
  // If verbose, build a longer name name in the form of:
  // CheckBox axid#28 <input.someClass#cbox1> name="checkbox"
#if !defined(NDEBUG)
  if (IsDetached() && verbose) {
    return "(detached) " + detached_object_debug_info_;
  }
#endif

  String string_builder = InternalRoleName(RoleValue()).EncodeForDebugging();

  if (IsDetached()) {
    return string_builder + " (detached)";
  }

  if (AXObjectCache().HasBeenDisposed()) {
    return string_builder + " (doc shutdown) #" + String::Number(AXObjectID());
  }

  if (verbose) {
    string_builder = string_builder + " axid#" + String::Number(AXObjectID());
    // Add useful HTML element info, like <div.myClass#myId>.
    if (GetNode()) {
      string_builder = string_builder + " " + GetNodeString(GetNode());
      if (IsA<Document>(GetNode())) {
        if (IsRoot())
          string_builder = string_builder + " isRoot";
        if (GetDocument()->GetFrame() &&
            GetDocument()->GetFrame()->PagePopupOwner()) {
          string_builder = string_builder + " isPopup";
        }
      }
    }

    if (!GetDocument())
      string_builder = string_builder + " missingDocument";

    // Add properties of interest that often contribute to errors:
    if (HasARIAOwns(GetElement())) {
      string_builder =
          string_builder + " aria-owns=" +
          GetElement()->FastGetAttribute(html_names::kAriaOwnsAttr);
    }

    if (GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant)) {
      string_builder =
          string_builder + " aria-activedescendant=" +
          GetElement()->FastGetAttribute(html_names::kAriaOwnsAttr);
    }
    if (IsFocused())
      string_builder = string_builder + " focused";
    if (!IsDetached() && AXObjectCache().IsAriaOwned(this))
      string_builder = string_builder + " isAriaOwned";
    if (cached_values_only ? LastKnownIsIgnoredValue()
                           : AccessibilityIsIgnored()) {
      string_builder = string_builder + " isIgnored";
#if defined(AX_FAIL_FAST_BUILD)
      // TODO(accessibility) Move this out of AX_FAIL_FAST_BUILD by having a new
      // ax_enum, and a ToString() in ax_enum_utils, as well as move out of
      // String IgnoredReasonName(AXIgnoredReason reason) in
      // inspector_type_builder_helper.cc.
      if (!cached_values_only && !IsDetached()) {
        AXObject::IgnoredReasons reasons;
        ComputeAccessibilityIsIgnored(&reasons);
        string_builder = string_builder + GetIgnoredReasonsDebugString(reasons);
      }
#endif
      if (cached_values_only ? !LastKnownIsIncludedInTreeValue()
                             : !AccessibilityIsIncludedInTree())
        string_builder = string_builder + " isRemovedFromTree";
    }
    if (GetNode() && GetDocument()->Lifecycle().GetState() >=
                         DocumentLifecycle::kLayoutClean) {
      if (GetNode()->OwnerShadowHost()) {
        string_builder = string_builder + (GetNode()->IsInUserAgentShadowRoot()
                                               ? " inUserAgentShadowRoot:"
                                               : " inShadowRoot:");
        string_builder = string_builder + "<" +
                         GetNode()->OwnerShadowHost()->tagName().LowerASCII() +
                         ">";
      }
      if (GetNode()->GetShadowRoot()) {
        string_builder = string_builder + " hasShadowRoot";
      }
      if (DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
              *GetNode(), DisplayLockActivationReason::kAccessibility)) {
        string_builder = string_builder + " isDisplayLocked";
      }
    }
    if (cached_values_only) {
      if (cached_is_aria_hidden_)
        string_builder = string_builder + " ariaHidden";
    } else if (IsAriaHidden()) {
      const AXObject* aria_hidden_root = AriaHiddenRoot();
      if (aria_hidden_root) {
        string_builder = string_builder + " ariaHiddenRoot";
        if (aria_hidden_root != this) {
          string_builder =
              string_builder + GetNodeString(aria_hidden_root->GetNode());
        }
      } else {
        string_builder = string_builder + " ariaHiddenRootMissing";
      }
    } else if (AriaHiddenRoot()) {
      string_builder = string_builder + " ariaHiddenRootExtra";
    }
    if (cached_values_only ? cached_is_hidden_via_style_ : IsHiddenViaStyle()) {
      string_builder = string_builder + " isHiddenViaCSS";
    }
    if (cached_values_only ? cached_is_inert_ : IsInert())
      string_builder = string_builder + " isInert";
    if (IsMissingParent())
      string_builder = string_builder + " isMissingParent";
    if (children_dirty_) {
      string_builder = string_builder + " needsToUpdateChildren";
    } else if (!children_.empty()) {
      string_builder = string_builder + " #children=";
      string_builder = string_builder + String::Number(children_.size());
    }
    if (!GetLayoutObject())
      string_builder = string_builder + " missingLayout";

    if (!cached_values_only)
      string_builder = string_builder + " name=";
  } else {
    string_builder = string_builder + ": ";
  }

  // Append name last, in case it is long.
  if (!cached_values_only)
    string_builder = string_builder + ComputedName().EncodeForDebugging();

  return string_builder;
}

bool operator==(const AXObject& first, const AXObject& second) {
  if (first.IsDetached() || second.IsDetached())
    return false;
  if (&first == &second) {
    DCHECK_EQ(first.AXObjectID(), second.AXObjectID());
    return true;
  }
  return false;
}

bool operator!=(const AXObject& first, const AXObject& second) {
  return !(first == second);
}

bool operator<(const AXObject& first, const AXObject& second) {
  if (first.IsDetached() || second.IsDetached())
    return false;

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor = AXObject::LowestCommonAncestor(
      first, second, &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  return index_in_ancestor1 < index_in_ancestor2;
}

bool operator<=(const AXObject& first, const AXObject& second) {
  return first == second || first < second;
}

bool operator>(const AXObject& first, const AXObject& second) {
  if (first.IsDetached() || second.IsDetached())
    return false;

  int index_in_ancestor1, index_in_ancestor2;
  const AXObject* ancestor = AXObject::LowestCommonAncestor(
      first, second, &index_in_ancestor1, &index_in_ancestor2);
  DCHECK_GE(index_in_ancestor1, -1);
  DCHECK_GE(index_in_ancestor2, -1);
  if (!ancestor)
    return false;
  return index_in_ancestor1 > index_in_ancestor2;
}

bool operator>=(const AXObject& first, const AXObject& second) {
  return first == second || first > second;
}

std::ostream& operator<<(std::ostream& stream, const AXObject* obj) {
  if (obj)
    return stream << obj->ToString(true).Utf8();
  else
    return stream << "<AXObject nullptr>";
}

std::ostream& operator<<(std::ostream& stream, const AXObject& obj) {
  return stream << obj.ToString(true).Utf8();
}

void AXObject::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(cached_live_region_root_);
  visitor->Trace(ax_object_cache_);
}

}  // namespace blink
