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

#include "SkMatrix44.h"
#include "third_party/blink/public/platform/web_scroll_into_view_params.h"
#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/core/aom/accessible_node_list.h"
#include "third_party/blink/renderer/core/css/resolver/style_resolver.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/user_gesture_indicator.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/visible_position.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/html_dialog_element.h"
#include "third_party/blink/renderer/core/html/html_frame_owner_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/input/context_menu_allowed_scope.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/page/chrome_client.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/accessibility/ax_object_cache_impl.h"
#include "third_party/blink/renderer/modules/accessibility/ax_range.h"
#include "third_party/blink/renderer/modules/accessibility/ax_sparse_attribute_setter.h"
#include "third_party/blink/renderer/platform/language.h"
#include "third_party/blink/renderer/platform/scroll/scroll_alignment.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/not_found.h"
#include "third_party/blink/renderer/platform/wtf/std_lib_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

using blink::WebLocalizedString;

namespace blink {

using namespace HTMLNames;

namespace {

struct RoleHashTraits : HashTraits<ax::mojom::Role> {
  static const bool kEmptyValueIsZero = true;
  static ax::mojom::Role EmptyValue() { return ax::mojom::Role::kUnknown; }
};

using ARIARoleMap = HashMap<String,
                            ax::mojom::Role,
                            CaseFoldingHash,
                            HashTraits<String>,
                            RoleHashTraits>;

struct RoleEntry {
  const char* aria_role;
  ax::mojom::Role webcore_role;
};

// Mapping of ARIA role name to internal role name.
const RoleEntry kRoles[] = {
    {"alert", ax::mojom::Role::kAlert},
    {"alertdialog", ax::mojom::Role::kAlertDialog},
    {"application", ax::mojom::Role::kApplication},
    {"article", ax::mojom::Role::kArticle},
    {"banner", ax::mojom::Role::kBanner},
    {"blockquote", ax::mojom::Role::kBlockquote},
    {"button", ax::mojom::Role::kButton},
    {"caption", ax::mojom::Role::kCaption},
    {"cell", ax::mojom::Role::kCell},
    {"checkbox", ax::mojom::Role::kCheckBox},
    {"columnheader", ax::mojom::Role::kColumnHeader},
    {"combobox", ax::mojom::Role::kComboBoxGrouping},
    {"complementary", ax::mojom::Role::kComplementary},
    {"contentinfo", ax::mojom::Role::kContentInfo},
    {"definition", ax::mojom::Role::kDefinition},
    {"dialog", ax::mojom::Role::kDialog},
    {"directory", ax::mojom::Role::kDirectory},
    // -------------------------------------------------
    // DPub Roles:
    // www.w3.org/TR/dpub-aam-1.0/#mapping_role_table
    {"doc-abstract", ax::mojom::Role::kDocAbstract},
    {"doc-acknowledgments", ax::mojom::Role::kDocAcknowledgments},
    {"doc-afterword", ax::mojom::Role::kDocAfterword},
    {"doc-appendix", ax::mojom::Role::kDocAppendix},
    {"doc-backlink", ax::mojom::Role::kDocBackLink},
    {"doc-biblioentry", ax::mojom::Role::kDocBiblioEntry},
    {"doc-bibliography", ax::mojom::Role::kDocBibliography},
    {"doc-biblioref", ax::mojom::Role::kDocBiblioRef},
    {"doc-chapter", ax::mojom::Role::kDocChapter},
    {"doc-colophon", ax::mojom::Role::kDocColophon},
    {"doc-conclusion", ax::mojom::Role::kDocConclusion},
    {"doc-cover", ax::mojom::Role::kDocCover},
    {"doc-credit", ax::mojom::Role::kDocCredit},
    {"doc-credits", ax::mojom::Role::kDocCredits},
    {"doc-dedication", ax::mojom::Role::kDocDedication},
    {"doc-endnote", ax::mojom::Role::kDocEndnote},
    {"doc-endnotes", ax::mojom::Role::kDocEndnotes},
    {"doc-epigraph", ax::mojom::Role::kDocEpigraph},
    {"doc-epilogue", ax::mojom::Role::kDocEpilogue},
    {"doc-errata", ax::mojom::Role::kDocErrata},
    {"doc-example", ax::mojom::Role::kDocExample},
    {"doc-footnote", ax::mojom::Role::kDocFootnote},
    {"doc-foreword", ax::mojom::Role::kDocForeword},
    {"doc-glossary", ax::mojom::Role::kDocGlossary},
    {"doc-glossref", ax::mojom::Role::kDocGlossRef},
    {"doc-index", ax::mojom::Role::kDocIndex},
    {"doc-introduction", ax::mojom::Role::kDocIntroduction},
    {"doc-noteref", ax::mojom::Role::kDocNoteRef},
    {"doc-notice", ax::mojom::Role::kDocNotice},
    {"doc-pagebreak", ax::mojom::Role::kDocPageBreak},
    {"doc-pagelist", ax::mojom::Role::kDocPageList},
    {"doc-part", ax::mojom::Role::kDocPart},
    {"doc-preface", ax::mojom::Role::kDocPreface},
    {"doc-prologue", ax::mojom::Role::kDocPrologue},
    {"doc-pullquote", ax::mojom::Role::kDocPullquote},
    {"doc-qna", ax::mojom::Role::kDocQna},
    {"doc-subtitle", ax::mojom::Role::kDocSubtitle},
    {"doc-tip", ax::mojom::Role::kDocTip},
    {"doc-toc", ax::mojom::Role::kDocToc},
    // End DPub roles.
    // -------------------------------------------------
    {"document", ax::mojom::Role::kDocument},
    {"feed", ax::mojom::Role::kFeed},
    {"figure", ax::mojom::Role::kFigure},
    {"form", ax::mojom::Role::kForm},
    // -------------------------------------------------
    // ARIA Graphics module roles:
    // https://rawgit.com/w3c/graphics-aam/master/
    {"graphics-document", ax::mojom::Role::kGraphicsDocument},
    {"graphics-object", ax::mojom::Role::kGraphicsObject},
    {"graphics-symbol", ax::mojom::Role::kGraphicsSymbol},
    // End ARIA Graphics module roles.
    // -------------------------------------------------
    {"grid", ax::mojom::Role::kGrid},
    {"gridcell", ax::mojom::Role::kCell},
    {"group", ax::mojom::Role::kGroup},
    {"heading", ax::mojom::Role::kHeading},
    {"img", ax::mojom::Role::kImage},
    {"link", ax::mojom::Role::kLink},
    {"list", ax::mojom::Role::kList},
    {"listbox", ax::mojom::Role::kListBox},
    {"listitem", ax::mojom::Role::kListItem},
    {"log", ax::mojom::Role::kLog},
    {"main", ax::mojom::Role::kMain},
    {"marquee", ax::mojom::Role::kMarquee},
    {"math", ax::mojom::Role::kMath},
    {"menu", ax::mojom::Role::kMenu},
    {"menubar", ax::mojom::Role::kMenuBar},
    {"menuitem", ax::mojom::Role::kMenuItem},
    {"menuitemcheckbox", ax::mojom::Role::kMenuItemCheckBox},
    {"menuitemradio", ax::mojom::Role::kMenuItemRadio},
    {"navigation", ax::mojom::Role::kNavigation},
    {"none", ax::mojom::Role::kNone},
    {"note", ax::mojom::Role::kNote},
    {"option", ax::mojom::Role::kListBoxOption},
    {"paragraph", ax::mojom::Role::kParagraph},
    {"presentation", ax::mojom::Role::kPresentational},
    {"progressbar", ax::mojom::Role::kProgressIndicator},
    {"radio", ax::mojom::Role::kRadioButton},
    {"radiogroup", ax::mojom::Role::kRadioGroup},
    // TODO(accessibility) region should only be mapped
    // if name present. See http://crbug.com/840819.
    {"region", ax::mojom::Role::kRegion},
    {"row", ax::mojom::Role::kRow},
    {"rowheader", ax::mojom::Role::kRowHeader},
    {"scrollbar", ax::mojom::Role::kScrollBar},
    {"search", ax::mojom::Role::kSearch},
    {"searchbox", ax::mojom::Role::kSearchBox},
    {"separator", ax::mojom::Role::kSplitter},
    {"slider", ax::mojom::Role::kSlider},
    {"spinbutton", ax::mojom::Role::kSpinButton},
    {"status", ax::mojom::Role::kStatus},
    {"switch", ax::mojom::Role::kSwitch},
    {"tab", ax::mojom::Role::kTab},
    {"table", ax::mojom::Role::kTable},
    {"tablist", ax::mojom::Role::kTabList},
    {"tabpanel", ax::mojom::Role::kTabPanel},
    {"term", ax::mojom::Role::kTerm},
    {"text", ax::mojom::Role::kStaticText},
    {"textbox", ax::mojom::Role::kTextField},
    {"timer", ax::mojom::Role::kTimer},
    {"toolbar", ax::mojom::Role::kToolbar},
    {"tooltip", ax::mojom::Role::kTooltip},
    {"tree", ax::mojom::Role::kTree},
    {"treegrid", ax::mojom::Role::kTreeGrid},
    {"treeitem", ax::mojom::Role::kTreeItem}};

struct InternalRoleEntry {
  ax::mojom::Role webcore_role;
  const char* internal_role_name;
};

const InternalRoleEntry kInternalRoles[] = {
    {ax::mojom::Role::kNone, "None"},
    {ax::mojom::Role::kAbbr, "Abbr"},
    {ax::mojom::Role::kAlertDialog, "AlertDialog"},
    {ax::mojom::Role::kAlert, "Alert"},
    {ax::mojom::Role::kAnchor, "Anchor"},
    {ax::mojom::Role::kAnnotation, "Annotation"},
    {ax::mojom::Role::kApplication, "Application"},
    {ax::mojom::Role::kArticle, "Article"},
    {ax::mojom::Role::kAudio, "Audio"},
    {ax::mojom::Role::kBanner, "Banner"},
    {ax::mojom::Role::kBlockquote, "Blockquote"},
    {ax::mojom::Role::kButton, "Button"},
    {ax::mojom::Role::kCanvas, "Canvas"},
    {ax::mojom::Role::kCaption, "Caption"},
    {ax::mojom::Role::kCaret, "Caret"},
    {ax::mojom::Role::kCell, "Cell"},
    {ax::mojom::Role::kCheckBox, "CheckBox"},
    {ax::mojom::Role::kClient, "Client"},
    {ax::mojom::Role::kColorWell, "ColorWell"},
    {ax::mojom::Role::kColumnHeader, "ColumnHeader"},
    {ax::mojom::Role::kColumn, "Column"},
    {ax::mojom::Role::kComboBoxGrouping, "ComboBox"},
    {ax::mojom::Role::kComboBoxMenuButton, "ComboBox"},
    {ax::mojom::Role::kComplementary, "Complementary"},
    {ax::mojom::Role::kContentDeletion, "ContentDeletion"},
    {ax::mojom::Role::kContentInsertion, "ContentInsertion"},
    {ax::mojom::Role::kContentInfo, "ContentInfo"},
    {ax::mojom::Role::kDate, "Date"},
    {ax::mojom::Role::kDateTime, "DateTime"},
    {ax::mojom::Role::kDefinition, "Definition"},
    {ax::mojom::Role::kDescriptionListDetail, "DescriptionListDetail"},
    {ax::mojom::Role::kDescriptionList, "DescriptionList"},
    {ax::mojom::Role::kDescriptionListTerm, "DescriptionListTerm"},
    {ax::mojom::Role::kDesktop, "Desktop"},
    {ax::mojom::Role::kDetails, "Details"},
    {ax::mojom::Role::kDialog, "Dialog"},
    {ax::mojom::Role::kDirectory, "Directory"},
    {ax::mojom::Role::kDisclosureTriangle, "DisclosureTriangle"},
    // --------------------------------------------------------------
    // DPub Roles:
    // https://www.w3.org/TR/dpub-aam-1.0/#mapping_role_table
    {ax::mojom::Role::kDocAbstract, "DocAbstract"},
    {ax::mojom::Role::kDocAcknowledgments, "DocAcknowledgments"},
    {ax::mojom::Role::kDocAfterword, "DocAfterword"},
    {ax::mojom::Role::kDocAppendix, "DocAppendix"},
    {ax::mojom::Role::kDocBackLink, "DocBackLink"},
    {ax::mojom::Role::kDocBiblioEntry, "DocBiblioentry"},
    {ax::mojom::Role::kDocBibliography, "DocBibliography"},
    {ax::mojom::Role::kDocBiblioRef, "DocBiblioref"},
    {ax::mojom::Role::kDocChapter, "DocChapter"},
    {ax::mojom::Role::kDocColophon, "DocColophon"},
    {ax::mojom::Role::kDocConclusion, "DocConclusion"},
    {ax::mojom::Role::kDocCover, "DocCover"},
    {ax::mojom::Role::kDocCredit, "DocCredit"},
    {ax::mojom::Role::kDocCredits, "DocCredits"},
    {ax::mojom::Role::kDocDedication, "DocDedication"},
    {ax::mojom::Role::kDocEndnote, "DocEndnote"},
    {ax::mojom::Role::kDocEndnotes, "DocEndnotes"},
    {ax::mojom::Role::kDocEpigraph, "DocEpigraph"},
    {ax::mojom::Role::kDocEpilogue, "DocEpilogue"},
    {ax::mojom::Role::kDocErrata, "DocErrata"},
    {ax::mojom::Role::kDocExample, "DocExample"},
    {ax::mojom::Role::kDocFootnote, "DocFootnote"},
    {ax::mojom::Role::kDocForeword, "DocForeword"},
    {ax::mojom::Role::kDocGlossary, "DocGlossary"},
    {ax::mojom::Role::kDocGlossRef, "DocGlossref"},
    {ax::mojom::Role::kDocIndex, "DocIndex"},
    {ax::mojom::Role::kDocIntroduction, "DocIntroduction"},
    {ax::mojom::Role::kDocNoteRef, "DocNoteref"},
    {ax::mojom::Role::kDocNotice, "DocNotice"},
    {ax::mojom::Role::kDocPageBreak, "DocPagebreak"},
    {ax::mojom::Role::kDocPageList, "DocPagelist"},
    {ax::mojom::Role::kDocPart, "DocPart"},
    {ax::mojom::Role::kDocPreface, "DocPreface"},
    {ax::mojom::Role::kDocPrologue, "DocPrologue"},
    {ax::mojom::Role::kDocPullquote, "DocPullquote"},
    {ax::mojom::Role::kDocQna, "DocQna"},
    {ax::mojom::Role::kDocSubtitle, "DocSubtitle"},
    {ax::mojom::Role::kDocTip, "DocTip"},
    {ax::mojom::Role::kDocToc, "DocToc"},
    // End DPub roles.
    // --------------------------------------------------------------
    {ax::mojom::Role::kDocument, "Document"},
    {ax::mojom::Role::kEmbeddedObject, "EmbeddedObject"},
    {ax::mojom::Role::kFeed, "feed"},
    {ax::mojom::Role::kFigcaption, "Figcaption"},
    {ax::mojom::Role::kFigure, "Figure"},
    {ax::mojom::Role::kFooter, "Footer"},
    {ax::mojom::Role::kForm, "Form"},
    {ax::mojom::Role::kGenericContainer, "GenericContainer"},
    // --------------------------------------------------------------
    // ARIA Graphics module roles:
    // https://rawgit.com/w3c/graphics-aam/master/#mapping_role_table
    {ax::mojom::Role::kGraphicsDocument, "GraphicsDocument"},
    {ax::mojom::Role::kGraphicsObject, "GraphicsObject"},
    {ax::mojom::Role::kGraphicsSymbol, "GraphicsSymbol"},
    // End ARIA Graphics module roles.
    // --------------------------------------------------------------
    {ax::mojom::Role::kGrid, "Grid"},
    {ax::mojom::Role::kGroup, "Group"},
    {ax::mojom::Role::kHeading, "Heading"},
    {ax::mojom::Role::kIframePresentational, "IframePresentational"},
    {ax::mojom::Role::kIframe, "Iframe"},
    {ax::mojom::Role::kIgnored, "Ignored"},
    {ax::mojom::Role::kImageMap, "ImageMap"},
    {ax::mojom::Role::kImage, "Image"},
    {ax::mojom::Role::kInlineTextBox, "InlineTextBox"},
    {ax::mojom::Role::kInputTime, "InputTime"},
    {ax::mojom::Role::kKeyboard, "Keyboard"},
    {ax::mojom::Role::kLabelText, "Label"},
    {ax::mojom::Role::kLayoutTable, "LayoutTable"},
    {ax::mojom::Role::kLayoutTableCell, "LayoutCellTable"},
    {ax::mojom::Role::kLayoutTableColumn, "LayoutColumnTable"},
    {ax::mojom::Role::kLayoutTableRow, "LayoutRowTable"},
    {ax::mojom::Role::kLegend, "Legend"},
    {ax::mojom::Role::kLink, "Link"},
    {ax::mojom::Role::kLineBreak, "LineBreak"},
    {ax::mojom::Role::kListBoxOption, "ListBoxOption"},
    {ax::mojom::Role::kListBox, "ListBox"},
    {ax::mojom::Role::kListItem, "ListItem"},
    {ax::mojom::Role::kListMarker, "ListMarker"},
    {ax::mojom::Role::kList, "List"},
    {ax::mojom::Role::kLog, "Log"},
    {ax::mojom::Role::kMain, "Main"},
    {ax::mojom::Role::kMark, "Mark"},
    {ax::mojom::Role::kMarquee, "Marquee"},
    {ax::mojom::Role::kMath, "Math"},
    {ax::mojom::Role::kMenuBar, "MenuBar"},
    {ax::mojom::Role::kMenuButton, "MenuButton"},
    {ax::mojom::Role::kMenuItem, "MenuItem"},
    {ax::mojom::Role::kMenuItemCheckBox, "MenuItemCheckBox"},
    {ax::mojom::Role::kMenuItemRadio, "MenuItemRadio"},
    {ax::mojom::Role::kMenuListOption, "MenuListOption"},
    {ax::mojom::Role::kMenuListPopup, "MenuListPopup"},
    {ax::mojom::Role::kMenu, "Menu"},
    {ax::mojom::Role::kMeter, "Meter"},
    {ax::mojom::Role::kNavigation, "Navigation"},
    {ax::mojom::Role::kNote, "Note"},
    {ax::mojom::Role::kPane, "Pane"},
    {ax::mojom::Role::kParagraph, "Paragraph"},
    {ax::mojom::Role::kPopUpButton, "PopUpButton"},
    {ax::mojom::Role::kPre, "Pre"},
    {ax::mojom::Role::kPresentational, "Presentational"},
    {ax::mojom::Role::kProgressIndicator, "ProgressIndicator"},
    {ax::mojom::Role::kRadioButton, "RadioButton"},
    {ax::mojom::Role::kRadioGroup, "RadioGroup"},
    {ax::mojom::Role::kRegion, "Region"},
    {ax::mojom::Role::kRootWebArea, "WebArea"},
    {ax::mojom::Role::kRowHeader, "RowHeader"},
    {ax::mojom::Role::kRow, "Row"},
    {ax::mojom::Role::kRuby, "Ruby"},
    {ax::mojom::Role::kSvgRoot, "SVGRoot"},
    {ax::mojom::Role::kScrollBar, "ScrollBar"},
    {ax::mojom::Role::kScrollView, "ScrollView"},
    {ax::mojom::Role::kSearch, "Search"},
    {ax::mojom::Role::kSearchBox, "SearchBox"},
    {ax::mojom::Role::kSlider, "Slider"},
    {ax::mojom::Role::kSliderThumb, "SliderThumb"},
    {ax::mojom::Role::kSpinButton, "SpinButton"},
    {ax::mojom::Role::kSplitter, "Splitter"},
    {ax::mojom::Role::kStaticText, "StaticText"},
    {ax::mojom::Role::kStatus, "Status"},
    {ax::mojom::Role::kSwitch, "Switch"},
    {ax::mojom::Role::kTabList, "TabList"},
    {ax::mojom::Role::kTabPanel, "TabPanel"},
    {ax::mojom::Role::kTab, "Tab"},
    {ax::mojom::Role::kTableHeaderContainer, "TableHeaderContainer"},
    {ax::mojom::Role::kTable, "Table"},
    {ax::mojom::Role::kTerm, "Term"},
    {ax::mojom::Role::kTextField, "TextField"},
    {ax::mojom::Role::kTextFieldWithComboBox, "ComboBox"},
    {ax::mojom::Role::kTime, "Time"},
    {ax::mojom::Role::kTimer, "Timer"},
    {ax::mojom::Role::kTitleBar, "TitleBar"},
    {ax::mojom::Role::kToggleButton, "ToggleButton"},
    {ax::mojom::Role::kToolbar, "Toolbar"},
    {ax::mojom::Role::kTreeGrid, "TreeGrid"},
    {ax::mojom::Role::kTreeItem, "TreeItem"},
    {ax::mojom::Role::kTree, "Tree"},
    {ax::mojom::Role::kTooltip, "UserInterfaceTooltip"},
    {ax::mojom::Role::kUnknown, "Unknown"},
    {ax::mojom::Role::kVideo, "Video"},
    {ax::mojom::Role::kWebArea, "WebArea"},
    {ax::mojom::Role::kWebView, "WebView"},
    {ax::mojom::Role::kWindow, "Window"}};

static_assert(base::size(kInternalRoles) ==
                  static_cast<size_t>(ax::mojom::Role::kMaxValue) + 1,
              "Not all internal roles have an entry in internalRoles array");

// Roles which we need to map in the other direction
const RoleEntry kReverseRoles[] = {
    {"button", ax::mojom::Role::kToggleButton},
    {"combobox", ax::mojom::Role::kPopUpButton},
    {"contentinfo", ax::mojom::Role::kFooter},
    {"menuitem", ax::mojom::Role::kMenuButton},
    {"menuitem", ax::mojom::Role::kMenuListOption},
    {"progressbar", ax::mojom::Role::kMeter},
    {"textbox", ax::mojom::Role::kTextField},
    {"combobox", ax::mojom::Role::kComboBoxMenuButton},
    {"combobox", ax::mojom::Role::kTextFieldWithComboBox}};

static ARIARoleMap* CreateARIARoleMap() {
  ARIARoleMap* role_map = new ARIARoleMap;

  for (size_t i = 0; i < base::size(kRoles); ++i)
    role_map->Set(String(kRoles[i].aria_role), kRoles[i].webcore_role);

  // Grids "ignore" their non-row children during computation of children.
  role_map->Set(String("rowgroup"), ax::mojom::Role::kIgnored);

  return role_map;
}

static Vector<AtomicString>* CreateRoleNameVector() {
  Vector<AtomicString>* role_name_vector =
      new Vector<AtomicString>(base::size(kInternalRoles));
  for (size_t i = 0; i < base::size(kInternalRoles); i++)
    (*role_name_vector)[i] = g_null_atom;

  for (size_t i = 0; i < base::size(kRoles); ++i) {
    (*role_name_vector)[static_cast<size_t>(kRoles[i].webcore_role)] =
        AtomicString(kRoles[i].aria_role);
  }

  for (size_t i = 0; i < base::size(kReverseRoles); ++i) {
    (*role_name_vector)[static_cast<size_t>(kReverseRoles[i].webcore_role)] =
        AtomicString(kReverseRoles[i].aria_role);
  }

  return role_name_vector;
}

static Vector<AtomicString>* CreateInternalRoleNameVector() {
  Vector<AtomicString>* internal_role_name_vector =
      new Vector<AtomicString>(base::size(kInternalRoles));
  for (size_t i = 0; i < base::size(kInternalRoles); i++) {
    (*internal_role_name_vector)[static_cast<size_t>(
        kInternalRoles[i].webcore_role)] =
        AtomicString(kInternalRoles[i].internal_role_name);
  }

  return internal_role_name_vector;
}

HTMLDialogElement* GetActiveDialogElement(Node* node) {
  return node->GetDocument().ActiveModalDialog();
}

}  // namespace

unsigned AXObject::number_of_live_ax_objects_ = 0;

AXObject::AXObject(AXObjectCacheImpl& ax_object_cache)
    : id_(0),
      have_children_(false),
      role_(ax::mojom::Role::kUnknown),
      aria_role_(ax::mojom::Role::kUnknown),
      last_known_is_ignored_value_(kDefaultBehavior),
      explicit_container_id_(0),
      parent_(nullptr),
      last_modification_count_(-1),
      cached_is_ignored_(false),
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

void AXObject::Init() {
  role_ = DetermineAccessibilityRole();
}

void AXObject::Detach() {
  // Clear any children and call detachFromParent on them so that
  // no children are left with dangling pointers to their parent.
  ClearChildren();

  ax_object_cache_ = nullptr;
}

bool AXObject::IsDetached() const {
  return !ax_object_cache_;
}

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

void AXObject::GetSparseAXAttributes(
    AXSparseAttributeClient& sparse_attribute_client) const {
  AXSparseAttributeAOMPropertyClient property_client(*ax_object_cache_,
                                                     sparse_attribute_client);
  HashSet<QualifiedName> shadowed_aria_attributes;

  AccessibleNode* accessible_node = GetAccessibleNode();
  if (accessible_node) {
    accessible_node->GetAllAOMProperties(&property_client,
                                         shadowed_aria_attributes);
  }

  Element* element = GetElement();
  if (!element)
    return;

  AXSparseAttributeSetterMap& ax_sparse_attribute_setter_map =
      GetSparseAttributeSetterMap();
  AttributeCollection attributes = element->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    if (shadowed_aria_attributes.Contains(attr.GetName()))
      continue;

    AXSparseAttributeSetter* setter =
        ax_sparse_attribute_setter_map.at(attr.GetName());
    if (setter)
      setter->Run(*this, sparse_attribute_client, attr.Value());
  }
}

bool AXObject::IsARIATextControl() const {
  return AriaRoleAttribute() == ax::mojom::Role::kTextField ||
         AriaRoleAttribute() == ax::mojom::Role::kSearchBox ||
         AriaRoleAttribute() == ax::mojom::Role::kTextFieldWithComboBox;
}

bool AXObject::IsButton() const {
  ax::mojom::Role role = RoleValue();

  return role == ax::mojom::Role::kButton ||
         role == ax::mojom::Role::kPopUpButton ||
         role == ax::mojom::Role::kToggleButton;
}

bool AXObject::IsCheckable() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kToggleButton:
      return true;
    case ax::mojom::Role::kTreeItem:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuListOption:
      return AriaCheckedIsPresent();
    default:
      return false;
  }
}

// Why this is here instead of AXNodeObject:
// Because an AXMenuListOption (<option>) can
// have an ARIA role of menuitemcheckbox/menuitemradio
// yet does not inherit from AXNodeObject
ax::mojom::CheckedState AXObject::CheckedState() const {
  if (!IsCheckable())
    return ax::mojom::CheckedState::kNone;

  // Try ARIA checked/pressed state
  const ax::mojom::Role role = RoleValue();
  const auto prop = role == ax::mojom::Role::kToggleButton
                        ? AOMStringProperty::kPressed
                        : AOMStringProperty::kChecked;
  const AtomicString& checked_attribute = GetAOMPropertyOrARIAAttribute(prop);
  if (checked_attribute) {
    if (EqualIgnoringASCIICase(checked_attribute, "mixed")) {
      // Only checkable role that doesn't support mixed is the switch.
      if (role != ax::mojom::Role::kSwitch)
        return ax::mojom::CheckedState::kMixed;
    }

    // Anything other than "false" should be treated as "true".
    return EqualIgnoringASCIICase(checked_attribute, "false")
               ? ax::mojom::CheckedState::kFalse
               : ax::mojom::CheckedState::kTrue;
  }

  // Native checked state
  if (role != ax::mojom::Role::kToggleButton) {
    const Node* node = this->GetNode();
    if (!node)
      return ax::mojom::CheckedState::kNone;

    // Expose native checkbox mixed state as accessibility mixed state. However,
    // do not expose native radio mixed state as accessibility mixed state.
    // This would confuse the JAWS screen reader, which reports a mixed radio as
    // both checked and partially checked, but a native mixed native radio
    // button sinply means no radio buttons have been checked in the group yet.
    if (IsNativeCheckboxInMixedState(node))
      return ax::mojom::CheckedState::kMixed;

    if (IsHTMLInputElement(*node) &&
        ToHTMLInputElement(*node).ShouldAppearChecked()) {
      return ax::mojom::CheckedState::kTrue;
    }
  }

  return ax::mojom::CheckedState::kFalse;
}

bool AXObject::IsNativeCheckboxInMixedState(const Node* node) {
  if (!IsHTMLInputElement(node))
    return false;

  const HTMLInputElement* input = ToHTMLInputElement(node);
  const auto inputType = input->type();
  if (inputType != InputTypeNames::checkbox)
    return false;
  return input->ShouldAppearIndeterminate();
}

bool AXObject::IsLandmarkRelated() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocToc:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kSearch:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsMenuRelated() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsPasswordFieldAndShouldHideValue() const {
  Settings* settings = GetDocument()->GetSettings();
  if (!settings || settings->GetAccessibilityPasswordValuesEnabled())
    return false;

  return IsPasswordField();
}

bool AXObject::IsTextObject() const {
  // Objects with |ax::mojom::Role::kLineBreak| are HTML <br> elements and are
  // not backed by DOM text nodes. We can't mark them as text objects for that
  // reason.
  switch (RoleValue()) {
    case ax::mojom::Role::kInlineTextBox:
    case ax::mojom::Role::kStaticText:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsClickable() const {
  if (IsButton() || IsLink() || IsTextControl())
    return true;

  // TODO(dmazzoni): Ensure that ax::mojom::Role::kColorWell and
  // ax::mojom::Role::kSpinButton are correctly handled here via their
  // constituent parts.
  switch (RoleValue()) {
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
      return true;
    default:
      return false;
  }
}

bool AXObject::AccessibilityIsIgnored() const {
  Node* node = GetNode();
  if (!node) {
    AXObject* parent = this->ParentObject();
    while (!node && parent) {
      node = parent->GetNode();
      parent = parent->ParentObject();
    }
  }

  if (node)
    node->UpdateDistributionForFlatTreeTraversal();

  // TODO(aboxhall): Instead of this, propagate inert down through frames
  Document* document = GetDocument();
  while (document && document->LocalOwner()) {
    document->LocalOwner()->UpdateDistributionForFlatTreeTraversal();
    document = document->LocalOwner()->ownerDocument();
  }

  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_ignored_;
}

void AXObject::UpdateCachedAttributeValuesIfNeeded() const {
  if (IsDetached())
    return;

  AXObjectCacheImpl& cache = AXObjectCache();

  if (cache.ModificationCount() == last_modification_count_)
    return;

  last_modification_count_ = cache.ModificationCount();
  cached_background_color_ = ComputeBackgroundColor();
  cached_is_inert_or_aria_hidden_ = ComputeIsInertOrAriaHidden();
  cached_is_descendant_of_leaf_node_ = !!LeafNodeAncestor();
  cached_is_descendant_of_disabled_node_ = !!DisabledAncestor();
  cached_has_inherited_presentational_role_ =
      !!InheritsPresentationalRoleFrom();
  cached_is_ignored_ = ComputeAccessibilityIsIgnored();
  cached_is_editable_root_ = ComputeIsEditableRoot();
  // TODO(dmazzoni): remove this const_cast.
  cached_live_region_root_ =
      IsLiveRegion()
          ? const_cast<AXObject*>(this)
          : (ParentObjectIfExists() ? ParentObjectIfExists()->LiveRegionRoot()
                                    : nullptr);
  cached_aria_column_index_ = ComputeAriaColumnIndex();
  cached_aria_row_index_ = ComputeAriaRowIndex();
  if (cached_is_ignored_ != LastKnownIsIgnoredValue()) {
    last_known_is_ignored_value_ =
        cached_is_ignored_ ? kIgnoreObject : kIncludeObject;

    AXObject* parent = ParentObjectIfExists();
    if (parent)
      parent->ChildrenChanged();
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

AXObjectInclusion AXObject::AccessibilityPlatformIncludesObject() const {
  if (IsMenuListPopup() || IsMenuListOption())
    return kIncludeObject;

  return kDefaultBehavior;
}

AXObjectInclusion AXObject::DefaultObjectInclusion(
    IgnoredReasons* ignored_reasons) const {
  if (IsInertOrAriaHidden()) {
    if (ignored_reasons)
      ComputeIsInertOrAriaHidden(ignored_reasons);
    return kIgnoreObject;
  }

  return AccessibilityPlatformIncludesObject();
}

bool AXObject::IsInertOrAriaHidden() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_is_inert_or_aria_hidden_;
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
  Element* element = node->IsElementNode()
                         ? ToElement(node)
                         : FlatTreeTraversal::ParentElement(*node);
  while (element) {
    if (element->hasAttribute(inertAttr))
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
    Element* element = GetElement();
    if (element)
      target = element->accessibleNode();
  }
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

const AXObject* AXObject::DatetimeAncestor(int max_levels_to_check) const {
  switch (RoleValue()) {
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kInputTime:
    case ax::mojom::Role::kTime:
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
  if (last_known_is_ignored_value_ == kDefaultBehavior) {
    last_known_is_ignored_value_ =
        AccessibilityIsIgnored() ? kIgnoreObject : kIncludeObject;
  }

  return last_known_is_ignored_value_ == kIgnoreObject;
}

void AXObject::SetLastKnownIsIgnoredValue(bool is_ignored) {
  last_known_is_ignored_value_ = is_ignored ? kIgnoreObject : kIncludeObject;
}

bool AXObject::HasInheritedPresentationalRole() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_has_inherited_presentational_role_;
}

bool AXObject::CanReceiveAccessibilityFocus() const {
  const Element* elem = GetElement();
  if (!elem)
    return false;

  // Focusable, and not forwarding the focus somewhere else
  if (elem->IsFocusable() &&
      !GetAOMPropertyOrARIAAttribute(AOMRelationProperty::kActiveDescendant))
    return true;

  // aria-activedescendant focus
  return elem->FastHasAttribute(idAttr) && CanBeActiveDescendant();
}

bool AXObject::CanSetValueAttribute() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTime:
    case ax::mojom::Role::kSearchBox:
      return Restriction() == kNone;
    default:
      break;
  }
  return false;
}

bool AXObject::CanSetFocusAttribute() const {
  Node* node = GetNode();
  if (!node)
    return false;

  if (IsWebArea())
    return true;

  // Children of elements with an aria-activedescendant attribute should be
  // focusable if they have a (non-presentational) ARIA role.
  if (!IsPresentational() && AriaRoleAttribute() != ax::mojom::Role::kUnknown &&
      CanBeActiveDescendant()) {
    return true;
  }

  // NOTE: It would be more accurate to ask the document whether
  // setFocusedNode() would do anything. For example, setFocusedNode() will do
  // nothing if the current focused node will not relinquish the focus.
  if (IsDisabledFormControl(node))
    return false;

  // Check for options here because AXListBoxOption and AXMenuListOption
  // don't help when the <option> is canvas fallback, and because
  // a common case for aria-owns from a textbox that points to a list
  // does not change the hierarchy (textboxes don't support children)
  if (RoleValue() == ax::mojom::Role::kListBoxOption ||
      RoleValue() == ax::mojom::Role::kMenuListOption)
    return true;

  return node->IsElementNode() && ToElement(node)->SupportsFocus();
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
  return IsARIAControlledByTextboxWithActiveDescendant() ||
         AncestorExposesActiveDescendant();
}

bool AXObject::IsARIAControlledByTextboxWithActiveDescendant() const {
  // This situation should mostly arise when using an active descendant on a
  // textbox inside an ARIA 1.1 combo box widget, which points to the selected
  // option in a list. In such situations, the active descendant is useful only
  // when the textbox is focused. Therefore, we don't currently need to keep
  // track of all aria-controls relationships.
  const AXObject* focused_object = AXObjectCache().FocusedObject();
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

bool AXObject::HasIndirectChildren() const {
  return IsTableCol() || RoleValue() == ax::mojom::Role::kTableHeaderContainer;
}

bool AXObject::CanSetSelectedAttribute() const {
  // Sub-widget elements can be selected if not disabled (native or ARIA)
  return IsSubWidget() && Restriction() != kDisabled;
}

bool AXObject::IsSubWidget() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
    case ax::mojom::Role::kColumn:
    case ax::mojom::Role::kRow: {
      // If it has an explicit ARIA role, it's a subwidget.
      //
      // Reasoning:
      // Static table cells are not selectable, but ARIA grid cells
      // and rows definitely are according to the spec. To support
      // ARIA 1.0, it's sufficient to just check if there's any
      // ARIA role at all, because if so then it must be a grid-related
      // role so it must be selectable.
      //
      // TODO: an ARIA 1.1+ role of "cell", or a role of "row" inside
      // an ARIA 1.1 role of "table", should not be selectable. We may
      // need to create separate role enums for grid cells vs table cells
      // to implement this.
      if (AriaRoleAttribute() != ax::mojom::Role::kUnknown)
        return true;

      // Otherwise it's only a subwidget if it's in a grid or treegrid,
      // not in a table.
      AXObject* parent = ParentObjectUnignored();
      while (parent && !parent->IsTableLikeRole())
        parent = parent->ParentObjectUnignored();
      if (parent && (parent->RoleValue() == ax::mojom::Role::kGrid ||
                     parent->RoleValue() == ax::mojom::Role::kTreeGrid))
        return true;
      return false;
    }
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      break;
  }
  return false;
}

bool AXObject::SupportsARIASetSizeAndPosInSet() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      break;
  }

  return false;
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
  ax::mojom::NameFrom name_from;
  AXObject::AXObjectVector name_objects;
  return GetName(name_from, &name_objects);
}

String AXObject::GetName(ax::mojom::NameFrom& name_from,
                         AXObject::AXObjectVector* name_objects) const {
  HeapHashSet<Member<const AXObject>> visited;
  AXRelatedObjectVector related_objects;
  String text = TextAlternative(false, false, visited, name_from,
                                &related_objects, nullptr);

  ax::mojom::Role role = RoleValue();
  if (!GetNode() ||
      (!IsHTMLBRElement(GetNode()) && role != ax::mojom::Role::kStaticText &&
       role != ax::mojom::Role::kInlineTextBox))
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
  ax::mojom::NameFrom tmp_name_from;
  AXRelatedObjectVector tmp_related_objects;
  String text = TextAlternative(false, false, visited, tmp_name_from,
                                &tmp_related_objects, name_sources);
  text = text.SimplifyWhiteSpace(IsHTMLSpace<UChar>);
  return text;
}

String AXObject::RecursiveTextAlternative(const AXObject& ax_obj,
                                          bool in_aria_labelled_by_traversal,
                                          AXObjectSet& visited) {
  ax::mojom::NameFrom tmp_name_from;
  return RecursiveTextAlternative(ax_obj, in_aria_labelled_by_traversal,
                                  visited, tmp_name_from);
}

String AXObject::RecursiveTextAlternative(const AXObject& ax_obj,
                                          bool in_aria_labelled_by_traversal,
                                          AXObjectSet& visited,
                                          ax::mojom::NameFrom& name_from) {
  if (visited.Contains(&ax_obj) && !in_aria_labelled_by_traversal)
    return String();

  return ax_obj.TextAlternative(true, in_aria_labelled_by_traversal, visited,
                                name_from, nullptr, nullptr);
}

bool AXObject::IsHiddenForTextAlternativeCalculation() const {
  if (AOMPropertyOrARIAAttributeIsFalse(AOMBooleanProperty::kHidden))
    return false;

  if (GetLayoutObject())
    return GetLayoutObject()->Style()->Visibility() != EVisibility::kVisible;

  // This is an obscure corner case: if a node has no LayoutObject, that means
  // it's not rendered, but we still may be exploring it as part of a text
  // alternative calculation, for example if it was explicitly referenced by
  // aria-labelledby. So we need to explicitly call the style resolver to check
  // whether it's invisible or display:none, rather than relying on the style
  // cached in the LayoutObject.
  Document* document = GetDocument();
  if (!document || !document->GetFrame())
    return false;
  if (Node* node = GetNode()) {
    if (node->isConnected() && node->IsElementNode()) {
      scoped_refptr<ComputedStyle> style =
          document->EnsureStyleResolver().StyleForElement(ToElement(node));
      return style->Display() == EDisplay::kNone ||
             style->Visibility() != EVisibility::kVisible;
    }
  }
  return false;
}

String AXObject::AriaTextAlternative(bool recursive,
                                     bool in_aria_labelled_by_traversal,
                                     AXObjectSet& visited,
                                     ax::mojom::NameFrom& name_from,
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
    name_from = ax::mojom::NameFrom::kRelatedElement;

    // Check AOM property first.
    HeapVector<Member<Element>> elements;
    if (HasAOMProperty(AOMRelationListProperty::kLabeledBy, elements)) {
      if (name_sources) {
        name_sources->push_back(
            NameSource(*found_text_alternative, aria_labelledbyAttr));
        name_sources->back().type = name_from;
      }

      // Operate on a copy of |visited| so that if |nameSources| is not null,
      // the set of visited objects is preserved unmodified for future
      // calculations.
      AXObjectSet visited_copy = visited;
      text_alternative =
          TextFromElements(true, visited_copy, elements, related_objects);
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
    } else {
      // Now check ARIA attribute
      const QualifiedName& attr =
          HasAttribute(aria_labeledbyAttr) && !HasAttribute(aria_labelledbyAttr)
              ? aria_labeledbyAttr
              : aria_labelledbyAttr;

      if (name_sources) {
        name_sources->push_back(NameSource(*found_text_alternative, attr));
        name_sources->back().type = name_from;
      }

      const AtomicString& aria_labelledby = GetAttribute(attr);
      if (!aria_labelledby.IsNull()) {
        if (name_sources)
          name_sources->back().attribute_value = aria_labelledby;

        // Operate on a copy of |visited| so that if |nameSources| is not null,
        // the set of visited objects is preserved unmodified for future
        // calculations.
        AXObjectSet visited_copy = visited;
        Vector<String> ids;
        text_alternative =
            TextFromAriaLabelledby(visited_copy, related_objects, ids);
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
  name_from = ax::mojom::NameFrom::kAttribute;
  if (name_sources) {
    name_sources->push_back(
        NameSource(*found_text_alternative, aria_labelAttr));
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
      local_related_objects.push_back(
          new NameSourceRelatedObject(ax_element, result));
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
  TokenVectorFromAttribute(ids, attribute);
  if (ids.IsEmpty())
    return;

  TreeScope& scope = GetNode()->GetTreeScope();
  for (const auto& id : ids) {
    if (Element* id_element = scope.getElementById(AtomicString(id)))
      elements.push_back(id_element);
  }
}

void AXObject::AriaLabelledbyElementVector(
    HeapVector<Member<Element>>& elements,
    Vector<String>& ids) const {
  // Try both spellings, but prefer aria-labelledby, which is the official spec.
  ElementsFromAttribute(elements, aria_labelledbyAttr, ids);
  if (!ids.size())
    ElementsFromAttribute(elements, aria_labeledbyAttr, ids);
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
  ElementsFromAttribute(elements, aria_describedbyAttr, ids);
  return TextFromElements(true, visited, elements, related_objects);
}

RGBA32 AXObject::BackgroundColor() const {
  UpdateCachedAttributeValuesIfNeeded();
  return cached_background_color_;
}

AccessibilityOrientation AXObject::Orientation() const {
  // In ARIA 1.1, the default value for aria-orientation changed from
  // horizontal to undefined.
  return kAccessibilityOrientationUndefined;
}

void AXObject::Markers(Vector<DocumentMarker::MarkerType>&,
                       Vector<AXRange>&) const {}

void AXObject::TextCharacterOffsets(Vector<int>&) const {}

void AXObject::GetWordBoundaries(Vector<AXRange>&) const {}

ax::mojom::DefaultActionVerb AXObject::Action() const {
  Element* action_element = ActionElement();
  if (!action_element)
    return ax::mojom::DefaultActionVerb::kNone;

  // TODO(dmazzoni): Ensure that combo box text field is handled here.
  if (IsTextControl())
    return ax::mojom::DefaultActionVerb::kActivate;

  if (IsCheckable()) {
    return CheckedState() != ax::mojom::CheckedState::kTrue
               ? ax::mojom::DefaultActionVerb::kCheck
               : ax::mojom::DefaultActionVerb::kUncheck;
  }

  switch (RoleValue()) {
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kToggleButton:
      return ax::mojom::DefaultActionVerb::kPress;
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuListOption:
      return ax::mojom::DefaultActionVerb::kSelect;
    case ax::mojom::Role::kLink:
      return ax::mojom::DefaultActionVerb::kJump;
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kPopUpButton:
      return ax::mojom::DefaultActionVerb::kOpen;
    default:
      if (action_element == GetNode())
        return ax::mojom::DefaultActionVerb::kClick;
      return ax::mojom::DefaultActionVerb::kClickAncestor;
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
    case ax::mojom::Role::kAlertDialog:
    case ax::mojom::Role::kAlert:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDialog:
    case ax::mojom::Role::kDirectory:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kFeed:
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kLog:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kMarquee:
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kNote:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kRegion:
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kRowHeader:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kStatus:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kTabPanel:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTimer:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTooltip:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTreeItem:
      return true;
    default:
      return false;
  }
}

bool AXObject::HasGlobalARIAAttribute() const {
  if (!GetElement())
    return false;

  AttributeCollection attributes = GetElement()->AttributesWithoutUpdate();
  for (const Attribute& attr : attributes) {
    // Attributes cache their uppercase names.
    auto name = attr.GetName().LocalNameUpper();
    if (!name.StartsWith("ARIA"))
      continue;
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
  }
  return false;
}

bool AXObject::SupportsRangeValue() const {
  return IsProgressIndicator() || IsMeter() || IsSlider() || IsScrollbar() ||
         IsSpinButton() || IsMoveableSplitter();
}

int AXObject::IndexInParent() const {
  if (!ParentObjectUnignored())
    return 0;

  const AXObjectVector& siblings = ParentObjectUnignored()->Children();
  size_t index = siblings.Find(this);
  return (index == kNotFound) ? 0 : static_cast<int>(index);
}

bool AXObject::IsLiveRegion() const {
  const AtomicString& live_region = LiveRegionStatus();
  return !live_region.IsEmpty() && !EqualIgnoringASCIICase(live_region, "off");
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
      return kDisabled;
  } else if (CanSetFocusAttribute() && IsDescendantOfDisabledNode()) {
    // No aria-disabled, but other markup says it's disabled.
    return kDisabled;
  }

  // Check aria-readonly if supported by current role.
  bool is_read_only;
  if (SupportsARIAReadOnly() &&
      HasAOMPropertyOrARIAAttribute(AOMBooleanProperty::kReadOnly,
                                    is_read_only)) {
    // ARIA overrides other readonly state markup.
    return is_read_only ? kReadOnly : kNone;
  }

  // This is a node that is not readonly and not disabled.
  return kNone;
}

ax::mojom::Role AXObject::DetermineAccessibilityRole() {
  aria_role_ = DetermineAriaRoleAttribute();
  return aria_role_;
}

ax::mojom::Role AXObject::AriaRoleAttribute() const {
  return aria_role_;
}

ax::mojom::Role AXObject::DetermineAriaRoleAttribute() const {
  const AtomicString& aria_role =
      GetAOMPropertyOrARIAAttribute(AOMStringProperty::kRole);
  if (aria_role.IsNull() || aria_role.IsEmpty())
    return ax::mojom::Role::kUnknown;

  ax::mojom::Role role = AriaRoleToWebCoreRole(aria_role);

  // ARIA states if an item can get focus, it should not be presentational.
  // It also states user agents should ignore the presentational role if
  // the element has global ARIA states and properties.
  if ((role == ax::mojom::Role::kNone ||
       role == ax::mojom::Role::kPresentational) &&
      (CanSetFocusAttribute() || HasGlobalARIAAttribute()))
    return ax::mojom::Role::kUnknown;

  if (role == ax::mojom::Role::kButton)
    role = ButtonRoleType();

  role = RemapAriaRoleDueToParent(role);

  // Distinguish between different uses of the "combobox" role:
  //
  // ax::mojom::Role::kComboBoxGrouping:
  //   <div role="combobox"><input></div>
  // ax::mojom::Role::kTextFieldWithComboBox:
  //   <input role="combobox">
  // ax::mojom::Role::kComboBoxMenuButton:
  //   <div tabindex=0 role="combobox">Select</div>
  if (role == ax::mojom::Role::kComboBoxGrouping) {
    if (IsNativeTextControl())
      role = ax::mojom::Role::kTextFieldWithComboBox;
    else if (GetElement() && GetElement()->SupportsFocus())
      role = ax::mojom::Role::kComboBoxMenuButton;
  }

  if (role != ax::mojom::Role::kUnknown)
    return role;

  return ax::mojom::Role::kUnknown;
}

ax::mojom::Role AXObject::RemapAriaRoleDueToParent(ax::mojom::Role role) const {
  // Some objects change their role based on their parent.
  // However, asking for the unignoredParent calls accessibilityIsIgnored(),
  // which can trigger a loop.  While inside the call stack of creating an
  // element, we need to avoid accessibilityIsIgnored().
  // https://bugs.webkit.org/show_bug.cgi?id=65174

  if (role != ax::mojom::Role::kListBoxOption &&
      role != ax::mojom::Role::kMenuItem)
    return role;

  for (AXObject* parent = ParentObject();
       parent && !parent->AccessibilityIsIgnored();
       parent = parent->ParentObject()) {
    ax::mojom::Role parent_aria_role = parent->AriaRoleAttribute();

    // Selects and listboxes both have options as child roles, but they map to
    // different roles within WebCore.
    if (role == ax::mojom::Role::kListBoxOption &&
        parent_aria_role == ax::mojom::Role::kMenu)
      return ax::mojom::Role::kMenuItem;
    // An aria "menuitem" may map to MenuButton or MenuItem depending on its
    // parent.
    if (role == ax::mojom::Role::kMenuItem &&
        parent_aria_role == ax::mojom::Role::kGroup)
      return ax::mojom::Role::kMenuButton;

    // If the parent had a different role, then we don't need to continue
    // searching up the chain.
    if (parent_aria_role != ax::mojom::Role::kUnknown)
      break;
  }

  return role;
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
  return RoleValue() == ax::mojom::Role::kAlert ||
         RoleValue() == ax::mojom::Role::kStatus;
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
  for (const auto& child : children_) {
    if (child->IsMockObject() &&
        child->GetBoundsInFrameCoordinates().Contains(point))
      return child->ElementAccessibilityHitTest(point);
  }

  return const_cast<AXObject*>(this);
}

AXObject::AncestorsIterator AXObject::AncestorsBegin() {
  AXObject* parent = ParentObjectUnignored();
  if (parent)
    return AXObject::AncestorsIterator(*parent);
  return AncestorsEnd();
}

AXObject::AncestorsIterator AXObject::AncestorsEnd() {
  return AXObject::AncestorsIterator();
}

AXObject::InOrderTraversalIterator AXObject::GetInOrderTraversalIterator() {
  return InOrderTraversalIterator(*this);
}

int AXObject::ChildCount() const {
  return HasIndirectChildren() ? 0 : static_cast<int>(Children().size());
}

const AXObject::AXObjectVector& AXObject::Children() const {
  return const_cast<AXObject*>(this)->Children();
}

const AXObject::AXObjectVector& AXObject::Children() {
  UpdateChildrenIfNecessary();

  return children_;
}

AXObject* AXObject::FirstChild() const {
  return ChildCount() ? *Children().begin() : nullptr;
}

AXObject* AXObject::LastChild() const {
  return ChildCount() ? *(Children().end() - 1) : nullptr;
}

AXObject* AXObject::DeepestFirstChild() const {
  if (!ChildCount())
    return nullptr;

  AXObject* deepest_child = FirstChild();
  while (deepest_child->ChildCount())
    deepest_child = deepest_child->FirstChild();

  return deepest_child;
}

AXObject* AXObject::DeepestLastChild() const {
  if (!ChildCount())
    return nullptr;

  AXObject* deepest_child = LastChild();
  while (deepest_child->ChildCount())
    deepest_child = deepest_child->LastChild();

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

AXObject* AXObject::NextSibling() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return nullptr;

  if (AccessibilityIsIgnored())
    NOTREACHED() << "We don't support finding siblings for ignored objects.";

  if (IndexInParent() < parent->ChildCount() - 1)
    return *(parent->Children().begin() + IndexInParent() + 1);

  return nullptr;
}

AXObject* AXObject::PreviousSibling() const {
  AXObject* parent = ParentObjectUnignored();
  if (!parent)
    return nullptr;

  if (AccessibilityIsIgnored())
    NOTREACHED() << "We don't support finding siblings for ignored objects.";

  if (IndexInParent() > 0)
    return *(parent->Children().begin() + IndexInParent() - 1);

  return nullptr;
}

AXObject* AXObject::NextInTreeObject(bool can_wrap_to_first_element) const {
  // We don't support finding the next sibling for an ignored object, so we
  // return the next sibling of the deepest unignored ancestor, which is the
  // next best thing that doesn't violate next-in-order semantics.
  if (!AccessibilityIsIgnored()) {
    if (ChildCount())
      return FirstChild();

    if (NextSibling())
      return NextSibling();
  }

  AXObject* current_object = const_cast<AXObject*>(this);
  while (current_object->ParentObjectUnignored()) {
    current_object = current_object->ParentObjectUnignored();
    AXObject* sibling = current_object->NextSibling();
    if (sibling)
      return sibling;
  }

  return can_wrap_to_first_element ? current_object : nullptr;
}

AXObject* AXObject::PreviousInTreeObject(bool can_wrap_to_last_element) const {
  // We don't support finding the previous sibling for an ignored object, so we
  // return the deepest unignored ancestor instead, which is the next best thing
  // that doesn't violate previous-in-order semantics.
  AXObject* sibling = AccessibilityIsIgnored() ? nullptr : PreviousSibling();
  if (!sibling) {
    if (ParentObjectUnignored())
      return ParentObjectUnignored();
    return can_wrap_to_last_element ? DeepestLastChild() : nullptr;
  }

  if (sibling->ChildCount())
    return sibling->DeepestLastChild();

  return sibling;
}

AXObject* AXObject::ParentObject() const {
  if (IsDetached())
    return nullptr;

  if (parent_)
    return parent_;

  if (AXObjectCache().IsAriaOwned(this))
    return AXObjectCache().GetAriaOwnedParent(this);

  return ComputeParent();
}

AXObject* AXObject::ParentObjectIfExists() const {
  if (IsDetached())
    return nullptr;

  if (parent_)
    return parent_;

  return ComputeParentIfExists();
}

AXObject* AXObject::ParentObjectUnignored() const {
  AXObject* parent;
  for (parent = ParentObject(); parent && parent->AccessibilityIsIgnored();
       parent = parent->ParentObject()) {
  }

  return parent;
}

// Container widgets are those that a user tabs into and arrows around
// sub-widgets
bool AXObject::IsContainerWidget() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kTree:
      return true;
    default:
      return false;
  }
}

AXObject* AXObject::ContainerWidget() const {
  AXObject* ancestor = ParentObjectUnignored();
  while (ancestor && !ancestor->IsContainerWidget())
    ancestor = ancestor->ParentObjectUnignored();

  return ancestor;
}

void AXObject::UpdateChildrenIfNecessary() {
  if (!HasChildren())
    AddChildren();
}

void AXObject::ClearChildren() {
  // Detach all weak pointers from objects to their parents.
  for (const auto& child : children_)
    child->DetachFromParent();

  children_.clear();
  have_children_ = false;
}

void AXObject::AddAccessibleNodeChildren() {
  Element* element = GetElement();
  if (!element)
    return;

  AccessibleNode* accessible_node = element->ExistingAccessibleNode();
  if (!accessible_node)
    return;

  for (const auto& child : accessible_node->GetChildren())
    children_.push_back(AXObjectCache().GetOrCreate(child));
}

Element* AXObject::GetElement() const {
  Node* node = GetNode();
  return node && node->IsElementNode() ? ToElement(node) : nullptr;
}

Document* AXObject::GetDocument() const {
  LocalFrameView* frame_view = DocumentFrameView();
  if (!frame_view)
    return nullptr;

  return frame_view->GetFrame().GetDocument();
}

LocalFrameView* AXObject::DocumentFrameView() const {
  const AXObject* object = this;
  while (object && !object->IsAXLayoutObject())
    object = object->ParentObject();

  if (!object)
    return nullptr;

  return object->DocumentFrameView();
}

AtomicString AXObject::Language() const {
  // This method is used when the style engine is either not available on this
  // object, e.g. for canvas fallback content, or is unable to determine the
  // document's language. We use the following signals to detect the element's
  // language, in decreasing priority:
  // 1. The [language of a node] as defined in HTML, if known.
  // 2. The list of languages the browser sends in the [Accept-Language] header.
  // 3. The browser's default language.

  const AtomicString& lang = GetAttribute(langAttr);
  if (!lang.IsEmpty())
    return lang;

  AXObject* parent = ParentObject();
  if (parent)
    return parent->Language();

  const Document* document = GetDocument();
  if (document) {
    // Fall back to the first content language specified in the meta tag.
    // This is not part of what the HTML5 Standard suggests but it still appears
    // to be necessary.
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

bool AXObject::HasAttribute(const QualifiedName& attribute) const {
  if (Element* element = GetElement())
    return element->FastHasAttribute(attribute);
  return false;
}

const AtomicString& AXObject::GetAttribute(
    const QualifiedName& attribute) const {
  if (Element* element = GetElement())
    return element->FastGetAttribute(attribute);
  return g_null_atom;
}

//
// Scrollable containers.
//

bool AXObject::IsScrollableContainer() const {
  return !!GetScrollableAreaIfScrollable();
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
                        kProgrammaticScroll);
}

bool AXObject::IsTableLikeRole() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kTreeGrid:
      return true;
    default:
      return false;
  }
}

bool AXObject::IsTableRowLikeRole() const {
  return RoleValue() == ax::mojom::Role::kRow ||
         RoleValue() == ax::mojom::Role::kLayoutTableRow;
}

bool AXObject::IsTableCellLikeRole() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kLayoutTableCell:
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kRowHeader:
      return true;
    default:
      return false;
  }
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
      if (cell->RoleValue() == ax::mojom::Role::kColumnHeader)
        headers.push_back(cell);
    }
  }
}

void AXObject::RowHeaders(AXObjectVector& headers) const {
  if (!IsTableLikeRole())
    return;

  for (const auto& row : TableRowChildren()) {
    for (const auto& cell : row->TableCellChildren()) {
      if (cell->RoleValue() == ax::mojom::Role::kRowHeader)
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
  // since it's only needed for Blink layout tests and not for production.
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
  if (row_count == (int)RowCount() || row_count != -1)
    return 0;

  // In the spec, -1 explicitly means an unknown number of rows.
  return -1;
}

unsigned AXObject::ColumnIndex() const {
  if (!IsTableCellLikeRole())
    return 0;

  const AXObject* row = TableRowParent();
  if (!row)
    return 0;

  unsigned column_index = 0;
  for (const auto& child : row->TableCellChildren()) {
    if (child == this)
      break;
    column_index++;
  }
  return column_index;
}

unsigned AXObject::RowIndex() const {
  const AXObject* row = nullptr;
  if (IsTableRowLikeRole())
    row = this;
  else if (IsTableCellLikeRole())
    row = TableRowParent();

  if (!row)
    return 0;

  const AXObject* table = row->TableParent();
  if (!table)
    return 0;

  unsigned row_index = 0;
  for (const auto& child : table->TableRowChildren()) {
    if (child == row)
      break;
    if (!child->IsTableRowLikeRole())
      continue;
    row_index++;
  }
  return row_index;
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
  if (!IsTableCellLikeRole())
    return 0;

  // First see if it has an ARIA column index explicitly set.
  uint32_t col_index;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kColIndex, col_index) &&
      col_index >= 1) {
    return col_index;
  }

  // Get the previous sibling.
  // TODO(dmazzoni): this code depends on the DOM; move this code out of Blink
  // and make it more general.
  AXObject* previous = nullptr;
  if (GetNode()) {
    Node* previousNode = ElementTraversal::PreviousSibling(*GetNode());
    previous = AXObjectCache().GetOrCreate(previousNode);
  }

  // It has a previous sibling, so if that cell has a column index, this one's
  // index is one greater.
  if (previous) {
    col_index = previous->AriaColumnIndex();
    if (col_index)
      return col_index + 1;
    return 0;
  }

  // No previous cell, so check the row to see if it sets a column index.
  const AXObject* row = TableRowParent();
  if (!row)
    return 0;
  if (row->HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kColIndex,
                                         col_index)) {
    return col_index;
  }

  // Otherwise there's no ARIA column index.
  return 0;
}

unsigned AXObject::ComputeAriaRowIndex() const {
  if (!IsTableCellLikeRole() && !IsTableRowLikeRole())
    return 0;

  // First check if there's an ARIA row index explicitly set.
  uint32_t row_index;
  if (HasAOMPropertyOrARIAAttribute(AOMUIntProperty::kRowIndex, row_index) &&
      row_index >= 1) {
    return row_index;
  }

  // If this is a cell, return the ARIA row index of the containing row.
  if (IsTableCellLikeRole()) {
    const AXObject* row = TableRowParent();
    if (row)
      return row->AriaRowIndex();
    return 0;
  }

  // Otherwise, this is a row. Find the previous sibling row.
  // TODO(dmazzoni): this code depends on the DOM; move this code out of Blink
  // and make it more general.
  if (!GetNode())
    return 0;
  Node* previousNode = ElementTraversal::PreviousSibling(*GetNode());
  AXObject* previous = AXObjectCache().GetOrCreate(previousNode);
  if (!previous || !previous->IsTableRowLikeRole())
    return 0;

  // If the previous row has an ARIA row index, this one is the same index
  // plus one.
  row_index = previous->AriaRowIndex();
  if (row_index)
    return row_index + 1;

  // Otherwise there's no ARIA row index.
  return 0;
}

AXObject::AXObjectVector AXObject::TableRowChildren() const {
  AXObjectVector result;
  for (const auto& child : Children()) {
    if (child->IsTableRowLikeRole())
      result.push_back(child);
    else if (child->RoleValue() == ax::mojom::Role::kGenericContainer)
      result.AppendVector(child->TableRowChildren());
  }
  return result;
}

AXObject::AXObjectVector AXObject::TableCellChildren() const {
  AXObjectVector result;
  for (const auto& child : Children()) {
    if (child->IsTableCellLikeRole())
      result.push_back(child);
    else if (child->RoleValue() == ax::mojom::Role::kGenericContainer)
      result.AppendVector(child->TableCellChildren());
  }
  return result;
}

const AXObject* AXObject::TableRowParent() const {
  const AXObject* row = ParentObjectUnignored();
  while (row && !row->IsTableRowLikeRole() &&
         row->RoleValue() == ax::mojom::Role::kGenericContainer)
    row = row->ParentObjectUnignored();
  return row;
}

const AXObject* AXObject::TableParent() const {
  const AXObject* table = ParentObjectUnignored();
  while (table && !table->IsTableLikeRole() &&
         table->RoleValue() == ax::mojom::Role::kGenericContainer)
    table = table->ParentObjectUnignored();
  return table;
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

  LayoutObject* layout_object = LayoutObjectForRelativeBounds();
  if (!layout_object)
    return;

  if (clips_children) {
    if (IsWebArea())
      *clips_children = true;
    else
      *clips_children = layout_object->HasOverflowClip();
  }

  if (IsWebArea()) {
    if (layout_object->GetFrame()->View()) {
      out_bounds_in_container.SetSize(
          FloatSize(layout_object->GetFrame()->View()->Size()));
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
        FloatRect(ToLayoutBox(layout_object)->PhysicalContentBoxRect());
  }

  // If the container has a scroll offset, subtract that out because we want our
  // bounds to be relative to the *unscrolled* position of the container object.
  if (auto* scrollable_area = container->GetScrollableAreaIfScrollable())
    out_bounds_in_container.Move(scrollable_area->GetScrollOffset());

  // Compute the transform between the container's coordinate space and this
  // object.
  TransformationMatrix transform = layout_object->LocalToAncestorTransform(
      ToLayoutBoxModelObject(container_layout_object));

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

bool AXObject::RequestDecrementAction() {
  Event* event = Event::CreateCancelable(EventTypeNames::accessibledecrement);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeDecrementAction();
}

bool AXObject::RequestClickAction() {
  Event* event = Event::CreateCancelable(EventTypeNames::accessibleclick);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeClickAction();
}

bool AXObject::OnNativeClickAction() {
  Document* document = GetDocument();
  if (!document)
    return false;

  std::unique_ptr<UserGestureIndicator> gesture_indicator =
      LocalFrame::NotifyUserActivation(document->GetFrame(),
                                       UserGestureToken::kNewGesture);

  Element* element = GetElement();
  if (!element && GetNode())
    element = GetNode()->parentElement();

  if (element) {
    element->AccessKeyAction(true);
    return true;
  }

  if (CanSetFocusAttribute())
    return OnNativeFocusAction();

  return false;
}

bool AXObject::RequestFocusAction() {
  Event* event = Event::CreateCancelable(EventTypeNames::accessiblefocus);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeFocusAction();
}

bool AXObject::RequestIncrementAction() {
  Event* event = Event::CreateCancelable(EventTypeNames::accessibleincrement);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeIncrementAction();
}

bool AXObject::RequestScrollToGlobalPointAction(const IntPoint& point) {
  return OnNativeScrollToGlobalPointAction(point);
}

bool AXObject::RequestScrollToMakeVisibleAction() {
  Event* event =
      Event::CreateCancelable(EventTypeNames::accessiblescrollintoview);
  if (DispatchEventToAOMEventListeners(*event))
    return true;

  return OnNativeScrollToMakeVisibleAction();
}

bool AXObject::RequestScrollToMakeVisibleWithSubFocusAction(
    const IntRect& subfocus) {
  return OnNativeScrollToMakeVisibleWithSubFocusAction(subfocus);
}

bool AXObject::RequestSetSelectedAction(bool selected) {
  return OnNativeSetSelectedAction(selected);
}

bool AXObject::RequestSetSelectionAction(const AXSelection& selection) {
  return OnNativeSetSelectionAction(selection);
}

bool AXObject::RequestSetSequentialFocusNavigationStartingPointAction() {
  return OnNativeSetSequentialFocusNavigationStartingPointAction();
}

bool AXObject::RequestSetValueAction(const String& value) {
  return OnNativeSetValueAction(value);
}

bool AXObject::RequestShowContextMenuAction() {
  Event* event = Event::CreateCancelable(EventTypeNames::accessiblecontextmenu);
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

bool AXObject::OnNativeScrollToMakeVisibleAction() const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  layout_object->ScrollRectToVisible(
      target_rect,
      WebScrollIntoViewParams(ScrollAlignment::kAlignCenterIfNeeded,
                              ScrollAlignment::kAlignCenterIfNeeded,
                              kProgrammaticScroll, false, kScrollBehaviorAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::Event::kLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToMakeVisibleWithSubFocusAction(
    const IntRect& rect) const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(
      layout_object->LocalToAbsoluteQuad(FloatQuad(FloatRect(rect)))
          .BoundingBox());
  // TODO(szager): This scroll alignment is intended to preserve existing
  // behavior to the extent possible, but it's not clear that this behavior is
  // well-spec'ed or optimal.  In particular, it favors centering things in
  // the visible viewport rather than snapping them to the closest edge, which
  // is the default behavior of element.scrollIntoView.
  ScrollAlignment scroll_alignment = {
      kScrollAlignmentNoScroll, kScrollAlignmentCenter, kScrollAlignmentCenter};
  layout_object->ScrollRectToVisible(
      target_rect,
      WebScrollIntoViewParams(scroll_alignment, scroll_alignment,
                              kProgrammaticScroll, false, kScrollBehaviorAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::Event::kLocationChanged);
  return true;
}

bool AXObject::OnNativeScrollToGlobalPointAction(
    const IntPoint& global_point) const {
  Node* node = GetNode();
  LayoutObject* layout_object = node ? node->GetLayoutObject() : nullptr;
  if (!layout_object || !node->isConnected())
    return false;
  LayoutRect target_rect(layout_object->AbsoluteBoundingBoxRect());
  target_rect.MoveBy(-global_point);
  layout_object->ScrollRectToVisible(
      target_rect,
      WebScrollIntoViewParams(ScrollAlignment::kAlignLeftAlways,
                              ScrollAlignment::kAlignTopAlways,
                              kProgrammaticScroll, false, kScrollBehaviorAuto));
  AXObjectCache().PostNotification(
      AXObjectCache().GetOrCreate(GetDocument()->GetLayoutView()),
      ax::mojom::Event::kLocationChanged);
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

bool AXObject::OnNativeSetSelectionAction(const AXSelection& selection) {
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
  document->GetFrame()->GetEventHandler().ShowNonLocatedContextMenu(element);
  return true;
}

void AXObject::SelectionChanged() {
  if (AXObject* parent = ParentObjectIfExists())
    parent->SelectionChanged();
}

int AXObject::LineForPosition(const VisiblePosition& position) const {
  if (position.IsNull() || !GetNode())
    return -1;

  // If the position is not in the same editable region as this AX object,
  // return -1.
  Node* container_node = position.DeepEquivalent().ComputeContainerNode();
  if (!container_node->IsShadowIncludingInclusiveAncestorOf(GetNode()) &&
      !GetNode()->IsShadowIncludingInclusiveAncestorOf(container_node))
    return -1;

  int line_count = -1;
  VisiblePosition current_position = position;
  VisiblePosition previous_position;

  // Move up until we get to the top.
  // FIXME: This only takes us to the top of the rootEditableElement, not the
  // top of the top document.
  do {
    previous_position = current_position;
    current_position = PreviousLinePosition(current_position, LayoutUnit(),
                                            kHasEditableAXRole);
    ++line_count;
  } while (current_position.IsNotNull() &&
           !InSameLine(current_position, previous_position));

  return line_count;
}

// static
bool AXObject::IsARIAControl(ax::mojom::Role aria_role) {
  return IsARIAInput(aria_role) || aria_role == ax::mojom::Role::kButton ||
         aria_role == ax::mojom::Role::kComboBoxMenuButton ||
         aria_role == ax::mojom::Role::kSlider;
}

// static
bool AXObject::IsARIAInput(ax::mojom::Role aria_role) {
  return aria_role == ax::mojom::Role::kRadioButton ||
         aria_role == ax::mojom::Role::kCheckBox ||
         aria_role == ax::mojom::Role::kTextField ||
         aria_role == ax::mojom::Role::kSwitch ||
         aria_role == ax::mojom::Role::kSearchBox ||
         aria_role == ax::mojom::Role::kTextFieldWithComboBox;
}

ax::mojom::Role AXObject::AriaRoleToWebCoreRole(const String& value) {
  DCHECK(!value.IsEmpty());

  static const ARIARoleMap* role_map = CreateARIARoleMap();

  Vector<String> role_vector;
  value.Split(' ', role_vector);
  ax::mojom::Role role = ax::mojom::Role::kUnknown;
  for (const auto& child : role_vector) {
    role = role_map->at(child);
    if (role != ax::mojom::Role::kUnknown)
      return role;
  }

  return role;
}

bool AXObject::NameFromSelectedOption(bool recursive) const {
  switch (RoleValue()) {
    // Step 2E from: http://www.w3.org/TR/accname-aam-1.1
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kPopUpButton:
      return recursive;
    default:
      return false;
  }
}

bool AXObject::NameFromContents(bool recursive) const {
  // ARIA 1.1, section 5.2.7.5.
  bool result = false;

  switch (RoleValue()) {
    // ----- NameFrom: contents -------------------------
    // Get their own name from contents, or contribute to ancestors
    case ax::mojom::Role::kAnchor:
    case ax::mojom::Role::kButton:
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDocBackLink:
    case ax::mojom::Role::kDocBiblioRef:
    case ax::mojom::Role::kDocNoteRef:
    case ax::mojom::Role::kDocGlossRef:
    case ax::mojom::Role::kDisclosureTriangle:
    case ax::mojom::Role::kHeading:
    case ax::mojom::Role::kLayoutTableCell:
    case ax::mojom::Role::kLineBreak:
    case ax::mojom::Role::kLink:
    case ax::mojom::Role::kListBoxOption:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItem:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kMenuListOption:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioButton:
    case ax::mojom::Role::kRowHeader:
    case ax::mojom::Role::kStaticText:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTab:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kTreeItem:
    case ax::mojom::Role::kTooltip:
      result = true;
      break;

    // ----- No name from contents -------------------------
    // These never have or contribute a name from contents, as they are
    // containers for many subobjects. Superset of nameFrom:author ARIA roles.
    case ax::mojom::Role::kAlert:
    case ax::mojom::Role::kAlertDialog:
    case ax::mojom::Role::kApplication:
    case ax::mojom::Role::kAudio:
    case ax::mojom::Role::kArticle:
    case ax::mojom::Role::kBanner:
    case ax::mojom::Role::kBlockquote:
    case ax::mojom::Role::kCaret:
    case ax::mojom::Role::kClient:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kColumn:
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComplementary:
    case ax::mojom::Role::kContentInfo:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kDefinition:
    case ax::mojom::Role::kDesktop:
    case ax::mojom::Role::kDialog:
    case ax::mojom::Role::kDirectory:
    case ax::mojom::Role::kDocCover:
    case ax::mojom::Role::kDocBiblioEntry:
    case ax::mojom::Role::kDocEndnote:
    case ax::mojom::Role::kDocFootnote:
    case ax::mojom::Role::kDocPageBreak:
    case ax::mojom::Role::kDocAbstract:
    case ax::mojom::Role::kDocAcknowledgments:
    case ax::mojom::Role::kDocAfterword:
    case ax::mojom::Role::kDocAppendix:
    case ax::mojom::Role::kDocBibliography:
    case ax::mojom::Role::kDocChapter:
    case ax::mojom::Role::kDocColophon:
    case ax::mojom::Role::kDocConclusion:
    case ax::mojom::Role::kDocCredit:
    case ax::mojom::Role::kDocCredits:
    case ax::mojom::Role::kDocDedication:
    case ax::mojom::Role::kDocEndnotes:
    case ax::mojom::Role::kDocEpigraph:
    case ax::mojom::Role::kDocEpilogue:
    case ax::mojom::Role::kDocErrata:
    case ax::mojom::Role::kDocExample:
    case ax::mojom::Role::kDocForeword:
    case ax::mojom::Role::kDocGlossary:
    case ax::mojom::Role::kDocIndex:
    case ax::mojom::Role::kDocIntroduction:
    case ax::mojom::Role::kDocNotice:
    case ax::mojom::Role::kDocPageList:
    case ax::mojom::Role::kDocPart:
    case ax::mojom::Role::kDocPreface:
    case ax::mojom::Role::kDocPrologue:
    case ax::mojom::Role::kDocPullquote:
    case ax::mojom::Role::kDocQna:
    case ax::mojom::Role::kDocSubtitle:
    case ax::mojom::Role::kDocTip:
    case ax::mojom::Role::kDocToc:
    case ax::mojom::Role::kDocument:
    case ax::mojom::Role::kEmbeddedObject:
    case ax::mojom::Role::kFeed:
    case ax::mojom::Role::kFigure:
    case ax::mojom::Role::kForm:
    case ax::mojom::Role::kGraphicsDocument:
    case ax::mojom::Role::kGraphicsObject:
    case ax::mojom::Role::kGraphicsSymbol:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kGroup:
    case ax::mojom::Role::kIframePresentational:
    case ax::mojom::Role::kIframe:
    case ax::mojom::Role::kImage:
    case ax::mojom::Role::kInputTime:
    case ax::mojom::Role::kKeyboard:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kLog:
    case ax::mojom::Role::kMain:
    case ax::mojom::Role::kMarquee:
    case ax::mojom::Role::kMath:
    case ax::mojom::Role::kMenuListPopup:
    case ax::mojom::Role::kMenu:
    case ax::mojom::Role::kMenuBar:
    case ax::mojom::Role::kMeter:
    case ax::mojom::Role::kNavigation:
    case ax::mojom::Role::kNote:
    case ax::mojom::Role::kPane:
    case ax::mojom::Role::kProgressIndicator:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kRootWebArea:
    case ax::mojom::Role::kScrollBar:
    case ax::mojom::Role::kScrollView:
    case ax::mojom::Role::kSearch:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSplitter:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kStatus:
    case ax::mojom::Role::kSliderThumb:
    case ax::mojom::Role::kSvgRoot:
    case ax::mojom::Role::kTable:
    case ax::mojom::Role::kTableHeaderContainer:
    case ax::mojom::Role::kTabList:
    case ax::mojom::Role::kTabPanel:
    case ax::mojom::Role::kTerm:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kTitleBar:
    case ax::mojom::Role::kTime:
    case ax::mojom::Role::kTimer:
    case ax::mojom::Role::kToolbar:
    case ax::mojom::Role::kTree:
    case ax::mojom::Role::kTreeGrid:
    case ax::mojom::Role::kVideo:
    case ax::mojom::Role::kWebArea:
    case ax::mojom::Role::kWebView:
      result = false;
      break;

    // ----- Conditional: contribute to ancestor only, unless focusable -------
    // Some objects can contribute their contents to ancestor names, but
    // only have their own name if they are focusable
    case ax::mojom::Role::kAbbr:
    case ax::mojom::Role::kAnnotation:
    case ax::mojom::Role::kCanvas:
    case ax::mojom::Role::kCaption:
    case ax::mojom::Role::kContentDeletion:
    case ax::mojom::Role::kContentInsertion:
    case ax::mojom::Role::kDescriptionListDetail:
    case ax::mojom::Role::kDescriptionList:
    case ax::mojom::Role::kDescriptionListTerm:
    case ax::mojom::Role::kDetails:
    case ax::mojom::Role::kFigcaption:
    case ax::mojom::Role::kFooter:
    case ax::mojom::Role::kGenericContainer:
    case ax::mojom::Role::kIgnored:
    case ax::mojom::Role::kImageMap:
    case ax::mojom::Role::kInlineTextBox:
    case ax::mojom::Role::kLabelText:
    case ax::mojom::Role::kLayoutTable:
    case ax::mojom::Role::kLayoutTableColumn:
    case ax::mojom::Role::kLayoutTableRow:
    case ax::mojom::Role::kLegend:
    case ax::mojom::Role::kList:
    case ax::mojom::Role::kListItem:
    case ax::mojom::Role::kListMarker:
    case ax::mojom::Role::kMark:
    case ax::mojom::Role::kNone:
    case ax::mojom::Role::kParagraph:
    case ax::mojom::Role::kPre:
    case ax::mojom::Role::kPresentational:
    case ax::mojom::Role::kRegion:
    // Spec says we should always expose the name on rows,
    // but for performance reasons we only do it
    // if the row might receive focus
    case ax::mojom::Role::kRow:
    case ax::mojom::Role::kRuby:
      result = recursive || (CanReceiveAccessibilityFocus() && !IsEditable());
      break;

    case ax::mojom::Role::kUnknown:
    case ax::mojom::Role::kMaxValue:
      LOG(ERROR) << "ax::mojom::Role::kUnknown for " << GetNode();
      NOTREACHED();
      break;
  }

  return result;
}

bool AXObject::SupportsARIAReadOnly() const {
  switch (RoleValue()) {
    case ax::mojom::Role::kCell:
    case ax::mojom::Role::kCheckBox:
    case ax::mojom::Role::kColorWell:
    case ax::mojom::Role::kColumnHeader:
    case ax::mojom::Role::kComboBoxGrouping:
    case ax::mojom::Role::kComboBoxMenuButton:
    case ax::mojom::Role::kDate:
    case ax::mojom::Role::kDateTime:
    case ax::mojom::Role::kGrid:
    case ax::mojom::Role::kInputTime:
    case ax::mojom::Role::kListBox:
    case ax::mojom::Role::kMenuButton:
    case ax::mojom::Role::kMenuItemCheckBox:
    case ax::mojom::Role::kMenuItemRadio:
    case ax::mojom::Role::kPopUpButton:
    case ax::mojom::Role::kRadioGroup:
    case ax::mojom::Role::kRowHeader:
    case ax::mojom::Role::kSearchBox:
    case ax::mojom::Role::kSlider:
    case ax::mojom::Role::kSpinButton:
    case ax::mojom::Role::kSwitch:
    case ax::mojom::Role::kTextField:
    case ax::mojom::Role::kTextFieldWithComboBox:
    case ax::mojom::Role::kToggleButton:
    case ax::mojom::Role::kTreeGrid:
      return true;
    default:
      break;
  }
  return false;
}

ax::mojom::Role AXObject::ButtonRoleType() const {
  // If aria-pressed is present, then it should be exposed as a toggle button.
  // http://www.w3.org/TR/wai-aria/states_and_properties#aria-pressed
  if (AriaPressedIsPresent())
    return ax::mojom::Role::kToggleButton;
  if (HasPopup() != ax::mojom::HasPopup::kFalse)
    return ax::mojom::Role::kPopUpButton;
  // We don't contemplate RadioButtonRole, as it depends on the input
  // type.

  return ax::mojom::Role::kButton;
}

// static
const AtomicString& AXObject::RoleName(ax::mojom::Role role) {
  static const Vector<AtomicString>* role_name_vector = CreateRoleNameVector();

  return role_name_vector->at(static_cast<size_t>(role));
}

// static
const AtomicString& AXObject::InternalRoleName(ax::mojom::Role role) {
  static const Vector<AtomicString>* internal_role_name_vector =
      CreateInternalRoleNameVector();

  return internal_role_name_vector->at(static_cast<size_t>(role));
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
    ancestors1.push_back(ancestors1.back()->ParentObjectUnignored());

  HeapVector<Member<const AXObject>> ancestors2;
  ancestors2.push_back(&second);
  while (ancestors2.back())
    ancestors2.push_back(ancestors2.back()->ParentObjectUnignored());

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

VisiblePosition AXObject::VisiblePositionForIndex(int) const {
  return VisiblePosition();
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

std::ostream& operator<<(std::ostream& stream, const AXObject& obj) {
  return stream << AXObject::InternalRoleName(obj.RoleValue()) << ": "
                << obj.ComputedName();
}

void AXObject::Trace(blink::Visitor* visitor) {
  visitor->Trace(children_);
  visitor->Trace(parent_);
  visitor->Trace(cached_live_region_root_);
  visitor->Trace(ax_object_cache_);
}

}  // namespace blink
