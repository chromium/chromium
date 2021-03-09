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
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "third_party/blink/public/common/input/web_menu_source_type.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-blink.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/dom/focus_params.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/canvas/html_canvas_element.h"
#include "third_party/blink/renderer/core/html/custom/element_internals.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/html_head_element.h"
#include "third_party/blink/renderer/core/html/html_script_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html/html_table_cell_element.h"
#include "third_party/blink/renderer/core/html/html_table_element.h"
#include "third_party/blink/renderer/core/html/html_table_row_element.h"
#include "third_party/blink/renderer/core/html/html_table_section_element.h"
#include "third_party/blink/renderer/core/html/html_title_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/top_document_root_scroller_controller.h"
#include "third_party/blink/renderer/core/svg/svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_g_element.h"
#include "third_party/blink/renderer/core/svg/svg_style_element.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_option.h"
#include "third_party/blink/renderer/modules/accessibility/ax_menu_list_popup.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_selection.h"
#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"
#include "third_party/skia/include/core/SkMatrix44.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_common.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_role_properties.h"

namespace blink {

namespace {

#if defined(AX_FAIL_FAST_BUILD)
// TODO(accessibility) Move this out of DEBUG by having a new enum in
// ax_enums.mojom, and a matching ToString() in ax_enum_utils, as well as move
// out duplicate code of String IgnoredReasonName(AXIgnoredReason reason) in
// inspector_type_builder_helper.cc.
String IgnoredReasonName(AXIgnoredReason reason) {
  switch (reason) {
    case kAXActiveModalDialog:
      return "activeModalDialog";
    case kAXAriaModalDialog:
      return "activeAriaModalDialog";
    case kAXAncestorIsLeafNode:
      return "ancestorIsLeafNode";
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
    case kAXInheritsPresentation:
      return "inheritsPresentation";
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

String GetElementString(Element* element) {
  if (!element)
    return "<null>";

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

Node* GetParentNodeForComputeParent(Node* node) {
  if (!node)
    return nullptr;

  // Prefer LayoutTreeBuilderTraversal::Parent(), which handles pseudo content.
  Node* parent = LayoutTreeBuilderTraversal::Parent(*node);
  if (parent)
    return parent;

  // Unfortunately, LayoutTreeBuilderTraversal::Parent() can return nullptr for
  // a text node, such as inside a text area. Fall back on DOM parentNode().
  return node->parentNode();
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
    case ax::mojom::blink::Role::kIgnored:
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

struct RoleHashTraits : HashTraits<ax::mojom::blink::Role> {
  static const bool kEmptyValueIsZero = true;
  static ax::mojom::blink::Role EmptyValue() {
    return ax::mojom::blink::Role::kUnknown;
  }
};

using ARIARoleMap = HashMap<String,
                            ax::mojom::blink::Role,
                            CaseFoldingHash,
                            HashTraits<String>,
                            RoleHashTraits>;

struct RoleEntry {
  const char* aria_role;
  ax::mojom::blink::Role webcore_role;
};

// Mapping of ARIA role name to internal role name.
const RoleEntry kRoles[] = {
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
    {"none", ax::mojom::blink::Role::kNone},
    {"note", ax::mojom::blink::Role::kNote},
    {"option", ax::mojom::blink::Role::kListBoxOption},
    {"paragraph", ax::mojom::blink::Role::kParagraph},
    {"presentation", ax::mojom::blink::Role::kPresentational},
    {"progressbar", ax::mojom::blink::Role::kProgressIndicator},
    {"radio", ax::mojom::blink::Role::kRadioButton},
    {"radiogroup", ax::mojom::blink::Role::kRadioGroup},
    // TODO(accessibility) region should only be mapped
    // if name present. See http://crbug.com/840819.
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
    {"suggestion", ax::mojom::blink::Role::kSuggestion},
    {"switch", ax::mojom::blink::Role::kSwitch},
    {"tab", ax::mojom::blink::Role::kTab},
    {"table", ax::mojom::blink::Role::kTable},
    {"tablist", ax::mojom::blink::Role::kTabList},
    {"tabpanel", ax::mojom::blink::Role::kTabPanel},
    {"term", ax::mojom::blink::Role::kTerm},
    {"text", ax::mojom::blink::Role::kStaticText},
    {"textbox", ax::mojom::blink::Role::kTextField},
    {"time", ax::mojom::blink::Role::kTime},
    {"timer", ax::mojom::blink::Role::kTimer},
    {"toolbar", ax::mojom::blink::Role::kToolbar},
    {"tooltip", ax::mojom::blink::Role::kTooltip},
    {"tree", ax::mojom::blink::Role::kTree},
    {"treegrid", ax::mojom::blink::Role::kTreeGrid},
    {"treeitem", ax::mojom::blink::Role::kTreeItem}};

struct InternalRoleEntry {
  ax::mojom::blink::Role webcore_role;
  const char* internal_role_name;
};

const InternalRoleEntry kInternalRoles[] = {
    {ax::mojom::blink::Role::kNone, "None"},
    {ax::mojom::blink::Role::kAbbr, "Abbr"},
    {ax::mojom::blink::Role::kAlertDialog, "AlertDialog"},
    {ax::mojom::blink::Role::kAlert, "Alert"},
    {ax::mojom::blink::Role::kAnchor, "Anchor"},
    {ax::mojom::blink::Role::kComment, "Comment"},
    {ax::mojom::blink::Role::kApplication, "Application"},
    {ax::mojom::blink::Role::kArticle, "Article"},
    {ax::mojom::blink::Role::kAudio, "Audio"},
    {ax::mojom::blink::Role::kBanner, "Banner"},
    {ax::mojom::blink::Role::kBlockquote, "Blockquote"},
    {ax::mojom::blink::Role::kButton, "Button"},
    {ax::mojom::blink::Role::kCanvas, "Canvas"},
    {ax::mojom::blink::Role::kCaption, "Caption"},
    {ax::mojom::blink::Role::kCaret, "Caret"},
    {ax::mojom::blink::Role::kCell, "Cell"},
    {ax::mojom::blink::Role::kCheckBox, "CheckBox"},
    {ax::mojom::blink::Role::kClient, "Client"},
    {ax::mojom::blink::Role::kCode, "Code"},
    {ax::mojom::blink::Role::kColorWell, "ColorWell"},
    {ax::mojom::blink::Role::kColumnHeader, "ColumnHeader"},
    {ax::mojom::blink::Role::kColumn, "Column"},
    {ax::mojom::blink::Role::kComboBoxGrouping, "ComboBox"},
    {ax::mojom::blink::Role::kComboBoxMenuButton, "ComboBox"},
    {ax::mojom::blink::Role::kComplementary, "Complementary"},
    {ax::mojom::blink::Role::kContentDeletion, "ContentDeletion"},
    {ax::mojom::blink::Role::kContentInsertion, "ContentInsertion"},
    {ax::mojom::blink::Role::kContentInfo, "ContentInfo"},
    {ax::mojom::blink::Role::kDate, "Date"},
    {ax::mojom::blink::Role::kDateTime, "DateTime"},
    {ax::mojom::blink::Role::kDefinition, "Definition"},
    {ax::mojom::blink::Role::kDescriptionListDetail, "DescriptionListDetail"},
    {ax::mojom::blink::Role::kDescriptionList, "DescriptionList"},
    {ax::mojom::blink::Role::kDescriptionListTerm, "DescriptionListTerm"},
    {ax::mojom::blink::Role::kDesktop, "Desktop"},
    {ax::mojom::blink::Role::kDetails, "Details"},
    {ax::mojom::blink::Role::kDialog, "Dialog"},
    {ax::mojom::blink::Role::kDirectory, "Directory"},
    {ax::mojom::blink::Role::kDisclosureTriangle, "DisclosureTriangle"},
    // --------------------------------------------------------------
    // DPub Roles:
    // https://www.w3.org/TR/dpub-aam-1.0/#mapping_role_table
    {ax::mojom::blink::Role::kDocAbstract, "DocAbstract"},
    {ax::mojom::blink::Role::kDocAcknowledgments, "DocAcknowledgments"},
    {ax::mojom::blink::Role::kDocAfterword, "DocAfterword"},
    {ax::mojom::blink::Role::kDocAppendix, "DocAppendix"},
    {ax::mojom::blink::Role::kDocBackLink, "DocBackLink"},
    {ax::mojom::blink::Role::kDocBiblioEntry, "DocBiblioentry"},
    {ax::mojom::blink::Role::kDocBibliography, "DocBibliography"},
    {ax::mojom::blink::Role::kDocBiblioRef, "DocBiblioref"},
    {ax::mojom::blink::Role::kDocChapter, "DocChapter"},
    {ax::mojom::blink::Role::kDocColophon, "DocColophon"},
    {ax::mojom::blink::Role::kDocConclusion, "DocConclusion"},
    {ax::mojom::blink::Role::kDocCover, "DocCover"},
    {ax::mojom::blink::Role::kDocCredit, "DocCredit"},
    {ax::mojom::blink::Role::kDocCredits, "DocCredits"},
    {ax::mojom::blink::Role::kDocDedication, "DocDedication"},
    {ax::mojom::blink::Role::kDocEndnote, "DocEndnote"},
    {ax::mojom::blink::Role::kDocEndnotes, "DocEndnotes"},
    {ax::mojom::blink::Role::kDocEpigraph, "DocEpigraph"},
    {ax::mojom::blink::Role::kDocEpilogue, "DocEpilogue"},
    {ax::mojom::blink::Role::kDocErrata, "DocErrata"},
    {ax::mojom::blink::Role::kDocExample, "DocExample"},
    {ax::mojom::blink::Role::kDocFootnote, "DocFootnote"},
    {ax::mojom::blink::Role::kDocForeword, "DocForeword"},
    {ax::mojom::blink::Role::kDocGlossary, "DocGlossary"},
    {ax::mojom::blink::Role::kDocGlossRef, "DocGlossref"},
    {ax::mojom::blink::Role::kDocIndex, "DocIndex"},
    {ax::mojom::blink::Role::kDocIntroduction, "DocIntroduction"},
    {ax::mojom::blink::Role::kDocNoteRef, "DocNoteref"},
    {ax::mojom::blink::Role::kDocNotice, "DocNotice"},
    {ax::mojom::blink::Role::kDocPageBreak, "DocPagebreak"},
    {ax::mojom::blink::Role::kDocPageFooter, "DocPageFooter"},
    {ax::mojom::blink::Role::kDocPageHeader, "DocPageHeader"},
    {ax::mojom::blink::Role::kDocPageList, "DocPagelist"},
    {ax::mojom::blink::Role::kDocPart, "DocPart"},
    {ax::mojom::blink::Role::kDocPreface, "DocPreface"},
    {ax::mojom::blink::Role::kDocPrologue, "DocPrologue"},
    {ax::mojom::blink::Role::kDocPullquote, "DocPullquote"},
    {ax::mojom::blink::Role::kDocQna, "DocQna"},
    {ax::mojom::blink::Role::kDocSubtitle, "DocSubtitle"},
    {ax::mojom::blink::Role::kDocTip, "DocTip"},
    {ax::mojom::blink::Role::kDocToc, "DocToc"},
    // End DPub roles.
    // --------------------------------------------------------------
    {ax::mojom::blink::Role::kDocument, "Document"},
    {ax::mojom::blink::Role::kEmbeddedObject, "EmbeddedObject"},
    {ax::mojom::blink::Role::kEmphasis, "Emphasis"},
    {ax::mojom::blink::Role::kFeed, "feed"},
    {ax::mojom::blink::Role::kFigcaption, "Figcaption"},
    {ax::mojom::blink::Role::kFigure, "Figure"},
    {ax::mojom::blink::Role::kFooter, "Footer"},
    {ax::mojom::blink::Role::kFooterAsNonLandmark, "FooterAsNonLandmark"},
    {ax::mojom::blink::Role::kForm, "Form"},
    {ax::mojom::blink::Role::kGenericContainer, "GenericContainer"},
    // --------------------------------------------------------------
    // ARIA Graphics module roles:
    // https://rawgit.com/w3c/graphics-aam/master/#mapping_role_table
    {ax::mojom::blink::Role::kGraphicsDocument, "GraphicsDocument"},
    {ax::mojom::blink::Role::kGraphicsObject, "GraphicsObject"},
    {ax::mojom::blink::Role::kGraphicsSymbol, "GraphicsSymbol"},
    // End ARIA Graphics module roles.
    // --------------------------------------------------------------
    {ax::mojom::blink::Role::kGrid, "Grid"},
    {ax::mojom::blink::Role::kGroup, "Group"},
    {ax::mojom::blink::Role::kHeader, "Header"},
    {ax::mojom::blink::Role::kHeaderAsNonLandmark, "HeaderAsNonLandmark"},
    {ax::mojom::blink::Role::kHeading, "Heading"},
    {ax::mojom::blink::Role::kIframePresentational, "IframePresentational"},
    {ax::mojom::blink::Role::kIframe, "Iframe"},
    {ax::mojom::blink::Role::kIgnored, "Ignored"},
    {ax::mojom::blink::Role::kImageMap, "ImageMap"},
    {ax::mojom::blink::Role::kImage, "Image"},
    {ax::mojom::blink::Role::kImeCandidate, "ImeCandidate"},
    {ax::mojom::blink::Role::kInlineTextBox, "InlineTextBox"},
    {ax::mojom::blink::Role::kInputTime, "InputTime"},
    {ax::mojom::blink::Role::kKeyboard, "Keyboard"},
    {ax::mojom::blink::Role::kLabelText, "Label"},
    {ax::mojom::blink::Role::kLayoutTable, "LayoutTable"},
    {ax::mojom::blink::Role::kLayoutTableCell, "LayoutCellTable"},
    {ax::mojom::blink::Role::kLayoutTableRow, "LayoutRowTable"},
    {ax::mojom::blink::Role::kLegend, "Legend"},
    {ax::mojom::blink::Role::kLink, "Link"},
    {ax::mojom::blink::Role::kLineBreak, "LineBreak"},
    {ax::mojom::blink::Role::kListBox, "ListBox"},
    {ax::mojom::blink::Role::kListBoxOption, "ListBoxOption"},
    {ax::mojom::blink::Role::kListGrid, "ListGrid"},
    {ax::mojom::blink::Role::kListItem, "ListItem"},
    {ax::mojom::blink::Role::kListMarker, "ListMarker"},
    {ax::mojom::blink::Role::kList, "List"},
    {ax::mojom::blink::Role::kLog, "Log"},
    {ax::mojom::blink::Role::kMain, "Main"},
    {ax::mojom::blink::Role::kMark, "Mark"},
    {ax::mojom::blink::Role::kMarquee, "Marquee"},
    {ax::mojom::blink::Role::kMath, "Math"},
    {ax::mojom::blink::Role::kMenuBar, "MenuBar"},
    {ax::mojom::blink::Role::kMenuItem, "MenuItem"},
    {ax::mojom::blink::Role::kMenuItemCheckBox, "MenuItemCheckBox"},
    {ax::mojom::blink::Role::kMenuItemRadio, "MenuItemRadio"},
    {ax::mojom::blink::Role::kMenuListOption, "MenuListOption"},
    {ax::mojom::blink::Role::kMenuListPopup, "MenuListPopup"},
    {ax::mojom::blink::Role::kMenu, "Menu"},
    {ax::mojom::blink::Role::kMeter, "Meter"},
    {ax::mojom::blink::Role::kNavigation, "Navigation"},
    {ax::mojom::blink::Role::kNote, "Note"},
    {ax::mojom::blink::Role::kPane, "Pane"},
    {ax::mojom::blink::Role::kParagraph, "Paragraph"},
    {ax::mojom::blink::Role::kPdfActionableHighlight, "PdfActionableHighlight"},
    {ax::mojom::blink::Role::kPdfRoot, "PdfRoot"},
    {ax::mojom::blink::Role::kPluginObject, "PluginObject"},
    {ax::mojom::blink::Role::kPopUpButton, "PopUpButton"},
    {ax::mojom::blink::Role::kPortal, "Portal"},
    {ax::mojom::blink::Role::kPre, "Pre"},
    {ax::mojom::blink::Role::kPresentational, "Presentational"},
    {ax::mojom::blink::Role::kProgressIndicator, "ProgressIndicator"},
    {ax::mojom::blink::Role::kRadioButton, "RadioButton"},
    {ax::mojom::blink::Role::kRadioGroup, "RadioGroup"},
    {ax::mojom::blink::Role::kRegion, "Region"},
    {ax::mojom::blink::Role::kRootWebArea, "WebArea"},
    {ax::mojom::blink::Role::kRow, "Row"},
    {ax::mojom::blink::Role::kRowGroup, "RowGroup"},
    {ax::mojom::blink::Role::kRowHeader, "RowHeader"},
    {ax::mojom::blink::Role::kRuby, "Ruby"},
    {ax::mojom::blink::Role::kRubyAnnotation, "RubyAnnotation"},
    {ax::mojom::blink::Role::kSection, "Section"},
    {ax::mojom::blink::Role::kSvgRoot, "SVGRoot"},
    {ax::mojom::blink::Role::kScrollBar, "ScrollBar"},
    {ax::mojom::blink::Role::kScrollView, "ScrollView"},
    {ax::mojom::blink::Role::kSearch, "Search"},
    {ax::mojom::blink::Role::kSearchBox, "SearchBox"},
    {ax::mojom::blink::Role::kSlider, "Slider"},
    {ax::mojom::blink::Role::kSpinButton, "SpinButton"},
    {ax::mojom::blink::Role::kSplitter, "Splitter"},
    {ax::mojom::blink::Role::kStaticText, "StaticText"},
    {ax::mojom::blink::Role::kStatus, "Status"},
    {ax::mojom::blink::Role::kStrong, "Strong"},
    {ax::mojom::blink::Role::kSuggestion, "Suggestion"},
    {ax::mojom::blink::Role::kSwitch, "Switch"},
    {ax::mojom::blink::Role::kTab, "Tab"},
    {ax::mojom::blink::Role::kTabList, "TabList"},
    {ax::mojom::blink::Role::kTabPanel, "TabPanel"},
    {ax::mojom::blink::Role::kTable, "Table"},
    {ax::mojom::blink::Role::kTableHeaderContainer, "TableHeaderContainer"},
    {ax::mojom::blink::Role::kTerm, "Term"},
    {ax::mojom::blink::Role::kTextField, "TextField"},
    {ax::mojom::blink::Role::kTextFieldWithComboBox, "ComboBox"},
    {ax::mojom::blink::Role::kTime, "Time"},
    {ax::mojom::blink::Role::kTimer, "Timer"},
    {ax::mojom::blink::Role::kTitleBar, "TitleBar"},
    {ax::mojom::blink::Role::kToggleButton, "ToggleButton"},
    {ax::mojom::blink::Role::kToolbar, "Toolbar"},
    {ax::mojom::blink::Role::kTreeGrid, "TreeGrid"},
    {ax::mojom::blink::Role::kTreeItem, "TreeItem"},
    {ax::mojom::blink::Role::kTree, "Tree"},
    {ax::mojom::blink::Role::kTooltip, "UserInterfaceTooltip"},
    {ax::mojom::blink::Role::kUnknown, "Unknown"},
    {ax::mojom::blink::Role::kVideo, "Video"},
    {ax::mojom::blink::Role::kWebView, "WebView"},
    {ax::mojom::blink::Role::kWindow, "Window"}};

static_assert(base::size(kInternalRoles) ==
                  static_cast<size_t>(ax::mojom::blink::Role::kMaxValue) + 1,
              "Not all internal roles have an entry in internalRoles array");

// Roles which we need to map in the other direction
const RoleEntry kReverseRoles[] = {
    {"banner", ax::mojom::blink::Role::kHeader},
    {"button", ax::mojom::blink::Role::kToggleButton},
    {"combobox", ax::mojom::blink::Role::kPopUpButton},
    {"contentinfo", ax::mojom::blink::Role::kFooter},
    {"menuitem", ax::mojom::blink::Role::kMenuListOption},
    {"progressbar", ax::mojom::blink::Role::kMeter},
    {"region", ax::mojom::blink::Role::kSection},
    {"textbox", ax::mojom::blink::Role::kTextField},
    {"combobox", ax::mojom::blink::Role::kComboBoxMenuButton},
    {"combobox", ax::mojom::blink::Role::kTextFieldWithComboBox}};

static ARIARoleMap* CreateARIARoleMap() {
  ARIARoleMap* role_map = new ARIARoleMap;

  for (size_t i = 0; i < base::size(kRoles); ++i)
    role_map->Set(String(kRoles[i].aria_role), kRoles[i].webcore_role);

  return role_map;
}

static Vector<AtomicString>* CreateRoleNameVector() {
  Vector<AtomicString>* role_name_vector =
      new Vector<AtomicString>(base::size(kInternalRoles));
  for (wtf_size_t i = 0; i < base::size(kInternalRoles); i++)
    (*role_name_vector)[i] = g_null_atom;

  for (wtf_size_t i = 0; i < base::size(kRoles); ++i) {
    (*role_name_vector)[static_cast<wtf_size_t>(kRoles[i].webcore_role)] =
        AtomicString(kRoles[i].aria_role);
  }

  for (wtf_size_t i = 0; i < base::size(kReverseRoles); ++i) {
    (*role_name_vector)[static_cast<wtf_size_t>(
        kReverseRoles[i].webcore_role)] =
        AtomicString(kReverseRoles[i].aria_role);
  }

  return role_name_vector;
}

static Vector<AtomicString>* CreateInternalRoleNameVector() {
  Vector<AtomicString>* internal_role_name_vector =
      new Vector<AtomicString>(base::size(kInternalRoles));
  for (wtf_size_t i = 0; i < base::size(kInternalRoles); i++) {
    (*internal_role_name_vector)[static_cast<wtf_size_t>(
        kInternalRoles[i].webcore_role)] =
        AtomicString(kInternalRoles[i].internal_role_name);
  }

  return internal_role_name_vector;
}

HTMLDialogElement* GetActiveDialogElement(Node* node) {
  return node->GetDocument().ActiveModalDialog();
}

// TODO(dmazzoni): replace this with a call to RoleName().
std::string GetEquivalentAriaRoleString(const ax::mojom::blink::Role role) {
  switch (role) {
    case ax::mojom::blink::Role::kArticle:
      return "article";
    case ax::mojom::blink::Role::kBanner:
      return "banner";
    case ax::mojom::blink::Role::kButton:
      return "button";
    case ax::mojom::blink::Role::kComplementary:
      return "complementary";
    case ax::mojom::blink::Role::kFigure:
      return "figure";
    case ax::mojom::blink::Role::kFooter:
      return "contentinfo";
    case ax::mojom::blink::Role::kHeader:
      return "banner";
    case ax::mojom::blink::Role::kHeading:
      return "heading";
    case ax::mojom::blink::Role::kImage:
      return "img";
    case ax::mojom::blink::Role::kMain:
      return "main";
    case ax::mojom::blink::Role::kNavigation:
      return "navigation";
    case ax::mojom::blink::Role::kRadioButton:
      return "radio";
    case ax::mojom::blink::Role::kRegion:
      return "region";
    case ax::mojom::blink::Role::kSection:
      // A <section> element uses the 'region' ARIA role mapping.
      return "region";
    case ax::mojom::blink::Role::kSlider:
      return "slider";
    case ax::mojom::blink::Role::kTime:
      return "time";
    default:
      break;
  }

  return std::string();
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
    default:
      result = ax::mojom::blink::MarkerType::kNone;
      break;
  }

  return static_cast<int32_t>(result);
}

unsigned AXObject::number_of_live_ax_objects_ = 0;

AXObject::AXObject(AXObjectCacheImpl& ax_object_cache)
    : id_(0),
      parent_(nullptr),
      role_(ax::mojom::blink::Role::kUnknown),
      aria_role_(ax::mojom::blink::Role::kUnknown),
      explicit_container_id_(0),
      last_modification_count_(-1),
      cached_is_ignored_(false),
      cached_is_ignored_but_included_in_tree_(false),
      cached_is_inert_or_aria_hidden_(false),
      cached_is_descendant_of_leaf_node_(false),
      cached_is_descendant_of_disabled_node_(false),
      cached_has_inherited_presentational_role_(false),
      cached_is_editable_root_(false),
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

void AXObject::Init(AXObject* parent_if_known) {
#if DCHECK_IS_ON()
  DCHECK(!parent_);
  DCHECK(!is_initializing_);
  base::AutoReset<bool> reentrancy_protector(&is_initializing_, true);
#endif

  // The role must be determined immedediately.
  // Note: in order to avoid reentrancy, the role computation cannot use the
  // ParentObject(), although it can use the DOM parent.
  role_ = DetermineAccessibilityRole();
#if DCHECK_IS_ON()
  DCHECK(IsValidRole(role_)) << "Illegal " << role_ << " for " << GetNode()
                             << " " << GetLayoutObject();
#endif

  // Determine the parent as soon as possible.
  // Every AXObject must have a parent unless it's the root.
  SetParent(parent_if_known ? parent_if_known : ComputeParent());
  DCHECK(parent_ || IsA<Document>(GetNode()))
      << "The following node should have a parent: " << GetNode();

  // First time object is initialized, it hasn't computed children yet.
  SetNeedsToUpdateChildren();  // Called after role_ is set.

  // Initialize all other cached values.
  UpdateCachedAttributeValuesIfNeeded(false);
}

void AXObject::Detach() {
#if DCHECK_IS_ON()
  // Only mock objects can end up being detached twice, because their owner
  // may have needed to detach them when they were detached, but couldn't
  // remove them from the object cache yet.
  if (IsDetached()) {
    DCHECK(IsMockObject()) << "Object detached twice: " << RoleValue();
    return;
  }
  DCHECK(!is_adding_children_) << ToString(true, true);
  DCHECK(ax_object_cache_);
  DCHECK(!ax_object_cache_->IsFrozen())
      << "Do not detach children while the tree is frozen, in order to avoid "
         "an object detaching itself in the middle of computing its own "
         "accessibility properties.";
#endif

  // Clear any children and call DetachFromParent() on them so that
  // no children are left with dangling pointers to their parent.
  ClearChildren();

  parent_ = nullptr;
  ax_object_cache_ = nullptr;
}

bool AXObject::IsDetached() const {
  return !ax_object_cache_;
}

void AXObject::SetParent(AXObject* new_parent) {
  DCHECK(new_parent || IsA<Document>(GetNode()))
      << "Parent cannot be null, except at the root, was null at " << GetNode()
      << " " << GetLayoutObject();

#if DCHECK_IS_ON()
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
  }
#endif
  parent_ = new_parent;
}

// In many cases, ComputeParent() is not called, because the parent adding
// the child will pass itself into AXObject::Init() via parent_if_known.
// ComputeParent() is still necessary because some parts of the code,
// especially web tests, result in AXObjects being created in the middle of
// the tree before their parents are created.
// TODO(accessibility) Consider forcing all ax objects to be created from
// the top down, eliminating the need for ComputeParent().
AXObject* AXObject::ComputeParent() const {
  DCHECK(!IsDetached());

  DCHECK(!parent_ || parent_->IsDetached())
      << "Should use cached parent unless it's detached, and should not "
         "attempt to recompute it, occurred on "
      << GetNode();

  if (!GetNode() && !GetLayoutObject()) {
    NOTREACHED() << "Can't compute parent on AXObjects without a backing Node "
                    "or LayoutObject. Objects without those must set the "
                    "parent in Init(), |this| = "
                 << RoleValue();
    return nullptr;
  }

  return ComputeParentImpl();
}

AXObject* AXObject::ComputeParentImpl() const {
  DCHECK(!IsDetached());

  if (AXObjectCache().IsAriaOwned(this))
    return AXObjectCache().GetAriaOwnedParent(this);

  Node* current_node = GetNode();

  // A WebArea's parent should be the page popup owner, if any, otherwise null.
  if (IsA<Document>(current_node)) {
    LocalFrame* frame = GetLayoutObject()->GetFrame();
    return AXObjectCache().GetOrCreate(frame->PagePopupOwner());
  }

  if (IsVirtualObject()) {
    NOTREACHED()
        << "A virtual object must have a parent, and cannot exist without one. "
           "The parent is set when the object is constructed.";
    return nullptr;
  }

  // If no node, or a pseudo element, use the layout parent.
  if (!current_node) {
    LayoutObject* current_layout_obj = GetLayoutObject();
    if (!current_layout_obj) {
      NOTREACHED()
          << "Can't compute parent on AXObjects without a backing Node "
             "or LayoutObject. Objects without those must set the "
             "parent in Init(), |this| = "
          << RoleValue();
      return nullptr;
    }
    // If no DOM node and no parent, this must be an anonymous layout object.
    DCHECK(current_layout_obj->IsAnonymous());
    LayoutObject* parent_layout_obj = current_layout_obj->Parent();
    if (!parent_layout_obj)
      return nullptr;
    if (AXObject* ax_parent = AXObjectCache().GetOrCreate(parent_layout_obj)) {
      DCHECK(!ax_parent->IsDetached());
      return ax_parent;
    }
    // Switch to using DOM nodes. The only cases that should occur do not have
    // chains of multiple parents without DOM nodes.
    Node* parent_node = parent_layout_obj->GetNode();
    DCHECK(parent_node) << "Computing an accessible parent from the layout "
                           "parent did not yield an accessible object nor a "
                           "DOM node to walk up from, current_layout_obj = "
                        << current_layout_obj;
    if (!parent_node)
      return nullptr;
  }

  while (true) {
    current_node = GetParentNodeForComputeParent(current_node);
    if (!current_node)
      break;
    AXObject* ax_parent = AXObjectCache().GetOrCreate(current_node);
    if (ax_parent) {
      DCHECK(!ax_parent->IsDetached());
      return ax_parent;
    }
  }

  return nullptr;
}

#if DCHECK_IS_ON()
void AXObject::EnsureCorrectParentComputation() {
  if (!parent_)
    return;

  DCHECK(!parent_->IsDetached());

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

  // Don't check the computed parent if the cached parent is an image map:
  // <area> children's location in the DOM and HTML hierarchy does not match.
  // TODO(aleventhal) Try to remove this rule, it may be unnecessary now.
  if (parent_->RoleValue() == ax::mojom::blink::Role::kImageMap)
    return;

  // TODO(aleventhal) Different in test fast/css/first-letter-removed-added.html
  // when run with --force-renderer-accessibility.
  if (GetNode() && GetNode()->IsPseudoElement())
    return;

  // Verify that the algorithm in ComputeParentImpl() provides same results as
  // parents that init their children with themselves as the parent_if_known.
  // Inconsistency indicates a problem could potentially exist where a child's
  // parent does not include the child in its children.
  DCHECK(ComputeParentImpl())
      << "Computed parent was null for " << this << ", expected " << parent_;
  DCHECK_EQ(ComputeParentImpl(), parent_)
      << "\n**** ComputeParent should have provided the same result as "
         "the known parent.\n**** Computed parent layout object was "
      << ComputeParentImpl()->GetLayoutObject()
      << "\n**** Actual parent's layout object was "
      << parent_->GetLayoutObject() << "\n**** Child was " << this;
}
#endif

const AtomicString& AXObject::GetAOMPropertyOrARIAAttribute(
    AOMStringProperty property) const {
  Element* element = this->GetElement();
  if (!element)
    return g_null_atom;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

Element* AXObject::GetAOMPropertyOrARIAAttribute(
    AOMRelationProperty property) const {
  Element* element = this->GetElement();
  if (!element)
    return nullptr;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property);
}

bool AXObject::HasAOMProperty(AOMRelationListProperty property,
                              HeapVector<Member<Element>>& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetProperty(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(
    AOMRelationListProperty property,
    HeapVector<Member<Element>>& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  return AccessibleNode::GetPropertyOrARIAAttribute(element, property, result);
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMBooleanProperty property,
                                             bool& result) const {
  Element* element = this->GetElement();
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
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMIntProperty property,
                                             int32_t& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMFloatProperty property,
                                             float& result) const {
  Element* element = this->GetElement();
  if (!element)
    return false;

  bool is_null = true;
  result =
      AccessibleNode::GetPropertyOrARIAAttribute(element, property, is_null);
  return !is_null;
}

bool AXObject::HasAOMPropertyOrARIAAttribute(AOMStringProperty property,
                                             AtomicString& result) const {
  Element* element = this->GetElement();
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
  node_data->role = RoleValue();
  node_data->id = AXObjectID();

  DCHECK(!IsDetached()) << "Do not serialize detached nodes: "
                        << ToString(true, true);
  DCHECK(AccessibilityIsIncludedInTree())
      << "Do not serialize unincluded nodes: " << ToString(true, true);

  // Serialize a few things that we need even for ignored nodes.
  bool is_focusable = CanSetFocusAttribute();
  if (is_focusable)
    node_data->AddState(ax::mojom::blink::State::kFocusable);

  bool is_visible = IsVisible();
  if (!is_visible)
    node_data->AddState(ax::mojom::blink::State::kInvisible);

  if (is_visible || is_focusable) {
    // Preserve continuity in subtrees of richly editable content by including
    // richlyEditable state even if ignored.
    if (IsEditable()) {
      node_data->AddState(ax::mojom::blink::State::kEditable);
      if (IsEditableRoot()) {
        node_data->AddBoolAttribute(
            ax::mojom::blink::BoolAttribute::kEditableRoot, true);
      }
      if (IsRichlyEditable())
        node_data->AddState(ax::mojom::blink::State::kRichlyEditable);
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

  if (AccessibilityIsIgnored()) {
    node_data->AddState(ax::mojom::blink::State::kIgnored);
    // Early return for ignored, unfocusable nodes, avoiding unnecessary work.
    if (!is_focusable)
      return;
  }

  SerializeUnignoredAttributes(node_data, accessibility_mode);
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

  if (HasPopup() != ax::mojom::blink::HasPopup::kFalse)
    node_data->SetHasPopup(HasPopup());
  else if (RoleValue() == ax::mojom::blink::Role::kPopUpButton)
    node_data->SetHasPopup(ax::mojom::blink::HasPopup::kMenu);

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

  // If this is an HTMLFrameOwnerElement (such as an iframe), we may need
  // to embed the ID of the child frame.
  if (auto* html_frame_owner_element =
          DynamicTo<HTMLFrameOwnerElement>(GetElement())) {
    if (Frame* child_frame = html_frame_owner_element->ContentFrame()) {
      base::Optional<base::UnguessableToken> child_token =
          child_frame->GetEmbeddingToken();
      if (child_token && !(IsDetached() || ChildCountIncludingIgnored())) {
        node_data->AddStringAttribute(
            ax::mojom::blink::StringAttribute::kChildTreeId,
            child_token->ToString());
      }
    }
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
    if (ui::IsHeading(RoleValue()) && HeadingLevel()) {
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

  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kValue,
                                GetValueForControl().Utf8());

  switch (Restriction()) {
    case AXRestriction::kRestrictionReadOnly:
      node_data->SetRestriction(ax::mojom::blink::Restriction::kReadOnly);
      break;
    case AXRestriction::kRestrictionDisabled:
      node_data->SetRestriction(ax::mojom::blink::Restriction::kDisabled);
      break;
    case AXRestriction::kRestrictionNone:
      if (CanSetValueAttribute())
        node_data->AddAction(ax::mojom::blink::Action::kSetValue);
      break;
  }

  if (!Url().IsEmpty()) {
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kUrl,
                                  Url().GetString().Utf8());
  }

  if (accessibility_mode.has_mode(ui::AXMode::kScreenReader)) {
    SerializeMarkerAttributes(node_data);
    SerializeStyleAttributes(node_data);
  }

  SerializeSparseAttributes(node_data);

  if (Element* element = this->GetElement()) {
    if (const AtomicString& aria_role =
            GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole)) {
      TruncateAndAddStringAttribute(node_data,
                                    ax::mojom::blink::StringAttribute::kRole,
                                    aria_role.Utf8());
    } else {
      std::string role_str = GetEquivalentAriaRoleString(RoleValue());
      if (!role_str.empty()) {
        TruncateAndAddStringAttribute(node_data,
                                      ax::mojom::blink::StringAttribute::kRole,
                                      GetEquivalentAriaRoleString(RoleValue()));
      }
    }

    if (IsEditable()) {
      if (IsNativeTextControl()) {
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
        node_data->AddIntAttribute(
            ax::mojom::blink::IntAttribute::kTextSelStart, start);
        node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kTextSelEnd,
                                   end);
      }
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

void AXObject::SerializeScrollAttributes(ui::AXNodeData* node_data) {
  // Only mark as scrollable if user has actual scrollbars to use.
  node_data->AddBoolAttribute(ax::mojom::blink::BoolAttribute::kScrollable,
                              IsUserScrollable());
  // Provide x,y scroll info if scrollable in any way (programmatically or via
  // user).
  const gfx::Point& scroll_offset = GetScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollX,
                             scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollY,
                             scroll_offset.y());

  const gfx::Point& min_scroll_offset = MinimumScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollXMin,
                             min_scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollYMin,
                             min_scroll_offset.y());

  const gfx::Point& max_scroll_offset = MaximumScrollOffset();
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollXMax,
                             max_scroll_offset.x());
  node_data->AddIntAttribute(ax::mojom::blink::IntAttribute::kScrollYMax,
                             max_scroll_offset.y());
}

void AXObject::SerializeElementAttributes(ui::AXNodeData* node_data) {
  Element* element = this->GetElement();
  if (!element)
    return;

  if (const AtomicString& class_name = element->GetClassAttribute()) {
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kClassName,
                                  class_name.Utf8());
  }

  // ARIA role.
  if (const AtomicString& aria_role =
          GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole)) {
    TruncateAndAddStringAttribute(
        node_data, ax::mojom::blink::StringAttribute::kRole, aria_role.Utf8());
  } else {
    std::string role = GetEquivalentAriaRoleString(RoleValue());
    if (!role.empty()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kRole, role);
    }
  }
}

void AXObject::SerializeHTMLTagAndClass(ui::AXNodeData* node_data) {
  Element* element = GetElement();
  if (!element)
    return;

  TruncateAndAddStringAttribute(node_data,
                                ax::mojom::blink::StringAttribute::kHtmlTag,
                                element->tagName().LowerASCII().Utf8());

  if (const AtomicString& class_name = element->GetClassAttribute()) {
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kClassName,
                                  class_name.Utf8());
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
#if defined(OS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
  if (node_data->role == ax::mojom::blink::Role::kMath &&
      element->innerHTML().length()) {
    TruncateAndAddStringAttribute(node_data,
                                  ax::mojom::blink::StringAttribute::kInnerHtml,
                                  element->innerHTML().Utf8());
  }
#endif
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

void AXObject::SerializeStyleAttributes(ui::AXNodeData* node_data) {
  AXObject* parent = ParentObjectUnignored();
  if (FontFamily().length()) {
    if (!parent || parent->FontFamily() != FontFamily()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kFontFamily,
          FontFamily().Utf8());
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
    AXSparseSetterFunc callback = setter_map.at(attr.GetName());

    if (callback)
      callback.Run(this, node_data, attr.Value());
  }

  if (!element->DidAttachInternals())
    return;
  const auto& internals_attributes =
      element->EnsureElementInternals().GetAttributes();
  for (const QualifiedName& attr : internals_attributes.Keys()) {
    if (set_attributes.Contains(attr))
      continue;

    AXSparseSetterFunc callback = setter_map.at(attr);

    if (callback)
      callback.Run(this, node_data, internals_attributes.at(attr));
  }
}

void AXObject::SerializeLangAttribute(ui::AXNodeData* node_data) {
  AXObject* parent = ParentObject();
  if (Language().length()) {
    // TODO(chrishall): should we still trim redundant languages off here?
    if (!parent || parent->Language() != Language()) {
      TruncateAndAddStringAttribute(
          node_data, ax::mojom::blink::StringAttribute::kLanguage,
          Language().Utf8());
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

void AXObject::SerializeMarkerAttributes(ui::AXNodeData* node_data) const {
  // Implemented in subclasses.
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

void AXObject::TruncateAndAddStringAttribute(
    ui::AXNodeData* dst,
    ax::mojom::blink::StringAttribute attribute,
    const std::string& value,
    uint32_t max_len) const {
  if (value.size() > max_len) {
    std::string truncated;
    base::TruncateUTF8ToByteSize(value, max_len, &truncated);
    dst->AddStringAttribute(attribute, truncated);
  } else {
    dst->AddStringAttribute(attribute, value);
  }
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

ax::mojom::blink::Role AXObject::RoleValue() const {
  return role_;
}

bool AXObject::IsARIATextControl() const {
  return AriaRoleAttribute() == ax::mojom::blink::Role::kTextField ||
         AriaRoleAttribute() == ax::mojom::blink::Role::kSearchBox ||
         AriaRoleAttribute() == ax::mojom::blink::Role::kTextFieldWithComboBox;
}

bool AXObject::IsAnchor() const {
  return IsLink() && !IsNativeImage();
}

bool AXObject::IsButton() const {
  return ui::IsButton(RoleValue());
}

bool AXObject::IsCanvas() const {
  return RoleValue() == ax::mojom::blink::Role::kCanvas;
}

bool AXObject::IsCheckboxOrRadio() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kMenuItemCheckBox:
    case ax::mojom::blink::Role::kMenuItemRadio:
    case ax::mojom::blink::Role::kRadioButton:
      return true;
    default:
      break;
  }
  return false;
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

bool AXObject::IsInPageLinkTarget() const {
  return false;
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
    const Node* node = this->GetNode();
    if (!node)
      return ax::mojom::blink::CheckedState::kNone;

    // Expose native checkbox mixed state as accessibility mixed state. However,
    // do not expose native radio mixed state as accessibility mixed state.
    // This would confuse the JAWS screen reader, which reports a mixed radio as
    // both checked and partially checked, but a native mixed native radio
    // button sinply means no radio buttons have been checked in the group yet.
    if (IsNativeCheckboxInMixedState(node))
      return ax::mojom::blink::CheckedState::kMixed;

    auto* html_input_element = DynamicTo<HTMLInputElement>(node);
    if (html_input_element && html_input_element->ShouldAppearChecked()) {
      return ax::mojom::blink::CheckedState::kTrue;
    }
  }

  return ax::mojom::blink::CheckedState::kFalse;
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

bool AXObject::IsLandmarkRelated() const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kApplication:
    case ax::mojom::blink::Role::kArticle:
    case ax::mojom::blink::Role::kBanner:
    case ax::mojom::blink::Role::kComplementary:
    case ax::mojom::blink::Role::kContentInfo:
    case ax::mojom::blink::Role::kDocAcknowledgments:
    case ax::mojom::blink::Role::kDocAfterword:
    case ax::mojom::blink::Role::kDocAppendix:
    case ax::mojom::blink::Role::kDocBibliography:
    case ax::mojom::blink::Role::kDocChapter:
    case ax::mojom::blink::Role::kDocConclusion:
    case ax::mojom::blink::Role::kDocCredits:
    case ax::mojom::blink::Role::kDocEndnotes:
    case ax::mojom::blink::Role::kDocEpilogue:
    case ax::mojom::blink::Role::kDocErrata:
    case ax::mojom::blink::Role::kDocForeword:
    case ax::mojom::blink::Role::kDocGlossary:
    case ax::mojom::blink::Role::kDocIntroduction:
    case ax::mojom::blink::Role::kDocPart:
    case ax::mojom::blink::Role::kDocPreface:
    case ax::mojom::blink::Role::kDocPrologue:
    case ax::mojom::blink::Role::kDocToc:
    case ax::mojom::blink::Role::kFooter:
    case ax::mojom::blink::Role::kForm:
    case ax::mojom::blink::Role::kHeader:
    case ax::mojom::blink::Role::kMain:
    case ax::mojom::blink::Role::kNavigation:
    case ax::mojom::blink::Role::kRegion:
    case ax::mojom::blink::Role::kSearch:
    case ax::mojom::blink::Role::kSection:
      return true;
    default:
      return false;
  }
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

bool AXObject::IsNativeTextControl() const {
  return false;
}

bool AXObject::IsNonNativeTextControl() const {
  return false;
}

bool AXObject::IsPasswordField() const {
  return false;
}

bool AXObject::IsPasswordFieldAndShouldHideValue() const {
  Settings* settings = GetDocument()->GetSettings();
  if (!settings || settings->GetAccessibilityPasswordValuesEnabled())
    return false;

  return IsPasswordField();
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

bool AXObject::IsClickable() const {
  return ui::IsClickable(RoleValue());
}

bool AXObject::AccessibilityIsIgnored() const {
  UpdateCachedAttributeValuesIfNeeded();
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
  DCHECK(!is_updating_cached_values_)
      << "Reentering UpdateCachedAttributeValuesIfNeeded() on same node: "
      << GetNode();

  base::AutoReset<bool> reentrancy_protector(&is_updating_cached_values_, true);

  DCHECK(!GetDocument() || GetDocument()->Lifecycle().GetState() >=
                               DocumentLifecycle::kAfterPerformLayout)
      << "Unclean document at lifecycle "
      << GetDocument()->Lifecycle().ToString();
#endif

  // TODO(accessibility) Every AXObject must have a parent except the root.
  // Sometimes the parent is detached and a new parent isn't yet reattached.
  if (!parent_)
    parent_ = ComputeParent();

  cached_is_hidden_via_style = ComputeIsHiddenViaStyle();

  // Decisions in what subtree descendants are included (each descendant's
  // cached children_) depends on the ARIA hidden state. When it changes,
  // the entire subtree needs to recompute descendants.
  // In addition, the below computations for is_ignored_but_included_in_tree is
  // dependent on having the correct new cached value.
  bool is_inert_or_aria_hidden = ComputeIsInertOrAriaHidden();
  if (cached_is_inert_or_aria_hidden_ != is_inert_or_aria_hidden) {
    // If children already dirty (e.g. at Init() time), then setting the flag
    // again will cause no additional work.
    children_dirty_ = CanHaveChildren();
    cached_is_inert_or_aria_hidden_ = is_inert_or_aria_hidden;
  }
  cached_is_descendant_of_leaf_node_ = !!LeafNodeAncestor();
  cached_is_descendant_of_disabled_node_ = !!DisabledAncestor();
  cached_has_inherited_presentational_role_ =
      !!InheritsPresentationalRoleFrom();

  bool is_ignored = ComputeAccessibilityIsIgnored();
  bool is_ignored_but_included_in_tree =
      is_ignored && ComputeAccessibilityIsIgnoredButIncludedInTree();
#if DCHECK_IS_ON()
  // Ensure that display-locked text is pruned from the tree. This means that
  // they will be missed in the virtual buffer; therefore, it may be a rule
  // subject to change. Note that changing the rule would potentially cause a
  // lot of display-locked whitespace to be exposed.
  if (is_ignored && RoleValue() == ax::mojom::blink::Role::kStaticText &&
      GetNode() &&
      DisplayLockUtilities::NearestLockedExclusiveAncestor(*GetNode())) {
    DCHECK(!cached_is_ignored_but_included_in_tree_)
        << "Display locked text should not be included in the tree (subject to "
           "future rule change)";
  }
#endif
  bool included_in_tree_changed = false;

  // If the child's "included in tree" state changes, we will be notifying the
  // parent to recompute it's children.
  // Exceptions:
  // - Caller passes in |notify_parent_of_ignored_changes = false| -- this
  //   occurs when this is a new child, or when a parent is in the middle of
  //   adding this child, and doing this would be redundant.
  // - Inline text boxes: their "included in tree" state is entirely dependent
  //   on their static text parent.
  if (notify_parent_of_ignored_changes &&
      RoleValue() != ax::mojom::blink::Role::kInlineTextBox) {
    bool is_included_in_tree = !is_ignored || is_ignored_but_included_in_tree;
    if (is_included_in_tree != LastKnownIsIncludedInTreeValue())
      included_in_tree_changed = true;
  }

  // Presence of inline text children depends on ignored state.
  if (is_ignored != LastKnownIsIgnoredValue() &&
      ui::CanHaveInlineTextBoxChildren(RoleValue())) {
    // TODO(accessibility) Would like to call SetNeedsToUpdateChildren(), but
    // can't call a non-const method from a const method.
    children_dirty_ = CanHaveChildren();
  }

  cached_is_ignored_ = is_ignored;
  cached_is_ignored_but_included_in_tree_ = is_ignored_but_included_in_tree;
  cached_is_editable_root_ = ComputeIsEditableRoot();
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
  } else {
    // Is a live region root if this or an ancestor is a live region.
    DCHECK(parent_) << "No parent and not a document, node=" << GetNode();
    if (!parent_)
      return;
    cached_live_region_root_ = IsLiveRegionRoot() ? const_cast<AXObject*>(this)
                                                  : parent_->LiveRegionRoot();
  }
  cached_aria_column_index_ = ComputeAriaColumnIndex();
  cached_aria_row_index_ = ComputeAriaRowIndex();

  if (included_in_tree_changed) {
    DCHECK(!ax_object_cache_->IsFrozen())
        << "Attempting to change children on an ancestor is dangerous during "
           "serialization, because the ancestor may have already been visited. "
           "Reaching this line indicates that AXObjectCacheImpl did not handle "
           "a signal and call ChilldrenChanged() earlier.";
    if (AXObject* parent = CachedParentObject()) {
      // Defer this ChildrenChanged(), otherwise it can cause reentry into
      // UpdateCachedAttributeValuesIfNeeded() on |this|.
      AXObjectCache().ChildrenChanged(parent);
    }
  }

  if (GetLayoutObject() && GetLayoutObject()->IsText()) {
    cached_local_bounding_box_rect_for_accessibility_ =
        GetLayoutObject()->LocalBoundingBoxRectForAccessibility();
  }
}

bool AXObject::AccessibilityIsIgnoredByDefault(
    IgnoredReasons* ignored_reasons) const {
  return DefaultObjectInclusion(ignored_reasons) == kIgnoreObject;
}

AXObjectInclusion AXObject::DefaultObjectInclusion(
    IgnoredReasons* ignored_reasons) const {
  if (IsInertOrAriaHidden()) {
    // Keep focusable elements that are aria-hidden in tree, so that they can
    // still fire events such as focus and value changes.
    if (!CanSetFocusAttribute()) {
      if (ignored_reasons)
        ComputeIsInertOrAriaHidden(ignored_reasons);
      return kIgnoreObject;
    }
  }

  return kDefaultBehavior;
}

bool AXObject::IsInertOrAriaHidden() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_inert_or_aria_hidden_;
}

bool AXObject::IsAriaHidden() const {
  return IsInertOrAriaHidden() && AriaHiddenRoot();
}

bool AXObject::ComputeIsInertOrAriaHidden(
    IgnoredReasons* ignored_reasons) const {
  if (GetNode()) {
    if (GetNode()->IsInert()) {
      if (ignored_reasons) {
        HTMLDialogElement* dialog = GetActiveDialogElement(GetNode());
        if (dialog) {
          AXObject* dialog_object = AXObjectCache().GetOrCreate(dialog);
          if (dialog_object) {
            ignored_reasons->push_back(
                IgnoredReason(kAXActiveModalDialog, dialog_object));
          } else {
            ignored_reasons->push_back(IgnoredReason(kAXInertElement));
          }
        } else {
          const AXObject* inert_root_el = InertRoot();
          if (inert_root_el == this) {
            ignored_reasons->push_back(IgnoredReason(kAXInertElement));
          } else {
            ignored_reasons->push_back(
                IgnoredReason(kAXInertSubtree, inert_root_el));
          }
        }
      }
      return true;
    } else if (IsBlockedByAriaModalDialog(ignored_reasons)) {
      return true;
    }
  } else {
    AXObject* parent = ParentObject();
    if (parent && parent->IsInertOrAriaHidden()) {
      if (ignored_reasons)
        parent->ComputeIsInertOrAriaHidden(ignored_reasons);
      return true;
    }
  }

  const AXObject* hidden_root = AriaHiddenRoot();
  if (hidden_root) {
    if (ignored_reasons) {
      if (hidden_root == this) {
        ignored_reasons->push_back(IgnoredReason(kAXAriaHiddenElement));
      } else {
        ignored_reasons->push_back(
            IgnoredReason(kAXAriaHiddenSubtree, hidden_root));
      }
    }
    return true;
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
  return !IsInertOrAriaHidden() && !IsHiddenViaStyle();
}

bool AXObject::IsDescendantOfLeafNode() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_descendant_of_leaf_node_;
}

AXObject* AXObject::LeafNodeAncestor() const {
  if (AXObject* parent = ParentObject()) {
    if (!parent->CanHaveChildren())
      return parent;

    return parent->LeafNodeAncestor();
  }

  return nullptr;
}

const AXObject* AXObject::AriaHiddenRoot() const {
  for (const AXObject* object = this; object; object = object->ParentObject()) {
    if (object->AOMPropertyOrARIAAttributeIsTrue(AOMBooleanProperty::kHidden))
      return object;
  }

  return nullptr;
}

const AXObject* AXObject::InertRoot() const {
  const AXObject* object = this;
  if (!RuntimeEnabledFeatures::InertAttributeEnabled())
    return nullptr;

  while (object && !object->IsAXNodeObject())
    object = object->ParentObject();
  Node* node = object->GetNode();
  auto* element = DynamicTo<Element>(node);
  if (!element)
    element = FlatTreeTraversal::ParentElement(*node);

  while (element) {
    if (element->FastHasAttribute(html_names::kInertAttr))
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
  event.SetEventPhase(Event::kCapturingPhase);
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
  event.SetEventPhase(Event::kAtTarget);
  event.SetCurrentTarget(event_path[0]);
  event_path[0]->FireEventListeners(event);
  if (event.PropagationStopped())
    return true;

  // Bubbling phase.
  event.SetEventPhase(Event::kBubblingPhase);
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

const AXObject* AXObject::DisabledAncestor() const {
  bool disabled = false;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled, disabled)) {
    if (disabled)
      return this;
    return nullptr;
  }

  if (AXObject* parent = ParentObject())
    return parent->DisabledAncestor();

  return nullptr;
}

bool AXObject::ComputeAccessibilityIsIgnoredButIncludedInTree() const {
  if (RuntimeEnabledFeatures::AccessibilityExposeIgnoredNodesEnabled())
    return true;

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
      DCHECK(IsMockObject() || IsVirtualObject() ||
             RoleValue() == ax::mojom::blink::Role::kInlineTextBox)
          << "Nodeless, layout-less object found with role " << RoleValue();
    }
    // By including all of these objects in the tree, it is ensured that
    // ClearChildren() will be able to find these children and detach them
    // from their parent.
    return true;
  }

  // Include all pseudo element content. Any anonymous subtree is included
  // from above, in the condition where there is no node.
  if (node->IsPseudoElement())
    return true;

  // Use a flag to control whether or not the <html> element is included
  // in the accessibility tree. Either way it's always marked as "ignored",
  // but eventually we want to always include it in the tree to simplify
  // some logic.
  if (IsA<HTMLHtmlElement>(node))
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

  // Allow the browser side ax tree to access "visibility: [hidden|collapse]"
  // and "display: none" nodes. This is useful for APIs that return the node
  // referenced by aria-labeledby and aria-describedby.
  // An element must have an id attribute or it cannot be referenced by
  // aria-labelledby or aria-describedby.
  if (RuntimeEnabledFeatures::AccessibilityExposeDisplayNoneEnabled()) {
    if (Element* element = GetElement()) {
      if (element->FastHasAttribute(html_names::kIdAttr) &&
          IsHiddenViaStyle()) {
        return true;
      }
    }
  } else if (GetLayoutObject()) {
    if (GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible)
      return true;
  }

  // Allow the browser side ax tree to access "aria-hidden" nodes.
  // This is useful for APIs that return the node referenced by
  // aria-labeledby and aria-describedby.
  if (GetLayoutObject() && IsAriaHidden())
    return true;

  // Preserve SVG grouping elements.
  if (IsA<SVGGElement>(node))
    return true;

  // Keep table-related elements in the tree, because it's too easy for them
  // to in and out of being ignored based on their ancestry, as their role
  // can depend on several levels up in the hierarchy.
  if (IsA<HTMLTableElement>(node) || IsA<HTMLTableSectionElement>(node) ||
      IsA<HTMLTableRowElement>(node) || IsA<HTMLTableCellElement>(node)) {
    return true;
  }

  // Preserve nodes with language attributes.
  if (HasAttribute(html_names::kLangAttr))
    return true;

  return false;
}

const AXObject* AXObject::GetNativeTextControlAncestor(
    int max_levels_to_check) const {
  if (IsNativeTextControl())
    return this;

  if (max_levels_to_check == 0)
    return nullptr;

  if (AXObject* parent = ParentObject())
    return parent->GetNativeTextControlAncestor(max_levels_to_check - 1);

  return nullptr;
}

const AXObject* AXObject::DatetimeAncestor(int max_levels_to_check) const {
  switch (RoleValue()) {
    case ax::mojom::blink::Role::kDateTime:
    case ax::mojom::blink::Role::kDate:
    case ax::mojom::blink::Role::kInputTime:
    case ax::mojom::blink::Role::kTime:
      return this;
    default:
      break;
  }

  if (max_levels_to_check == 0)
    return nullptr;

  if (AXObject* parent = ParentObject())
    return parent->DatetimeAncestor(max_levels_to_check - 1);

  return nullptr;
}

bool AXObject::LastKnownIsIgnoredValue() const {
  return cached_is_ignored_;
}

bool AXObject::LastKnownIsIgnoredButIncludedInTreeValue() const {
  return cached_is_ignored_but_included_in_tree_;
}

bool AXObject::LastKnownIsIncludedInTreeValue() const {
  return !LastKnownIsIgnoredValue() ||
         LastKnownIsIgnoredButIncludedInTreeValue();
}

bool AXObject::HasInheritedPresentationalRole() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_has_inherited_presentational_role_;
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

  // If we're in a canvas subtree, then use the canvas visibility instead of
  // self visibility. The elements in a canvas subtree are fallback elements,
  // which are not necessarily rendered but are allowed to be focusable.
  if (element->IsInCanvasSubtree()) {
    const HTMLCanvasElement* canvas =
        Traversal<HTMLCanvasElement>::FirstAncestorOrSelf(*element);
    DCHECK(canvas);
    return canvas->GetLayoutObject() &&
           canvas->GetLayoutObject()->Style()->Visibility() ==
               EVisibility::kVisible;
  }

  return GetLayoutObject() &&
         GetLayoutObject()->Style()->Visibility() == EVisibility::kVisible;
}

// This does not use Element::IsFocusable(), as that can sometimes recalculate
// styles because of IsFocusableStyle() check, resetting the document lifecycle.
bool AXObject::CanSetFocusAttribute() const {
  if (IsDetached())
    return false;

  // Objects within a portal are not focusable.
  // Note that they are ignored but can be included in the tree.
  bool inside_portal = GetDocument() && GetDocument()->GetPage() &&
                       GetDocument()->GetPage()->InsidePortal();
  if (inside_portal)
    return false;

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

  // NOT focusable: inert elements.
  if (elem->IsInert())
    return false;

  // NOT focusable: disabled form controls.
  if (IsDisabledFormControl(elem))
    return false;

  // Focusable: options in a combobox or listbox.
  // Even though they are not treated as supporting focus by Blink (the parent
  // widget is), they are considered focusable in the accessibility sense,
  // behaving like potential active descendants, and handling focus actions.
  // Menu list options are handled before visibility check, because they
  // are considered focusable even when part of collapsed drop down.
  if (RoleValue() == ax::mojom::blink::Role::kMenuListOption)
    return true;

  // NOT focusable: hidden elements.
  // TODO(aleventhal) Consider caching visibility when it's safe to compute.
  if (!IsA<HTMLAreaElement>(elem) && !IsFocusableStyleUsingBestAvailableState())
    return false;

  // Focusable: options in a combobox or listbox.
  // Similar to menu list option treatment above, but not focusable if hidden.
  if (RoleValue() == ax::mojom::blink::Role::kListBoxOption)
    return true;

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

  // Focusable: can be an active descendant.
  if (CanBeActiveDescendant())
    return true;

  // NOT focusable: everything else.
  return false;
}

// From ARIA 1.1.
// 1. The value of aria-activedescendant refers to an element that is either a
// descendant of the element with DOM focus or is a logical descendant as
// indicated by the aria-owns attribute. 2. The element with DOM focus is a
// textbox with aria-controls referring to an element that supports
// aria-activedescendant, and the value of aria-activedescendant specified for
// the textbox refers to either a descendant of the element controlled by the
// textbox or is a logical descendant of that controlled element as indicated by
// the aria-owns attribute.
bool AXObject::CanBeActiveDescendant() const {
  // Require an element with an id attribute.
  // TODO(accessibility): this code currently requires both an id and role
  // attribute, as well as an ancestor or controlling aria-activedescendant.
  // However, with element reflection it may be possible to set an active
  // descendant without an id, so at some point we may need to remove the
  // requirement for an id attribute.
  if (!GetElement() || !GetElement()->FastHasAttribute(html_names::kIdAttr))
    return false;

  // Does not make sense to use aria-activedescendant to point to a
  // presentational object.
  if (IsPresentational())
    return false;

  // Does not make sense to use aria-activedescendant to point to an HTML
  // element that requires real focus, therefore an ARIA role is necessary.
  if (AriaRoleAttribute() == ax::mojom::blink::Role::kUnknown)
    return false;

  return IsARIAControlledByTextboxWithActiveDescendant() ||
         AncestorExposesActiveDescendant();
}

bool AXObject::IsARIAControlledByTextboxWithActiveDescendant() const {
  if (IsDetached() || !GetDocument())
    return false;

  // This situation should mostly arise when using an active descendant on a
  // textbox inside an ARIA 1.1 combo box widget, which points to the selected
  // option in a list. In such situations, the active descendant is useful only
  // when the textbox is focused. Therefore, we don't currently need to keep
  // track of all aria-controls relationships.
  const Element* focused_element = GetDocument()->FocusedElement();
  if (!focused_element)
    return false;

  const AXObject* focused_object = AXObjectCache().GetOrCreate(focused_element);
  if (!focused_object || !focused_object->IsTextControl())
    return false;

  if (!focused_object->GetAOMPropertyOrARIAAttribute(
          AOMRelationProperty::kActiveDescendant)) {
    return false;
  }

  HeapVector<Member<Element>> controlled_by_elements;
  if (!focused_object->HasAOMPropertyOrARIAAttribute(
          AOMRelationListProperty::kControls, controlled_by_elements)) {
    return false;
  }

  for (const auto& controlled_by_element : controlled_by_elements) {
    const AXObject* controlled_by_object =
        AXObjectCache().GetOrCreate(controlled_by_element);
    if (!controlled_by_object)
      continue;

    const AXObject* object = this;
    while (object && object != controlled_by_object)
      object = object->ParentObjectUnignored();
    if (object)
      return true;
  }

  return false;
}

bool AXObject::AncestorExposesActiveDescendant() const {
  const AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return false;

  if (parent->GetAOMPropertyOrARIAAttribute(
          AOMRelationProperty::kActiveDescendant)) {
    return true;
  }

  return parent->AncestorExposesActiveDescendant();
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
    case ax::mojom::blink::Role::kRow:
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
      return std::any_of(
          UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
          [](const AXObject& ancestor) {
            return ancestor.RoleValue() == ax::mojom::blink::Role::kGrid ||
                   ancestor.RoleValue() == ax::mojom::blink::Role::kTreeGrid;
          });

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
  return ui::IsSetLike(RoleValue()) || ui::IsItemLike(RoleValue());
}

// Simplify whitespace, but preserve a single leading and trailing whitespace
// character if it's present.
// static
String AXObject::CollapseWhitespace(const String& str) {
  StringBuilder result;
  if (!str.IsEmpty() && IsHTMLSpace<UChar>(str[0]))
    result.Append(' ');
  result.Append(str.SimplifyWhiteSpace(IsHTMLSpace<UChar>));
  if (!str.IsEmpty() && IsHTMLSpace<UChar>(str[str.length() - 1]))
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
  String text = TextAlternative(false, false, visited, name_from,
                                &related_objects, nullptr);

  ax::mojom::blink::Role role = RoleValue();
  if (!GetNode() || (!IsA<HTMLBRElement>(GetNode()) &&
                     role != ax::mojom::blink::Role::kStaticText &&
                     role != ax::mojom::blink::Role::kInlineTextBox))
    text = CollapseWhitespace(text);

  if (name_objects) {
    name_objects->clear();
    for (NameSourceRelatedObject* related_object : related_objects)
      name_objects->push_back(related_object->object);
  }

  return text;
}

String AXObject::GetName(NameSources* name_sources) const {
  AXObjectSet visited;
  ax::mojom::blink::NameFrom tmp_name_from;
  AXRelatedObjectVector tmp_related_objects;
  String text = TextAlternative(false, false, visited, tmp_name_from,
                                &tmp_related_objects, name_sources);
  text = text.SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  return text;
}

String AXObject::RecursiveTextAlternative(const AXObject& ax_obj,
                                          bool in_aria_labelled_by_traversal,
                                          AXObjectSet& visited) {
  ax::mojom::blink::NameFrom tmp_name_from;
  return RecursiveTextAlternative(ax_obj, in_aria_labelled_by_traversal,
                                  visited, tmp_name_from);
}

String AXObject::RecursiveTextAlternative(
    const AXObject& ax_obj,
    bool in_aria_labelled_by_traversal,
    AXObjectSet& visited,
    ax::mojom::blink::NameFrom& name_from) {
  if (visited.Contains(&ax_obj) && !in_aria_labelled_by_traversal)
    return String();

  return ax_obj.TextAlternative(true, in_aria_labelled_by_traversal, visited,
                                name_from, nullptr, nullptr);
}

bool AXObject::ComputeIsHiddenViaStyle() const {
  Node* node = GetNode();
  if (!node)
    return false;

  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*node)) {
    // Ensure contents of head, style and script are never exposed.
    // Note: an AXObject is created for <title> to gather the document's name.
    DCHECK(!Traversal<SVGStyleElement>::FirstAncestorOrSelf(*node)) << node;
    DCHECK(!Traversal<HTMLHeadElement>::FirstAncestorOrSelf(*node) ||
           IsA<HTMLTitleElement>(node))
        << node;
    DCHECK(!Traversal<HTMLStyleElement>::FirstAncestorOrSelf(*node)) << node;
    DCHECK(!Traversal<HTMLScriptElement>::FirstAncestorOrSelf(*node)) << node;

    // content-visibility: hidden subtrees are always hidden.
    return DisplayLockUtilities::ShouldIgnoreNodeDueToDisplayLock(
        *node, DisplayLockActivationReason::kAccessibility);
  }

  // For elements with layout objects we can get their style directly.
  if (GetLayoutObject())
    return GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible;

  // No layout object: must ensure computed style.
  if (Element* element = DynamicTo<Element>(node)) {
    const ComputedStyle* style = element->EnsureComputedStyle();
    return !style || style->IsEnsuredInDisplayNone() ||
           style->Visibility() != EVisibility::kVisible;
  }
  return false;
}

bool AXObject::IsHiddenViaStyle() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_hidden_via_style;
}

// Return true if this should be removed from accessible name computations,
// unless it is reached by following an aria-labelledby. When that happens, this
// is not checked, because aria-labelledby can use hidden subtrees.
// Because aria-labelledby can use hidden subtrees, when it has entered a hidden
// subtree, it is not enough to check if the element was hidden by an ancestor.
// In this case, return true only if the hiding style targeted the node
// directly, as opposed to having inherited the hiding style. Using inherited
// hiding styles is problematic because it would prevent name contributions from
// deeper nodes in hidden aria-labelledby subtrees.
bool AXObject::IsHiddenForTextAlternativeCalculation() const {
  // aria-hidden=false allows hidden contents to be used in name from contents.
  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
    return false;

  auto* node = GetNode();
  if (!node)
    return false;

  // Display-locked elements are available for text/name resolution.
  if (DisplayLockUtilities::NearestLockedExclusiveAncestor(*node))
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

  // If this is hidden but its parent isn't, then it appears the hiding style
  // targeted this node directly. Do not recurse into it for name from contents.
  return IsHiddenViaStyle() &&
         (!ParentObject() || !ParentObject()->IsHiddenViaStyle());
}

String AXObject::AriaTextAlternative(bool recursive,
                                     bool in_aria_labelled_by_traversal,
                                     AXObjectSet& visited,
                                     ax::mojom::blink::NameFrom& name_from,
                                     AXRelatedObjectVector* related_objects,
                                     NameSources* name_sources,
                                     bool* found_text_alternative) const {
  String text_alternative;
  bool already_visited = visited.Contains(this);
  visited.insert(this);

  // Step 2A from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!in_aria_labelled_by_traversal &&
      IsHiddenForTextAlternativeCalculation()) {
    *found_text_alternative = true;
    return String();
  }

  // Step 2B from: http://www.w3.org/TR/accname-aam-1.1
  // If you change this logic, update AXNodeObject::nameFromLabelElement, too.
  if (!in_aria_labelled_by_traversal && !already_visited) {
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
      ElementsFromAttribute(elements_from_attribute, attr, ids);

      const AtomicString& aria_labelledby = GetAttribute(attr);

      if (!aria_labelledby.IsNull()) {
        if (name_sources)
          name_sources->back().attribute_value = aria_labelledby;

        // Operate on a copy of |visited| so that if |name_sources| is not
        // null, the set of visited objects is preserved unmodified for future
        // calculations.
        AXObjectSet visited_copy = visited;
        text_alternative = TextFromElements(
            true, visited, elements_from_attribute, related_objects);
        if (!ids.IsEmpty())
          AXObjectCache().UpdateReverseRelations(this, ids);
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
  if (!aria_label.IsEmpty()) {
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

      String result = RecursiveTextAlternative(
          *ax_element, in_aria_labelledby_traversal, visited);
      visited.insert(ax_element);
      local_related_objects.push_back(
          MakeGarbageCollected<NameSourceRelatedObject>(ax_element, result));
      if (!result.IsEmpty()) {
        if (!accumulated_text.IsEmpty())
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

void AXObject::TokenVectorFromAttribute(Vector<String>& tokens,
                                        const QualifiedName& attribute) const {
  Node* node = this->GetNode();
  if (!node || !node->IsElementNode())
    return;

  String attribute_value = GetAttribute(attribute).GetString();
  if (attribute_value.IsEmpty())
    return;

  attribute_value = attribute_value.SimplifyWhiteSpace();
  attribute_value.Split(' ', tokens);
}

void AXObject::ElementsFromAttribute(HeapVector<Member<Element>>& elements,
                                     const QualifiedName& attribute,
                                     Vector<String>& ids) const {
  // We compute the attr-associated elements, which are either explicitly set
  // element references set via the IDL, or computed from the content attribute.
  TokenVectorFromAttribute(ids, attribute);
  Element* element = GetElement();
  if (!element)
    return;

  base::Optional<HeapVector<Member<Element>>> attr_associated_elements =
      element->GetElementArrayAttribute(attribute);
  if (!attr_associated_elements)
    return;

  for (const auto& element : attr_associated_elements.value())
    elements.push_back(element);
}

void AXObject::AriaLabelledbyElementVector(
    HeapVector<Member<Element>>& elements,
    Vector<String>& ids) const {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  ElementsFromAttribute(elements, html_names::kAriaLabelledbyAttr, ids);
  if (!ids.size())
    ElementsFromAttribute(elements, html_names::kAriaLabeledbyAttr, ids);
}

String AXObject::TextFromAriaLabelledby(AXObjectSet& visited,
                                        AXRelatedObjectVector* related_objects,
                                        Vector<String>& ids) const {
  HeapVector<Member<Element>> elements;
  AriaLabelledbyElementVector(elements, ids);
  return TextFromElements(true, visited, elements, related_objects);
}

String AXObject::TextFromAriaDescribedby(AXRelatedObjectVector* related_objects,
                                         Vector<String>& ids) const {
  AXObjectSet visited;
  HeapVector<Member<Element>> elements;
  ElementsFromAttribute(elements, html_names::kAriaDescribedbyAttr, ids);
  return TextFromElements(true, visited, elements, related_objects);
}

AccessibilityOrientation AXObject::Orientation() const {
  // In ARIA 1.1, the default value for aria-orientation changed from
  // horizontal to undefined.
  return kAccessibilityOrientationUndefined;
}

void AXObject::LoadInlineTextBoxes() {}

AXObject* AXObject::NextOnLine() const {
  return nullptr;
}

AXObject* AXObject::PreviousOnLine() const {
  return nullptr;
}

base::Optional<const DocumentMarker::MarkerType>
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
    return base::nullopt;
  if (EqualIgnoringASCIICase(aria_invalid_value, "spelling"))
    return DocumentMarker::kSpelling;
  if (EqualIgnoringASCIICase(aria_invalid_value, "grammar"))
    return DocumentMarker::kGrammar;
  return base::nullopt;
}

void AXObject::TextCharacterOffsets(Vector<int>&) const {}

void AXObject::GetWordBoundaries(Vector<int>& word_starts,
                                 Vector<int>& word_ends) const {}

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
  if (IsTextControl())
    return ax::mojom::blink::DefaultActionVerb::kActivate;

  if (IsCheckable()) {
    return CheckedState() != ax::mojom::blink::CheckedState::kTrue
               ? ax::mojom::blink::DefaultActionVerb::kCheck
               : ax::mojom::blink::DefaultActionVerb::kUncheck;
  }

  // If this object cannot receive focus and has a button role, use click as
  // the default action. On the AuraLinux platform, the press action is a
  // signal to users that they can trigger the action using the keyboard, while
  // a click action means the user should trigger the action via a simulated
  // click. If this object cannot receive focus, it's impossible to trigger it
  // with a key press.
  if (RoleValue() == ax::mojom::blink::Role::kButton && !CanSetFocusAttribute())
    return ax::mojom::blink::DefaultActionVerb::kClick;

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

bool IsGlobalARIAAttribute(const AtomicString& name) {
  if (!name.StartsWith("ARIA"))
    return false;
  if (name.StartsWith("ARIA-ATOMIC"))
    return true;
  if (name.StartsWith("ARIA-BUSY"))
    return true;
  if (name.StartsWith("ARIA-CONTROLS"))
    return true;
  if (name.StartsWith("ARIA-CURRENT"))
    return true;
  if (name.StartsWith("ARIA-DESCRIBEDBY"))
    return true;
  if (name.StartsWith("ARIA-DETAILS"))
    return true;
  if (name.StartsWith("ARIA-DISABLED"))
    return true;
  if (name.StartsWith("ARIA-DROPEFFECT"))
    return true;
  if (name.StartsWith("ARIA-ERRORMESSAGE"))
    return true;
  if (name.StartsWith("ARIA-FLOWTO"))
    return true;
  if (name.StartsWith("ARIA-GRABBED"))
    return true;
  if (name.StartsWith("ARIA-HASPOPUP"))
    return true;
  if (name.StartsWith("ARIA-HIDDEN"))
    return true;
  if (name.StartsWith("ARIA-INVALID"))
    return true;
  if (name.StartsWith("ARIA-KEYSHORTCUTS"))
    return true;
  if (name.StartsWith("ARIA-LABEL"))
    return true;
  if (name.StartsWith("ARIA-LABELEDBY"))
    return true;
  if (name.StartsWith("ARIA-LABELLEDBY"))
    return true;
  if (name.StartsWith("ARIA-LIVE"))
    return true;
  if (name.StartsWith("ARIA-OWNS"))
    return true;
  if (name.StartsWith("ARIA-RELEVANT"))
    return true;
  if (name.StartsWith("ARIA-ROLEDESCRIPTION"))
    return true;
  return false;
}

bool AXObject::HasGlobalARIAAttribute() const {
  auto* element = GetElement();
  if (!element)
    return false;

  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    auto name = attr.GetName().LocalNameUpper();
    if (IsGlobalARIAAttribute(name))
      return true;
  }
  if (!element->DidAttachInternals())
    return false;
  const auto& internals_attributes =
      element->EnsureElementInternals().GetAttributes();
  for (const QualifiedName& attr : internals_attributes.Keys()) {
    if (IsGlobalARIAAttribute(attr.LocalNameUpper()))
      return true;
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
  return !live_region.IsEmpty();
}

bool AXObject::IsActiveLiveRegionRoot() const {
  const AtomicString& live_region = LiveRegionStatus();
  return !live_region.IsEmpty() && !EqualIgnoringASCIICase(live_region, "off");
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
  if (live_region_status.IsEmpty()) {
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
  if (relevant.IsEmpty())
    return default_live_region_relevant;

  return relevant;
}

AXRestriction AXObject::Restriction() const {
  // According to ARIA, all elements of the base markup can be disabled.
  // According to CORE-AAM, any focusable descendant of aria-disabled
  // ancestor is also disabled.
  bool is_disabled;
  if (HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kDisabled,
                                    is_disabled)) {
    // Has aria-disabled, overrides native markup determining disabled.
    if (is_disabled)
      return kRestrictionDisabled;
  } else if (CanSetFocusAttribute() && IsDescendantOfDisabledNode()) {
    // aria-disabled on an ancestor propagates to focusable descendants.
    return kRestrictionDisabled;
  }

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
  return aria_role_;
}

ax::mojom::blink::Role AXObject::DetermineAriaRoleAttribute() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsNull() || aria_role.IsEmpty())
    return ax::mojom::blink::Role::kUnknown;

  ax::mojom::blink::Role role = AriaRoleToWebCoreRole(aria_role);

  // ARIA states if an item can get focus, it should not be presentational.
  // It also states user agents should ignore the presentational role if
  // the element has global ARIA states and properties.
  if ((role == ax::mojom::blink::Role::kNone ||
       role == ax::mojom::blink::Role::kPresentational) &&
      (CanSetFocusAttribute() || HasGlobalARIAAttribute()))
    return ax::mojom::blink::Role::kUnknown;

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
    if (IsNativeTextControl())
      role = ax::mojom::blink::Role::kTextFieldWithComboBox;
    else if (GetElement() && GetElement()->SupportsFocus())
      role = ax::mojom::blink::Role::kComboBoxMenuButton;
  }

  if (role != ax::mojom::blink::Role::kUnknown)
    return role;

  return ax::mojom::blink::Role::kUnknown;
}

bool AXObject::IsEditableRoot() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_editable_root_;
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

AXObject* AXObject::ElementAccessibilityHitTest(const IntPoint& point) const {
  // Check if there are any mock elements that need to be handled.
  for (const auto& child : ChildrenIncludingIgnored()) {
    if (child->IsMockObject() &&
        child->GetBoundsInFrameCoordinates().Contains(point))
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

AXObject::InOrderTraversalIterator AXObject::GetInOrderTraversalIterator() {
  return InOrderTraversalIterator(*this);
}

int AXObject::ChildCountIncludingIgnored() const {
  return int{ChildrenIncludingIgnored().size()};
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
    NOTREACHED() << "We don't support iterating children of objects excluded "
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
  return int{UnignoredChildren().size()};
}

AXObject* AXObject::UnignoredChildAt(int index) const {
  const AXObjectVector unignored_children = UnignoredChildren();
  if (index < 0 || index >= int{unignored_children.size()})
    return nullptr;
  return unignored_children[index];
}

AXObject* AXObject::UnignoredNextSibling() const {
  if (AccessibilityIsIgnored()) {
    NOTREACHED() << "We don't support finding unignored siblings for ignored "
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
  if (!parent_) {
    DCHECK(!IsVirtualObject())
        << "A virtual object must have a parent, and cannot exist without one. "
           "The parent is set when the object is constructed.";
    parent_ = ComputeParent();
    DCHECK(parent_ || IsA<Document>(GetNode()))
        << "The following node should have a parent: " << GetNode();
  }

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

// Only use layout object traversal for pseudo elements and their descendants.
bool AXObject::ShouldUseLayoutObjectTraversalForChildren() const {
  if (!GetLayoutObject())
    return false;

  // If no node, this is an anonymous layout object. The only way this can be
  // reached is inside a pseudo element subtree.
  if (!GetNode()) {
    DCHECK(GetLayoutObject()->IsAnonymous());
    DCHECK(AXObjectCacheImpl::IsPseudoElementDescendant(*GetLayoutObject()));
    return true;
  }

  // The only other case for using layout builder traversal is for a pseudo
  // element, such as ::before. Pseudo element child text and images are not
  // visibited by LayoutBuilderTraversal.
  return GetNode()->IsPseudoElement();
}

void AXObject::UpdateChildrenIfNecessary() {
#if DCHECK_IS_ON()
  DCHECK(GetDocument()) << ToString(true, true);
  DCHECK(GetDocument()->IsActive());
  DCHECK(!GetDocument()->IsDetached());
  DCHECK(GetDocument()->GetPage());
  DCHECK(GetDocument()->View());
  DCHECK(!AXObjectCache().HasBeenDisposed());

  // Store previous children for DCHECK to ensure no lost children, left
  // without parents.
  HashSet<AXID> prev_children_axids;
  for (const auto& child : children_) {
    if (!child->IsDetached()) {
      AXID prev_child_axid = child->AXObjectID();
      prev_children_axids.insert(prev_child_axid);
      DCHECK(AXObjectCache().ObjectFromAXID(prev_child_axid));
    }
  }
#endif

  if (!NeedsToUpdateChildren())
    return;

  // Clear current children and get new children.
  ClearChildren();
  if (IsMenuList()) {
    // AXMenuList is special and keeps its popup child.
    DCHECK_LE(children_.size(), 1U);
  } else {
    // Ensure children have been correctly cleared.
    DCHECK_EQ(children_.size(), 0U)
        << GetNode() << "  with " << children_.size() << " children";
  }
  AddChildren();

#if DCHECK_IS_ON()
  // Remove current children's AXIDs from set so that we can see what children
  // have been lost.
  for (const auto& child : children_)
    prev_children_axids.erase(child->AXObjectID());

  // Report lost children. These are children that had |this| as a parent at
  // the start of this method, but no longer do, yet the cache still considers
  // them alive.
  bool has_lost_children = false;
  for (AXID prev_child_axid : prev_children_axids) {
    AXObject* prev_child = nullptr;
    if (prev_child_axid)
      AXObjectCache().ObjectFromAXID(prev_child_axid);
    if (prev_child) {
      has_lost_children = true;
      LOG(ERROR) << "Lost child: " << prev_child->ToString(true, true)
                 << "\nThis: " << ToString(true, true);
    }
  }

  DCHECK(!has_lost_children);
#endif
}

bool AXObject::NeedsToUpdateChildren() const {
  DCHECK(!children_dirty_ || CanHaveChildren())
      << "Needs to update children but cannot have children: " << GetNode();
  return children_dirty_;
}

void AXObject::SetNeedsToUpdateChildren() {
  children_dirty_ = CanHaveChildren();
}

void AXObject::ClearChildren() {
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

  for (const auto& child : children_) {
    if (child->CachedParentObject() == this)
      child->DetachFromParent();
  }

  // Detach anything else that may still point to this parent.
  if (GetNode()) {
    DetachSelectSlotChildFromParent();
    // Only direct AXObjects for direct DOM children can be unincluded.
    // If they were unincluded, then they couldn't detach from parent in the
    // first loop above, because they aren't in children_.
    for (Node* child_node = LayoutTreeBuilderTraversal::FirstChild(*GetNode());
         child_node;
         child_node = LayoutTreeBuilderTraversal::NextSibling(*child_node)) {
      AXObject* ax_child_from_node = AXObjectCache().Get(child_node);
      if (ax_child_from_node &&
          ax_child_from_node->CachedParentObject() == this) {
        // Child was not cleared from first loop.
        // It must have been an unincluded node who's parent is this,
        // although it may now be included since the children were last updated.
        // Check current parent first. It may be owned by another node.
        ax_child_from_node->DetachFromParent();
      }
    }
  }

  children_.clear();
}

// TODO(accessibility) Refactor AXMenuListObject to be an AXNodeObject
// that exposes its contents via the shadow DOM, and remove this.
void AXObject::DetachSelectSlotChildFromParent() {
  // An <select>is  assigned a shadow root by the user agent, in order to
  // help with layout. The last child is a <slot> container that holds the
  // options. It will have an AXObject that points to |this| as a parent.
  // Detaching it is important in order to guarantee that |this| detached object
  // will not be reached while traversing parents, e.g. of <option> children,
  // which still exist even if the <select> is invalidated for a size change.

  DCHECK(GetNode());

  HTMLSelectElement* select_element = DynamicTo<HTMLSelectElement>(GetNode());
  if (!select_element)
    return;

  ShadowRoot* shadow_root = select_element->GetShadowRoot();
  if (!shadow_root)
    return;

  DCHECK(shadow_root->IsUserAgent());

  Node* shadow_contents = shadow_root->lastChild();
  if (!shadow_contents) {
    NOTREACHED();
    return;
  }

  DCHECK(IsA<HTMLSlotElement>(shadow_contents));

  if (AXObject* ax_shadow_contents = AXObjectCache().Get(shadow_contents))
    ax_shadow_contents->DetachFromParent();
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
  if (!lang.IsEmpty())
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
        if (!languages.IsEmpty())
          return AtomicString(languages[0].StripWhiteSpace());
      }

      if (document->GetPage()) {
        // Use the first accept language preference if present.
        const String accept_languages =
            document->GetPage()->GetChromeClient().AcceptLanguages();
        Vector<String> languages;
        accept_languages.Split(',', languages);
        if (!languages.IsEmpty())
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
         To<LayoutBox>(GetLayoutObject())->CanBeScrolledAndHasScrollableArea();
}

IntPoint AXObject::GetScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->ScrollOffsetInt().Width(),
                  area->ScrollOffsetInt().Height());
}

IntPoint AXObject::MinimumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->MinimumScrollOffsetInt().Width(),
                  area->MinimumScrollOffsetInt().Height());
}

IntPoint AXObject::MaximumScrollOffset() const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return IntPoint();

  return IntPoint(area->MaximumScrollOffsetInt().Width(),
                  area->MaximumScrollOffsetInt().Height());
}

void AXObject::SetScrollOffset(const IntPoint& offset) const {
  ScrollableArea* area = GetScrollableAreaIfScrollable();
  if (!area)
    return;

  // TODO(bokan): This should potentially be a UserScroll.
  area->SetScrollOffset(ScrollOffset(offset.X(), offset.Y()),
                        mojom::blink::ScrollType::kProgrammatic);
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

  if (row_count > int{RowCount()})
    return row_count;

  // Spec says that if all of the rows are present in the DOM, it is
  // not necessary to set this attribute as the user agent can
  // automatically calculate the total number of rows.
  // It returns 0 in order not to set this attribute.
  if (row_count == int{RowCount()} || row_count != -1)
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
                                 FloatRect& out_bounds_in_container,
                                 SkMatrix44& out_container_transform,
                                 bool* clips_children) const {
  *out_container = nullptr;
  out_bounds_in_container = FloatRect();
  out_container_transform.setIdentity();

  // First check if it has explicit bounds, for example if this element is tied
  // to a canvas path. When explicit coordinates are provided, the ID of the
  // explicit container element that the coordinates are relative to must be
  // provided too.
  if (!explicit_element_rect_.IsEmpty()) {
    *out_container = AXObjectCache().ObjectFromAXID(explicit_container_id_);
    if (*out_container) {
      out_bounds_in_container = FloatRect(explicit_element_rect_);
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
    if (IsWebArea())
      *clips_children = true;
    else
      *clips_children = layout_object->HasNonVisibleOverflow();
  }

  if (IsWebArea()) {
    if (LocalFrameView* view = layout_object->GetFrame()->View()) {
      out_bounds_in_container.SetSize(FloatSize(view->Size()));

      // If it's a popup, account for the popup window's offset.
      if (view->GetPage()->GetChromeClient().IsPopup()) {
        IntRect frame_rect = view->FrameToScreen(view->FrameRect());
        LocalFrameView* root_view =
            AXObjectCache().GetDocument().GetFrame()->View();
        IntRect root_frame_rect =
            root_view->FrameToScreen(root_view->FrameRect());

        // Screen coordinates are in DIP without device scale factor applied.
        // Accessibility expects device scale factor applied here which is
        // unapplied at the destination AXTree.
        float scale_factor =
            view->GetPage()->GetChromeClient().WindowToViewportScalar(
                layout_object->GetFrame(), 1.0f);
        out_bounds_in_container.SetLocation(
            FloatPoint(scale_factor * (frame_rect.X() - root_frame_rect.X()),
                       scale_factor * (frame_rect.Y() - root_frame_rect.Y())));
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
            if (container->IsWebArea())
              break;
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
        FloatRect(To<LayoutBox>(layout_object)->PhysicalContentBoxRect());
  }

  // If the container has a scroll offset, subtract that out because we want our
  // bounds to be relative to the *unscrolled* position of the container object.
  if (auto* scrollable_area = container->GetScrollableAreaIfScrollable())
    out_bounds_in_container.Move(scrollable_area->GetScrollOffset());

  // Compute the transform between the container's coordinate space and this
  // object.
  TransformationMatrix transform = layout_object->LocalToAncestorTransform(
      To<LayoutBoxModelObject>(container_layout_object));

  // If the transform is just a simple translation, apply that to the
  // bounding box, but if it's a non-trivial transformation like a rotation,
  // scaling, etc. then return the full matrix instead.
  if (transform.IsIdentityOr2DTranslation()) {
    out_bounds_in_container.Move(transform.To2DTranslation());
  } else {
    out_container_transform = TransformationMatrix::ToSkMatrix44(transform);
  }
}

FloatRect AXObject::LocalBoundingBoxRectForAccessibility() {
  if (!GetLayoutObject())
    return FloatRect();
  DCHECK(GetLayoutObject()->IsText());
  UpdateCachedAttributeValuesIfNeeded();
  return cached_local_bounding_box_rect_for_accessibility_;
}

LayoutRect AXObject::GetBoundsInFrameCoordinates() const {
  AXObject* container = nullptr;
  FloatRect bounds;
  SkMatrix44 transform;
  GetRelativeBounds(&container, bounds, transform);
  FloatRect computed_bounds(0, 0, bounds.Width(), bounds.Height());
  while (container && container != this) {
    computed_bounds.Move(bounds.X(), bounds.Y());
    if (!container->IsWebArea()) {
      computed_bounds.Move(-container->GetScrollOffset().X(),
                           -container->GetScrollOffset().Y());
    }
    if (!transform.isIdentity()) {
      TransformationMatrix transformation_matrix(transform);
      transformation_matrix.MapRect(computed_bounds);
    }
    container->GetRelativeBounds(&container, bounds, transform);
  }
  return LayoutRect(computed_bounds);
}

//
// Modify or take an action on an object.
//

bool AXObject::PerformAction(const ui::AXActionData& action_data) {
  switch (action_data.action) {
    case ax::mojom::blink::Action::kBlur:
      return RequestFocusAction();
    case ax::mojom::blink::Action::kClearAccessibilityFocus:
      return InternalClearAccessibilityFocusAction();
    case ax::mojom::blink::Action::kDecrement:
      return RequestDecrementAction();
    case ax::mojom::blink::Action::kDoDefault:
      return RequestClickAction();
    case ax::mojom::blink::Action::kFocus:
      return RequestFocusAction();
    case ax::mojom::blink::Action::kIncrement:
      return RequestIncrementAction();
    case ax::mojom::blink::Action::kScrollToPoint:
      return RequestScrollToGlobalPointAction(
          IntPoint(action_data.target_point));
    case ax::mojom::blink::Action::kSetAccessibilityFocus:
      return InternalSetAccessibilityFocusAction();
    case ax::mojom::blink::Action::kSetScrollOffset:
      SetScrollOffset(IntPoint(action_data.target_point));
      return true;
    case ax::mojom::blink::Action::kSetSequentialFocusNavigationStartingPoint:
      return RequestSetSequentialFocusNavigationStartingPointAction();
      break;
    case ax::mojom::blink::Action::kSetValue:
      return RequestSetValueAction(
          WTF::String::FromUTF8(action_data.value.c_str()));
    case ax::mojom::blink::Action::kShowContextMenu:
      return RequestShowContextMenuAction();

    case ax::mojom::blink::Action::kAnnotatePageImages:
    case ax::mojom::blink::Action::kCollapse:
    case ax::mojom::blink::Action::kCustomAction:
    case ax::mojom::blink::Action::kExpand:
    case ax::mojom::blink::Action::kGetImageData:
    case ax::mojom::blink::Action::kGetTextLocation:
    case ax::mojom::blink::Action::kHideTooltip:
    case ax::mojom::blink::Action::kHitTest:
    case ax::mojom::blink::Action::kInternalInvalidateTree:
    case ax::mojom::blink::Action::kLoadInlineTextBoxes:
    case ax::mojom::blink::Action::kNone:
    case ax::mojom::blink::Action::kReplaceSelectedText:
    case ax::mojom::blink::Action::kScrollBackward:
    case ax::mojom::blink::Action::kScrollDown:
    case ax::mojom::blink::Action::kScrollForward:
    case ax::mojom::blink::Action::kScrollLeft:
    case ax::mojom::blink::Action::kScrollRight:
    case ax::mojom::blink::Action::kScrollToMakeVisible:
    case ax::mojom::blink::Action::kScrollUp:
    case ax::mojom::blink::Action::kSetSelection:
    case ax::mojom::blink::Action::kShowTooltip:
    case ax::mojom::blink::Action::kSignalEndOfTest:
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

  Element* element = GetElement();
  if (!element && GetNode())
    element = GetNode()->parentElement();

  if (IsTextControl())
    return OnNativeFocusAction();

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

bool AXObject::RequestScrollToGlobalPointAction(const IntPoint& point) {
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
    const IntRect& subfocus,
    blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
    blink::mojom::blink::ScrollAlignment vertical_scroll_alignment) {
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

bool AXObject::InternalClearAccessibilityFocusAction() {
  return false;
}

bool AXObject::InternalSetAccessibilityFocusAction() {
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

bool AXObject::OnNativeScrollToMakeVisibleAction() const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;
  PhysicalRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  layout_object->ScrollRectToVisible(
      target_rect,
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
    const IntRect& rect,
    blink::mojom::blink::ScrollAlignment horizontal_scroll_alignment,
    blink::mojom::blink::ScrollAlignment vertical_scroll_alignment) const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;

  PhysicalRect target_rect =
      layout_object->LocalToAbsoluteRect(PhysicalRect(rect));
  layout_object->ScrollRectToVisible(
      target_rect, ScrollAlignment::CreateScrollIntoViewParams(
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
    const IntPoint& global_point) const {
  LayoutObject* layout_object = GetLayoutObjectForNativeScrollAction();
  if (!layout_object)
    return false;

  PhysicalRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  target_rect.Move(-PhysicalOffset(global_point));
  layout_object->ScrollRectToVisible(
      target_rect,
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

  ContextMenuAllowedScope scope;
  document->GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(
      element, kMenuSourceKeyboard);
  return true;
}

void AXObject::SelectionChanged() {
  if (AXObject* parent = CachedParentObject())
    parent->SelectionChanged();
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
bool AXObject::HasARIAOwns(Element* element) {
  if (!element)
    return false;

  // A LayoutObject is not required, because an invisible object can still
  // use aria-owns to point to visible children.

  const AtomicString& aria_owns =
      element->FastGetAttribute(html_names::kAriaOwnsAttr);

  // TODO: do we need to check !AriaOwnsElements.empty() ? Is that fundamentally
  // different from HasExplicitlySetAttrAssociatedElements()? And is an element
  // even necessary in the case of virtual nodes?
  return !aria_owns.IsEmpty() ||
         element->HasExplicitlySetAttrAssociatedElements(
             html_names::kAriaOwnsAttr);
}

ax::mojom::blink::Role AXObject::AriaRoleToWebCoreRole(const String& value) {
  DCHECK(!value.IsEmpty());

  static const ARIARoleMap* role_map = CreateARIARoleMap();

  Vector<String> role_vector;
  value.Split(' ', role_vector);
  ax::mojom::blink::Role role = ax::mojom::blink::Role::kUnknown;
  for (const auto& child : role_vector) {
    role = role_map->at(child);
    if (role != ax::mojom::blink::Role::kUnknown)
      return role;
  }

  return role;
}

bool AXObject::SupportsNameFromContents(bool recursive) const {
  // ARIA 1.1, section 5.2.7.5.
  bool result = false;

  switch (RoleValue()) {
    // ----- NameFrom: contents -------------------------
    // Get their own name from contents, or contribute to ancestors
    case ax::mojom::blink::Role::kAnchor:
    case ax::mojom::blink::Role::kButton:
    case ax::mojom::blink::Role::kCell:
    case ax::mojom::blink::Role::kCheckBox:
    case ax::mojom::blink::Role::kColumnHeader:
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

    // ----- Conditional: contribute to ancestor only, unless focusable -------
    // Some objects can contribute their contents to ancestor names, but
    // only have their own name if they are focusable
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
    case ax::mojom::blink::Role::kGenericContainer:
    case ax::mojom::blink::Role::kHeaderAsNonLandmark:
    case ax::mojom::blink::Role::kImageMap:
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
    case ax::mojom::blink::Role::kPresentational:
    case ax::mojom::blink::Role::kRegion:
    // Spec says we should always expose the name on rows,
    // but for performance reasons we only do it
    // if the row might receive focus
    case ax::mojom::blink::Role::kRow:
    case ax::mojom::blink::Role::kRuby:
    case ax::mojom::blink::Role::kSection:
    case ax::mojom::blink::Role::kStrong:
    case ax::mojom::blink::Role::kTime:
      if (recursive) {
        // Use contents if part of a recursive name computation.
        result = true;
      } else {
        // Use contents if focusable, so that there is a name in the case
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
        bool is_focusable_scrollable =
            RuntimeEnabledFeatures::KeyboardFocusableScrollersEnabled() &&
            IsUserScrollable();
        bool is_focusable = is_focusable_scrollable || CanSetFocusAttribute();
        result = is_focusable && !IsEditable() &&
                 !GetAOMPropertyOrARIAAttribute(
                     AOMRelationProperty::kActiveDescendant);
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
      bool is_main_frame =
          document.GetFrame() && document.GetFrame()->IsMainFrame();
      bool is_inside_portal =
          document.GetPage() && document.GetPage()->InsidePortal();
      return is_inside_portal && is_main_frame;
    }

    case ax::mojom::blink::Role::kCaret:
    case ax::mojom::blink::Role::kClient:
    case ax::mojom::blink::Role::kColumn:
    case ax::mojom::blink::Role::kDesktop:
    case ax::mojom::blink::Role::kKeyboard:
    case ax::mojom::blink::Role::kIgnored:
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
    return std::any_of(
        UnignoredAncestorsBegin(), UnignoredAncestorsEnd(),
        [](const AXObject& ancestor) {
          return ancestor.RoleValue() == ax::mojom::blink::Role::kGrid ||
                 ancestor.RoleValue() == ax::mojom::blink::Role::kTreeGrid;
        });
  }

  return false;
}

ax::mojom::blink::Role AXObject::ButtonRoleType() const {
  // If aria-pressed is present, then it should be exposed as a toggle button.
  // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
  if (AriaPressedIsPresent())
    return ax::mojom::blink::Role::kToggleButton;
  if (HasPopup() != ax::mojom::blink::HasPopup::kFalse)
    return ax::mojom::blink::Role::kPopUpButton;
  // We don't contemplate RadioButtonRole, as it depends on the input
  // type.

  return ax::mojom::blink::Role::kButton;
}

// static
const AtomicString& AXObject::RoleName(ax::mojom::blink::Role role) {
  static const Vector<AtomicString>* role_name_vector = CreateRoleNameVector();

  return role_name_vector->at(static_cast<wtf_size_t>(role));
}

// static
const AtomicString& AXObject::InternalRoleName(ax::mojom::blink::Role role) {
  static const Vector<AtomicString>* internal_role_name_vector =
      CreateInternalRoleNameVector();

  return internal_role_name_vector->at(static_cast<wtf_size_t>(role));
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
  while (!ancestors1.IsEmpty() && !ancestors2.IsEmpty() &&
         ancestors1.back() == ancestors2.back()) {
    common_ancestor = ancestors1.back();
    ancestors1.pop_back();
    ancestors2.pop_back();
  }

  if (common_ancestor) {
    if (!ancestors1.IsEmpty())
      *index_in_ancestor1 = ancestors1.back()->IndexInParent();
    if (!ancestors2.IsEmpty())
      *index_in_ancestor2 = ancestors2.back()->IndexInParent();
  }

  return common_ancestor;
}

String AXObject::ToString(bool verbose, bool cached_values_only) const {
  // Build a friendly name for debugging the object.
  // If verbose, build a longer name name in the form of:
  // CheckBox axid#28 <input.someClass#cbox1> name="checkbox"
  String string_builder =
      AXObject::InternalRoleName(RoleValue()).GetString().EncodeForDebugging();

  if (IsDetached())
    string_builder = string_builder + " (detached)";

  if (verbose) {
    string_builder = string_builder + " axid#" + String::Number(AXObjectID());
    // Add useful HTML element info, like <div.myClass#myId>.
    if (GetElement())
      string_builder = string_builder + " " + GetElementString(GetElement());

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
      if (!cached_values_only) {
        AXObject::IgnoredReasons reasons;
        ComputeAccessibilityIsIgnored(&reasons);
        string_builder = string_builder + GetIgnoredReasonsDebugString(reasons);
      }
#endif
      if (cached_values_only ? !LastKnownIsIncludedInTreeValue()
                             : !AccessibilityIsIncludedInTree())
        string_builder = string_builder + " isRemovedFromTree";
    }
    if (GetNode()) {
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
    if (const AXObject* aria_hidden_root = AriaHiddenRoot()) {
      string_builder = string_builder + " ariaHiddenRoot";
      if (aria_hidden_root != this) {
        string_builder =
            string_builder + GetElementString(aria_hidden_root->GetElement());
      }
    }
    if (GetDocument() && GetDocument()->Lifecycle().GetState() <
                             DocumentLifecycle::kLayoutClean) {
      string_builder = string_builder + " styleInfoUnavailable";
    } else if (IsHiddenViaStyle()) {
      string_builder = string_builder + " isHiddenViaCSS";
    }
    if (GetNode() && GetNode()->IsInert())
      string_builder = string_builder + " isInert";
    if (NeedsToUpdateChildren())
      string_builder = string_builder + " needsToUpdateChildren";
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
