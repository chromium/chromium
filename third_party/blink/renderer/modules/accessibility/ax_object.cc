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
#include "third_party/blink/renderer/core/accessibility/ax_object_cache.h"
#include "third_party/blink/renderer/core/accessibility/axid.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/element.h"
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
#include "third_party/blink/renderer/core/html/forms/html_button_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_label_element.h"
#include "third_party/blink/renderer/core/html/forms/html_opt_group_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/forms/html_text_area_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_element.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_element.h"
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_hr_element.h"
#include "third_party/blink/renderer/core/html/html_html_element.h"
#include "third_party/blink/renderer/core/html/html_image_element.h"
#include "third_party/blink/renderer/core/html/html_map_element.h"
#include "third_party/blink/renderer/core/html/html_no_script_element.h"
#include "third_party/blink/renderer/core/html/html_progress_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/html_span_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
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
#include "third_party/blink/renderer/core/svg/svg_desc_element.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/core/svg/svg_title_element.h"
#include "third_party/blink/renderer/modules/accessibility/aria_notification.h"
#include "third_party/blink/renderer/modules/accessibility/ax_enums.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#if DCHECK_IS_ON()
#include "third_party/blink/renderer/modules/accessibility/ax_debug_utils.h"
#endif
#include "third_party/blink/renderer/bindings/core/v8/v8_highlight_type.h"
#include "third_party/blink/renderer/modules/accessibility/ax_image_map_link.h"
#include "third_party/blink/renderer/modules/accessibility/ax_inline_text_box.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/keyboard_codes.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_enums.mojom-blink-forward.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/accessibility/ax_tree_source.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/gfx/geometry/transform.h"

namespace blink {

using mojom::blink::FormControlType;

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
    case kAXHiddenByChildTree:
      return "hiddenByChildTree";
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
  NOTREACHED_IN_MIGRATION();
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
    case ax::mojom::blink::Role::kPreDeprecated:
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

constexpr wtf_size_t kNumRoles =
    static_cast<wtf_size_t>(ax::mojom::blink::Role::kMaxValue) + 1;

using ARIARoleMap =
    HashMap<String, ax::mojom::blink::Role, CaseFoldingHashTraits<String>>;

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
    {"directory", ax::mojom::blink::Role::kList},
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
    {"gridcell", ax::mojom::blink::Role::kGridCell},
    {"group", ax::mojom::blink::Role::kGroup},
    {"heading", ax::mojom::blink::Role::kHeading},
    {"img", ax::mojom::blink::Role::kImage},
    // role="image" is listed after role="img" to treat the synonym img
    // as a computed name image
    {"image", ax::mojom::blink::Role::kImage},
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
    {"sectionfooter", ax::mojom::blink::Role::kSectionFooter},
    {"sectionheader", ax::mojom::blink::Role::kSectionHeader},
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

// More friendly names for debugging, and for WPT tests.
// These are roles which map from the ARIA role name to the internal role when
// building the tree, but in DevTools or testing, we want to show the ARIA
// role name, since that is the publicly visible concept.
const RoleEntry kReverseRoles[] = {
    {"banner", ax::mojom::blink::Role::kHeader},
    {"button", ax::mojom::blink::Role::kToggleButton},
    {"button", ax::mojom::blink::Role::kPopUpButton},
    {"contentinfo", ax::mojom::blink::Role::kFooter},
    {"option", ax::mojom::blink::Role::kMenuListOption},
    {"option", ax::mojom::blink::Role::kListBoxOption},
    {"group", ax::mojom::blink::Role::kDetails},
    {"generic", ax::mojom::blink::Role::kSectionWithoutName},
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
static Vector<AtomicString>* CreateAriaRoleNameVector() {
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
    if (!obj->IsIgnored()) {
      ids.push_back(obj->AXObjectID());
    }
  }
  if (!ids.empty())
    node_data->AddIntListAttribute(attr, ids);
}

// Max length for attributes such as aria-label.
static constexpr uint32_t kMaxStringAttributeLength = 10000;
// Max length for a static text name.
// Length of War and Peace (http://www.gutenberg.org/files/2600/2600-0.txt).
static constexpr uint32_t kMaxStaticTextLength = 3227574;

std::string TruncateString(const String& str,
                           uint32_t max_len = kMaxStringAttributeLength) {
  auto str_utf8 = str.Utf8(kStrictUTF8Conversion);
  if (str_utf8.size() > max_len) {
    std::string truncated;
    base::TruncateUTF8ToByteSize(str_utf8, max_len, &truncated);
    return truncated;
  }
  return str_utf8;
}

bool TruncateAndAddStringAttribute(
    ui::AXNodeData* dst,
    ax::mojom::blink::StringAttribute attribute,
    const String& value,
    uint32_t max_len = kMaxStringAttributeLength) {
  if (!value.empty()) {
    std::string value_utf8 = TruncateString(value, max_len);
    if (!value_utf8.empty()) {
      dst->AddStringAttribute(attribute, value_utf8);
      return true;
    }
  }
  return false;
}

void AddIntAttribute(const AXObject* obj,
                     ax::mojom::blink::IntAttribute node_data_attr,
                     const QualifiedName& attr_name,
                     ui::AXNodeData* node_data,
                     int min_value = INT_MIN) {
  const AtomicString& value = obj->AriaAttribute(attr_name);
  if (!value.empty()) {
    int value_as_int = value.ToInt();
    if (value_as_int >= min_value) {
      node_data->AddIntAttribute(node_data_attr, value_as_int);
    }
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

const QualifiedName& DeprecatedAriaColtextAttrName() {
  DEFINE_STATIC_LOCAL(QualifiedName, aria_coltext_attr,
                      (AtomicString("aria-coltext")));
  return aria_coltext_attr;
}

const QualifiedName& DeprecatedAriaRowtextAttrName() {
  DEFINE_STATIC_LOCAL(QualifiedName, aria_rowtext_attr,
                      (AtomicString("aria-rowtext")));
  return aria_rowtext_attr;
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
const QualifiedName& MathMLAttrName() {
  DEFINE_STATIC_LOCAL(QualifiedName, mathml_attr,
                      (AtomicString("data-mathml")));
  return mathml_attr;
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)

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

int32_t ToAXHighlightType(const V8HighlightType& highlight_type) {
  switch (highlight_type.AsEnum()) {
    case V8HighlightType::Enum::kHighlight:
      return static_cast<int32_t>(ax::mojom::blink::HighlightType::kHighlight);
    case V8HighlightType::Enum::kSpellingError:
      return static_cast<int32_t>(
          ax::mojom::blink::HighlightType::kSpellingError);
    case V8HighlightType::Enum::kGrammarError:
      return static_cast<int32_t>(
          ax::mojom::blink::HighlightType::kGrammarError);
  }
  NOTREACHED();
}

const AXObject* FindAncestorWithAriaHidden(const AXObject* start) {
  for (const AXObject* object = start;
       object && !IsA<Document>(object->GetNode());
       object = object->ParentObject()) {
    if (object->IsAriaHiddenRoot()) {
      return object;
    }
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
      cached_live_region_root_(nullptr),
      ax_object_cache_(&ax_object_cache) {
  ++number_of_live_ax_objects_;
}

AXObject::~AXObject() {
  DCHECK(IsDetached());
  --number_of_live_ax_objects_;
}

void AXObject::SetHasDirtyDescendants(bool dirty) {
  CHECK(!dirty || CachedIsIncludedInTree())
      << "Only included nodes can be marked as having dirty descendants: "
      << this;
  has_dirty_descendants_ = dirty;
}

void AXObject::SetAncestorsHaveDirtyDescendants() {
  CHECK(!IsDetached());
  CHECK(AXObjectCache()
            .lifecycle()
            .StateAllowsAXObjectsToGainFinalizationNeededBit())
      << AXObjectCache();

  // Set the dirty bit for the root AX object when created. For all other
  // objects, this is set by a descendant needing to be updated, and
  // AXObjectCacheImpl::UpdateTreeIfNeeded will therefore process an object
  // if its parent has has_dirty_descendants_ set. The root, however, has no
  // parent, so there is no parent to mark in order to cause the root to update
  // itself. Therefore this bit serves a second purpose of determining
  // whether AXObjectCacheImpl::UpdateTreeIfNeeded needs to update the root
  // object.
  if (IsRoot()) {
    // Need at least the root object to be flagged in order for
    // UpdateTreeIfNeeded() to do anything.
    SetHasDirtyDescendants(true);
    return;
  }

  if (AXObjectCache().EntireDocumentIsDirty()) {
    // No need to walk parent chain when marking the entire document dirty,
    // as every node will have the bit set. In addition, attempting to repair
    // the parent chain while marking everything dirty is actually against
    // the point, because all child-parent relationships will be rebuilt
    // from the top down.
    if (CachedIsIncludedInTree()) {
      SetHasDirtyDescendants(true);
    }
    return;
  }

  AXObject* ancestor = this;

  while (true) {
    ancestor = ancestor->ParentObject();
    if (!ancestor) {
      break;
    }
    DCHECK(!ancestor->IsDetached());

    // We need to to continue setting bits through AX objects for which
    // IsIncludedInTree is false, since those objects are omitted
    // from the generated tree. However, don't set the bit on unincluded
    // objects, during the clearing phase in
    // AXObjectCacheImpl::UpdateTreeIfNeeded(), only included nodes are
    // visited.
    if (!ancestor->CachedIsIncludedInTree()) {
      continue;
    }
    if (ancestor->has_dirty_descendants_) {
      break;
    }
    ancestor->SetHasDirtyDescendants(true);
  }
#if DCHECK_IS_ON()
  // Walk up the tree looking for dirty bits that failed to be set. If any
  // are found, this is a bug.
  bool fail = false;
  for (auto* obj = ParentObject(); obj; obj = obj->ParentObject()) {
    if (obj->CachedIsIncludedInTree() && !obj->has_dirty_descendants_) {
      fail = true;
      break;
    }
  }
  DCHECK(!fail) << "Failed to set dirty bits on some ancestors:\n"
                << ParentChainToStringHelper(this);
#endif
}

void AXObject::Init(AXObject* parent) {
  CHECK(!parent_) << "Should not already have a cached parent:"
                  << "\n* Child = " << GetNode() << " / " << GetLayoutObject()
                  << "\n* Parent = " << parent_
                  << "\n* Equal to passed-in parent? " << (parent == parent_);
  // Every AXObject must have a parent unless it's the root.
  CHECK(parent || IsRoot())
      << "The following node should have a parent: " << GetNode();
  CHECK(!AXObjectCache().IsFrozen());
#if DCHECK_IS_ON()
  CHECK(!is_initializing_);
  base::AutoReset<bool> reentrancy_protector(&is_initializing_, true);
#endif  // DCHECK_IS_ON()

  // Set the parent as soon as possible, so that we can use it in computations
  // for the role and cached value. We will set it again at the end of the
  // method using SetParent(), to ensure all of the normal code paths for
  // setting the parent are followed.
  parent_ = parent;

  // The role must be determined immediately.
  // Note: in order to avoid reentrancy, the role computation cannot use the
  // ParentObject(), although it can use the DOM parent.
  role_ = DetermineRoleValue();
#if DCHECK_IS_ON()
  DCHECK(IsValidRole(role_)) << "Illegal " << role_ << " for\n"
                             << GetNode() << '\n'
                             << GetLayoutObject();
  // The parent cannot have children. This object must be destroyed.
  DCHECK(!parent_ || parent_->CanHaveChildren())
      << "Tried to set a parent that cannot have children:" << "\n* Parent = "
      << parent_ << "\n* Child = " << this;
#endif

  children_dirty_ = true;

  UpdateCachedAttributeValuesIfNeeded(false);

  DCHECK(GetDocument()) << "All AXObjects must have a document: " << this;

  // Set the parent again, this time via SetParent(), so that all related checks
  // and calls occur now that we have the role and updated cached values.
  SetParent(parent_);
}

void AXObject::Detach() {
#if DCHECK_IS_ON()
  DCHECK(!is_updating_cached_values_)
      << "Don't detach in the middle of updating cached values: " << this;
  DCHECK(!IsDetached());
#endif
  // Prevents LastKnown*() methods from returning the wrong values.
  cached_is_ignored_ = true;
  cached_is_ignored_but_included_in_tree_ = false;

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(ax_object_cache_);
  SANITIZER_CHECK(!ax_object_cache_->IsFrozen())
      << "Do not detach children while the tree is frozen, in order to avoid "
         "an object detaching itself in the middle of computing its own "
         "accessibility properties.";
  SANITIZER_CHECK(!is_adding_children_) << this;
#endif

#if !defined(NDEBUG)
  // Facilitates debugging of detached objects by providing info on what it was.
  if (!ax_object_cache_->HasBeenDisposed()) {
    detached_object_debug_info_ = ToString();
  }
#endif

  if (AXObjectCache().HasBeenDisposed()) {
    // Shutting down a11y, just clear the children.
    children_.clear();
  } else {
    // Clear children and call DetachFromParent() on them so that
    // no children are left with dangling pointers to their parent.
    ClearChildren();
  }

  parent_ = nullptr;
  ax_object_cache_ = nullptr;
  children_dirty_ = false;
  child_cached_values_need_update_ = false;
  cached_values_need_update_ = false;
  has_dirty_descendants_ = false;
  id_ = 0;
}

bool AXObject::IsDetached() const {
  return !ax_object_cache_;
}

bool AXObject::IsRoot() const {
  return GetNode() && GetNode() == &AXObjectCache().GetDocument();
}

void AXObject::SetParent(AXObject* new_parent) {
  CHECK(!AXObjectCache().IsFrozen());
#if DCHECK_IS_ON()
  if (!new_parent && !IsRoot()) {
    std::ostringstream message;
    message << "Parent cannot be null, except at the root."
            << "\nThis: " << this
            << "\nDOM parent chain , starting at |this->GetNode()|:";
    int count = 0;
    for (Node* node = GetNode(); node;
         node = GetParentNodeForComputeParent(AXObjectCache(), node)) {
      message << "\n"
              << (++count) << ". " << node
              << "\n  LayoutObject=" << node->GetLayoutObject();
      if (AXObject* obj = AXObjectCache().Get(node))
        message << "\n  " << obj;
      if (!node->isConnected()) {
        break;
      }
    }
    NOTREACHED_IN_MIGRATION() << message.str();
  }

  if (new_parent) {
    DCHECK(!new_parent->IsDetached())
        << "Cannot set parent to a detached object:" << "\n* Child: " << this
        << "\n* New parent: " << new_parent;

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
      DUMP_WILL_BE_CHECK(child != this)
          << "Previous parent still has |this| child:\n"
          << this << " should be a child of " << new_parent << " not of "
          << parent_;
    }
    // TODO(accessibility) This should not be reached unless this method is
    // called on an AXObject of role kRootWebArea or when the parent's
    // children are dirty, aka parent_->NeedsToUpdateChildren());
    // Ideally we will also ensure |this| is in the parent's children now, so
    // that ClearChildren() can later find the child to detach from the parent.
  }

#endif
  parent_ = new_parent;
  if (AXObjectCache().IsUpdatingTree()) {
    // If updating tree, tell the newly included parent to iterate through
    // all of its children to look for the has dirty descendants flag.
    // However, we do not set the flag on higher ancestors since
    // they have already been walked by the tree update loop.
    if (AXObject* ax_included_parent = ParentObjectIncludedInTree()) {
      ax_included_parent->SetHasDirtyDescendants(true);
    }
  } else {
    SetAncestorsHaveDirtyDescendants();
  }
}

bool AXObject::IsMissingParent() const {
  if (!parent_) {
    // Do not attempt to repair the ParentObject() of a validation message
    // object, because hidden ones are purposely kept around without being in
    // the tree, and without a parent, for potential later reuse.
    bool is_missing = !IsRoot();
    DUMP_WILL_BE_CHECK(!is_missing || !AXObjectCache().IsFrozen())
        << "Should not have missing parent in frozen tree: " << this;
    return is_missing;
  }

  if (parent_->IsDetached()) {
    CHECK(!AXObjectCache().IsFrozen())
        << "Should not have detached parent in frozen tree: " << this;

    return true;
  }

  return false;
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
      << "Computed parent should never be detached:" << "\n* Child: " << this
      << "\n* Parent: " << ax_parent;

  return ax_parent;
}

// Same as ComputeParent, but without the extra check for valid parent in the
// end. This is for use in RestoreParentOrPrune.
AXObject* AXObject::ComputeParentOrNull() const {
  CHECK(!IsDetached());

  CHECK(GetNode() || GetLayoutObject())
      << "Can't compute parent on AXObjects without a backing Node or "
         "LayoutObject. Objects without those must set the "
         "parent in Init(), |this| = "
      << RoleValue();

  AXObject* ax_parent = nullptr;
  if (IsAXInlineTextBox()) {
    NOTREACHED_IN_MIGRATION()
        << "AXInlineTextBox box tried to compute a new parent, but they are "
           "not allowed to exist even temporarily without a parent, as their "
           "existence depends on the parent text object. Parent text = "
        << AXObjectCache().Get(GetNode());
  } else if (AXObjectCache().IsAriaOwned(this)) {
    ax_parent = AXObjectCache().ValidatedAriaOwner(this);
  }
  if (!ax_parent) {
    ax_parent = ComputeNonARIAParent(AXObjectCache(), GetNode());
  }

  return ax_parent;
}

// static
Node* AXObject::GetParentNodeForComputeParent(AXObjectCacheImpl& cache,
                                              Node* node) {
  if (!node || !node->isConnected()) {
    return nullptr;
  }

  // A document's parent should be the page popup owner, if any, otherwise null.
  if (auto* document = DynamicTo<Document>(node)) {
    LocalFrame* frame = document->GetFrame();
    DCHECK(frame);
    return frame->PagePopupOwner();
  }

  // Avoid a CHECK that disallows calling LayoutTreeBuilderTraversal::Parent() with a shadow root node.
  if (node->IsShadowRoot()) {
    return node->OwnerShadowHost();
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

  // The parent of a customizable select's popup is the select.
  if (IsA<HTMLDataListElement>(node)) {
    if (auto* select = DynamicTo<HTMLSelectElement>(node->OwnerShadowHost())) {
      if (node == select->PopoverForAppearanceBase()) {
        return select;
      }
    }
  }

  // For the content of a customizable select, the parent must be the element
  // assigned the role of kMenuListPopup. To accomplish this, it is necessary to
  // adapt to unusual DOM structure. If no parent, or the parent has a <select>
  // shadow host, then the actual parent should be the <select>.
  // TODO(aleventhal, jarhar): try to simplify this code. @jarhar wrote in code
  // review: "I don't think that a UA <slot> will ever get returned by
  // LayoutTreeBuilderTraversal::Parent. In this case, I think
  // LayoutTreeBuilderTraversal::Parent should just return the <select>."

  HTMLSelectElement* owner_select = nullptr;
  if (IsA<HTMLSlotElement>(parent) && parent->IsInUserAgentShadowRoot()) {
    owner_select = DynamicTo<HTMLSelectElement>(parent->OwnerShadowHost());
  } else if (!parent) {
    owner_select = DynamicTo<HTMLSelectElement>(NodeTraversal::Parent(*node));
  }
  if (owner_select && owner_select->IsAppearanceBasePicker()) {
    // Return the popup's <datalist> element.
    return owner_select->PopoverForAppearanceBase();
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

  if (IsA<HTMLImageElement>(element)) {
    return GetMapForImage(&element);
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
    return !input->IsCheckable() &&
           input->FormControlType() != FormControlType::kInputRange;
  }

  // For consistency with the past, options with a single text child are leaves.
  // However, options can now sometimes have interesting children, for
  // a <select> menulist that uses appearance:base-select.
  if (auto* option = DynamicTo<HTMLOptionElement>(element)) {
    return option->OwnerSelectElement() &&
           option->OwnerSelectElement()->IsAppearanceBasePicker() &&
           !option->HasOneTextChild();
  }

  if (IsA<HTMLProgressElement>(element)) {
    return false;
  }

  return true;
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
  Node* parent_node = GetParentNodeForComputeParent(cache, current_node);
  return cache.Get(parent_node);
}

#if DCHECK_IS_ON()
std::string AXObject::GetAXTreeForThis() const {
  return TreeToStringWithMarkedObjectHelper(AXObjectCache().Root(), this);
}

void AXObject::ShowAXTreeForThis() const {
  DLOG(INFO) << "\n" << GetAXTreeForThis();
}

#endif

// static
bool AXObject::HasAriaAttribute(Element& element,
                                const QualifiedName& attribute) {
  return element.FastHasAttribute(attribute) ||
         (element.DidAttachInternals() &&
          element.EnsureElementInternals().HasAttribute(attribute));
}

bool AXObject::HasAriaAttribute(const QualifiedName& attribute) const {
  Element* element = GetElement();
  if (!element) {
    return false;
  }
  return HasAriaAttribute(*element, attribute);
}

// static
const AtomicString& AXObject::AriaAttribute(Element& element,
                                            const QualifiedName& attribute) {
  const AtomicString& value = element.FastGetAttribute(attribute);
  if (!value.IsNull()) {
    return value;
  }
  return GetInternalsAttribute(element, attribute);
}

const AtomicString& AXObject::AriaAttribute(
    const QualifiedName& attribute) const {
  return GetElement() ? AriaAttribute(*GetElement(), attribute) : g_null_atom;
}

// static
bool AXObject::IsAriaAttributeTrue(Element& element,
                                   const QualifiedName& attribute) {
  const AtomicString& value = AriaAttribute(element, attribute);
  return !value.empty() && !EqualIgnoringASCIICase(value, "undefined") &&
         !EqualIgnoringASCIICase(value, "false");
}

// ARIA attributes are true if they are not empty, "false" or "undefined".
bool AXObject::IsAriaAttributeTrue(const QualifiedName& attribute) const {
  return GetElement() ? IsAriaAttributeTrue(*GetElement(), attribute) : false;
}

bool AXObject::AriaBooleanAttribute(const QualifiedName& attribute,
                                    bool* out_value) const {
  const AtomicString& value = AriaAttribute(attribute);
  if (value == g_null_atom || value.empty() ||
      EqualIgnoringASCIICase(value, "undefined")) {
    if (out_value) {
      *out_value = false;
    }
    return false;
  }
  if (out_value) {
    *out_value = !EqualIgnoringASCIICase(value, "false");
  }
  return true;
}

bool AXObject::AriaIntAttribute(const QualifiedName& attribute,
                                int32_t* out_value) const {
  const AtomicString& value = AriaAttribute(attribute);
  if (value == g_null_atom || value.empty()) {
    if (out_value) {
      *out_value = 0;
    }
    return false;
  }

  int int_value = value.ToInt();
  int value_if_less_than_1 = 1;

  if (attribute == html_names::kAriaSetsizeAttr) {
    // -1 is a special "indeterminate" value for aria-setsize.
    // However, any value that's not a positive number should be given the
    // intederminate treatment.
    value_if_less_than_1 = -1;
  } else if (attribute == html_names::kAriaPosinsetAttr ||
             attribute == html_names::kAriaLevelAttr) {
    value_if_less_than_1 = 1;
  } else {
    // For now, try to get the illegal attribute, but catch the error.
    NOTREACHED(base::NotFatalUntil::M133) << "Not an int attribute.";
  }

  if (out_value) {
    *out_value = int_value < 1 ? value_if_less_than_1 : int_value;
  }

  return true;
}

bool AXObject::AriaFloatAttribute(const QualifiedName& attribute,
                                  float* out_value) const {
  const AtomicString& value = AriaAttribute(attribute);
  if (value == g_null_atom) {
    if (out_value) {
      *out_value = 0.0;
    }
    return false;
  }

  if (out_value) {
    *out_value = value.ToFloat();
  }
  return true;
}

const AtomicString& AXObject::AriaTokenAttribute(
    const QualifiedName& attribute) const {
  DEFINE_STATIC_LOCAL(const AtomicString, undefined_value, ("undefined"));
  const AtomicString& value = AriaAttribute(attribute);
  if (attribute == html_names::kAriaAutocompleteAttr ||
      attribute == html_names::kAriaCheckedAttr ||
      attribute == html_names::kAriaCurrentAttr ||
      attribute == html_names::kAriaHaspopupAttr ||
      attribute == html_names::kAriaInvalidAttr ||
      attribute == html_names::kAriaLiveAttr ||
      attribute == html_names::kAriaOrientationAttr ||
      attribute == html_names::kAriaPressedAttr ||
      attribute == html_names::kAriaRelevantAttr ||
      attribute == html_names::kAriaSortAttr) {
    // These properties support a list of tokens, and "undefined"/"" is
    // equivalent to not setting the attribute.
    return value.empty() || value == undefined_value ? g_null_atom : value;
  }
  DCHECK(false) << "Not a token attribute. Use AriaFloatAttribute(), "
                   "AriaIntAttribute(), AriaStringAttribute(), etc. instead.";
  return value;
}

// static
const AtomicString& AXObject::GetInternalsAttribute(
    Element& element,
    const QualifiedName& attribute) {
  if (!element.DidAttachInternals()) {
    return g_null_atom;
  }
  return element.EnsureElementInternals().FastGetAttribute(attribute);
}

Element* AXObject::GetAOMPropertyOrARIAAttribute(
    AOMRelationProperty property) const {
  Element* element = GetElement();
  if (!element)
    return nullptr;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(
    AOMRelationListProperty property,
    HeapVector<Member<Element>>& result) const {
  Element* element = GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property, result);
}

namespace {

void SerializeAriaNotificationAttributes(const AriaNotifications& notifications,
                                         ui::AXNodeData* node_data) {
  DCHECK(node_data);

  const auto size = notifications.Size();
  if (!size) {
    // Avoid serializing empty attribute lists if there are no notifications.
    return;
  }

  std::vector<std::string> announcements;
  std::vector<std::string> notification_ids;
  std::vector<int32_t> interrupt_properties;
  std::vector<int32_t> priority_properties;

  announcements.reserve(size);
  notification_ids.reserve(size);
  interrupt_properties.reserve(size);
  priority_properties.reserve(size);

  for (const auto& notification : notifications) {
    announcements.emplace_back(TruncateString(notification.Announcement()));
    notification_ids.emplace_back(
        TruncateString(notification.NotificationId()));
    interrupt_properties.emplace_back(
        static_cast<int32_t>(notification.Interrupt()));
    priority_properties.emplace_back(
        static_cast<int32_t>(notification.Priority()));
  }

  node_data->AddStringListAttribute(
      ax::mojom::blink::StringListAttribute::kAriaNotificationAnnouncements,
      announcements);
  node_data->AddStringListAttribute(
      ax::mojom::blink::StringListAttribute::kAriaNotificationIds,
      notification_ids);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kAriaNotificationInterruptProperties,
      interrupt_properties);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kAriaNotificationPriorityProperties,
      priority_properties);
}

}  // namespace

void AXObject::Serialize(ui::AXNodeData* node_data,
                         ui::AXMode accessibility_mode,
                         bool is_snapshot) const {
  // Reduce redundant ancestor chain walking for display lock computations.
  auto memoization_scope =
      DisplayLockUtilities::CreateLockCheckMemoizationScope();

  node_data->role = ComputeFinalRoleForSerialization();
  node_data->id = AXObjectID();

  PreSerializationConsistencyCheck();

  if (node_data->role == ax::mojom::blink::Role::kInlineTextBox) {
    SerializeInlineTextBox(node_data);
    return;
  }

  // Serialize a few things that we need even for ignored nodes.
  if (CanSetFocusAttribute()) {
    node_data->AddState(ax::mojom::blink::State::kFocusable);
  }

  bool is_visible = IsVisible();
  if (!is_visible)
    node_data->AddState(ax::mojom::blink::State::kInvisible);

  if (is_visible || CanSetFocusAttribute()) {
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

  if (!accessibility_mode.has_mode(ui::AXMode::kPDFPrinting)) {
    SerializeBoundingBoxAttributes(*node_data);
  }

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader)) {
    // TODO(accessibility) We serialize these even on ignored nodes, in order
    // for the browser side to compute inherited colors for descendants, but we
    // do not ensure that elements that change foreground/background color are
    // included in the tree. Could this lead to errors?
    // See All/DumpAccess*.AccessibilityCSSBackgroundColorTransparent/blink.
    SerializeColorAttributes(node_data);  // Blends using all nodes' values.
  }

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader) ||
      accessibility_mode.has_mode(ui::AXMode::kPDFPrinting)) {
    SerializeLangAttribute(node_data);  // Propagates using all nodes' values.
  }

  // Always try to serialize child tree ids.
  SerializeChildTreeID(node_data);

  // Return early. The following attributes are unnecessary for ignored nodes.
  // Exception: focusable ignored nodes are fully serialized, so that reasonable
  // verbalizations can be made if they actually receive focus.
  if (IsIgnored()) {
    node_data->AddState(ax::mojom::blink::State::kIgnored);
    if (!CanSetFocusAttribute()) {
      return;
    }
  }

  if (RoleValue() != ax::mojom::blink::Role::kStaticText) {
    // Needed on Android for testing frameworks.
    SerializeHTMLId(node_data);
  }

  SerializeUnignoredAttributes(node_data, accessibility_mode, is_snapshot);

  if (!accessibility_mode.has_mode(ui::AXMode::kScreenReader)) {
    // Return early. None of the following attributes are needed outside of
    // screen reader mode.
    return;
  }

  SerializeScreenReaderAttributes(node_data);

  if (accessibility_mode.has_mode(ui::AXMode::kPDFPrinting)) {
    // Return early. None of the following attributes are needed for PDFs.
    return;
  }

  if (LiveRegionRoot())
    SerializeLiveRegionAttributes(node_data);

  SerializeOtherScreenReaderAttributes(node_data);
  SerializeMathContent(node_data);
  SerializeAriaNotificationAttributes(
      AXObjectCache().RetrieveAriaNotifications(this), node_data);
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
  gfx::Point scroll_offset = GetScrollOffset();
  AXObjectCache().SetCachedBoundingBox(AXObjectID(), dst.relative_bounds,
                                       scroll_offset.x(), scroll_offset.y());
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

void AXObject::SerializeActionAttributes(ui::AXNodeData* node_data) const {
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

void AXObject::SerializeChildTreeID(ui::AXNodeData* node_data) const {
  // If a child tree has explicitly been stitched at this object via the
  // `ax::mojom::blink::Action::kStitchChildTree`, then override any child trees
  // coming from HTML.
  if (child_tree_id_) {
    node_data->AddChildTreeId(*child_tree_id_);
    return;
  }

  // If this is an HTMLFrameOwnerElement (such as an iframe), we may need to
  // embed the ID of the child frame.
  if (!IsEmbeddingElement()) {
    // TODO(crbug.com/1342603) Determine why these are firing in the wild and,
    // once fixed, turn into a DCHECK.
    SANITIZER_CHECK(!IsFrame(GetNode()))
        << "If this is an iframe, it should also be a child tree owner: "
        << this;
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
    SANITIZER_CHECK(IsDisabled()) << this;
    return;
  }

  std::optional<base::UnguessableToken> child_token =
      child_frame->GetEmbeddingToken();
  if (!child_token)
    return;  // No child token means that the connection isn't ready yet.

  DCHECK_EQ(ChildCountIncludingIgnored(), 0)
      << "Children won't exist until the trees are stitched together in the "
         "browser process. A failure means that a child node was incorrectly "
         "considered relevant by AXObjectCacheImpl."
      << "\n* Parent: " << this
      << "\n* Frame owner: " << IsA<HTMLFrameOwnerElement>(GetNode())
      << "\n* Element src: "
      << GetElement()->FastGetAttribute(html_names::kSrcAttr)
      << "\n* First child: " << FirstChildIncludingIgnored();

  ui::AXTreeID child_tree_id = ui::AXTreeID::FromToken(child_token.value());
  node_data->AddChildTreeId(child_tree_id);
}

void AXObject::SerializeChooserPopupAttributes(ui::AXNodeData* node_data) const {
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

void AXObject::SerializeColorAttributes(ui::AXNodeData* node_data) const {
  // Text attributes.
  if (RGBA32 bg_color = BackgroundColor()) {
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kBackgroundColor,
                               bg_color);
  }

  if (RGBA32 color = GetColor())
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kColor, color);
}

void AXObject::SerializeElementAttributes(ui::AXNodeData* node_data) const {
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
  const AtomicString& role_str = GetRoleStringForSerialization(node_data);
  TruncateAndAddStringAttribute(
      node_data, ax::mojom::blink::StringAttribute::kRole, role_str);
}

void AXObject::SerializeHTMLTagAndClass(ui::AXNodeData* node_data) const {
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

void AXObject::SerializeHTMLId(ui::AXNodeData* node_data) const {
  Element* element = GetElement();
  if (!element) {
    return;
  }

  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kHtmlId,
                                element->GetIdAttribute());
}

void AXObject::SerializeHTMLAttributes(ui::AXNodeData* node_data) const {
  Element* element = GetElement();
  DCHECK(element);
  for (const Attribute& attr : element->AttributesWithoutUpdate()) {
    std::string name = attr.LocalName().LowerASCII().Utf8();
    if (name == "id" || name == "class") {
      // Attribute already in kHtmlId or kClassName.
      continue;
    }
    std::string value = attr.Value().Utf8();
    node_data->html_attributes.push_back(std::make_pair(name, value));
  }
}

void AXObject::SerializeInlineTextBox(ui::AXNodeData* node_data) const {
  DCHECK_EQ(ax::mojom::blink::Role::kInlineTextBox, node_data->role);

  SerializeLineAttributes(node_data);
  SerializeMarkerAttributes(node_data);
  SerializeBoundingBoxAttributes(*node_data);
  if (GetTextDirection() != ax::mojom::blink::WritingDirection::kNone) {
    node_data->SetTextDirection(GetTextDirection());
  }

  Vector<int> character_offsets;
  TextCharacterOffsets(character_offsets);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kCharacterOffsets, character_offsets,
      node_data);

  // TODO(kevers): This data can be calculated on demand from the text content
  // and should not need to be serialized.
  Vector<int> word_starts;
  Vector<int> word_ends;
  GetWordBoundaries(word_starts, word_ends);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordStarts, word_starts, node_data);
  AddIntListAttributeFromOffsetVector(
      ax::mojom::blink::IntListAttribute::kWordEnds, word_ends, node_data);

  // TODO(accessibility) Only need these editable states on ChromeOS, which
  // should be able to infer them from the static text parent.
  if (IsEditable()) {
    node_data->AddState(ax::mojom::blink::State::kEditable);
    if (IsRichlyEditable()) {
      node_data->AddState(ax::mojom::blink::State::kRichlyEditable);
    }
  }

  ax::mojom::blink::NameFrom name_from;
  AXObjectVector name_objects;
  String name = GetName(name_from, &name_objects);
  DCHECK_EQ(name_from, ax::mojom::blink::NameFrom::kContents);
  node_data->SetNameFrom(ax::mojom::blink::NameFrom::kContents);

  if (::features::IsAccessibilityPruneRedundantInlineTextEnabled()) {
    DCHECK(parent_);
    DCHECK(parent_->GetLayoutObject()->IsText());
    if (IsOnlyChild()) {
      auto* layout_text = To<LayoutText>(parent_->GetLayoutObject());
      String visible_text = layout_text->PlainText();
      if (name == visible_text) {
        // The text of an only-child inline text box can be inferred directly
        // from the parent. No need to serialize redundant data.
        return;
      }
    }
  }

  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kName, name,
                                kMaxStringAttributeLength);
}

void AXObject::SerializeLangAttribute(ui::AXNodeData* node_data) const {
  AXObject* parent = ParentObject();
  if (Language().length()) {
    // Trim redundant languages.
    if (!parent || parent->Language() != Language()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kLanguage, Language());
    }
  }
}

void AXObject::SerializeLineAttributes(ui::AXNodeData* node_data) const {
  AXObjectCache().ComputeNodesOnLine(GetLayoutObject());

  if (AXObject* next_on_line = NextOnLine()) {
    CHECK(!next_on_line->IsDetached());
    node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kNextOnLineId,
                               next_on_line->AXObjectID());
  }

  if (AXObject* prev_on_line = PreviousOnLine()) {
    CHECK(!prev_on_line->IsDetached());
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kPreviousOnLineId,
        prev_on_line->AXObjectID());
  }
}

void AXObject::SerializeListAttributes(ui::AXNodeData* node_data) const {
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
    DCHECK_NE(name_from, ax::mojom::blink::NameFrom::kProhibited);
    int max_length = node_data->role == ax::mojom::blink::Role::kStaticText
                         ? kMaxStaticTextLength
                         : kMaxStringAttributeLength;
    if (TruncateAndAddStringAttribute(node_data,
                                      ax::mojom::blink::StringAttribute::kName,
                                      name, max_length)) {
      node_data->SetNameFrom(name_from);
      AddIntListAttributeFromObjects(
          ax::mojom::blink::IntListAttribute::kLabelledbyIds, name_objects,
          node_data);
    }
  } else if (name_from == ax::mojom::blink::NameFrom::kProhibited) {
    DCHECK(name.empty());
    node_data->SetNameFrom(ax::mojom::blink::NameFrom::kProhibited);
  }

  ax::mojom::blink::DescriptionFrom description_from;
  AXObjectVector description_objects;
  String description =
      Description(name_from, description_from, &description_objects);
  if (description.empty()) {
    DCHECK_NE(name_from, ax::mojom::blink::NameFrom::kProhibited)
        << "Should expose prohibited name as description: " << GetNode();
  } else {
    DCHECK_NE(description_from, ax::mojom::blink::DescriptionFrom::kNone)
        << this << "\n* Description: " << description;
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

void AXObject::SerializeScreenReaderAttributes(ui::AXNodeData* node_data) const {
  if (ui::IsText(RoleValue())) {
    // Don't serialize these attributes on text, where it is uninteresting.
    return;
  }
  SerializeHTMLTagAndClass(node_data);

  String display_style;
  if (Element* element = GetElement()) {
    if (const ComputedStyle* computed_style = element->GetComputedStyle()) {
      display_style = CSSProperty::Get(CSSPropertyID::kDisplay)
                          .CSSValueFromComputedStyle(
                              *computed_style, /* layout_object */ nullptr,
                              /* allow_visited_style */ false,
                              CSSValuePhase::kComputedValue)
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

  if (IsA<Document>(GetNode())) {
    // The busy attribute is only relevant for actual Documents, not popups.
    if (RoleValue() == ax::mojom::blink::Role::kRootWebArea && !IsLoaded()) {
      node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy, true);
    }

    if (AXObject* parent = ParentObject()) {
      DCHECK(parent->ChooserPopup() == this)
          << "ChooserPopup missing for: " << parent;
      node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kPopupForId,
                                 parent->AXObjectID());
    }
  } else {
    SerializeLineAttributes(node_data);
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

  AXObjectVector error_messages = ErrorMessage();
  if (error_messages.size() > 0) {
    AddIntListAttributeFromObjects(
        ax::mojom::blink::IntListAttribute::kErrormessageIds, error_messages,
        node_data);
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
}

void AXObject::SerializeMathContent(ui::AXNodeData* node_data) const {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  const Element* element = GetElement();
  if (!element) {
    return;
  }
  if (const AtomicString& math_ml = AriaAttribute(MathMLAttrName())) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kMathContent, math_ml,
        kMaxStaticTextLength);
    return;
  }
  if (node_data->role == ax::mojom::blink::Role::kMath ||
      node_data->role == ax::mojom::blink::Role::kMathMLMath) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kMathContent,
        element->innerHTML(), kMaxStaticTextLength);
  }
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
}

void AXObject::SerializeScrollAttributes(ui::AXNodeData* node_data) const {
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

void AXObject::SerializeRelationAttributes(ui::AXNodeData* node_data) const {
  // No need to call this for objects without an element, as relations are
  // only possible between elements.
  DCHECK(GetElement());

  // TODO(accessibility) Consider checking role before serializing
  // aria-activedescendant, aria-errormessage.

  if (AXObject* active_descendant = ActiveDescendant()) {
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kActivedescendantId,
        active_descendant->AXObjectID());
  }

  AddIntListAttributeFromObjects(
      ax::mojom::blink::IntListAttribute::kControlsIds,
      RelationVectorFromAria(html_names::kAriaControlsAttr), node_data);
  AddIntListAttributeFromObjects(
      ax::mojom::blink::IntListAttribute::kDetailsIds,
      RelationVectorFromAria(html_names::kAriaDetailsAttr), node_data);
  AddIntListAttributeFromObjects(
      ax::mojom::blink::IntListAttribute::kFlowtoIds,
      RelationVectorFromAria(html_names::kAriaFlowtoAttr), node_data);
}

void AXObject::SerializeStyleAttributes(ui::AXNodeData* node_data) const {
  // Only serialize font family if there is one, and it is different from the
  // parent. Use the value from computed style first since that is a fast lookup
  // and comparison, and serialize the user-friendly name at points in the tree
  // where the font family changes between parent/child.
  // TODO(accessibility) No need to serialize these for inline text boxes.
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

  if (GetTextAlign() != ax::mojom::blink::TextAlign::kNone) {
    node_data->SetTextAlign(GetTextAlign());
  }

  if (GetTextIndent() != 0.0f) {
    node_data->AddFloatAttribute(ax::mojom::blink::FloatAttribute::kTextIndent,
                                 GetTextIndent());
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

void AXObject::SerializeTableAttributes(ui::AXNodeData* node_data) const {
  if (ui::IsTableLike(RoleValue())) {
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaColumnCount,
                    html_names::kAriaColcountAttr, node_data,
                    ColumnCount() + 1);
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaRowCount,
                    html_names::kAriaRowcountAttr, node_data, RowCount() + 1);
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
    // Both HTML rowspan/colspan and ARIA rowspan/colspan are serialized.
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaCellColumnSpan,
                    html_names::kAriaColspanAttr, node_data, 1);
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTableCellColumnSpan, ColumnSpan());
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaCellRowSpan,
                    html_names::kAriaRowspanAttr, node_data, 1);
    node_data->AddIntAttribute(
        ax::mojom::blink::IntAttribute::kTableCellRowSpan, RowSpan());
  }

  if (ui::IsCellOrTableHeader(RoleValue()) || ui::IsTableRow(RoleValue())) {
    // aria-rowindex and aria-colindex are supported on cells, headers and
    // rows.
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaCellRowIndex,
                    html_names::kAriaRowindexAttr, node_data, 1);
    AddIntAttribute(this, ax::mojom::blink::IntAttribute::kAriaCellColumnIndex,
                    html_names::kAriaColindexAttr, node_data, 1);
    if (RuntimeEnabledFeatures::AriaRowColIndexTextEnabled()) {
      // TODO(accessibility): Remove deprecated attribute support for
      // aria-rowtext/aria-coltext once Sheets uses standard attribute names
      // aria-rowindextext/aria-colindextext.
      AtomicString aria_cell_row_index_text =
          AriaAttribute(html_names::kAriaRowindextextAttr);
      if (aria_cell_row_index_text.empty()) {
        aria_cell_row_index_text =
            AriaAttribute(DeprecatedAriaRowtextAttrName());
      }
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kAriaCellRowIndexText,
          aria_cell_row_index_text);
      AtomicString aria_cell_column_index_text =
          AriaAttribute(html_names::kAriaColindextextAttr);
      if (aria_cell_column_index_text.empty()) {
        aria_cell_column_index_text =
            AriaAttribute(DeprecatedAriaColtextAttrName());
      }
      TruncateAndAddStringAttribute(
          node_data,
          ax::mojom::blink::StringAttribute::kAriaCellColumnIndexText,
          aria_cell_column_index_text);
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
                                            ui::AXMode accessibility_mode,
                                            bool is_snapshot) const {
  SerializeNameAndDescriptionAttributes(accessibility_mode, node_data);

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader)) {
    SerializeMarkerAttributes(node_data);
#if BUILDFLAG(IS_ANDROID)
    // On Android, style attributes are only serialized for snapshots.
    if (is_snapshot) {
      SerializeStyleAttributes(node_data);
    } else {
      // For non-snapshots we include writing direction for image descriptions.
      // TODO(mschillaci): Remove this after content is updated.
      if (IsImage() &&
          GetTextDirection() != ax::mojom::blink::WritingDirection::kNone) {
        node_data->SetTextDirection(GetTextDirection());
      }
    }
#else
    SerializeStyleAttributes(node_data);
#endif
  }

  if (IsLinked()) {
    node_data->AddState(ax::mojom::blink::State::kLinked);
    if (IsVisited()) {
      node_data->AddState(ax::mojom::blink::State::kVisited);
    }
  }

  if (IsNotUserSelectable()) {
    node_data->AddBoolAttribute(
        ax::mojom::blink::BoolAttribute::kNotUserSelectableStyle, true);
  }

  // If text, return early as a performance tweak, as the rest of the properties
  // in this method do not apply to text.
  if (RoleValue() == ax::mojom::blink::Role::kStaticText) {
    return;
  }

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

  if (IsHovered())
    node_data->AddState(ax::mojom::blink::State::kHovered);

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

  if (Orientation() == kAccessibilityOrientationVertical)
    node_data->AddState(ax::mojom::blink::State::kVertical);
  else if (Orientation() == blink::kAccessibilityOrientationHorizontal)
    node_data->AddState(ax::mojom::blink::State::kHorizontal);

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader) ||
      accessibility_mode.has_mode(ui::AXMode::kPDFPrinting)) {
    // Heading level.
    if (ui::IsHeading(role) && HeadingLevel()) {
      node_data->AddIntAttribute(
          ax::mojom::blink::IntAttribute::kHierarchicalLevel, HeadingLevel());
    }

    SerializeListAttributes(node_data);
    SerializeTableAttributes(node_data);
  }

  if (accessibility_mode.has_mode(ui::AXMode::kPDFPrinting)) {
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

  if (Element* element = GetElement()) {
    SerializeRelationAttributes(node_data);

    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kAriaBrailleLabel,
        AriaAttribute(html_names::kAriaBraillelabelAttr));
    if (RoleValue() != ax::mojom::blink::Role::kGenericContainer) {
      // ARIA 1.2 prohibits aria-roledescription on the "generic" role.
      TruncateAndAddStringAttribute(
          node_data,
          ax::mojom::blink::StringAttribute::kAriaBrailleRoleDescription,
          AriaAttribute(html_names::kAriaBrailleroledescriptionAttr));
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kRoleDescription,
          AriaAttribute(html_names::kAriaRoledescriptionAttr));
    }
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kKeyShortcuts,
        AriaAttribute(html_names::kAriaKeyshortcutsAttr));
    if (RuntimeEnabledFeatures::AccessibilityAriaVirtualContentEnabled()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kVirtualContent,
          AriaAttribute(html_names::kAriaVirtualcontentAttr));
    }

    if (IsAriaAttributeTrue(html_names::kAriaBusyAttr)) {
      node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kBusy, true);
    }

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
      int start = ax_selection.Anchor().IsTextPosition()
                      ? ax_selection.Anchor().TextOffset()
                      : ax_selection.Anchor().ChildIndex();
      int end = ax_selection.Focus().IsTextPosition()
                    ? ax_selection.Focus().TextOffset()
                    : ax_selection.Focus().ChildIndex();
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

  SerializeImageDataAttributes(node_data);
  SerializeTextInsertionDeletionOffsetAttributes(node_data);
}

void AXObject::SerializeComputedDetailsRelation(
    ui::AXNodeData* node_data) const {
  // aria-details was used -- it may have set a relation, unless the attribute
  // value did not point to valid elements (e.g aria-details=""). Whether it
  // actually set the relation or not, the author's intent in using the
  // aria-details attribute is understood to mean that no automatic relation
  // should be set.
  if (HasAriaAttribute(html_names::kAriaDetailsAttr)) {
    if (!node_data
             ->GetIntListAttribute(ax::mojom::IntListAttribute::kDetailsIds)
             .empty()) {
      node_data->SetDetailsFrom(ax::mojom::blink::DetailsFrom::kAriaDetails);
    }
    return;
  }

  // Add aria-details for a popover invoker.
  // TODO(https://crbug.com/1426607) Support this for non-plain hint popovers.
  if (AXObject* popover = GetTargetPopoverForInvoker()) {
    node_data->AddIntListAttribute(
        ax::mojom::blink::IntListAttribute::kDetailsIds,
        {static_cast<int32_t>(popover->AXObjectID())});
    node_data->SetDetailsFrom(ax::mojom::blink::DetailsFrom::kPopoverAttribute);
    return;
  }

  // Add aria-details for the element anchored to this object.
  if (AXObject* positioned_obj = GetPositionedObjectForAnchor(node_data)) {
    node_data->AddIntListAttribute(
        ax::mojom::blink::IntListAttribute::kDetailsIds,
        {static_cast<int32_t>(positioned_obj->AXObjectID())});
    node_data->SetDetailsFrom(ax::mojom::blink::DetailsFrom::kCssAnchor);
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
  return AXObjectCache().Get(target_popover);
}

AXObject* AXObject::GetPositionedObjectForAnchor(ui::AXNodeData* data) const {
  AXObject* positioned_obj = AXObjectCache().GetPositionedObjectForAnchor(this);
  if (!positioned_obj) {
    return nullptr;
  }

  // Check for cases where adding an aria-details relationship between the
  // anchor and the positioned elements would add extra noise.
  // https://github.com/w3c/html-aam/issues/545
  if (positioned_obj->RoleValue() == ax::mojom::blink::Role::kTooltip) {
    return nullptr;
  }

  // Elements are direct DOM siblings.
  if (ElementTraversal::NextSkippingChildren(*GetNode()) ==
      positioned_obj->GetElement()) {
    return nullptr;
  }

  // Check for existing labelledby/describedby/controls relationships.
  for (auto attr : {ax::mojom::blink::IntListAttribute::kLabelledbyIds,
                    ax::mojom::blink::IntListAttribute::kDescribedbyIds,
                    ax::mojom::blink::IntListAttribute::kControlsIds}) {
    auto attr_ids = data->GetIntListAttribute(attr);
    if (std::find(attr_ids.begin(), attr_ids.end(),
                  positioned_obj->AXObjectID()) != attr_ids.end()) {
      return nullptr;
    }
  }

  // Check for existing parent/child relationship (includes case where the
  // anchor has an aria-owns relationship with the positioned element).
  if (positioned_obj->ParentObject() == this) {
    return nullptr;
  }

  return positioned_obj;
}

// Try to get an aria-controls for an <input role="combobox">, because it
// helps identify focusable options in the listbox using activedescendant
// detection, even though the focus is on the textbox and not on the listbox
// ancestor.
AXObject* AXObject::GetControlsListboxForTextfieldCombobox() const {
  // Only perform work for textfields.
  if (!ui::IsTextField(RoleValue()))
    return nullptr;

  // Object is ignored for some reason, most likely hidden.
  if (IsIgnored()) {
    return nullptr;
  }

  // Authors used to be told to use aria-owns to point from the textfield to the
  // listbox. However, the aria-owns  on a textfield must be ignored for its
  // normal purpose because a textfield cannot have children. This code allows
  // the textfield's invalid aria-owns to be remapped to aria-controls.
  DCHECK(GetElement());
  HeapVector<Member<Element>> owned_elements;
  AXObject* listbox_candidate = nullptr;
  if (ElementsFromAttribute(GetElement(), owned_elements,
                            html_names::kAriaOwnsAttr) &&
      owned_elements.size() > 0) {
    DCHECK(owned_elements[0]);
    listbox_candidate = AXObjectCache().Get(owned_elements[0]);
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
    if (!ui::IsComboBoxContainer(listbox_candidate->RawAriaRole())) {
      return nullptr;
    }
    // Naming a listbox within a composite combobox widget is not part of a
    // known/used pattern. If it has a name, it's an indicator that it's
    // probably a separate listbox widget.
    if (!listbox_candidate->ComputedName().empty())
      return nullptr;
  }

  if (!listbox_candidate ||
      !ui::IsComboBoxContainer(listbox_candidate->RoleValue())) {
    return nullptr;
  }

  return listbox_candidate;
}

const AtomicString& AXObject::GetRoleStringForSerialization(
    ui::AXNodeData* node_data) const {
  // All ARIA roles are exposed in xml-roles.
  if (const AtomicString& role_str = AriaAttribute(html_names::kRoleAttr)) {
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
  return AriaRoleName(landmark_role);
}

void AXObject::SerializeMarkerAttributes(ui::AXNodeData* node_data) const {
  // Implemented in subclasses.
}

void AXObject::SerializeImageDataAttributes(ui::AXNodeData* node_data) const {
  if (AXObjectID() != AXObjectCache().image_data_node_id()) {
    return;
  }

  // In general, string attributes should be truncated using
  // TruncateAndAddStringAttribute, but ImageDataUrl contains a data url
  // representing an image, so add it directly using AddStringAttribute.
  node_data->AddStringAttribute(
      ax::mojom::blink::StringAttribute::kImageDataUrl,
      ImageDataUrl(AXObjectCache().max_image_data_size()).Utf8());
}

void AXObject::SerializeTextInsertionDeletionOffsetAttributes(
    ui::AXNodeData* node_data) const {
  if (!IsEditable()) {
    return;
  }

  WTF::Vector<TextChangedOperation>* offsets =
      AXObjectCache().GetFromTextOperationInNodeIdMap(AXObjectID());
  if (!offsets) {
    return;
  }

  std::vector<int> start_offsets;
  std::vector<int> end_offsets;
  std::vector<int> start_anchor_ids;
  std::vector<int> end_anchor_ids;
  std::vector<int> operations_ints;

  start_offsets.reserve(offsets->size());
  end_offsets.reserve(offsets->size());
  start_anchor_ids.reserve(offsets->size());
  end_anchor_ids.reserve(offsets->size());
  operations_ints.reserve(offsets->size());

  for (auto operation : *offsets) {
    start_offsets.push_back(operation.start);
    end_offsets.push_back(operation.end);
    start_anchor_ids.push_back(operation.start_anchor_id);
    end_anchor_ids.push_back(operation.end_anchor_id);
    operations_ints.push_back(static_cast<int>(operation.op));
  }

  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kTextOperationStartOffsets,
      start_offsets);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kTextOperationEndOffsets,
      end_offsets);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kTextOperationStartAnchorIds,
      start_anchor_ids);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kTextOperationEndAnchorIds,
      end_anchor_ids);
  node_data->AddIntListAttribute(
      ax::mojom::blink::IntListAttribute::kTextOperations, operations_ints);
  AXObjectCache().ClearTextOperationInNodeIdMap();
}

bool AXObject::IsAXNodeObject() const {
  return false;
}

bool AXObject::IsAXInlineTextBox() const {
  return false;
}

bool AXObject::IsList() const {
  return ui::IsList(RoleValue());
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

ax::mojom::blink::Role AXObject::ComputeFinalRoleForSerialization() const {
  // An SVG with no accessible children should be exposed as an image rather
  // than a document. See https://github.com/w3c/svg-aam/issues/12.
  // We do this check here for performance purposes: When
  // AXNodeObject::RoleFromLayoutObjectOrNode is called, that node's
  // accessible children have not been calculated. Rather than force calculation
  // there, wait until we have the full tree.
  if (role_ == ax::mojom::blink::Role::kSvgRoot &&
      IsIncludedInTree() && !UnignoredChildCount()) {
    return ax::mojom::blink::Role::kImage;
  }

  // DPUB ARIA 1.1 deprecated doc-biblioentry and doc-endnote, but it's still
  // possible to create these internal roles / platform mappings with a listitem
  // (native or ARIA) inside of a doc-bibliography or doc-endnotes section.
  if (role_ == ax::mojom::blink::Role::kListItem) {
    AXObject* ancestor = ParentObject();
    if (ancestor && ancestor->RoleValue() == ax::mojom::blink::Role::kList) {
      // Go up to the root, or next list, checking to see if the list item is
      // inside an endnote or bibliography section. If it is, remap the role.
      // The remapping does not occur for list items multiple levels deep.
      while (true) {
        ancestor = ancestor->ParentObject();
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
      return ax::mojom::blink::Role::kSectionHeader;
    }
  }

  if (role_ == ax::mojom::blink::Role::kFooter) {
    if (IsDescendantOfLandmarkDisallowedElement()) {
      return ax::mojom::blink::Role::kSectionFooter;
    }
  }

  // An <aside> element should not be considered a landmark region
  // if it is a child of a landmark disallowed element, UNLESS it has
  // an accessible name.
  if (role_ == ax::mojom::blink::Role::kComplementary &&
      RawAriaRole() != ax::mojom::blink::Role::kComplementary) {
    if (IsDescendantOfLandmarkDisallowedElement() &&
        !IsNameFromAuthorAttribute()) {
      return ax::mojom::blink::Role::kGenericContainer;
    }
  }

  // Treat a named <section> as role="region".
  if (role_ == ax::mojom::blink::Role::kSection) {
    return IsNameFromAuthorAttribute()
               ? ax::mojom::blink::Role::kRegion
               : ax::mojom::blink::Role::kSectionWithoutName;
  }

  if (role_ == ax::mojom::blink::Role::kCell) {
    AncestorsIterator ancestor = base::ranges::find_if(
        UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
        &AXObject::IsTableLikeRole);
    if (ancestor.current_ &&
        (ancestor.current_->RoleValue() == ax::mojom::blink::Role::kGrid ||
         ancestor.current_->RoleValue() == ax::mojom::blink::Role::kTreeGrid)) {
      return ax::mojom::blink::Role::kGridCell;
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
  return RawAriaRole() == ax::mojom::blink::Role::kTextField ||
         RawAriaRole() == ax::mojom::blink::Role::kSearchBox ||
         RawAriaRole() == ax::mojom::blink::Role::kTextFieldWithComboBox;
}

bool AXObject::IsButton() const {
  return ui::IsButton(RoleValue());
}

bool AXObject::ShouldUseComboboxMenuButtonRole() const {
  DCHECK(GetElement());
  if (GetElement()->SupportsFocus(
          Element::UpdateBehavior::kNoneForAccessibility) !=
      FocusableState::kNotFocusable) {
    return true;
  }
  if (IsA<HTMLButtonElement>(GetNode())) {
    return true;
  }
  if (auto* input = DynamicTo<HTMLInputElement>(GetNode())) {
    if (input && input->IsButton()) {
      return true;
    }
  }
  return false;
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
      return AriaTokenAttribute(html_names::kAriaCheckedAttr) != g_null_atom;
    default:
      return false;
  }
}

ax::mojom::blink::CheckedState AXObject::CheckedState() const {
  const Node* node = GetNode();
  if (!IsCheckable() || !node) {
    return ax::mojom::blink::CheckedState::kNone;
  }

  // First test for native checked state
  if (IsA<HTMLInputElement>(*node)) {
    const auto* input = DynamicTo<HTMLInputElement>(node);
    if (!input) {
      return ax::mojom::blink::CheckedState::kNone;
    }

    const auto inputType = input->type();
    // The native checked state is processed exlusively. Aria is ignored because
    // the native checked value takes precedence for input elements with type
    // `checkbox` or `radio` according to the HTML-AAM specification.
    if (inputType == input_type_names::kCheckbox ||
        inputType == input_type_names::kRadio) {
      // Expose native checkbox mixed state as accessibility mixed state (unless
      // the role is switch). However, do not expose native radio mixed state as
      // accessibility mixed state. This would confuse the JAWS screen reader,
      // which reports a mixed radio as both checked and partially checked, but
      // a native mixed native radio button simply means no radio buttons have
      // been checked in the group yet.
      if (IsNativeCheckboxInMixedState(node)) {
        return ax::mojom::blink::CheckedState::kMixed;
      }

      return input->ShouldAppearChecked()
                 ? ax::mojom::blink::CheckedState::kTrue
                 : ax::mojom::blink::CheckedState::kFalse;
    }
  }

  // Try ARIA checked/pressed state
  const ax::mojom::blink::Role role = RoleValue();
  const QualifiedName& prop = role == ax::mojom::blink::Role::kToggleButton
                                  ? html_names::kAriaPressedAttr
                                  : html_names::kAriaCheckedAttr;
  const AtomicString& checked_attribute = AriaTokenAttribute(prop);
  if (checked_attribute) {
    if (EqualIgnoringASCIICase(checked_attribute, "mixed")) {
      if (role == ax::mojom::blink::Role::kCheckBox ||
          role == ax::mojom::blink::Role::kMenuItemCheckBox ||
          role == ax::mojom::blink::Role::kListBoxOption ||
          role == ax::mojom::blink::Role::kToggleButton ||
          role == ax::mojom::blink::Role::kTreeItem) {
        // Mixed value is supported in these roles: checkbox, menuitemcheckbox,
        // option, togglebutton, and treeitem.
        return ax::mojom::blink::CheckedState::kMixed;
      } else {
        // Mixed value is not supported in these roles: radio, menuitemradio,
        // and switch.
        return ax::mojom::blink::CheckedState::kFalse;
      }
    }

    // Anything other than "false" should be treated as "true".
    return EqualIgnoringASCIICase(checked_attribute, "false")
               ? ax::mojom::blink::CheckedState::kFalse
               : ax::mojom::blink::CheckedState::kTrue;
  }

  return ax::mojom::blink::CheckedState::kFalse;
}

String AXObject::GetValueForControl() const {
  return String();
}

String AXObject::GetValueForControl(AXObjectSet& visited) const {
  return String();
}

String AXObject::SlowGetValueForControlIncludingContentEditable() const {
  return String();
}

String AXObject::SlowGetValueForControlIncludingContentEditable(
    AXObjectSet& visited) const {
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

AXObject* AXObject::GetTextFieldAncestor() {
  AXObject* ancestor = this;
  while (ancestor && !ancestor->IsTextField()) {
    ancestor = ancestor->ParentObject();
  }
  return ancestor;
}

bool AXObject::IsPasswordField() const {
  auto* input_element = DynamicTo<HTMLInputElement>(GetNode());
  return input_element &&
         input_element->FormControlType() == FormControlType::kInputPassword;
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

bool AXObject::IsHovered() const {
  return false;
}

bool AXObject::IsLineBreakingObject() const {
  // We assume that most images on the Web are inline.
  return !IsImage() && ui::IsStructure(RoleValue()) &&
         !IsA<SVGElement>(GetNode());
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

bool AXObject::IsNotUserSelectable() const {
  return false;
}

bool AXObject::IsVisited() const {
  return false;
}

bool AXObject::IsIgnored() const {
  DCHECK(cached_is_ignored_ || !IsDetached())
      << "A detached object should always indicate that it is ignored so that "
         "it won't ever accidentally be included in the tree.";
  return cached_is_ignored_;
}

bool AXObject::IsIgnored() {
  CheckCanAccessCachedValues();
  UpdateCachedAttributeValuesIfNeeded();
#if defined(AX_FAIL_FAST_BUILD)
  if (!cached_is_ignored_ && IsDetached()) {
    NOTREACHED_IN_MIGRATION()
        << "A detached node cannot be ignored: " << this
        << "\nThe Detach() method sets cached_is_ignored_ to true, but "
           "something has recomputed it.";
  }
#endif
  return cached_is_ignored_;
}

bool AXObject::IsIgnoredButIncludedInTree() const {
  return cached_is_ignored_but_included_in_tree_;
}

bool AXObject::IsIgnoredButIncludedInTree() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_ignored_but_included_in_tree_;
}

// IsIncludedInTree should be true for all nodes that should be
// included in the tree, even if they are ignored
bool AXObject::CachedIsIncludedInTree() const {
  return !cached_is_ignored_ || cached_is_ignored_but_included_in_tree_;
}

bool AXObject::IsIncludedInTree() const {
  return CachedIsIncludedInTree();
}

bool AXObject::IsIncludedInTree() {
  return !IsIgnored() || IsIgnoredButIncludedInTree();
}

void AXObject::CheckCanAccessCachedValues() const {
  if (!IsDetached() && AXObjectCache().IsFrozen()) {
    DUMP_WILL_BE_CHECK(!NeedsToUpdateCachedValues())
        << "Stale values: " << this;
  }
}

void AXObject::InvalidateCachedValues() {
  CHECK(AXObjectCache().lifecycle().StateAllowsAXObjectsToBeDirtied())
      << AXObjectCache();
#if DCHECK_IS_ON()
  DCHECK(!is_updating_cached_values_)
      << "Should not invalidate cached values while updating them.";
#endif

  cached_values_need_update_ = true;
}

void AXObject::UpdateCachedAttributeValuesIfNeeded(
    bool notify_parent_of_ignored_changes) {
  if (IsDetached()) {
    cached_is_ignored_ = true;
    cached_is_ignored_but_included_in_tree_ = false;
    return;
  }

  if (!NeedsToUpdateCachedValues()) {
    return;
  }

  cached_values_need_update_ = false;

  CHECK(AXObjectCache().lifecycle().StateAllowsImmediateTreeUpdates())
      << AXObjectCache();

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

  DUMP_WILL_BE_CHECK(!IsMissingParent()) << "Missing parent: " << this;

  const ComputedStyle* style = GetComputedStyle();

  cached_is_hidden_via_style_ = ComputeIsHiddenViaStyle(style);

  // Decisions in what subtree descendants are included (each descendant's
  // cached children_) depends on the ARIA hidden state. When it changes,
  // the entire subtree needs to recompute descendants.
  // In addition, the below computations for is_ignored_but_included_in_tree is
  // dependent on having the correct new cached value.
  bool is_inert = ComputeIsInertViaStyle(style);
  bool is_aria_hidden = ComputeIsAriaHidden();
  bool is_in_menu_list_subtree = ComputeIsInMenuListSubtree();
  bool is_descendant_of_disabled_node = ComputeIsDescendantOfDisabledNode();
  bool is_changing_inherited_values = false;
  if (cached_is_inert_ != is_inert ||
      cached_is_aria_hidden_ != is_aria_hidden ||
      cached_is_in_menu_list_subtree_ != is_in_menu_list_subtree ||
      cached_is_descendant_of_disabled_node_ !=
          is_descendant_of_disabled_node) {
    is_changing_inherited_values = true;
    cached_is_inert_ = is_inert;
    cached_is_aria_hidden_ = is_aria_hidden;
    cached_is_in_menu_list_subtree_ = is_in_menu_list_subtree;
    cached_is_descendant_of_disabled_node_ = is_descendant_of_disabled_node;
  }

  // Must be after inert computation, because focusability depends on that, but
  // before the included in tree computation, which depends on focusability.
  CHECK(!IsDetached());
  cached_can_set_focus_attribute_ = ComputeCanSetFocusAttribute();
  CHECK(!IsDetached());

  // Must be computed before is_used_for_label_or_description computation.
  bool was_included_in_tree = IsIncludedInTree();
  bool is_ignored = ComputeIsIgnored();
  if (is_ignored != IsIgnored()) {
    // Presence of inline text children depends on ignored state.
    if (ui::CanHaveInlineTextBoxChildren(RoleValue())) {
      is_changing_inherited_values = true;
    }
    cached_is_ignored_ = is_ignored;
  }

  // This depends on cached_is_ignored_ and cached_can_set_focus_attribute_.
  bool is_used_for_label_or_description = ComputeIsUsedForLabelOrDescription();
  if (is_used_for_label_or_description !=
      cached_is_used_for_label_or_description_) {
    is_changing_inherited_values = true;
    cached_is_used_for_label_or_description_ = is_used_for_label_or_description;
  }

  // This depends on cached_is_used_for_label_or_description_.
  bool is_ignored_but_included_in_tree =
      is_ignored && ComputeIsIgnoredButIncludedInTree();
  bool is_included_in_tree = !is_ignored || is_ignored_but_included_in_tree;
#if DCHECK_IS_ON()
  if (!is_included_in_tree && GetNode()) {
    Node* dom_parent = NodeTraversal::Parent(*GetNode());
    DCHECK(dom_parent)
        << "A node with no DOM parent must be included in the tree, so that it "
           "can be found while traversing descendants.";
    Node* flat_tree_parent = LayoutTreeBuilderTraversal::Parent(*GetNode());
    DCHECK_EQ(dom_parent, flat_tree_parent)
        << "\nA node with a different flat tree parent must be included in the "
           "tree, so that it can be found while traversing descendants.";
  }
#endif
  bool included_in_tree_changed = is_included_in_tree != was_included_in_tree;
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
    if (notify_parent_of_ignored_changes) {
      notify_included_in_tree_changed = true;
    }
  }

  // If the child's "included in tree" state changes, we will be notifying the
  // parent to recompute it's children.
  // Exceptions:
  // - Caller passes in |notify_parent_of_ignored_changes = false| -- this
  //   occurs when this is a new child, or when a parent is in the middle of
  //   adding this child, and doing this would be redundant.
  // - Inline text boxes: their "included in tree" state is entirely dependent
  //   on their static text parent.
  // This must be called before cached_is_ignored_* are updated, otherwise a
  // performance optimization depending on IsIncludedInTree()
  // may misfire.
  if (RoleValue() != ax::mojom::blink::Role::kInlineTextBox) {
    if (notify_included_in_tree_changed) {
      if (AXObject* parent = ParentObject()) {
        SANITIZER_CHECK(!AXObjectCache().IsFrozen())
            << "Objects cannot change their inclusion state during "
               "serialization:\n"
            << "* Object: " << this << "\n* Ignored will become " << is_ignored
            << "\n* Included in tree will become "
            << (!is_ignored || is_ignored_but_included_in_tree)
            << "\n* Parent: " << parent;
        // Defers a ChildrenChanged() on the first included ancestor.
        // Must defer it, otherwise it can cause reentry into
        // UpdateCachedAttributeValuesIfNeeded() on |this|.
        // ParentObjectUnignored()->SetNeedsToUpdateChildren();
        AXObjectCache().ChildrenChangedOnAncestorOf(this);
      }
    } else if (included_in_tree_changed && AXObjectCache().IsUpdatingTree()) {
      // In some cases changes to inherited properties can cause an object
      // inclusion change in the tree updating phase, where it's too late to use
      // the usual dirty object mechanisms, but we can still queue the dirty
      // object for the serializer. The dirty object is the parent.
      // TODO(accessibility) Do we need to de-dupe these?
      AXObject* unignored_parent = ParentObjectUnignored();
      CHECK(unignored_parent);
      AXObjectCache().AddDirtyObjectToSerializationQueue(unignored_parent);
    }
  }

  cached_is_ignored_ = is_ignored;
  cached_is_ignored_but_included_in_tree_ = is_ignored_but_included_in_tree;

  // Compute live region root, which can be from any ARIA live value, including
  // "off", or from an automatic ARIA live value, e.g. from role="status".
  AXObject* previous_live_region_root = cached_live_region_root_;
  if (RoleValue() == ax::mojom::blink::Role::kInlineTextBox) {
    // Inline text boxes do not need live region properties.
    cached_live_region_root_ = nullptr;
  } else if (IsA<Document>(GetNode())) {
    // The document root is never a live region root.
    cached_live_region_root_ = nullptr;
  } else {
    DCHECK(parent_);
    // Is a live region root if this or an ancestor is a live region.
    cached_live_region_root_ =
        IsLiveRegionRoot() ? this : parent_->LiveRegionRoot();
  }
  if (cached_live_region_root_ != previous_live_region_root) {
    is_changing_inherited_values = true;
  }

  if (GetLayoutObject() && GetLayoutObject()->IsText()) {
    cached_local_bounding_box_ =
        GetLayoutObject()->LocalBoundingBoxRectForAccessibility();
  }

  if (is_changing_inherited_values) {
    // Update children if not already dirty.
    OnInheritedCachedValuesChanged();
  }

#if DCHECK_IS_ON()
  DCHECK(!NeedsToUpdateCachedValues())
      << "While recomputing cached values, they were invalidated again.";
  if (included_in_tree_changed) {
    AXObjectCache().UpdateIncludedNodeCount(this);
  }
#endif
}

void AXObject::OnInheritedCachedValuesChanged() {
  // When a cached value that can inherit its value changes, it means that
  // all descendants need to recompute its value. We do this by ensuring
  // that UpdateTreeIfNeeded() will visit all descendants and recompute
  // cached values.
  if (!CanHaveChildren()) {
    return;  // Nothing to do.
  }

  // This flag is checked and cleared when children are added.
  child_cached_values_need_update_ = true;

  if (children_dirty_) {
    return;
  }

  if (AXObjectCache().IsUpdatingTree()) {
    // When already in the middle of updating the tree, we know we are building
    // from the top down, and that its ok to mark things below (descendants) as
    // dirty and alter/rebuild them, but at this point we must not alter
    // ancestors. Mark the current children and their cached values dirty, and
    // set a flag so that
    children_dirty_ = true;
    if (AXObject* parent = ParentObjectIncludedInTree()) {
      // Make sure the loop in UpdateTreeIfNeeded() recursively will continue
      // and rebuild children whenever cached values of children have changed.
      // The loop continues if |has_dirty_descendants_| is set on the parent
      // that added this child.
      parent->SetHasDirtyDescendants(true);
    }
  } else {
    // Ensure that all children of this node will be updated during the next
    // tree update in AXObjectCacheImpl::UpdateTreeIfNeeded().
    SetNeedsToUpdateChildren();
    if (!IsIncludedInTree()) {
      // Make sure that, starting at an included node, children will
      // recursively be updated until we reach |this|.
      AXObjectCache().ChildrenChangedOnAncestorOf(this);
    }
  }
}

bool AXObject::ComputeIsIgnored(
    IgnoredReasons* ignored_reasons) const {
  return ShouldIgnoreForHiddenOrInert(ignored_reasons);
}

bool AXObject::ShouldIgnoreForHiddenOrInert(
    IgnoredReasons* ignored_reasons) const {
  DUMP_WILL_BE_CHECK(!cached_values_need_update_)
      << "Tried to compute ignored value without up-to-date hidden/inert "
         "values on "
      << this;

  // All nodes must have an unignored parent within their tree under
  // the root node of the web area, so force that node to always be unignored.
  if (IsA<Document>(GetNode())) {
    return false;
  }

  if (cached_is_aria_hidden_) {
    if (ignored_reasons) {
      ComputeIsAriaHidden(ignored_reasons);
    }
    return true;
  }

  if (cached_is_inert_) {
    if (ignored_reasons) {
      ComputeIsInert(ignored_reasons);
    }
    return true;
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
      (!GetElement() || !GetElement()->HasDisplayContentsStyle()) &&
      !IsA<HTMLOptionElement>(GetNode())) {
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
bool AXObject::IsInert() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_inert_;
}

bool AXObject::ComputeIsInertViaStyle(const ComputedStyle* style,
                                      IgnoredReasons* ignored_reasons) const {
  if (IsAXInlineTextBox()) {
    return ParentObject()->IsInert()
               ? ParentObject()->ComputeIsInertViaStyle(style, ignored_reasons)
               : false;
  }
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
          if (AXObject* dialog_object = AXObjectCache().Get(dialog)) {
            ignored_reasons->push_back(
                IgnoredReason(kAXActiveModalDialog, dialog_object));
            return true;
          }
        } else if (Element* fullscreen =
                       Fullscreen::FullscreenElementFrom(document)) {
          if (AXObject* fullscreen_object = AXObjectCache().Get(fullscreen)) {
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

bool AXObject::IsAriaHiddenRoot() const {
  if (AXObjectCache().HasBadAriaHidden(*this)) {
    return false;
  }

  // aria-hidden:true works a bit like display:none.
  // * aria-hidden=true affects entire subtree.
  // * aria-hidden=false is a noop.
  if (!IsAriaAttributeTrue(html_names::kAriaHiddenAttr)) {
    return false;
  }

  auto* node = GetNode();

  // The aria-hidden attribute is not valid for the main html and body elements:
  // See more at https://github.com/w3c/aria/pull/1880.
  // Also ignored for <option> because it would unnecessarily complicate the
  // logic in the case where the option is selected, and aria-hidden does not
  // prevent selection of the option (it cannot because ARIA does not affect
  // behavior outside of assistive tech driven by a11y API).
  if (IsA<HTMLBodyElement>(node) || node == GetDocument()->documentElement() ||
      IsA<HTMLOptionElement>(node)) {
    AXObjectCache().DiscardBadAriaHiddenBecauseOfElement(*this);
    return false;
  }

  return true;
}

bool AXObject::IsAriaHidden() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_aria_hidden_;
}

bool AXObject::ComputeIsAriaHidden(IgnoredReasons* ignored_reasons) const {
  // The root node of a document or popup document cannot be aria-hidden:
  // - The root node of the main document cannot be hidden because there
  // is no element to place aria-hidden markup on.
  // - The root node of the popup document cannot be aria-hidden because it
  // seems like a bad idea to not allow access to it if it's actually there and
  // visible.
  if (IsA<Document>(GetNode())) {
    return false;
  }

  if (IsAriaHiddenRoot()) {
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
  if (AriaBooleanAttribute(html_names::kAriaModalAttr, &modal)) {
    return modal;
  }

  if (GetNode() && IsA<HTMLDialogElement>(*GetNode()))
    return To<Element>(GetNode())->IsInTopLayer();

  return false;
}

bool AXObject::IsBlockedByAriaModalDialog(
    IgnoredReasons* ignored_reasons) const {
  if (IsDetached()) {
    return false;
  }

  Element* active_aria_modal_dialog =
      AXObjectCache().GetActiveAriaModalDialog();

  // On platforms that don't require manual pruning of the accessibility tree,
  // the active aria modal dialog should never be set, so has no effect.
  if (!active_aria_modal_dialog) {
    return false;
  }

  if ((!GetNode() || GetNode()->IsPseudoElement()) && ParentObject()) {
    return ParentObject()->IsBlockedByAriaModalDialog();
  }

  if (FlatTreeTraversal::Contains(*active_aria_modal_dialog, *GetNode())) {
    return false;
  }

  if (ignored_reasons) {
    ignored_reasons->push_back(IgnoredReason(
        kAXAriaModalDialog, AXObjectCache().Get(active_aria_modal_dialog)));
  }
  return true;
}

bool AXObject::IsVisible() const {
  // TODO(accessibility) Consider exposing inert objects as visible, since they
  // are visible. It should be fine, since the objexcts are ignored.
  return !IsDetached() && !IsAriaHidden() && !IsInert() && !IsHiddenViaStyle();
}

const AXObject* AXObject::AriaHiddenRoot() const {
  return IsAriaHidden() ? FindAncestorWithAriaHidden(this) : nullptr;
}

const AXObject* AXObject::InertRoot() const {
  const AXObject* object = this;
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
      return AXObjectCache().Get(element);
    element = FlatTreeTraversal::ParentElement(*element);
  }

  return nullptr;
}

bool AXObject::IsDescendantOfDisabledNode() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_descendant_of_disabled_node_;
}

bool AXObject::ComputeIsDescendantOfDisabledNode() {
  if (IsA<Document>(GetNode()))
    return false;

  bool disabled = false;
  if (AriaBooleanAttribute(html_names::kAriaDisabledAttr, &disabled)) {
    return disabled;
  }

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

  // Nodes at which another tree has been stitched should always remain in the
  // tree so that browser code can traverse through them to the child tree.
  if (child_tree_id_) {
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

bool AXObject::ComputeIsIgnoredButIncludedInTree() {
  CHECK(!IsDetached());

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
      NOTREACHED();
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

  Node* dom_parent = NodeTraversal::Parent(*node);
  if (!dom_parent) {
    // No DOM parent, so will not be able to reach this node when cleaning uo
    // subtrees.
    return true;
  }

  if (dom_parent != LayoutTreeBuilderTraversal::Parent(*node)) {
    // If the flat tree parent from LayoutTreeBuilderTraversal is different than
    // the DOM parent, we must include this object in the tree so that we can
    // find it using cached children_. Otherwise, the object could be missed --
    // LayoutTreeBuilderTraversal and its cousin FlatTreeTraversal cannot
    // always be safely used, e.g. when slot assignments are pending.
    return true;
  }

  // Include children of <label> elements, for accname calculation purposes.
  // <span>s are ignored because they are considered uninteresting. Do not add
  // them back inside labels.
  if (IsA<HTMLLabelElement>(dom_parent) && !IsA<HTMLSpanElement>(node)) {
    return true;
  }

  // Always include the children of a map.
  if (IsA<HTMLMapElement>(dom_parent)) {
    return true;
  }

  // Necessary to calculate the accessible description of a ruby node.
  if (dom_parent->HasTagName(html_names::kRtTag)) {
    return true;
  }

  if (const Element* owner = node->OwnerShadowHost()) {
    // The ignored state of media controls can change without a layout update.
    // Keep them in the tree at all times so that the serializer isn't
    // accidentally working with unincluded nodes, which is not allowed.
    if (IsA<HTMLMediaElement>(owner)) {
      return true;
    }

    // Do not include ignored descendants of an <input type="search"> or
    // <input type="number"> because they interfere with AXPosition code that
    // assumes a plain input field structure. Specifically, due to the ignored
    // node at the end of textfield, end of editable text position will get
    // adjusted to past text field or caret moved events will not be emitted for
    // the final offset because the associated tree position. In some cases
    // platform accessibility code will instead incorrectly emit a caret moved
    // event for the AXPosition which follows the input.
    if (IsA<HTMLInputElement>(owner) &&
        (DynamicTo<HTMLInputElement>(owner)->FormControlType() ==
             FormControlType::kInputSearch ||
         DynamicTo<HTMLInputElement>(owner)->FormControlType() ==
             FormControlType::kInputNumber)) {
      return false;
    }
  }

  Element* element = GetElement();

  // Include all pseudo element content. Any anonymous subtree is included
  // from above, in the condition where there is no node.
  if (element && element->IsPseudoElement()) {
    return true;
  }

  // Include all parents of ::before/::after/::marker/::scroll-marker-group
  // pseudo elements to help ClearChildren() find all children, and assist
  // naming computation. It is unnecessary to include a rule for other types of
  // pseudo elements: Specifically, ::first-letter/::backdrop are not visited by
  // LayoutTreeBuilderTraversal, and cannot be in the tree, therefore do not add
  // a special rule to include their parents.
  if (element && (element->GetPseudoElement(kPseudoIdBefore) ||
                  element->GetPseudoElement(kPseudoIdAfter) ||
                  element->GetPseudoElement(kPseudoIdMarker) ||
                  element->GetPseudoElement(kPseudoIdScrollNextButton) ||
                  element->GetPseudoElement(kPseudoIdScrollPrevButton) ||
                  element->GetPseudoElement(kPseudoIdScrollMarkerGroupBefore) ||
                  element->GetPseudoElement(kPseudoIdScrollMarkerGroupAfter) ||
                  element->GetPseudoElement(kPseudoIdScrollMarker))) {
    return true;
  }

  if (IsUsedForLabelOrDescription()) {
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
      if (!IsA<HTMLDataListElement>(node) && !IsA<HTMLOptionElement>(node)) {
        return true;
      }
    } else {  // GetLayoutObject() != nullptr.
      // We identify hidden or collapsed nodes by their associated style values.
      if (IsHiddenViaStyle()) {
        return true;
      }

      // Allow the browser side ax tree to access "aria-hidden" nodes.
      // This is useful for APIs that return the node referenced by
      // aria-labeledby and aria-describedby.
      // Exception: iframes. Do not expose aria-hidden iframes, where
      // there is no possibility for the content within to know it's
      // aria-hidden, and therefore the entire iframe must be hidden from the
      // outer document.
      if (IsAriaHidden()) {
        return !IsEmbeddingElement();
      }
    }
  }

  if (IsExcludedByFormControlsFilter()) {
    return false;
  }

  if (!element)
    return false;

  // Include the <html> element in the accessibility tree, which will be
  // "ignored".
  if (IsA<HTMLHtmlElement>(element))
    return true;

  // Keep the internal accessibility tree consistent for videos which lack
  // a player and also inner text.
  if (RoleValue() == ax::mojom::blink::Role::kVideo ||
      RoleValue() == ax::mojom::blink::Role::kAudio) {
    return true;
  }

  // Expose menus even if hidden, enabling event generation as they open.
  if (RoleValue() == ax::mojom::blink::Role::kMenu) {
    return true;
  }

  // Always pass through Line Breaking objects, this is necessary to
  // detect paragraph edges, which are defined as hard-line breaks.
  if (IsLineBreakingObject() && IsVisible()) {
    return true;
  }

  // Ruby annotations (i.e. <rt> elements) need to be included because they are
  // used for calculating an accessible description for the ruby. We explicitly
  // exclude from the tree any <rp> elements, even though they also have the
  // kRubyAnnotation role, because such elements provide fallback content for
  // browsers that do not support ruby. Hence, their contents should not be
  // included in the accessible description, unless another condition in this
  // method decides to keep them in the tree for some reason.
  if (element->HasTagName(html_names::kRtTag)) {
    return true;
  }

  // Keep table-related elements in the tree, because it's too easy for them
  // to in and out of being ignored based on their ancestry, as their role
  // can depend on several levels up in the hierarchy.
  if (IsA<HTMLTableElement>(element) ||
      element->HasTagName(html_names::kTbodyTag) ||
      IsA<HTMLTableRowElement>(element) || IsA<HTMLTableCellElement>(element)) {
    return true;
  }

  // Preserve nodes with language attributes.
  if (HasAriaAttribute(html_names::kLangAttr)) {
    return true;
  }

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
  if (!shadow_root || shadow_root->GetMode() != ShadowRootMode::kUserAgent) {
    return nullptr;
  }
  auto* input = DynamicTo<HTMLInputElement>(&shadow_root->host());
  if (!input) {
    return nullptr;
  }
  FormControlType type = input->FormControlType();
  if (type != FormControlType::kInputDatetimeLocal &&
      type != FormControlType::kInputDate &&
      type != FormControlType::kInputTime &&
      type != FormControlType::kInputMonth &&
      type != FormControlType::kInputWeek) {
    return nullptr;
  }
  return AXObjectCache().Get(input);
}

ax::mojom::blink::Role AXObject::DetermineRoleValue() {
#if DCHECK_IS_ON()
  base::AutoReset<bool> reentrancy_protector(&is_computing_role_, true);
  DCHECK(!IsDetached());
  // Check parent object to work around circularity issues during AXObject::Init
  // (DetermineRoleValue is called there but before the parent is set).
  if (ParentObject()) {
    DCHECK(GetDocument());
    DCHECK(GetDocument()->Lifecycle().GetState() >=
           DocumentLifecycle::kLayoutClean)
        << "Unclean document at lifecycle "
        << GetDocument()->Lifecycle().ToString();
  }
#endif

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

bool AXObject::CanSetFocusAttribute() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_can_set_focus_attribute_;
}

// TODO(accessibility) Look at reusing Element::IsFocusable() or
// AXObject::IsKeyboardFocusable(). As long as we guard against style recalc by
// returning early if IsHiddenViaStyle() is true, we can call
// Element::IsKeyboardFocusable(), which would otherwise recalculate style at an
// awkward time.
bool AXObject::ComputeCanSetFocusAttribute() {
  DCHECK(!IsDetached());
  DCHECK(GetDocument());

  // Focusable: web area -- this is the only focusable non-element.
  if (IsWebArea()) {
    return true;
  }

  // NOT focusable: objects with no DOM node, e.g. extra layout blocks inserted
  // as filler, or objects where the node is not an element, such as a text
  // node or an HTML comment.
  Element* elem = GetElement();
  if (!elem)
    return false;

  if (cached_is_inert_) {
    return false;
  }

  // NOT focusable: child tree owners (it's the content area that will be marked
  // focusable in the a11y tree).
  if (IsEmbeddingElement()) {
    return false;
  }

  // NOT focusable: disabled form controls.
  if (IsDisabledFormControl(elem))
    return false;

  // Option elements do not receive DOM focus but they do receive a11y focus,
  // unless they are part of a <datalist>, in which case they can be displayed
  // by the browser process, but not the renderer.
  // TODO(crbug.com/1399852) Address gaps in datalist a11y.
  if (auto* option = DynamicTo<HTMLOptionElement>(elem)) {
    return !option->OwnerDataListElement();
  }

  // Invisible nodes are never focusable.
  // We already have these cached, so it's a very quick check.
  // This also prevents implementations of Element::SupportsFocus()
  // from trying to update style on descendants of content-visibility:hidden
  // nodes, or display:none nodes, which are the only nodes that don't have
  // updated style at this point.
  if (cached_is_hidden_via_style_) {
    return false;
  }

  // TODO(crbug.com/1489580) Investigate why this is not yet true, and the
  // early return is necessary, rather than just having a CHECK().
  // At this point, all nodes that are not display:none or
  // content-visibility:hidden should have updated style, which means it is safe
  // to call Element::SupportsFocus(), Element::IsKeyboardFocusable(), and
  // Element::IsFocusableStyle() without causing an update.
  // Updates are problematic when we are expecting the tree to be frozen,
  // or are in the middle of ProcessDeferredAccessibilityEvents(), where an
  // update would cause unwanted recursion.
  // Code that pvoes that this is impossible to reach is at:
  // AXObjectCacheImpl::CheckStyleIsComplete().
  if (elem->NeedsStyleRecalc()) {
    DCHECK(false) << "Avoiding IsFocusableStyle() crash for style update on:"
                  << "\n* Element: " << elem
                  << "\n* LayoutObject: " << elem->GetLayoutObject()
                  << "\n* NeedsStyleRecalc: " << elem->NeedsStyleRecalc()
                  << "\n* IsDisplayLockedPreventingPaint: "
                  << DisplayLockUtilities::IsDisplayLockedPreventingPaint(elem);
    return false;
  }

  // NOT focusable: hidden elements.
  if (!IsA<HTMLAreaElement>(elem) &&
      !elem->IsFocusableStyle(Element::UpdateBehavior::kNoneForAccessibility)) {
    return false;
  }

  // Customizable select: get focusable state from displayed button if present.
  if (auto* select = DynamicTo<HTMLSelectElement>(elem)) {
    if (auto* button = select->SlottedButton()) {
      elem = button;
    }
  }

  // We should not need style updates at this point.
  CHECK(!elem->NeedsStyleRecalc())
      << "\n* Element: " << elem << "\n* Object: " << this
      << "\n* LayoutObject: " << GetLayoutObject();

  // Focusable: element supports focus.
  return elem->SupportsFocus(Element::UpdateBehavior::kNoneForAccessibility) !=
         FocusableState::kNotFocusable;
}

bool AXObject::IsKeyboardFocusable() const {
  if (!CanSetFocusAttribute()) {
    return false;
  }

  Element& element = *GetElement();
  CHECK(!element.NeedsStyleRecalc())
      << "\n* Element: " << element << "\n* Object: " << this
      << "\n* LayoutObject: " << GetLayoutObject();

  if (element.IsKeyboardFocusable(
          Element::UpdateBehavior::kNoneForAccessibility)) {
    DCHECK(element.IsFocusable(Element::UpdateBehavior::kNoneForAccessibility));
    return true;
  }

  return false;
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
      // It's only a subwidget if it's in a grid or treegrid, not in a table.
      AncestorsIterator ancestor = base::ranges::find_if(
          UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
          &AXObject::IsTableLikeRole);
      return ancestor.current_ &&
             (ancestor.current_->RoleValue() == ax::mojom::blink::Role::kGrid ||
              ancestor.current_->RoleValue() ==
                  ax::mojom::blink::Role::kTreeGrid);
    }
    case ax::mojom::blink::Role::kGridCell:
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

std::string AXObject::GetProhibitedNameError(
    const String& prohibited_name,
    ax::mojom::blink::NameFrom& prohibited_name_from) const {
  std::ostringstream error;
  error << "An accessible name was placed on a prohibited role. This "
           "causes inconsistent behavior in screen readers. For example, "
           "<div aria-label=\"foo\"> is invalid as it is not accessible in "
           "every screen reader, because they expect only to read the inner "
           "contents of certain types of containers. Please add a valid role "
           "or put the name on a different object. As a repair technique, "
           "the browser will place the prohibited name in the accessible "
           "description field. To learn more, see the section 'Roles which "
           "cannot be named' in the ARIA specification at "
           "https://w3c.github.io/aria/#namefromprohibited."
        << "\nError details:" << "\n* Element: " << GetElement()
        << "\n* Role: " << InternalRoleName(RoleValue())
        << "\n* Prohibited name: " << prohibited_name
        << "\n* Name from = " << prohibited_name_from << "\n";
  return error.str();
}

// Simplify whitespace, but preserve a single leading and trailing whitespace
// character if it's present.
String AXObject::SimplifyName(const String& str,
                              ax::mojom::blink::NameFrom& name_from) const {
  if (str.empty())
    return "";  // Fast path / early return for many objects in tree.

  // Do not simplify name for text, unless it is pseudo content.
  // TODO(accessibility) There seems to be relatively little value for the
  // special pseudo content rule, and that the null check for node can
  // probably be removed without harm.
  if (GetNode() && ui::IsText(RoleValue()))
    return str;

  String simplified = str.SimplifyWhiteSpace(IsHTMLSpace<UChar>);

  if (GetElement() && !simplified.empty() && IsNameProhibited()) {
    // Enforce that names cannot occur on prohibited roles in Web UI.
    static bool name_on_prohibited_role_error_shown;
    if (!name_on_prohibited_role_error_shown) {
      name_on_prohibited_role_error_shown = true;  // Reduce console spam.
#if DCHECK_IS_ON()
      if (AXObjectCache().IsInternalUICheckerOn(*this)) {
        DCHECK(false)
            << "A prohibited accessible name was used in chromium web UI:\n"
            << GetProhibitedNameError(str, name_from)
            << "* URL: " << GetDocument()->Url()
            << "\n* Outer html: " << GetElement()->outerHTML()
            << "\n* AXObject ancestry:\n"
            << ParentChainToStringHelper(this);
      }
#endif
      if (RuntimeEnabledFeatures::AccessibilityProhibitedNamesEnabled()) {
        // Log error on dev tools console the first time.
        GetElement()->AddConsoleMessage(
            mojom::blink::ConsoleMessageSource::kRendering,
            mojom::blink::ConsoleMessageLevel::kError,
            String(GetProhibitedNameError(str, name_from)));
      }
    }

    if (RuntimeEnabledFeatures::AccessibilityProhibitedNamesEnabled()) {
      // Prohibited names are repaired by moving them to the description field,
      // where they will not override the contents of the element for screen
      // reader users. Exception: if it would be redundant with the inner
      // contents, then the name is stripped out rather than repaired.
      name_from = ax::mojom::blink::NameFrom::kProhibited;
      // If already redundant with inner text, do not repair to description
      if (name_from == ax::mojom::blink::NameFrom::kContents ||
          simplified ==
              GetElement()->GetInnerTextWithoutUpdate().StripWhiteSpace()) {
        name_from = ax::mojom::blink::NameFrom::kProhibitedAndRedundant;
      }
      return "";
    }
  }

  bool has_before_space = IsHTMLSpace<UChar>(str[0]);
  bool has_after_space = IsHTMLSpace<UChar>(str[str.length() - 1]);
  if (!has_before_space && !has_after_space) {
    return simplified;
  }

  // Preserve a trailing and/or leading space.
  StringBuilder result;
  if (has_before_space) {
    result.Append(' ');
  }
  result.Append(simplified);
  if (has_after_space) {
    result.Append(' ');
  }
  return result.ToString();
}

String AXObject::ComputedName(ax::mojom::blink::NameFrom* name_from_out) const {
  ax::mojom::blink::NameFrom name_from;
  AXObjectVector name_objects;
  return GetName(name_from_out ? *name_from_out : name_from, &name_objects);
}

bool AXObject::IsNameProhibited() const {
  if (CanSetFocusAttribute() &&
      !RuntimeEnabledFeatures::AccessibilityMinRoleTabbableEnabled()) {
    // Make an exception for focusable elements. ATs will likely read the name
    // of a focusable element even if it has the wrong role.
    // We do not need to do this if/when AccessibilityMinRoleTabbableEnabled
    // becomes enabled by default, because focusable objects will get a
    // minimum role of group, which can support an accessible name.
    // TODO(crbug.com/350528330): Test to see whether the following content
    // works in all screen readers we support, and if not, we should return true
    // here: <div tabindex="0" aria-label="Some label"></div>. Either way,
    // we should still disallow this pattern in Chromium Web UI.
    return false;
  }

  // Will not be in platform tree.
  if (IsIgnored()) {
    return false;
  }
  // This is temporary in order for web-ui to pass tests.
  // For example, <cr-expand-button> in cr_expand_button.ts expects authors
  // to supply the label by putting aria-label on the <cr-expand-button>
  // element, which it then copies to a second element inside the shadow root,
  // and resulting in two elements with the same aria-label. This could be
  // modified to use data-label instead of aria-label.
  // TODO(crbug.com/350528330):  Fix WebUI and remove this.
  if (GetElement() && GetElement()->IsCustomElement()) {
    return false;
  }

  // The ARIA specification disallows providing accessible names on certain
  // roles because doing so causes problems in screen readers.
  // Roles which probit names: https://w3c.github.io/aria/#namefromprohibited.
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kCaption:
    case ax::mojom::blink::Role::kCode:
    case ax::mojom::blink::Role::kContentDeletion:
    case ax::mojom::blink::Role::kContentInsertion:
    case ax::mojom::blink::Role::kDefinition:
    case ax::mojom::blink::Role::kEmphasis:
    case ax::mojom::blink::Role::kGenericContainer:
    case ax::mojom::blink::Role::kMark:
    case ax::mojom::blink::Role::kNone:
    case ax::mojom::blink::Role::kParagraph:
    case ax::mojom::blink::Role::kStrong:
    case ax::mojom::blink::Role::kSuggestion:
    case ax::mojom::blink::Role::kTerm:
      return true;
    default:
      return false;
  }
}

String AXObject::GetName(ax::mojom::blink::NameFrom& name_from,
                         AXObject::AXObjectVector* name_objects) const {
  HeapHashSet<Member<const AXObject>> visited;
  AXRelatedObjectVector related_objects;

  // Initialize |name_from|, as TextAlternative() might never set it in some
  // cases.
  name_from = ax::mojom::blink::NameFrom::kNone;
  String text = TextAlternative(
      /*recursive=*/false, /*aria_label_or_description_root=*/nullptr, visited,
      name_from, &related_objects, /*name_sources=*/nullptr);

  if (name_objects) {
    name_objects->clear();
    for (NameSourceRelatedObject* related_object : related_objects)
      name_objects->push_back(related_object->object);
  }

  return SimplifyName(text, name_from);
}

String AXObject::GetName(NameSources* name_sources) const {
  AXObjectSet visited;
  ax::mojom::blink::NameFrom name_from;
  AXRelatedObjectVector tmp_related_objects;
  String text = TextAlternative(false, nullptr, visited, name_from,
                                &tmp_related_objects, name_sources);

  return SimplifyName(text, name_from);
}

String AXObject::RecursiveTextAlternative(
    const AXObject& ax_obj,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited) {
  ax::mojom::blink::NameFrom tmp_name_from;
  return RecursiveTextAlternative(ax_obj, aria_label_or_description_root,
                                  visited, tmp_name_from);
}

String AXObject::RecursiveTextAlternative(
    const AXObject& ax_obj,
    const AXObject* aria_label_or_description_root,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from) {
  if (visited.Contains(&ax_obj) && !aria_label_or_description_root) {
    return String();
  }

  return ax_obj.TextAlternative(true, aria_label_or_description_root, visited,
                                name_from, nullptr, nullptr);
}

const ComputedStyle* AXObject::GetComputedStyle() const {
  if (IsAXInlineTextBox()) {
    return ParentObject()->GetComputedStyle();
  }
  Node* node = GetNode();
  if (!node) {
    return nullptr;
  }

#if DCHECK_IS_ON()
  DCHECK(GetDocument());
  DCHECK(GetDocument()->Lifecycle().GetState() >=
         DocumentLifecycle::kLayoutClean)
      << "Unclean document at lifecycle "
      << GetDocument()->Lifecycle().ToString();
#endif

  // content-visibility:hidden or content-visibility: auto.
  if (DisplayLockUtilities::IsDisplayLockedPreventingPaint(node)) {
    return nullptr;
  }

  // For elements with layout objects we can get their style directly.
  if (GetLayoutObject()) {
    return GetLayoutObject()->Style();
  }
  if (const Element* element = GetElement()) {
    return element->GetComputedStyle();
  }
  return nullptr;
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
bool AXObject::ComputeIsHiddenViaStyle(const ComputedStyle* style) {
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
    if (GetLayoutObject()) {
      return style->UsedVisibility() != EVisibility::kVisible;
    }
    // TODO(crbug.com/1286465): It's not consistent to only check
    // IsEnsuredInDisplayNone() on layoutless elements.
    return GetNode() && GetNode()->IsElementNode() &&
           (style->IsEnsuredInDisplayNone() ||
            style->UsedVisibility() != EVisibility::kVisible);
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

bool AXObject::IsHiddenViaStyle() {
  CheckCanAccessCachedValues();

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

  // HTML Options build their accessible name even when they are hidden in a
  // collapsed <select>.
  if (!IsAriaHidden() && AncestorMenuListOption()) {
    return false;
  }

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
  // the previous if block, so we are safe to hide any node with a valid
  // aria-hidden=true at this point.
  if (IsAriaHiddenRoot()) {
    return true;
  }

  return IsHiddenViaStyle();
}

bool AXObject::IsUsedForLabelOrDescription() {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_used_for_label_or_description_;
}

bool AXObject::ComputeIsUsedForLabelOrDescription() {
  if (GetElement()) {
    // Return true if a <label> or the target of a naming/description
    // relation (<aria-labelledby or aria-describedby).
    if (AXObjectCache().IsLabelOrDescription(*GetElement())) {
      return true;
    }
    // Also return true if a visible, focusable object that gets its name
    // from contents. Requires visibility because a hidden node can only partake
    // in a name or description in the relation case. Note: objects that are
    // visible and focused but aria-hidden can still compute their name from
    // contents as a repair.
    // Note: this must match the SupportsNameFromContents() rule in
    // AXRelationCache::UpdateRelatedText().
    if ((!IsIgnored() || CanSetFocusAttribute()) &&
        SupportsNameFromContents(/*recursive*/ false)) {
      // Descendants of nodes that label themselves via their inner contents
      // and are visible are effectively part of the label for that node.
      return true;
    }
  }

  if (RoleValue() == ax::mojom::blink::Role::kGroup) {
    // Groups do not contribute to ancestor names. There are other roles that
    // don't (listed in SupportsNameFromContents()), but it is not worth
    // the complexity to list each case. Group is relatively common, and
    // also prevents us from considering the popup document (which has kGroup)
    // from returning true.
    return false;
  }

  // Finally, return true if an ancetor is part of a label or description and
  // visibility hasn't changed from visible to hidden
  if (AXObject* parent = ParentObject()) {
    if (parent->IsUsedForLabelOrDescription()) {
      // The parent was part of a label or description. If this object is not
      // hidden, or the parent was also hidden, continue the label/description
      // state into the child.
      bool is_hidden = IsHiddenViaStyle() || IsAriaHidden();
      bool is_parent_hidden =
          parent->IsHiddenViaStyle() || parent->IsAriaHidden();
      if (!is_hidden || is_parent_hidden) {
        return true;
      }
      // Visibility has changed to hidden, where the parent was visible.
      // Iterate through the ancestors that are part of a label/description.
      // If any are part of a label/description relation, consider the hidden
      // node as also part of the label/description, because label and
      // descriptions computed from relations include hidden nodes.
      while (parent && parent->IsUsedForLabelOrDescription()) {
        if (AXObjectCache().IsLabelOrDescription(*parent->GetElement())) {
          return true;
        }
        parent = parent->ParentObject();
      }
    }
  }

  return false;
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
  if (IsHiddenForTextAlternativeCalculation(aria_label_or_description_root)) {
    *found_text_alternative = true;
    return String();
  }

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXObject::IsNameFromAriaAttribute, too.
  if (!aria_label_or_description_root && !already_visited) {
    name_from = ax::mojom::blink::NameFrom::kRelatedElement;

    // Check ARIA attributes.
    const QualifiedName& attr =
        HasAriaAttribute(html_names::kAriaLabeledbyAttr) &&
                !HasAriaAttribute(html_names::kAriaLabelledbyAttr)
            ? html_names::kAriaLabeledbyAttr
            : html_names::kAriaLabelledbyAttr;

    if (name_sources) {
      name_sources->push_back(NameSource(*found_text_alternative, attr));
      name_sources->back().type = name_from;
    }

    Element* element = GetElement();
    if (element) {
      HeapVector<Member<Element>> elements_from_attribute;
      ElementsFromAttribute(element, elements_from_attribute, attr);

      const AtomicString& aria_labelledby = AriaAttribute(attr);

      if (!aria_labelledby.IsNull()) {
        if (name_sources)
          name_sources->back().attribute_value = aria_labelledby;

        text_alternative = TextFromElements(
            true, visited, elements_from_attribute, related_objects);
        if (!text_alternative.ContainsOnlyWhitespaceOrEmpty()) {
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
  // If you change this logic, update AXObject::IsNameFromAriaAttribute, too.
  name_from = ax::mojom::blink::NameFrom::kAttribute;
  if (name_sources) {
    name_sources->push_back(
        NameSource(*found_text_alternative, html_names::kAriaLabelAttr));
    name_sources->back().type = name_from;
  }
  const AtomicString& aria_label = AriaAttribute(html_names::kAriaLabelAttr);
  if (!aria_label.GetString().ContainsOnlyWhitespaceOrEmpty()) {
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

#if EXPENSIVE_DCHECKS_ARE_ON()
void AXObject::CheckSubtreeIsForLabelOrDescription(const AXObject* obj) const {
  DCHECK(obj->IsUsedForLabelOrDescription())
      << "This object is being used for a label or description, but isn't "
         "flagged as such, which will cause problems for determining whether "
         "invisible nodes should be included in the tree."
      << obj;

  // Set of all children, whether in included or not.
  HeapHashSet<Member<AXObject>> children;

  // If the current object is included, check its children.
  if (obj->IsIncludedInTree()) {
    for (const auto& child : obj->ChildrenIncludingIgnored()) {
      children.insert(child);
    }
  }

  if (obj->GetNode()) {
    // Also check unincluded children.
    for (Node* child_node = NodeTraversal::FirstChild(*obj->GetNode());
         child_node; child_node = NodeTraversal::NextSibling(*child_node)) {
      // Get the child object that should be detached from this parent.
      // Do not invalidate from layout, because it may be unsafe to check layout
      // at this time. However, do allow invalidations if an object changes its
      // display locking (content-visibility: auto) status, as this may be the
      // only chance to do that, and it's safe to do now.
      AXObject* ax_child_from_node = obj->AXObjectCache().Get(child_node);
      if (ax_child_from_node && ax_child_from_node->ParentObject() == this) {
        children.insert(ax_child_from_node);
      }
    }
  }

  for (const auto& ax_child : children) {
    if (ax_child->SupportsNameFromContents(/*recursive*/ false)) {
      CheckSubtreeIsForLabelOrDescription(ax_child);
    }
  }
}
#endif

String AXObject::TextFromElements(
    bool in_aria_labelledby_traversal,
    AXObjectSet& visited,
    HeapVector<Member<Element>>& elements,
    AXRelatedObjectVector* related_objects) const {
  StringBuilder accumulated_text;
  bool found_valid_element = false;
  AXRelatedObjectVector local_related_objects;

  for (const auto& element : elements) {
    AXObject* ax_element = AXObjectCache().Get(element);
    if (ax_element) {
#if EXPENSIVE_DCHECKS_ARE_ON()
      CheckSubtreeIsForLabelOrDescription(ax_element);
#endif
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
bool AXObject::ElementsFromAttribute(Element* from,
                                     HeapVector<Member<Element>>& elements,
                                     const QualifiedName& attribute) {
  if (!from)
    return false;

  HeapVector<Member<Element>>* attr_associated_elements =
      from->GetAttrAssociatedElements(attribute,
                                      /*resolve_reference_target=*/true);
  if (!attr_associated_elements)
    return false;

  for (const auto& element : *attr_associated_elements)
    elements.push_back(element);

  return elements.size();
}

// static
bool AXObject::AriaLabelledbyElementVector(
    Element* from,
    HeapVector<Member<Element>>& elements) {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  if (ElementsFromAttribute(from, elements, html_names::kAriaLabelledbyAttr) &&
      elements.size() > 0) {
    return true;
  }

  return ElementsFromAttribute(from, elements,
                               html_names::kAriaLabeledbyAttr) &&
         elements.size() > 0;
}

// static
bool AXObject::IsNameFromAriaAttribute(Element* element) {
  if (!element)
    return false;

  HeapVector<Member<Element>> elements_from_attribute;
  if (AriaLabelledbyElementVector(element, elements_from_attribute)) {
    return true;
  }

  const AtomicString& aria_label =
      AriaAttribute(*element, html_names::kAriaLabelAttr);
  if (!aria_label.GetString().ContainsOnlyWhitespaceOrEmpty()) {
    return true;
  }

  return false;
}

bool AXObject::IsNameFromAuthorAttribute() const {
  return GetElement() &&
         (IsNameFromAriaAttribute(GetElement()) ||
          GetElement()->FastHasAttribute(html_names::kTitleAttr));
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

void AXObject::LoadInlineTextBoxesHelper() {}

AXObject* AXObject::NextOnLine() const {
  return nullptr;
}

AXObject* AXObject::PreviousOnLine() const {
  return nullptr;
}

std::optional<const DocumentMarker::MarkerType>
AXObject::GetAriaSpellingOrGrammarMarker() const {
  // TODO(accessibility) It looks like we are walking ancestors when we
  // shouldn't need to do that. At the most we should need get this via
  // GetClosestElement().
  AtomicString aria_invalid_value;
  const AncestorsIterator iter = std::find_if(
      UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
      [&aria_invalid_value](const AXObject& ancestor) {
        aria_invalid_value =
            ancestor.AriaTokenAttribute(html_names::kAriaInvalidAttr);
        return aria_invalid_value || ancestor.IsLineBreakingObject();
      });

  if (iter == UnignoredAncestorsEnd())
    return std::nullopt;
  if (EqualIgnoringASCIICase(aria_invalid_value, "spelling"))
    return DocumentMarker::kSpelling;
  if (EqualIgnoringASCIICase(aria_invalid_value, "grammar"))
    return DocumentMarker::kGrammar;
  return std::nullopt;
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
    case ax::mojom::blink::Role::kDisclosureTriangleGrouped:
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
    case ax::mojom::blink::Role::kDisclosureTriangleGrouped:
    case ax::mojom::blink::Role::kGridCell:
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
        AtomicString("ARIA-FLOWTO"),
        AtomicString("ARIA-GRABBED"),
        AtomicString("ARIA-KEYSHORTCUTS"),
        AtomicString("ARIA-LABEL"),
        AtomicString("ARIA-LABELEDBY"),
        AtomicString("ARIA-LABELLEDBY"),
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
      RawAriaRole() != ax::mojom::blink::Role::kUnknown) {
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
  DCHECK(IsIncludedInTree())
      << "IndexInParent is only valid when a node is included in the tree";
  AXObject* ax_parent_included = ParentObjectIncludedInTree();
  if (!ax_parent_included)
    return 0;

  const AXObjectVector& siblings =
      ax_parent_included->ChildrenIncludingIgnored();

  wtf_size_t index = siblings.Find(this);

  DCHECK_NE(index, kNotFound)
      << "Could not find child in parent:" << "\nChild: " << this
      << "\nParent: " << ax_parent_included
      << "  #children=" << siblings.size();
  return (index == kNotFound) ? 0 : static_cast<int>(index);
}

bool AXObject::IsOnlyChild() const {
  DCHECK(IsIncludedInTree())
      << "IsOnlyChild is only valid when a node is included in the tree";
  AXObject* ax_parent_included = ParentObjectIncludedInTree();
  if (!ax_parent_included) {
    return false;
  }

  return ax_parent_included->ChildrenIncludingIgnored().size() == 1;
}

bool AXObject::IsInMenuListSubtree() {
  CheckCanAccessCachedValues();
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_in_menu_list_subtree_;
}

bool AXObject::ComputeIsInMenuListSubtree() {
  if (IsRoot()) {
    return false;
  }
  return IsMenuList() || ParentObject()->IsInMenuListSubtree();
}

bool AXObject::IsMenuList() const {
  return RoleValue() == ax::mojom::blink::Role::kComboBoxSelect;
}

const AXObject* AXObject::AncestorMenuListOption() const {
  if (!IsInMenuListSubtree()) {
    return nullptr;
  }
  for (const AXObject* ax_option = this; ax_option;
       ax_option = ax_option->ParentObject()) {
    if (ax_option->RoleValue() == ax::mojom::blink::Role::kMenuListOption &&
        IsA<HTMLOptionElement>(ax_option->GetNode())) {
      return ax_option;
    }
  }

  return nullptr;
}

const AXObject* AXObject::AncestorMenuList() const {
  if (!IsInMenuListSubtree()) {
    return nullptr;
  }
  for (const AXObject* ax_menu_list = this; ax_menu_list;
       ax_menu_list = ax_menu_list->ParentObject()) {
    if (ax_menu_list->IsMenuList()) {
      DCHECK(IsA<HTMLSelectElement>(ax_menu_list->GetNode()));
      DCHECK(To<HTMLSelectElement>(ax_menu_list->GetNode())->UsesMenuList());
      DCHECK(!To<HTMLSelectElement>(ax_menu_list->GetNode())->IsMultiple());
      return ax_menu_list;
    }
  }
#if DCHECK_IS_ON()
  NOTREACHED() << "No menu list found:\n" << ParentChainToStringHelper(this);
#else
  NOTREACHED();
#endif
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
      AriaTokenAttribute(html_names::kAriaLiveAttr);
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
      AriaTokenAttribute(html_names::kAriaRelevantAttr);

  // Default aria-relevant = "additions text".
  if (relevant.empty())
    return default_live_region_relevant;

  return relevant;
}

bool AXObject::IsDisabled() const {
  // <embed> or <object> with unsupported plugin, or more iframes than allowed.
  if (IsEmbeddingElement()) {
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
  if (IsAriaAttributeTrue(html_names::kAriaDisabledAttr)) {
    return true;
  }

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
      AriaBooleanAttribute(html_names::kAriaReadonlyAttr, &is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kRestrictionReadOnly : kRestrictionNone;
  }

  // This is a node that is not readonly and not disabled.
  return kRestrictionNone;
}

ax::mojom::blink::Role AXObject::RawAriaRole() const {
  return ax::mojom::blink::Role::kUnknown;
}

ax::mojom::blink::Role AXObject::DetermineRawAriaRole() const {
  const AtomicString& aria_role = AriaAttribute(html_names::kRoleAttr);
  if (aria_role.empty()) {
    return ax::mojom::blink::Role::kUnknown;
  }
  return FirstValidRoleInRoleString(aria_role);
}

ax::mojom::blink::Role AXObject::DetermineAriaRole() const {
  ax::mojom::blink::Role role = DetermineRawAriaRole();

  if ((role == ax::mojom::blink::Role::kForm ||
       role == ax::mojom::blink::Role::kRegion) &&
      !IsNameFromAuthorAttribute() &&
      !HasAriaAttribute(html_names::kAriaRoledescriptionAttr)) {
    // If form or region is nameless, use a valid fallback role (if present
    // in the role attribute) or the native element's role (by returning
    // kUnknown). We only check aria-label/aria-labelledby because those are the
    // only allowed ways to name an ARIA role.
    // TODO(accessibility) The aria-roledescription logic is required, otherwise
    // ChromeVox will ignore the aria-roledescription. It only speaks the role
    // description on certain roles, and ignores it on the generic role.
    // See also https://github.com/w3c/aria/issues/1463.
    if (const AtomicString& role_str = AriaAttribute(html_names::kRoleAttr)) {
      return FirstValidRoleInRoleString(role_str,
                                        /*ignore_form_and_region*/ true);
    }
    return ax::mojom::blink::Role::kUnknown;
  }

  // ARIA states if an item can get focus, it should not be presentational.
  // It also states user agents should ignore the presentational role if
  // the element has global ARIA states and properties.
  if (ui::IsPresentational(role)) {
    if (IsFrame(GetNode()))
      return ax::mojom::blink::Role::kIframePresentational;
    if ((GetElement() && GetElement()->SupportsFocus(
                             Element::UpdateBehavior::kNoneForAccessibility) !=
                             FocusableState::kNotFocusable) ||
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
  // Note: make sure to exclude this aria role if the element is a <select>,
  // where this role would be redundant and may cause problems for screen
  // readers.
  if (role == ax::mojom::blink::Role::kComboBoxGrouping) {
    if (IsAtomicTextField()) {
      role = ax::mojom::blink::Role::kTextFieldWithComboBox;
    } else if (auto* select_element =
                   DynamicTo<HTMLSelectElement>(*GetNode())) {
      if (select_element->UsesMenuList() && !select_element->IsMultiple()) {
        // This is a select element. Don't set the aria role for it.
        role = ax::mojom::blink::Role::kUnknown;
      }
    } else if (ShouldUseComboboxMenuButtonRole()) {
      role = ax::mojom::blink::Role::kComboBoxMenuButton;
    }
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
  const Node* node = GetClosestNode();
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
  if (AriaBooleanAttribute(html_names::kAriaMultilineAttr, &is_multiline)) {
    return is_multiline;
  }

  return IsA<HTMLTextAreaElement>(*GetNode()) ||
         HasContentEditableAttributeSet();
}

bool AXObject::IsRichlyEditable() const {
  const Node* node = GetClosestNode();
  if (IsDetached() || !node)
    return false;

  return node->IsRichlyEditableForAccessibility();
}

AXObject* AXObject::LiveRegionRoot() {
  CheckCanAccessCachedValues();

  UpdateCachedAttributeValuesIfNeeded();
  return cached_live_region_root_;
}

bool AXObject::LiveRegionAtomic() const {
  bool atomic = false;
  if (AriaBooleanAttribute(html_names::kAriaAtomicAttr, &atomic)) {
    return atomic;
  }

  // ARIA roles "alert" and "status" should have an implicit aria-atomic value
  // of true.
  return RoleValue() == ax::mojom::blink::Role::kAlert ||
         RoleValue() == ax::mojom::blink::Role::kStatus;
}

const AtomicString& AXObject::ContainerLiveRegionStatus() const {
  return cached_live_region_root_ ? cached_live_region_root_->LiveRegionStatus()
                                  : g_null_atom;
}

const AtomicString& AXObject::ContainerLiveRegionRelevant() const {
  return cached_live_region_root_
             ? cached_live_region_root_->LiveRegionRelevant()
             : g_null_atom;
}

bool AXObject::ContainerLiveRegionAtomic() const {
  return cached_live_region_root_ &&
         cached_live_region_root_->LiveRegionAtomic();
}

bool AXObject::ContainerLiveRegionBusy() const {
  return cached_live_region_root_ &&
         cached_live_region_root_->IsAriaAttributeTrue(
             html_names::kAriaBusyAttr);
}

AXObject* AXObject::ElementAccessibilityHitTest(const gfx::Point& point) const {
  // Check if the validation message contains the point.
  PhysicalOffset physical_point(point);
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (child->IsValidationMessage() &&
        child->GetBoundsInFrameCoordinates().Contains(physical_point)) {
      return child->ElementAccessibilityHitTest(point);
    }
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
  return ChildrenIncludingIgnored()[index].Get();
}

const AXObject::AXObjectVector& AXObject::ChildrenIncludingIgnored() const {
  DCHECK(!IsDetached());
  return children_;
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

  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support finding the unignored children of "
           "objects excluded from the accessibility tree: "
        << this;
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
    if (child->IsIgnored()) {
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
  return ChildCountIncludingIgnored() ? ChildrenIncludingIgnored().front()
                                      : nullptr;
}

AXObject* AXObject::LastChildIncludingIgnored() const {
  DCHECK(!IsDetached());
  return ChildCountIncludingIgnored() ? ChildrenIncludingIgnored().back()
                                      : nullptr;
}

AXObject* AXObject::DeepestFirstChildIncludingIgnored() const {
  if (IsDetached()) {
    NOTREACHED_IN_MIGRATION();
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
    NOTREACHED_IN_MIGRATION();
    return nullptr;
  }
  if (!ChildCountIncludingIgnored())
    return nullptr;

  AXObject* deepest_child = LastChildIncludingIgnored();
  while (deepest_child->ChildCountIncludingIgnored())
    deepest_child = deepest_child->LastChildIncludingIgnored();

  return deepest_child;
}

AXObject* AXObject::NextSiblingIncludingIgnored() const {
  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
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
  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
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

AXObject* AXObject::CachedPreviousSiblingIncludingIgnored() const {
  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
    return nullptr;
  }

  const AXObject* included_parent = ParentObjectIncludedInTree();
  if (!included_parent) {
    return nullptr;
  }

  const AXObjectVector& siblings =
      included_parent->CachedChildrenIncludingIgnored();

  for (wtf_size_t count = 0; count < siblings.size(); count++) {
    if (siblings[count] == this) {
      return count > 0 ? siblings[count - 1] : nullptr;
    }
  }
  return nullptr;
}

AXObject* AXObject::NextInPreOrderIncludingIgnored(
    const AXObject* within) const {
  if (!IsIncludedInTree()) {
    // TODO(crbug.com/1421052): Make sure this no longer fires then turn the
    // above into CHECK(IsIncludedInTree());
    DUMP_WILL_BE_NOTREACHED()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
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
  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
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
  if (!IsIncludedInTree()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support iterating children of objects excluded "
           "from the accessibility tree: "
        << this;
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

AXObject* AXObject::FirstObjectWithRole(ax::mojom::blink::Role role) const {
  AXObject* object = const_cast<AXObject*>(this);
  for (; object && object->RoleValue() != role;
       object = object->NextInPreOrderIncludingIgnored()) {
  }
  return object;
}

int AXObject::UnignoredChildCount() const {
  return static_cast<int>(UnignoredChildren().size());
}

AXObject* AXObject::UnignoredChildAt(int index) const {
  const AXObjectVector unignored_children = UnignoredChildren();
  if (index < 0 || index >= static_cast<int>(unignored_children.size()))
    return nullptr;
  return unignored_children[index].Get();
}

AXObject* AXObject::UnignoredNextSibling() const {
  if (IsIgnored()) {
    // TODO(crbug.com/1407397): Make sure this no longer fires then turn this
    // block into CHECK(!IsIgnored());
    DUMP_WILL_BE_NOTREACHED()
        << "We don't support finding unignored siblings for ignored "
           "objects because it is not clear whether to search for the "
           "sibling in the unignored tree or in the whole tree: "
        << this;
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
      while (sibling && sibling->IsIgnored()) {
        sibling = sibling->NextInPreOrderIncludingIgnored(unignored_parent);
      }
      return sibling;
    }

    // If a sibling has not been found, try again with the parent object,
    // until the unignored parent is reached.
    current_obj = current_obj->ParentObjectIncludedInTree();
    if (!current_obj || !current_obj->IsIgnored())
      return nullptr;
  }
  return nullptr;
}

AXObject* AXObject::UnignoredPreviousSibling() const {
  if (IsIgnored()) {
    NOTREACHED_IN_MIGRATION()
        << "We don't support finding unignored siblings for ignored "
           "objects because it is not clear whether to search for the "
           "sibling in the unignored tree or in the whole tree: "
        << this;
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
      while (sibling && sibling->IsIgnored()) {
        sibling =
            sibling->PreviousInPostOrderIncludingIgnored(unignored_parent);
      }
      return sibling;
    }

    // If a sibling has not been found, try again with the parent object,
    // until the unignored parent is reached.
    current_obj = current_obj->ParentObjectIncludedInTree();
    if (!current_obj || !current_obj->IsIgnored())
      return nullptr;
  }
  return nullptr;
}

AXObject* AXObject::UnignoredNextInPreOrder() const {
  AXObject* next = NextInPreOrderIncludingIgnored();
  while (next && next->IsIgnored()) {
    next = next->NextInPreOrderIncludingIgnored();
  }
  return next;
}

AXObject* AXObject::UnignoredPreviousInPreOrder() const {
  AXObject* previous = PreviousInPreOrderIncludingIgnored();
  while (previous && previous->IsIgnored()) {
    previous = previous->PreviousInPreOrderIncludingIgnored();
  }
  return previous;
}

AXObject* AXObject::ParentObject() const {
  DUMP_WILL_BE_CHECK(!IsDetached());
  DUMP_WILL_BE_CHECK(!IsMissingParent()) << "Missing parent: " << this;

  return parent_;
}

AXObject* AXObject::ParentObject() {
  DUMP_WILL_BE_CHECK(!IsDetached());
  // Calling IsMissingParent can cause us to dereference pointers that
  // are null on detached objects, return early here to avoid crashing.
  // TODO(accessibility) Remove early return and change above assertion
  // to CHECK() once this no longer occurs.
  if (IsDetached()) {
    return nullptr;
  }
  DUMP_WILL_BE_CHECK(!IsMissingParent()) << "Missing parent: " << this;

  // TODO(crbug.com/337178753): this should not be necessary once subtree
  // removals can be immediate, complete and safe.
  if (IsMissingParent()) {
    AXObjectCache().RemoveSubtree(GetNode());
    return nullptr;
  }

  return parent_;
}

AXObject* AXObject::ParentObjectUnignored() const {
  AXObject* parent;
  for (parent = ParentObject(); parent && parent->IsIgnored();
       parent = parent->ParentObject()) {
  }

  return parent;
}

AXObject* AXObject::ParentObjectIncludedInTree() const {
  AXObject* parent;
  for (parent = ParentObject();
       parent && !parent->IsIncludedInTree();
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
      // It's possible to have a parent without a node here if the parent is a
      // pseudo element descendant. Since we're looking for the nearest element,
      // keep going up the ancestor chain until we find a parent that has one.
      element = parent->GetElement();
      if (element) {
        return element;
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
  DCHECK(GetDocument()) << this;
  DCHECK(GetDocument()->IsActive());
  DCHECK(!GetDocument()->IsDetached());
  DCHECK(GetDocument()->GetPage());
  DCHECK(GetDocument()->View());
  DCHECK(!AXObjectCache().HasBeenDisposed());
#endif

  if (!NeedsToUpdateChildren()) {
    CHECK(!child_cached_values_need_update_)
        << "This should only be set when also setting children_dirty_ to true: "
        << this;
    return;
  }

  if (AXObjectCache().IsFrozen()) {
    DUMP_WILL_BE_CHECK(!AXObjectCache().IsFrozen())
        << "Object should have already had its children updated in "
           "AXObjectCacheImpl::UpdateTreeIfNeeded(): "
        << this;
    return;
  }

  if (!CanHaveChildren()) {
    // Clear any children in case the node previously allowed children.
    ClearChildren();
    SetNeedsToUpdateChildren(false);
    child_cached_values_need_update_ = false;
    return;
  }

  UpdateCachedAttributeValuesIfNeeded();

  ClearChildren();
  AddChildren();
  CHECK(!children_dirty_);
  CHECK(!child_cached_values_need_update_);
}

bool AXObject::NeedsToUpdateChildren() const {
  return children_dirty_;
}

#if DCHECK_IS_ON()
void AXObject::CheckIncludedObjectConnectedToRoot() const {
  if (!IsIncludedInTree() || IsRoot()) {
    return;
  }

  const AXObject* included_child = this;
  const AXObject* ancestor = nullptr;
  const AXObject* included_parent = nullptr;
  for (ancestor = ParentObject(); ancestor;
       ancestor = ancestor->ParentObject()) {
    if (ancestor->IsIncludedInTree()) {
      included_parent = ancestor;
      if (included_parent->CachedChildrenIncludingIgnored().Find(
              included_child) == kNotFound) {
        if (AXObject* parent_for_repair = ComputeParent()) {
          parent_for_repair->CheckIncludedObjectConnectedToRoot();
        }

        NOTREACHED_IN_MIGRATION()
            << "Cannot find included child in parents children:\n"
            << "\n* Child: " << included_child
            << "\n* Parent:  " << included_parent << "\n--------------\n"
            << included_parent->GetAXTreeForThis();
      }
      if (included_parent->IsRoot()) {
        return;
      }
      included_child = included_parent;
    }
  }

  NOTREACHED_IN_MIGRATION()
      << "Did not find included parent path to root:"
      << "\n* Last found included parent: " << included_parent
      << "\n* Current object in tree: " << GetAXTreeForThis();
}
#endif

void AXObject::SetNeedsToUpdateChildren(bool update) {
  CHECK(AXObjectCache().lifecycle().StateAllowsAXObjectsToBeDirtied())
      << AXObjectCache();

  if (!update) {
    children_dirty_ = false;
    child_cached_values_need_update_ = false;
    return;
  }

#if defined(AX_FAIL_FAST_BUILD)
  SANITIZER_CHECK(!is_adding_children_)
      << "Should not invalidate children while adding them: " << this;
#endif

  if (children_dirty_) {
    return;
  }

  children_dirty_ = true;
  SetAncestorsHaveDirtyDescendants();
}

// static
bool AXObject::CanSafelyUseFlatTreeTraversalNow(Document& document) {
  return !document.IsFlatTreeTraversalForbidden() &&
         !document.GetSlotAssignmentEngine().HasPendingSlotAssignmentRecalc();
}

bool AXObject::ShouldDestroyWhenDetachingFromParent() const {
  // Do not interfere with the destruction loop in AXObjectCacheImpl::Dispose().
  if (IsDetached() || AXObjectCache().IsDisposing() ||
      AXObjectCache().IsDisposing()) {
    return false;
  }

  // Destroy all pseudo-elements that can't compute their parents, because we
  // are only able to re-attach them via top-down tree walk and not via
  // RepairMissingParent. See GetParentNodeForComputeParent for more
  // commentary.
  auto* layout_object = GetLayoutObject();
  if (layout_object) {
    Node* closest_node =
        AXObjectCacheImpl::GetClosestNodeForLayoutObject(layout_object);
    if (closest_node && closest_node->IsPseudoElement()) {
      return true;
    }
  }

  // Inline textbox children are dependent on their parent's ignored state.
  if (IsAXInlineTextBox()) {
    return true;
  }

  // Image map children are entirely dependent on the parent image.
  if (ParentObject() && IsA<HTMLImageElement>(ParentObject()->GetNode())) {
    return true;
  }

  return false;
}

void AXObject::DetachFromParent() {
  if (IsDetached()) {
    return;
  }

  CHECK(!AXObjectCache().IsFrozen())
      << "Do not detach parent while tree is frozen: " << this;
  if (ShouldDestroyWhenDetachingFromParent()) {
    if (GetNode()) {
      AXObjectCache().RemoveSubtree(GetNode());
    } else {
      // This is rare, but technically a pseudo element descendant can have a
      // subtree, and they do not have nodes.
      AXObjectCache().RemoveIncludedSubtree(this, /* remove_root */ true);
    }
  }
  parent_ = nullptr;
}

void AXObject::SetChildTree(const ui::AXTreeID& child_tree_id) {
  CHECK(!IsDetached());
  CHECK_GE(GetDocument()->Lifecycle().GetState(),
           DocumentLifecycle::kLayoutClean)
      << "Stitching a child tree is an action, and all actions should be "
         "performed when the layout is clean.";
  if (child_tree_id == ui::AXTreeIDUnknown() ||
      child_tree_id_ == child_tree_id) {
    return;
  }
  child_tree_id_ = child_tree_id;
  // A node with a child tree is automatically considered a leaf, and
  // CanHaveChildren() will return false for it.
  AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);
  AXObjectCache().RemoveSubtree(GetNode(), /*remove_root*/ false);
  AXObjectCache().UpdateAXForAllDocuments();
}

void AXObject::ClearChildren() {
  CHECK(!IsDetached());
  CHECK(!AXObjectCache().IsFrozen())
      << "Do not clear children while tree is frozen: " << this;

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
      << this;
  SANITIZER_CHECK(!is_computing_text_from_descendants_)
      << "Should not attempt to simultaneously compute text from descendants "
         "and clear children on: "
      << this;
#endif

  // Detach included children from their parent (this).
  for (const auto& child : children_) {
    // Check parent first, as the child might be several levels down if there
    // are unincluded nodes in between, in which case the cached parent will
    // also be a descendant (unlike children_, parent_ does not skip levels).
    // Another case where the parent is not the same is when the child has been
    // reparented using aria-owns.
    if (child->ParentObjectIfPresent() == this) {
      child->DetachFromParent();
    }
  }

  children_.clear();

  Node* node = GetNode();
  if (!node) {
    return;
  }

  // Detach unincluded children from their parent (this).
  // These are children that were not cleared from first loop, as well as
  // children that will be included once the parent next updates its children.
  for (Node* child_node = NodeTraversal::FirstChild(*node); child_node;
       child_node = NodeTraversal::NextSibling(*child_node)) {
    // Get the child object that should be detached from this parent.
    // Do not invalidate from layout, because it may be unsafe to check layout
    // at this time. However, do allow invalidations if an object changes its
    // display locking (content-visibility: auto) status, as this may be the
    // only chance to do that, and it's safe to do now.
    AXObject* ax_child_from_node = AXObjectCache().Get(child_node);
    if (ax_child_from_node &&
        ax_child_from_node->ParentObjectIfPresent() == this) {
      ax_child_from_node->DetachFromParent();
    }
  }

  // On clearing of children, ensure that our plugin serializer, if it exists,
  // is properly reset.
  if (IsA<HTMLEmbedElement>(node)) {
    AXObjectCache().ResetPluginTreeSerializer();
  }
}

void AXObject::ChildrenChangedWithCleanLayout() {
  DCHECK(!IsDetached()) << "Don't call on detached node: " << this;

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
  if (!IsIncludedInTree()) {
    if (AXObject* ax_parent = ParentObject()) {
      ax_parent->ChildrenChangedWithCleanLayout();
      return;
    }
  }

  // TODO(accessibility) Move this up.
  if (!CanHaveChildren()) {
    return;
  }

  DCHECK(!IsDetached()) << "None of the above should be able to detach |this|: "
                        << this;

  AXObjectCache().MarkAXObjectDirtyWithCleanLayout(this);
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

  return AXObjectCache().Get(global_root_scroller);
}

LocalFrameView* AXObject::DocumentFrameView() const {
  if (Document* document = GetDocument())
    return document->View();
  return nullptr;
}

AtomicString AXObject::Language() const {
  if (GetElement()) {
    const AtomicString& lang =
        GetElement()->FastGetAttribute(html_names::kLangAttr);
    if (!lang.empty()) {
      return lang;
    }
  }

  // Return early for non-root nodes. The root node's language can't be set by
  // the author so we need to determine its language below.
  if (!IsWebArea()) {
    return g_null_atom;
  }

  // Return the language of the <html> element if present.
  const Document* document = GetDocument();
  DCHECK(document);
  if (Element* html_element = document->documentElement()) {
    if (const AtomicString& html_lang =
            html_element->getAttribute(html_names::kLangAttr)) {
      return html_lang;
    }
  }

  // Fall back to the content language specified in the meta tag.
  // This is not part of what the HTML5 Standard suggests but it still
  // appears to be necessary.
  if (const String languages = document->ContentLanguage()) {
    String first_language = languages.Substring(0, languages.Find(","));
    if (!first_language.empty()) {
      return AtomicString(first_language.StripWhiteSpace());
    }
  }

  // Use the first accept language preference if present.
  if (Page* page = document->GetPage()) {
    const String languages = page->GetChromeClient().AcceptLanguages();
    String first_language = languages.Substring(0, languages.Find(","));
    if (!first_language.empty()) {
      return AtomicString(first_language.StripWhiteSpace());
    }
  }

  // As a last resort, return the default language of the browser's UI.
  AtomicString default_language = DefaultLanguage();
  return default_language;
}

//
// Scrollable containers.
//

bool AXObject::IsScrollableContainer() const {
  return !!GetScrollableAreaIfScrollable();
}

bool AXObject::IsUserScrollable() const {
  Node* node = GetNode();
  if (!node) {
    return false;
  }

  // The element that scrolls the document is not the document itself.
  if (node->IsDocumentNode()) {
    Document& document = node->GetDocument();
    return document.GetLayoutView()->IsUserScrollable();
  }

  LayoutBox* layout_box = DynamicTo<LayoutBox>(node->GetLayoutObject());
  if (!layout_box) {
    return false;
  }

  return layout_box->IsUserScrollable();
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
      NOTREACHED_IN_MIGRATION();
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

  // Note that this code is only triggered if this is not a LayoutTable,
  // i.e. it's an ARIA grid/table.
  //
  // TODO(dmazzoni): delete this code or rename it "for testing only"
  // since it's only needed for Blink web tests and not for production.
  unsigned row_index = 0;
  for (const auto& row : TableRowChildren()) {
    unsigned column_index = 0;
    for (const auto& cell : row->TableCellChildren()) {
      if (target_column_index == column_index && target_row_index == row_index)
        return cell.Get();
      column_index++;
    }
    row_index++;
  }

  return nullptr;
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
    container = AXObjectCache().Get(GetDocument());
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
  CHECK(!cached_values_need_update_ || !AXObjectCache().IsFrozen());
  UpdateCachedAttributeValuesIfNeeded();
  return cached_local_bounding_box_;
}

PhysicalRect AXObject::GetBoundsInFrameCoordinates() const {
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
  return PhysicalRect::FastAndLossyFromRectF(computed_bounds);
}

void AXObject::UpdateStyleAndLayoutTreeForNode(Node& node) {
  // In most cases, UpdateAllLifecyclePhasesExceptPaint() is enough, but if
  // the action is part of a display locked node, that will not update the node
  // because it's not part of the layout update cycle yet. In that case, calling
  // UpdateStyleAndLayoutTreeForElement() is also necessary.
  if (const Element* element =
          FlatTreeTraversal::InclusiveParentElement(node)) {
    element->GetDocument().UpdateStyleAndLayoutTreeForElement(
        element, DocumentUpdateReason::kAccessibility);
  }
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
    if (!node) {
      return false;
    }
  }

  UpdateStyleAndLayoutTreeForNode(*node);
  cache.UpdateAXForAllDocuments();

  // Updating style and layout for the node can cause it to gain layout,
  // detaching the original AXNodeObject to make room for a new one with layout.
  if (IsDetached()) {
    AXObject* new_object = cache.Get(node);
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
      return RequestSetValueAction(String::FromUTF8(action_data.value));
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
    case ax::mojom::blink::Action::kStitchChildTree:
      if (action_data.child_tree_id == ui::AXTreeIDUnknown()) {
        return false;  // No child tree ID provided.;
      }
      // This action can only be performed on elements, since only elements can
      // be parents of child trees. The closest example in HTML is an iframe,
      // but this action extends the same functionality to all HTML elements.
      if (!GetElement()) {
        return false;
      }
      SetChildTree(action_data.child_tree_id);
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
      return false;  // Handled in `RenderAccessibilityImpl`.
  }
}

// TODO(crbug.com/369945541): remove these unnecessary methods.
bool AXObject::RequestDecrementAction() {
  return OnNativeDecrementAction();
}

bool AXObject::RequestClickAction() {
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

  // Forward default action on custom select to its button.
  if (auto* select = DynamicTo<HTMLSelectElement>(GetNode())) {
    if (select->IsAppearanceBaseButton()) {
      if (auto* button = select->SlottedButton()) {
        element = button;
      }
    }
  }

  if (element) {
    // Always set the sequential focus navigation starting point.
    // Even if this element isn't focusable, if you press "Tab" it will
    // start the search from this element.
    GetDocument()->SetSequentialFocusNavigationStartingPoint(element);

    // Explicitly focus the element if it's focusable but not currently
    // the focused element, to be consistent with
    // EventHandler::HandleMousePressEvent.
    if (element->IsFocusable(Element::UpdateBehavior::kNoneForAccessibility) &&
        !element->IsFocusedElementInDocument()) {
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
  return OnNativeFocusAction();
}

bool AXObject::RequestIncrementAction() {
  return OnNativeIncrementAction();
}

bool AXObject::RequestScrollToGlobalPointAction(const gfx::Point& point) {
  return OnNativeScrollToGlobalPointAction(point);
}

bool AXObject::RequestScrollToMakeVisibleAction() {
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
    if (!node) {
      return false;
    }
  }

  UpdateStyleAndLayoutTreeForNode(*node);

  document->View()->UpdateAllLifecyclePhasesExceptPaint(
      DocumentUpdateReason::kAccessibility);

  // Updating style and layout for the node can cause it to gain layout,
  // detaching the original AXNodeObject to make room for a new one with layout.
  if (IsDetached()) {
    AXObject* new_object = cache.Get(node);
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
  if (!node || !node->isConnected()) {
    return nullptr;
  }

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
      NOTREACHED_IN_MIGRATION();
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
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::CenterIfNeeded(), ScrollAlignment::CenterIfNeeded(),
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(GetDocument(),
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
      scroll_into_view_util::CreateScrollIntoViewParams(
          horizontal_scroll_alignment, vertical_scroll_alignment,
          mojom::blink::ScrollType::kProgrammatic,
          false /* make_visible_in_visual_viewport */,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(GetDocument(),
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
      scroll_into_view_util::CreateScrollIntoViewParams(
          ScrollAlignment::LeftAlways(), ScrollAlignment::TopAlways(),
          mojom::blink::ScrollType::kProgrammatic, false,
          mojom::blink::ScrollBehavior::kAuto));
  AXObjectCache().PostNotification(GetDocument(),
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
      AriaAttribute(*element, html_names::kAriaOwnsAttr);

  // TODO(accessibility): do we need to check !AriaOwnsElements.empty() ? Is
  // that fundamentally different from HasExplicitlySetAttrAssociatedElements()?
  // And is an element even necessary in the case of virtual nodes?
  return !aria_owns.empty() || element->HasExplicitlySetAttrAssociatedElements(
                                   html_names::kAriaOwnsAttr);
}

// static
ax::mojom::blink::Role AXObject::FirstValidRoleInRoleString(
    const String& value,
    bool ignore_form_and_region) {
  DCHECK(!value.empty());

  static const ARIARoleMap* role_map = CreateARIARoleMap();

  Vector<String> role_vector;
  value.SimplifyWhiteSpace().Split(' ', role_vector);
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kUnknown;
  for (const auto& child : role_vector) {
    auto it = role_map->find(child);
    if (it == role_map->end() ||
        (ignore_form_and_region &&
         (it->value == ax::mojom::blink::Role::kForm ||
          it->value == ax::mojom::blink::Role::kRegion))) {
      continue;
    }
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
    case ax::mojom::blink::Role::kDocBackLink:
    case ax::mojom::blink::Role::kDocBiblioRef:
    case ax::mojom::blink::Role::kDocNoteRef:
    case ax::mojom::blink::Role::kDocGlossRef:
    case ax::mojom::blink::Role::kDisclosureTriangle:
    case ax::mojom::blink::Role::kDisclosureTriangleGrouped:
    case ax::mojom::blink::Role::kGridCell:
    case ax::mojom::blink::Role::kHeading:
    case ax::mojom::blink::Role::kLayoutTableCell:
    case ax::mojom::blink::Role::kLineBreak:
    case ax::mojom::blink::Role::kLink:
    case ax::mojom::blink::Role::kListBoxOption:
    case ax::mojom::blink::Role::kListMarker:
    case ax::mojom::blink::Role::kMath:
    case ax::mojom::blink::Role::kMenuItem:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kPopUpButton:
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

    case ax::mojom::blink::Role::kMenuListOption:
      // If only has one text child, will use HTMLOptionElement::DisplayLabel().
      result = !GetElement()->HasOneTextChild();
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
    case ax::mojom::blink::Role::kComboBoxSelect:
    case ax::mojom::blink::Role::kComment:
    case ax::mojom::blink::Role::kComplementary:
    case ax::mojom::blink::Role::kContentInfo:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kDateTime:
    case ax::mojom::blink::Role::kDialog:
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
    case ax::mojom::blink::Role::kRootWebArea:
    case ax::mojom::blink::Role::kRowGroup:
    case ax::mojom::blink::Role::kScrollBar:
    case ax::mojom::blink::Role::kScrollView:
    case ax::mojom::blink::Role::kSearch:
    case ax::mojom::blink::Role::kSearchBox:
    case ax::mojom::blink::Role::kSectionFooter:
    case ax::mojom::blink::Role::kSectionHeader:
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
    // ARIA spec says to compute "name from content" on role="row" at
    // https://w3c.github.io/aria/#row.
    // However, for performance reasons we only do it if the row is the
    // descendant of a grid/treegrid.
    case ax::mojom::blink::Role::kRow: {
      if (GetDocument() == AXObjectCache().GetPopupDocumentIfShowing()) {
        // role="row" is used in date pickers, but rows are not focusable
        // there and don't need a name. If we do decide to use focusable
        // rows in built-in HTML the name should be set manually, e.g. via
        // aria-label, as the name-from-contents algorithm often leads to
        // overly verbose names for rows.
        return false;
      }
      // Check for relevant ancestor.
      AXObject* ancestor = ParentObjectUnignored();
      while (ancestor) {
        // If in a grid/treegrid that's after a combobox textfield using
        // aria-activedescendant, then consider the row focusable.
        if (ancestor->RoleValue() == ax::mojom::blink::Role::kGrid ||
            ancestor->RoleValue() == ax::mojom::blink::Role::kTreeGrid) {
          return true;
        }
        if (ancestor->RoleValue() !=
                ax::mojom::blink::Role::kGenericContainer &&
            ancestor->RoleValue() != ax::mojom::blink::Role::kNone &&
            ancestor->RoleValue() != ax::mojom::blink::Role::kGroup &&
            ancestor->RoleValue() != ax::mojom::blink::Role::kRowGroup) {
          // Any other role other than those that are neutral in a [tree]grid,
          // indicate that we are not in a [tree]grid.
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
      if (IsA<HTMLBodyElement>(GetNode()) ||
          GetNode() == GetDocument()->documentElement()) {
        return false;
      }
      [[fallthrough]];
    case ax::mojom::blink::Role::kAbbr:
    case ax::mojom::blink::Role::kCanvas:
    case ax::mojom::blink::Role::kCaption:
    case ax::mojom::blink::Role::kCode:
    case ax::mojom::blink::Role::kContentDeletion:
    case ax::mojom::blink::Role::kContentInsertion:
    case ax::mojom::blink::Role::kDefinition:
    case ax::mojom::blink::Role::kDescriptionList:
    case ax::mojom::blink::Role::kDetails:
    case ax::mojom::blink::Role::kEmphasis:
    case ax::mojom::blink::Role::kFigcaption:
    case ax::mojom::blink::Role::kFooter:
    case ax::mojom::blink::Role::kInlineTextBox:
    case ax::mojom::blink::Role::kLabelText:
    case ax::mojom::blink::Role::kLayoutTable:
    case ax::mojom::blink::Role::kLayoutTableRow:
    case ax::mojom::blink::Role::kLegend:
    case ax::mojom::blink::Role::kList:
    case ax::mojom::blink::Role::kListItem:
    case ax::mojom::blink::Role::kMark:
    case ax::mojom::blink::Role::kNone:
    case ax::mojom::blink::Role::kParagraph:
    case ax::mojom::blink::Role::kRegion:
    case ax::mojom::blink::Role::kRuby:
    case ax::mojom::blink::Role::kSection:
    case ax::mojom::blink::Role::kSectionWithoutName:
    case ax::mojom::blink::Role::kStrong:
    case ax::mojom::blink::Role::kSubscript:
    case ax::mojom::blink::Role::kSuperscript:
    case ax::mojom::blink::Role::kTime:
      // Usually these items don't have a name, but Blink provides one if they
      // are tabbable, as a repair, so that if a user navigates to one, screen
      // reader users have enough context to understand where they landed.
      if (recursive) {
        // Use contents if part of a recursive name computation. This doesn't
        // affect the final serialized name for this object, but it allows it
        // to contribute to an ancestor name.
        result = true;
      } else if (!GetElement() || GetElement()->IsInUserAgentShadowRoot()) {
        // Built-in UI must have correct accessibility without needing repairs.
        result = false;
      } else if (IsEditable() || GetAOMPropertyOrARIAAttribute(
                                     AOMRelationProperty::kActiveDescendant)) {
        // Handle exceptions:
        // 1.Elements with contenteditable, where using the contents as a name
        //   would cause them to be double-announced.
        // 2.Containers with aria-activedescendant, where the focus is being
        //   forwarded somewhere else.
        result = false;
      } else if (!CanSetFocusAttribute()) {
        // This check is added for performance reasons, as this value is cached.
        result = false;
      } else {
        // Don't repair name from contents to focusable elements unless
        // tabbable or focused, because providing a repaired accessible name
        // often leads to redundant verbalizations.
        result = GetDocument()->FocusedElement() == GetElement() ||
                 GetElement()->IsKeyboardFocusable(
                     Element::UpdateBehavior::kNoneForAccessibility);
#if DCHECK_IS_ON()
        // TODO(crbug.com/350528330): Add this check and address focusable
        // UI elements that are missing a role, or using an improper role.
        // DCHECK(!result || !AXObjectCache().IsInternalUICheckerOn(*this))
        //     << "A focusable node lacked proper accessibility markup, "
        //        "causing a repair situation:"
        //     << "\n* Is name prohibited: " << IsNameProhibited()
        //     << "\n* Role: " << RoleValue()
        //     << "\n* URL: " << GetDocument()->Url()
        //     << "\n* Outer html: " << GetElement()->outerHTML()
        //     << "\n* AXObject ancestry:\n"
        //     << ParentChainToStringHelper(this);
#endif
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

    case ax::mojom::blink::Role::kCaret:
    case ax::mojom::blink::Role::kClient:
    case ax::mojom::blink::Role::kColumn:
    case ax::mojom::blink::Role::kDescriptionListTermDeprecated:
    case ax::mojom::blink::Role::kDesktop:
    case ax::mojom::blink::Role::kDescriptionListDetailDeprecated:
    case ax::mojom::blink::Role::kDirectoryDeprecated:
    case ax::mojom::blink::Role::kKeyboard:
    case ax::mojom::blink::Role::kImeCandidate:
    case ax::mojom::blink::Role::kListGrid:
    case ax::mojom::blink::Role::kPane:
    case ax::mojom::blink::Role::kPdfActionableHighlight:
    case ax::mojom::blink::Role::kPdfRoot:
    case ax::mojom::blink::Role::kPreDeprecated:
    case ax::mojom::blink::Role::kPortalDeprecated:
    case ax::mojom::blink::Role::kTableHeaderContainer:
    case ax::mojom::blink::Role::kTitleBar:
    case ax::mojom::blink::Role::kUnknown:
    case ax::mojom::blink::Role::kWebView:
    case ax::mojom::blink::Role::kWindow:
      NOTREACHED() << "Role shouldn't occur in Blink: " << this;
  }

  return result;
}

bool AXObject::SupportsARIAReadOnly() const {
  // Ignore the readonly state if the element is set to contenteditable and
  // aria-readonly="true" according to the HTML-AAM specification.
  if (HasContentEditableAttributeSet()) {
    return false;
  }

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
  if (AriaTokenAttribute(html_names::kAriaPressedAttr)) {
    return ax::mojom::blink::Role::kToggleButton;
  }

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
const AtomicString& AXObject::AriaRoleName(ax::mojom::blink::Role role) {
  static const Vector<AtomicString>* aria_role_name_vector =
      CreateAriaRoleNameVector();

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
  if (const auto& role_name = AriaRoleName(role)) {
    return role_name.GetString();
  }

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

// Extra checks that only occur during serialization.
void AXObject::PreSerializationConsistencyCheck() const{
  CHECK(!IsDetached()) << "Do not serialize detached nodes: " << this;
  CHECK(AXObjectCache().IsFrozen());
  CHECK(!NeedsToUpdateCachedValues()) << "Stale values on: " << this;
  CHECK(!IsMissingParent());
  if (!IsIncludedInTree()) {
    AXObject* included_parent = ParentObjectIncludedInTree();
    // TODO(accessibility): Return to CHECK once it has been resolved,
    // so that the message does not bloat stable releases.
    DUMP_WILL_BE_NOTREACHED() << "Do not serialize unincluded nodes: " << this
                              << "\nIncluded parent: " << included_parent;
  }
#if defined(AX_FAIL_FAST_BUILD)
  // A bit more expensive, so only check in builds used for testing.
  CHECK_EQ(IsAriaHidden(), !!FindAncestorWithAriaHidden(this))
      << "IsAriaHidden() doesn't match existence of an aria-hidden ancestor: "
      << this;
#endif
}

String AXObject::ToString(bool verbose) const {
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

  bool cached_values_only = !AXObjectCache().IsFrozen();

  if (AXObjectCache().HasBeenDisposed() || AXObjectCache().IsDisposing()) {
    return string_builder + " (doc shutdown) #" + String::Number(AXObjectID());
  }

  if (verbose) {
    string_builder = string_builder + " axid#" + String::Number(AXObjectID());
    // The following can be useful for debugging locally when determining if
    // two objects with the same AXID were the same instance.
    // std::ostringstream pointer_str;
    // pointer_str << " hex:" << std::hex << reinterpret_cast<uintptr_t>(this);
    // string_builder = string_builder + String(pointer_str.str());

    // Add useful HTML element info, like <div.myClass#myId>.
    if (GetNode()) {
      string_builder = string_builder + " " + GetNodeString(GetNode());
      if (IsRoot()) {
        string_builder = string_builder + " isRoot";
      }
      if (GetDocument()) {
        if (GetDocument()->GetFrame() &&
            GetDocument()->GetFrame()->PagePopupOwner()) {
          string_builder = string_builder + " inPopup";
        }
      } else {
        string_builder = string_builder + " missingDocument";
      }

      if (!GetNode()->isConnected()) {
        // TODO(accessibility) Do we have a handy helper for determining whether
        // a node is still in the flat tree? That would be useful to log.
        string_builder = string_builder + " nodeDisconnected";
      }
    }

    if (NeedsToUpdateCachedValues()) {
      string_builder = string_builder + " needsToUpdateCachedValues";
      if (AXObjectCache().IsFrozen()) {
        cached_values_only = true;
        string_builder = string_builder + "/disallowed";
      }
    }
    if (child_cached_values_need_update_) {
      string_builder = string_builder + " childCachedValuesNeedUpdate";
    }
    if (!GetDocument()) {
      string_builder = string_builder + " missingDocument";
    } else if (!GetDocument()->GetFrame()) {
      string_builder = string_builder + " closedDocument";
    }

    // Add properties of interest that often contribute to errors:
    if (HasARIAOwns(GetElement())) {
      string_builder = string_builder +
                       " aria-owns=" + AriaAttribute(html_names::kAriaOwnsAttr);
    }

    if (GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant)) {
      string_builder = string_builder + " aria-activedescendant=" +
                       AriaAttribute(html_names::kAriaActivedescendantAttr);
    }
    if (IsFocused())
      string_builder = string_builder + " focused";
    if (cached_values_only ? cached_can_set_focus_attribute_
                           : CanSetFocusAttribute()) {
      string_builder = string_builder + " focusable";
    }
    if (!IsDetached() && AXObjectCache().IsAriaOwned(this, /*checks*/ false)) {
      string_builder = string_builder + " isAriaOwned";
    }
    if (IsIgnored()) {
      string_builder = string_builder + " isIgnored";
#if defined(AX_FAIL_FAST_BUILD)
      // TODO(accessibility) Move this out of AX_FAIL_FAST_BUILD by having a new
      // ax_enum, and a ToString() in ax_enum_utils, as well as move out of
      // String IgnoredReasonName(AXIgnoredReason reason) in
      // inspector_type_builder_helper.cc.
      if (!cached_values_only && !IsDetached()) {
        AXObject::IgnoredReasons reasons;
        ComputeIsIgnored(&reasons);
        string_builder = string_builder + GetIgnoredReasonsDebugString(reasons);
      }
#endif
      if (!IsIncludedInTree()) {
        string_builder = string_builder + " isRemovedFromTree";
      }
    }
    if (GetNode()) {
      if (GetNode()->OwnerShadowHost()) {
        string_builder = string_builder + (GetNode()->IsInUserAgentShadowRoot()
                                               ? " inUserAgentShadowRoot:"
                                               : " inShadowRoot:");
        string_builder =
            string_builder + GetNodeString(GetNode()->OwnerShadowHost());
      }
      if (GetNode()->GetShadowRoot()) {
        string_builder = string_builder + " hasShadowRoot";
      }

      if (GetDocument() && CanSafelyUseFlatTreeTraversalNow(*GetDocument()) &&
          DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
              *GetNode(), DisplayLockActivationReason::kAccessibility)) {
        string_builder = string_builder + " isDisplayLocked";
      }
    }
    if (cached_values_only ? !!cached_live_region_root_ : !!LiveRegionRoot()) {
      string_builder = string_builder + " inLiveRegion";
    }

    if (cached_values_only ? cached_is_in_menu_list_subtree_
                           : IsInMenuListSubtree()) {
      string_builder = string_builder + " inMenuList";
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
    if (children_dirty_) {
      string_builder = string_builder + " needsToUpdateChildren";
    }
    if (!children_.empty()) {
      string_builder = string_builder + " #children=";
      string_builder = string_builder + String::Number(children_.size());
    }
    if (HasDirtyDescendants()) {
      string_builder = string_builder + " hasDirtyDescendants";
    }
    const AXObject* included_parent = parent_;
    while (included_parent &&
           !included_parent->IsIncludedInTree()) {
      included_parent = included_parent->ParentObjectIfPresent();
    }
    if (included_parent) {
      if (!included_parent->HasDirtyDescendants() && children_dirty_) {
        string_builder =
            string_builder + " includedParentMissingHasDirtyDescendants";
      }
      if (IsIncludedInTree()) {
        // All cached children must be included.
        const HeapVector<Member<AXObject>>& siblings =
            included_parent->CachedChildrenIncludingIgnored();
        if (!siblings.Contains(this)) {
          string_builder = string_builder + " missingFromParentsChildren";
        }
      }
    } else if (!IsRoot()) {
      if (!parent_) {
        string_builder = string_builder + " isMissingParent";
      } else if (parent_->IsDetached()) {
        string_builder = string_builder + " detachedParent";
      }
    }
    if (!cached_values_only && !CanHaveChildren()) {
      string_builder = string_builder + " cannotHaveChildren";
    }
    if (!GetLayoutObject() && !IsAXInlineTextBox()) {
      string_builder = string_builder + " missingLayout";
    }

    if (cached_values_only ? cached_is_used_for_label_or_description_
                           : IsUsedForLabelOrDescription()) {
      string_builder = string_builder + " inLabelOrDesc";
    }

    if (!cached_values_only) {
      ax::mojom::blink::NameFrom name_from;
      String name = ComputedName(&name_from);
      std::ostringstream name_from_str;
      name_from_str << name_from;
      return string_builder + " nameFrom=" + String(name_from_str.str()) +
             " name=" + name;
    }
  } else {
    string_builder = string_builder + ": ";
  }

  // Append name last, in case it is long.
  if (!cached_values_only || !verbose)
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
    return stream << obj->ToString().Utf8();
  else
    return stream << "<AXObject nullptr>";
}

std::ostream& operator<<(std::ostream& stream, const AXObject& obj) {
  return stream << obj.ToString().Utf8();
}

void AXObject::Trace(Visitor* visitor) const {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(cached_live_region_root_);
  visitor->Trace(ax_object_cache_);
}

}  // namespace blink
