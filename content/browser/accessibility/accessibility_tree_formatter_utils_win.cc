// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_formatter_utils_win.h"

#include <oleacc.h>
#include <uiautomation.h>

#include <map>
#include <string>

#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/iaccessible2/ia2_api_all.h"

namespace content {
namespace {
struct PlatformConstantToNameEntry {
  int32_t value;
  const char* name;
};

base::string16 GetNameForPlatformConstant(
    const PlatformConstantToNameEntry table[],
    size_t table_size,
    int32_t value) {
  for (size_t i = 0; i < table_size; ++i) {
    auto& entry = table[i];
    if (entry.value == value)
      return base::ASCIIToUTF16(entry.name);
  }
  return L"";
}

struct HwndWithProcId {
  HwndWithProcId(const base::ProcessId id) : pid(id), hwnd(nullptr) {}
  const base::ProcessId pid;
  HWND hwnd;
};

BOOL CALLBACK EnumWindowsProcPid(HWND hwnd, LPARAM lParam) {
  DWORD process_id;
  GetWindowThreadProcessId(hwnd, &process_id);
  HwndWithProcId* hwnd_with_proc_id = (HwndWithProcId*)lParam;
  if (process_id == static_cast<DWORD>(hwnd_with_proc_id->pid)) {
    hwnd_with_proc_id->hwnd = hwnd;
    return FALSE;
  }
  return TRUE;
}

}  // namespace

#define QUOTE(X) \
  { X, #X }

CONTENT_EXPORT base::string16 IAccessibleRoleToString(int32_t ia_role) {
  // MSAA / IAccessible roles. Each one of these is also a valid
  // IAccessible2 role.
  static const PlatformConstantToNameEntry ia_table[] = {
      QUOTE(ROLE_SYSTEM_ALERT),          QUOTE(ROLE_SYSTEM_ANIMATION),
      QUOTE(ROLE_SYSTEM_APPLICATION),    QUOTE(ROLE_SYSTEM_BORDER),
      QUOTE(ROLE_SYSTEM_BUTTONDROPDOWN), QUOTE(ROLE_SYSTEM_BUTTONDROPDOWNGRID),
      QUOTE(ROLE_SYSTEM_BUTTONMENU),     QUOTE(ROLE_SYSTEM_CARET),
      QUOTE(ROLE_SYSTEM_CELL),           QUOTE(ROLE_SYSTEM_CHARACTER),
      QUOTE(ROLE_SYSTEM_CHART),          QUOTE(ROLE_SYSTEM_CHECKBUTTON),
      QUOTE(ROLE_SYSTEM_CLIENT),         QUOTE(ROLE_SYSTEM_CLOCK),
      QUOTE(ROLE_SYSTEM_COLUMN),         QUOTE(ROLE_SYSTEM_COLUMNHEADER),
      QUOTE(ROLE_SYSTEM_COMBOBOX),       QUOTE(ROLE_SYSTEM_CURSOR),
      QUOTE(ROLE_SYSTEM_DIAGRAM),        QUOTE(ROLE_SYSTEM_DIAL),
      QUOTE(ROLE_SYSTEM_DIALOG),         QUOTE(ROLE_SYSTEM_DOCUMENT),
      QUOTE(ROLE_SYSTEM_DROPLIST),       QUOTE(ROLE_SYSTEM_EQUATION),
      QUOTE(ROLE_SYSTEM_GRAPHIC),        QUOTE(ROLE_SYSTEM_GRIP),
      QUOTE(ROLE_SYSTEM_GROUPING),       QUOTE(ROLE_SYSTEM_HELPBALLOON),
      QUOTE(ROLE_SYSTEM_HOTKEYFIELD),    QUOTE(ROLE_SYSTEM_INDICATOR),
      QUOTE(ROLE_SYSTEM_IPADDRESS),      QUOTE(ROLE_SYSTEM_LINK),
      QUOTE(ROLE_SYSTEM_LIST),           QUOTE(ROLE_SYSTEM_LISTITEM),
      QUOTE(ROLE_SYSTEM_MENUBAR),        QUOTE(ROLE_SYSTEM_MENUITEM),
      QUOTE(ROLE_SYSTEM_MENUPOPUP),      QUOTE(ROLE_SYSTEM_OUTLINE),
      QUOTE(ROLE_SYSTEM_OUTLINEBUTTON),  QUOTE(ROLE_SYSTEM_OUTLINEITEM),
      QUOTE(ROLE_SYSTEM_PAGETAB),        QUOTE(ROLE_SYSTEM_PAGETABLIST),
      QUOTE(ROLE_SYSTEM_PANE),           QUOTE(ROLE_SYSTEM_PROGRESSBAR),
      QUOTE(ROLE_SYSTEM_PROPERTYPAGE),   QUOTE(ROLE_SYSTEM_PUSHBUTTON),
      QUOTE(ROLE_SYSTEM_RADIOBUTTON),    QUOTE(ROLE_SYSTEM_ROW),
      QUOTE(ROLE_SYSTEM_ROWHEADER),      QUOTE(ROLE_SYSTEM_SCROLLBAR),
      QUOTE(ROLE_SYSTEM_SEPARATOR),      QUOTE(ROLE_SYSTEM_SLIDER),
      QUOTE(ROLE_SYSTEM_SOUND),          QUOTE(ROLE_SYSTEM_SPINBUTTON),
      QUOTE(ROLE_SYSTEM_SPLITBUTTON),    QUOTE(ROLE_SYSTEM_STATICTEXT),
      QUOTE(ROLE_SYSTEM_STATUSBAR),      QUOTE(ROLE_SYSTEM_TABLE),
      QUOTE(ROLE_SYSTEM_TEXT),           QUOTE(ROLE_SYSTEM_TITLEBAR),
      QUOTE(ROLE_SYSTEM_TOOLBAR),        QUOTE(ROLE_SYSTEM_TOOLTIP),
      QUOTE(ROLE_SYSTEM_WHITESPACE),     QUOTE(ROLE_SYSTEM_WINDOW),
  };

  return GetNameForPlatformConstant(ia_table, base::size(ia_table), ia_role);
}

CONTENT_EXPORT base::string16 IAccessible2RoleToString(int32_t ia2_role) {
  base::string16 result = IAccessibleRoleToString(ia2_role);
  if (!result.empty())
    return result;

  static const PlatformConstantToNameEntry ia2_table[] = {
      QUOTE(IA2_ROLE_CANVAS),
      QUOTE(IA2_ROLE_CAPTION),
      QUOTE(IA2_ROLE_CHECK_MENU_ITEM),
      QUOTE(IA2_ROLE_COLOR_CHOOSER),
      QUOTE(IA2_ROLE_DATE_EDITOR),
      QUOTE(IA2_ROLE_DESKTOP_ICON),
      QUOTE(IA2_ROLE_DESKTOP_PANE),
      QUOTE(IA2_ROLE_DIRECTORY_PANE),
      QUOTE(IA2_ROLE_EDITBAR),
      QUOTE(IA2_ROLE_EMBEDDED_OBJECT),
      QUOTE(IA2_ROLE_ENDNOTE),
      QUOTE(IA2_ROLE_FILE_CHOOSER),
      QUOTE(IA2_ROLE_FONT_CHOOSER),
      QUOTE(IA2_ROLE_FOOTER),
      QUOTE(IA2_ROLE_FOOTNOTE),
      QUOTE(IA2_ROLE_FORM),
      QUOTE(IA2_ROLE_FRAME),
      QUOTE(IA2_ROLE_GLASS_PANE),
      QUOTE(IA2_ROLE_HEADER),
      QUOTE(IA2_ROLE_HEADING),
      QUOTE(IA2_ROLE_ICON),
      QUOTE(IA2_ROLE_IMAGE_MAP),
      QUOTE(IA2_ROLE_INPUT_METHOD_WINDOW),
      QUOTE(IA2_ROLE_INTERNAL_FRAME),
      QUOTE(IA2_ROLE_LABEL),
      QUOTE(IA2_ROLE_LAYERED_PANE),
      QUOTE(IA2_ROLE_NOTE),
      QUOTE(IA2_ROLE_OPTION_PANE),
      QUOTE(IA2_ROLE_PAGE),
      QUOTE(IA2_ROLE_PARAGRAPH),
      QUOTE(IA2_ROLE_RADIO_MENU_ITEM),
      QUOTE(IA2_ROLE_REDUNDANT_OBJECT),
      QUOTE(IA2_ROLE_ROOT_PANE),
      QUOTE(IA2_ROLE_RULER),
      QUOTE(IA2_ROLE_SCROLL_PANE),
      QUOTE(IA2_ROLE_SECTION),
      QUOTE(IA2_ROLE_SHAPE),
      QUOTE(IA2_ROLE_SPLIT_PANE),
      QUOTE(IA2_ROLE_TEAR_OFF_MENU),
      QUOTE(IA2_ROLE_TERMINAL),
      QUOTE(IA2_ROLE_TEXT_FRAME),
      QUOTE(IA2_ROLE_TOGGLE_BUTTON),
      QUOTE(IA2_ROLE_UNKNOWN),
      QUOTE(IA2_ROLE_VIEW_PORT),
      QUOTE(IA2_ROLE_COMPLEMENTARY_CONTENT),
      QUOTE(IA2_ROLE_LANDMARK),
      QUOTE(IA2_ROLE_LEVEL_BAR),
      QUOTE(IA2_ROLE_CONTENT_DELETION),
      QUOTE(IA2_ROLE_CONTENT_INSERTION),
  };

  return GetNameForPlatformConstant(ia2_table, base::size(ia2_table), ia2_role);
}

CONTENT_EXPORT base::string16 AccessibilityEventToString(int32_t event) {
  static const PlatformConstantToNameEntry event_table[] = {
      QUOTE(EVENT_OBJECT_CREATE),
      QUOTE(EVENT_OBJECT_DESTROY),
      QUOTE(EVENT_OBJECT_SHOW),
      QUOTE(EVENT_OBJECT_HIDE),
      QUOTE(EVENT_OBJECT_REORDER),
      QUOTE(EVENT_OBJECT_FOCUS),
      QUOTE(EVENT_OBJECT_SELECTION),
      QUOTE(EVENT_OBJECT_SELECTIONADD),
      QUOTE(EVENT_OBJECT_SELECTIONREMOVE),
      QUOTE(EVENT_OBJECT_SELECTIONWITHIN),
      QUOTE(EVENT_OBJECT_STATECHANGE),
      QUOTE(EVENT_OBJECT_LOCATIONCHANGE),
      QUOTE(EVENT_OBJECT_NAMECHANGE),
      QUOTE(EVENT_OBJECT_DESCRIPTIONCHANGE),
      QUOTE(EVENT_OBJECT_VALUECHANGE),
      QUOTE(EVENT_OBJECT_PARENTCHANGE),
      QUOTE(EVENT_OBJECT_HELPCHANGE),
      QUOTE(EVENT_OBJECT_DEFACTIONCHANGE),
      QUOTE(EVENT_OBJECT_ACCELERATORCHANGE),
      QUOTE(EVENT_OBJECT_INVOKED),
      QUOTE(EVENT_OBJECT_TEXTSELECTIONCHANGED),
      QUOTE(EVENT_OBJECT_CONTENTSCROLLED),
      QUOTE(EVENT_OBJECT_LIVEREGIONCHANGED),
      QUOTE(EVENT_OBJECT_HOSTEDOBJECTSINVALIDATED),
      QUOTE(EVENT_OBJECT_DRAGSTART),
      QUOTE(EVENT_OBJECT_DRAGCANCEL),
      QUOTE(EVENT_OBJECT_DRAGCOMPLETE),
      QUOTE(EVENT_OBJECT_DRAGENTER),
      QUOTE(EVENT_OBJECT_DRAGLEAVE),
      QUOTE(EVENT_OBJECT_DRAGDROPPED),
      QUOTE(EVENT_SYSTEM_ALERT),
      QUOTE(EVENT_SYSTEM_MENUPOPUPSTART),
      QUOTE(EVENT_SYSTEM_MENUPOPUPEND),
      QUOTE(EVENT_SYSTEM_SCROLLINGSTART),
      QUOTE(EVENT_SYSTEM_SCROLLINGEND),
      QUOTE(IA2_EVENT_ACTION_CHANGED),
      QUOTE(IA2_EVENT_ACTIVE_DESCENDANT_CHANGED),
      QUOTE(IA2_EVENT_DOCUMENT_ATTRIBUTE_CHANGED),
      QUOTE(IA2_EVENT_DOCUMENT_CONTENT_CHANGED),
      QUOTE(IA2_EVENT_DOCUMENT_LOAD_COMPLETE),
      QUOTE(IA2_EVENT_DOCUMENT_LOAD_STOPPED),
      QUOTE(IA2_EVENT_DOCUMENT_RELOAD),
      QUOTE(IA2_EVENT_HYPERLINK_END_INDEX_CHANGED),
      QUOTE(IA2_EVENT_HYPERLINK_NUMBER_OF_ANCHORS_CHANGED),
      QUOTE(IA2_EVENT_HYPERLINK_SELECTED_LINK_CHANGED),
      QUOTE(IA2_EVENT_HYPERTEXT_LINK_ACTIVATED),
      QUOTE(IA2_EVENT_HYPERTEXT_LINK_SELECTED),
      QUOTE(IA2_EVENT_HYPERLINK_START_INDEX_CHANGED),
      QUOTE(IA2_EVENT_HYPERTEXT_CHANGED),
      QUOTE(IA2_EVENT_HYPERTEXT_NLINKS_CHANGED),
      QUOTE(IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED),
      QUOTE(IA2_EVENT_PAGE_CHANGED),
      QUOTE(IA2_EVENT_SECTION_CHANGED),
      QUOTE(IA2_EVENT_TABLE_CAPTION_CHANGED),
      QUOTE(IA2_EVENT_TABLE_COLUMN_DESCRIPTION_CHANGED),
      QUOTE(IA2_EVENT_TABLE_COLUMN_HEADER_CHANGED),
      QUOTE(IA2_EVENT_TABLE_MODEL_CHANGED),
      QUOTE(IA2_EVENT_TABLE_ROW_DESCRIPTION_CHANGED),
      QUOTE(IA2_EVENT_TABLE_ROW_HEADER_CHANGED),
      QUOTE(IA2_EVENT_TABLE_SUMMARY_CHANGED),
      QUOTE(IA2_EVENT_TEXT_ATTRIBUTE_CHANGED),
      QUOTE(IA2_EVENT_TEXT_CARET_MOVED),
      QUOTE(IA2_EVENT_TEXT_CHANGED),
      QUOTE(IA2_EVENT_TEXT_COLUMN_CHANGED),
      QUOTE(IA2_EVENT_TEXT_INSERTED),
      QUOTE(IA2_EVENT_TEXT_REMOVED),
      QUOTE(IA2_EVENT_TEXT_UPDATED),
      QUOTE(IA2_EVENT_TEXT_SELECTION_CHANGED),
      QUOTE(IA2_EVENT_VISIBLE_DATA_CHANGED),
  };

  return GetNameForPlatformConstant(event_table, base::size(event_table),
                                    event);
}

void IAccessibleStateToStringVector(int32_t ia_state,
                                    std::vector<base::string16>* result) {
#define QUOTE_STATE(X) \
  { STATE_SYSTEM_##X, #X }
  // MSAA / IAccessible states. Unlike roles, these are not also IA2 states.
  //
  // Note: for historical reasons these are in numerical order rather than
  // alphabetical order. Changing the order would change the order in which
  // the states are printed, which would affect a bunch of tests.
  static const PlatformConstantToNameEntry ia_table[] = {
      QUOTE_STATE(UNAVAILABLE),     QUOTE_STATE(SELECTED),
      QUOTE_STATE(FOCUSED),         QUOTE_STATE(PRESSED),
      QUOTE_STATE(CHECKED),         QUOTE_STATE(MIXED),
      QUOTE_STATE(READONLY),        QUOTE_STATE(HOTTRACKED),
      QUOTE_STATE(DEFAULT),         QUOTE_STATE(EXPANDED),
      QUOTE_STATE(COLLAPSED),       QUOTE_STATE(BUSY),
      QUOTE_STATE(FLOATING),        QUOTE_STATE(MARQUEED),
      QUOTE_STATE(ANIMATED),        QUOTE_STATE(INVISIBLE),
      QUOTE_STATE(OFFSCREEN),       QUOTE_STATE(SIZEABLE),
      QUOTE_STATE(MOVEABLE),        QUOTE_STATE(SELFVOICING),
      QUOTE_STATE(FOCUSABLE),       QUOTE_STATE(SELECTABLE),
      QUOTE_STATE(LINKED),          QUOTE_STATE(TRAVERSED),
      QUOTE_STATE(MULTISELECTABLE), QUOTE_STATE(EXTSELECTABLE),
      QUOTE_STATE(ALERT_LOW),       QUOTE_STATE(ALERT_MEDIUM),
      QUOTE_STATE(ALERT_HIGH),      QUOTE_STATE(PROTECTED),
      QUOTE_STATE(HASPOPUP),
  };
  for (auto& entry : ia_table) {
    if (entry.value & ia_state)
      result->push_back(base::ASCIIToUTF16(entry.name));
  }
}

base::string16 IAccessibleStateToString(int32_t ia_state) {
  std::vector<base::string16> strings;
  IAccessibleStateToStringVector(ia_state, &strings);
  return base::JoinString(strings, base::ASCIIToUTF16(","));
}

void IAccessible2StateToStringVector(int32_t ia2_state,
                                     std::vector<base::string16>* result) {
  // Note: for historical reasons these are in numerical order rather than
  // alphabetical order. Changing the order would change the order in which
  // the states are printed, which would affect a bunch of tests.
  static const PlatformConstantToNameEntry ia2_table[] = {
      QUOTE(IA2_STATE_ACTIVE),
      QUOTE(IA2_STATE_ARMED),
      QUOTE(IA2_STATE_CHECKABLE),
      QUOTE(IA2_STATE_DEFUNCT),
      QUOTE(IA2_STATE_EDITABLE),
      QUOTE(IA2_STATE_HORIZONTAL),
      QUOTE(IA2_STATE_ICONIFIED),
      QUOTE(IA2_STATE_INVALID_ENTRY),
      QUOTE(IA2_STATE_MANAGES_DESCENDANTS),
      QUOTE(IA2_STATE_MODAL),
      QUOTE(IA2_STATE_MULTI_LINE),
      QUOTE(IA2_STATE_REQUIRED),
      QUOTE(IA2_STATE_SELECTABLE_TEXT),
      QUOTE(IA2_STATE_SINGLE_LINE),
      QUOTE(IA2_STATE_STALE),
      QUOTE(IA2_STATE_SUPPORTS_AUTOCOMPLETION),
      QUOTE(IA2_STATE_TRANSIENT),
      QUOTE(IA2_STATE_VERTICAL),
      // Untested states include those that would be repeated on nearly
      // every node or would vary based on window size.
      // QUOTE(IA2_STATE_OPAQUE) // Untested.
  };

  for (auto& entry : ia2_table) {
    if (entry.value & ia2_state)
      result->push_back(base::ASCIIToUTF16(entry.name));
  }
}

base::string16 IAccessible2StateToString(int32_t ia2_state) {
  std::vector<base::string16> strings;
  IAccessible2StateToStringVector(ia2_state, &strings);
  return base::JoinString(strings, base::ASCIIToUTF16(","));
}

CONTENT_EXPORT base::string16 UiaIdentifierToString(int32_t identifier) {
  static const PlatformConstantToNameEntry id_table[] = {
      // Patterns
      QUOTE(UIA_InvokePatternId),
      QUOTE(UIA_SelectionPatternId),
      QUOTE(UIA_ValuePatternId),
      QUOTE(UIA_RangeValuePatternId),
      QUOTE(UIA_ScrollPatternId),
      QUOTE(UIA_ExpandCollapsePatternId),
      QUOTE(UIA_GridPatternId),
      QUOTE(UIA_GridItemPatternId),
      QUOTE(UIA_MultipleViewPatternId),
      QUOTE(UIA_WindowPatternId),
      QUOTE(UIA_SelectionItemPatternId),
      QUOTE(UIA_DockPatternId),
      QUOTE(UIA_TablePatternId),
      QUOTE(UIA_TableItemPatternId),
      QUOTE(UIA_TextPatternId),
      QUOTE(UIA_TogglePatternId),
      QUOTE(UIA_TransformPatternId),
      QUOTE(UIA_ScrollItemPatternId),
      QUOTE(UIA_LegacyIAccessiblePatternId),
      QUOTE(UIA_ItemContainerPatternId),
      QUOTE(UIA_VirtualizedItemPatternId),
      QUOTE(UIA_SynchronizedInputPatternId),
      QUOTE(UIA_ObjectModelPatternId),
      QUOTE(UIA_AnnotationPatternId),
      QUOTE(UIA_TextPattern2Id),
      QUOTE(UIA_StylesPatternId),
      QUOTE(UIA_SpreadsheetPatternId),
      QUOTE(UIA_SpreadsheetItemPatternId),
      QUOTE(UIA_TransformPattern2Id),
      QUOTE(UIA_TextChildPatternId),
      QUOTE(UIA_DragPatternId),
      QUOTE(UIA_DropTargetPatternId),
      QUOTE(UIA_TextEditPatternId),
      QUOTE(UIA_CustomNavigationPatternId),
      QUOTE(UIA_SelectionPattern2Id),
      // Events
      QUOTE(UIA_ToolTipOpenedEventId),
      QUOTE(UIA_ToolTipClosedEventId),
      QUOTE(UIA_StructureChangedEventId),
      QUOTE(UIA_MenuOpenedEventId),
      QUOTE(UIA_AutomationPropertyChangedEventId),
      QUOTE(UIA_AutomationFocusChangedEventId),
      QUOTE(UIA_AsyncContentLoadedEventId),
      QUOTE(UIA_MenuClosedEventId),
      QUOTE(UIA_LayoutInvalidatedEventId),
      QUOTE(UIA_Invoke_InvokedEventId),
      QUOTE(UIA_SelectionItem_ElementAddedToSelectionEventId),
      QUOTE(UIA_SelectionItem_ElementRemovedFromSelectionEventId),
      QUOTE(UIA_SelectionItem_ElementSelectedEventId),
      QUOTE(UIA_Selection_InvalidatedEventId),
      QUOTE(UIA_Text_TextSelectionChangedEventId),
      QUOTE(UIA_Text_TextChangedEventId),
      QUOTE(UIA_Window_WindowOpenedEventId),
      QUOTE(UIA_Window_WindowClosedEventId),
      QUOTE(UIA_MenuModeStartEventId),
      QUOTE(UIA_MenuModeEndEventId),
      QUOTE(UIA_InputReachedTargetEventId),
      QUOTE(UIA_InputReachedOtherElementEventId),
      QUOTE(UIA_InputDiscardedEventId),
      QUOTE(UIA_SystemAlertEventId),
      QUOTE(UIA_LiveRegionChangedEventId),
      QUOTE(UIA_HostedFragmentRootsInvalidatedEventId),
      QUOTE(UIA_Drag_DragStartEventId),
      QUOTE(UIA_Drag_DragCancelEventId),
      QUOTE(UIA_Drag_DragCompleteEventId),
      QUOTE(UIA_DropTarget_DragEnterEventId),
      QUOTE(UIA_DropTarget_DragLeaveEventId),
      QUOTE(UIA_DropTarget_DroppedEventId),
      QUOTE(UIA_TextEdit_TextChangedEventId),
      QUOTE(UIA_TextEdit_ConversionTargetChangedEventId),
      QUOTE(UIA_ChangesEventId),
      QUOTE(UIA_NotificationEventId),
      // Properties
      QUOTE(UIA_RuntimeIdPropertyId),
      QUOTE(UIA_BoundingRectanglePropertyId),
      QUOTE(UIA_ProcessIdPropertyId),
      QUOTE(UIA_ControlTypePropertyId),
      QUOTE(UIA_LocalizedControlTypePropertyId),
      QUOTE(UIA_NamePropertyId),
      QUOTE(UIA_AcceleratorKeyPropertyId),
      QUOTE(UIA_AccessKeyPropertyId),
      QUOTE(UIA_HasKeyboardFocusPropertyId),
      QUOTE(UIA_IsKeyboardFocusablePropertyId),
      QUOTE(UIA_IsEnabledPropertyId),
      QUOTE(UIA_AutomationIdPropertyId),
      QUOTE(UIA_ClassNamePropertyId),
      QUOTE(UIA_HelpTextPropertyId),
      QUOTE(UIA_ClickablePointPropertyId),
      QUOTE(UIA_CulturePropertyId),
      QUOTE(UIA_IsControlElementPropertyId),
      QUOTE(UIA_IsContentElementPropertyId),
      QUOTE(UIA_LabeledByPropertyId),
      QUOTE(UIA_IsPasswordPropertyId),
      QUOTE(UIA_NativeWindowHandlePropertyId),
      QUOTE(UIA_ItemTypePropertyId),
      QUOTE(UIA_IsOffscreenPropertyId),
      QUOTE(UIA_OrientationPropertyId),
      QUOTE(UIA_FrameworkIdPropertyId),
      QUOTE(UIA_IsRequiredForFormPropertyId),
      QUOTE(UIA_ItemStatusPropertyId),
      QUOTE(UIA_IsDockPatternAvailablePropertyId),
      QUOTE(UIA_IsExpandCollapsePatternAvailablePropertyId),
      QUOTE(UIA_IsGridItemPatternAvailablePropertyId),
      QUOTE(UIA_IsGridPatternAvailablePropertyId),
      QUOTE(UIA_IsInvokePatternAvailablePropertyId),
      QUOTE(UIA_IsMultipleViewPatternAvailablePropertyId),
      QUOTE(UIA_IsRangeValuePatternAvailablePropertyId),
      QUOTE(UIA_IsScrollPatternAvailablePropertyId),
      QUOTE(UIA_IsScrollItemPatternAvailablePropertyId),
      QUOTE(UIA_IsSelectionItemPatternAvailablePropertyId),
      QUOTE(UIA_IsSelectionPatternAvailablePropertyId),
      QUOTE(UIA_IsTablePatternAvailablePropertyId),
      QUOTE(UIA_IsTableItemPatternAvailablePropertyId),
      QUOTE(UIA_IsTextPatternAvailablePropertyId),
      QUOTE(UIA_IsTogglePatternAvailablePropertyId),
      QUOTE(UIA_IsTransformPatternAvailablePropertyId),
      QUOTE(UIA_IsValuePatternAvailablePropertyId),
      QUOTE(UIA_IsWindowPatternAvailablePropertyId),
      QUOTE(UIA_ValueValuePropertyId),
      QUOTE(UIA_ValueIsReadOnlyPropertyId),
      QUOTE(UIA_RangeValueValuePropertyId),
      QUOTE(UIA_RangeValueIsReadOnlyPropertyId),
      QUOTE(UIA_RangeValueMinimumPropertyId),
      QUOTE(UIA_RangeValueMaximumPropertyId),
      QUOTE(UIA_RangeValueLargeChangePropertyId),
      QUOTE(UIA_RangeValueSmallChangePropertyId),
      QUOTE(UIA_ScrollHorizontalScrollPercentPropertyId),
      QUOTE(UIA_ScrollHorizontalViewSizePropertyId),
      QUOTE(UIA_ScrollVerticalScrollPercentPropertyId),
      QUOTE(UIA_ScrollVerticalViewSizePropertyId),
      QUOTE(UIA_ScrollHorizontallyScrollablePropertyId),
      QUOTE(UIA_ScrollVerticallyScrollablePropertyId),
      QUOTE(UIA_SelectionSelectionPropertyId),
      QUOTE(UIA_SelectionCanSelectMultiplePropertyId),
      QUOTE(UIA_SelectionIsSelectionRequiredPropertyId),
      QUOTE(UIA_GridRowCountPropertyId),
      QUOTE(UIA_GridColumnCountPropertyId),
      QUOTE(UIA_GridItemRowPropertyId),
      QUOTE(UIA_GridItemColumnPropertyId),
      QUOTE(UIA_GridItemRowSpanPropertyId),
      QUOTE(UIA_GridItemColumnSpanPropertyId),
      QUOTE(UIA_GridItemContainingGridPropertyId),
      QUOTE(UIA_DockDockPositionPropertyId),
      QUOTE(UIA_ExpandCollapseExpandCollapseStatePropertyId),
      QUOTE(UIA_MultipleViewCurrentViewPropertyId),
      QUOTE(UIA_MultipleViewSupportedViewsPropertyId),
      QUOTE(UIA_WindowCanMaximizePropertyId),
      QUOTE(UIA_WindowCanMinimizePropertyId),
      QUOTE(UIA_WindowWindowVisualStatePropertyId),
      QUOTE(UIA_WindowWindowInteractionStatePropertyId),
      QUOTE(UIA_WindowIsModalPropertyId),
      QUOTE(UIA_WindowIsTopmostPropertyId),
      QUOTE(UIA_SelectionItemIsSelectedPropertyId),
      QUOTE(UIA_SelectionItemSelectionContainerPropertyId),
      QUOTE(UIA_TableRowHeadersPropertyId),
      QUOTE(UIA_TableColumnHeadersPropertyId),
      QUOTE(UIA_TableRowOrColumnMajorPropertyId),
      QUOTE(UIA_TableItemRowHeaderItemsPropertyId),
      QUOTE(UIA_TableItemColumnHeaderItemsPropertyId),
      QUOTE(UIA_ToggleToggleStatePropertyId),
      QUOTE(UIA_TransformCanMovePropertyId),
      QUOTE(UIA_TransformCanResizePropertyId),
      QUOTE(UIA_TransformCanRotatePropertyId),
      QUOTE(UIA_IsLegacyIAccessiblePatternAvailablePropertyId),
      QUOTE(UIA_LegacyIAccessibleChildIdPropertyId),
      QUOTE(UIA_LegacyIAccessibleNamePropertyId),
      QUOTE(UIA_LegacyIAccessibleValuePropertyId),
      QUOTE(UIA_LegacyIAccessibleDescriptionPropertyId),
      QUOTE(UIA_LegacyIAccessibleRolePropertyId),
      QUOTE(UIA_LegacyIAccessibleStatePropertyId),
      QUOTE(UIA_LegacyIAccessibleHelpPropertyId),
      QUOTE(UIA_LegacyIAccessibleKeyboardShortcutPropertyId),
      QUOTE(UIA_LegacyIAccessibleSelectionPropertyId),
      QUOTE(UIA_LegacyIAccessibleDefaultActionPropertyId),
      QUOTE(UIA_AriaRolePropertyId),
      QUOTE(UIA_AriaPropertiesPropertyId),
      QUOTE(UIA_IsDataValidForFormPropertyId),
      QUOTE(UIA_ControllerForPropertyId),
      QUOTE(UIA_DescribedByPropertyId),
      QUOTE(UIA_FlowsToPropertyId),
      QUOTE(UIA_ProviderDescriptionPropertyId),
      QUOTE(UIA_IsItemContainerPatternAvailablePropertyId),
      QUOTE(UIA_IsVirtualizedItemPatternAvailablePropertyId),
      QUOTE(UIA_IsSynchronizedInputPatternAvailablePropertyId),
      QUOTE(UIA_OptimizeForVisualContentPropertyId),
      QUOTE(UIA_IsObjectModelPatternAvailablePropertyId),
      QUOTE(UIA_AnnotationAnnotationTypeIdPropertyId),
      QUOTE(UIA_AnnotationAnnotationTypeNamePropertyId),
      QUOTE(UIA_AnnotationAuthorPropertyId),
      QUOTE(UIA_AnnotationDateTimePropertyId),
      QUOTE(UIA_AnnotationTargetPropertyId),
      QUOTE(UIA_IsAnnotationPatternAvailablePropertyId),
      QUOTE(UIA_IsTextPattern2AvailablePropertyId),
      QUOTE(UIA_StylesStyleIdPropertyId),
      QUOTE(UIA_StylesStyleNamePropertyId),
      QUOTE(UIA_StylesFillColorPropertyId),
      QUOTE(UIA_StylesFillPatternStylePropertyId),
      QUOTE(UIA_StylesShapePropertyId),
      QUOTE(UIA_StylesFillPatternColorPropertyId),
      QUOTE(UIA_StylesExtendedPropertiesPropertyId),
      QUOTE(UIA_IsStylesPatternAvailablePropertyId),
      QUOTE(UIA_IsSpreadsheetPatternAvailablePropertyId),
      QUOTE(UIA_SpreadsheetItemFormulaPropertyId),
      QUOTE(UIA_SpreadsheetItemAnnotationObjectsPropertyId),
      QUOTE(UIA_SpreadsheetItemAnnotationTypesPropertyId),
      QUOTE(UIA_IsSpreadsheetItemPatternAvailablePropertyId),
      QUOTE(UIA_Transform2CanZoomPropertyId),
      QUOTE(UIA_IsTransformPattern2AvailablePropertyId),
      QUOTE(UIA_LiveSettingPropertyId),
      QUOTE(UIA_IsTextChildPatternAvailablePropertyId),
      QUOTE(UIA_IsDragPatternAvailablePropertyId),
      QUOTE(UIA_DragIsGrabbedPropertyId),
      QUOTE(UIA_DragDropEffectPropertyId),
      QUOTE(UIA_DragDropEffectsPropertyId),
      QUOTE(UIA_IsDropTargetPatternAvailablePropertyId),
      QUOTE(UIA_DropTargetDropTargetEffectPropertyId),
      QUOTE(UIA_DropTargetDropTargetEffectsPropertyId),
      QUOTE(UIA_DragGrabbedItemsPropertyId),
      QUOTE(UIA_Transform2ZoomLevelPropertyId),
      QUOTE(UIA_Transform2ZoomMinimumPropertyId),
      QUOTE(UIA_Transform2ZoomMaximumPropertyId),
      QUOTE(UIA_FlowsFromPropertyId),
      QUOTE(UIA_IsTextEditPatternAvailablePropertyId),
      QUOTE(UIA_IsPeripheralPropertyId),
      QUOTE(UIA_IsCustomNavigationPatternAvailablePropertyId),
      QUOTE(UIA_PositionInSetPropertyId),
      QUOTE(UIA_SizeOfSetPropertyId),
      QUOTE(UIA_LevelPropertyId),
      QUOTE(UIA_AnnotationTypesPropertyId),
      QUOTE(UIA_AnnotationObjectsPropertyId),
      QUOTE(UIA_LandmarkTypePropertyId),
      QUOTE(UIA_LocalizedLandmarkTypePropertyId),
      QUOTE(UIA_FullDescriptionPropertyId),
      QUOTE(UIA_FillColorPropertyId),
      QUOTE(UIA_OutlineColorPropertyId),
      QUOTE(UIA_FillTypePropertyId),
      QUOTE(UIA_VisualEffectsPropertyId),
      QUOTE(UIA_OutlineThicknessPropertyId),
      QUOTE(UIA_CenterPointPropertyId),
      QUOTE(UIA_RotationPropertyId),
      QUOTE(UIA_SizePropertyId),
      QUOTE(UIA_IsSelectionPattern2AvailablePropertyId),
      QUOTE(UIA_Selection2FirstSelectedItemPropertyId),
      QUOTE(UIA_Selection2LastSelectedItemPropertyId),
      QUOTE(UIA_Selection2CurrentSelectedItemPropertyId),
      QUOTE(UIA_Selection2ItemCountPropertyId),
      QUOTE(UIA_HeadingLevelPropertyId),
      // Attributes
      QUOTE(UIA_AnimationStyleAttributeId),
      QUOTE(UIA_BackgroundColorAttributeId),
      QUOTE(UIA_BulletStyleAttributeId),
      QUOTE(UIA_CapStyleAttributeId),
      QUOTE(UIA_CultureAttributeId),
      QUOTE(UIA_FontNameAttributeId),
      QUOTE(UIA_FontSizeAttributeId),
      QUOTE(UIA_FontWeightAttributeId),
      QUOTE(UIA_ForegroundColorAttributeId),
      QUOTE(UIA_HorizontalTextAlignmentAttributeId),
      QUOTE(UIA_IndentationFirstLineAttributeId),
      QUOTE(UIA_IndentationLeadingAttributeId),
      QUOTE(UIA_IndentationTrailingAttributeId),
      QUOTE(UIA_IsHiddenAttributeId),
      QUOTE(UIA_IsItalicAttributeId),
      QUOTE(UIA_IsReadOnlyAttributeId),
      QUOTE(UIA_IsSubscriptAttributeId),
      QUOTE(UIA_IsSuperscriptAttributeId),
      QUOTE(UIA_MarginBottomAttributeId),
      QUOTE(UIA_MarginLeadingAttributeId),
      QUOTE(UIA_MarginTopAttributeId),
      QUOTE(UIA_MarginTrailingAttributeId),
      QUOTE(UIA_OutlineStylesAttributeId),
      QUOTE(UIA_OverlineColorAttributeId),
      QUOTE(UIA_OverlineStyleAttributeId),
      QUOTE(UIA_StrikethroughColorAttributeId),
      QUOTE(UIA_StrikethroughStyleAttributeId),
      QUOTE(UIA_TabsAttributeId),
      QUOTE(UIA_TextFlowDirectionsAttributeId),
      QUOTE(UIA_UnderlineColorAttributeId),
      QUOTE(UIA_UnderlineStyleAttributeId),
      QUOTE(UIA_AnnotationTypesAttributeId),
      QUOTE(UIA_AnnotationObjectsAttributeId),
      QUOTE(UIA_StyleNameAttributeId),
      QUOTE(UIA_StyleIdAttributeId),
      QUOTE(UIA_LinkAttributeId),
      QUOTE(UIA_IsActiveAttributeId),
      QUOTE(UIA_SelectionActiveEndAttributeId),
      QUOTE(UIA_CaretPositionAttributeId),
      QUOTE(UIA_CaretBidiModeAttributeId),
      QUOTE(UIA_LineSpacingAttributeId),
      QUOTE(UIA_BeforeParagraphSpacingAttributeId),
      QUOTE(UIA_AfterParagraphSpacingAttributeId),
      QUOTE(UIA_SayAsInterpretAsAttributeId),
      // Control Types
      QUOTE(UIA_ButtonControlTypeId),
      QUOTE(UIA_CalendarControlTypeId),
      QUOTE(UIA_CheckBoxControlTypeId),
      QUOTE(UIA_ComboBoxControlTypeId),
      QUOTE(UIA_EditControlTypeId),
      QUOTE(UIA_HyperlinkControlTypeId),
      QUOTE(UIA_ImageControlTypeId),
      QUOTE(UIA_ListItemControlTypeId),
      QUOTE(UIA_ListControlTypeId),
      QUOTE(UIA_MenuControlTypeId),
      QUOTE(UIA_MenuBarControlTypeId),
      QUOTE(UIA_MenuItemControlTypeId),
      QUOTE(UIA_ProgressBarControlTypeId),
      QUOTE(UIA_RadioButtonControlTypeId),
      QUOTE(UIA_ScrollBarControlTypeId),
      QUOTE(UIA_SliderControlTypeId),
      QUOTE(UIA_SpinnerControlTypeId),
      QUOTE(UIA_StatusBarControlTypeId),
      QUOTE(UIA_TabControlTypeId),
      QUOTE(UIA_TabItemControlTypeId),
      QUOTE(UIA_TextControlTypeId),
      QUOTE(UIA_ToolBarControlTypeId),
      QUOTE(UIA_ToolTipControlTypeId),
      QUOTE(UIA_TreeControlTypeId),
      QUOTE(UIA_TreeItemControlTypeId),
      QUOTE(UIA_CustomControlTypeId),
      QUOTE(UIA_GroupControlTypeId),
      QUOTE(UIA_ThumbControlTypeId),
      QUOTE(UIA_DataGridControlTypeId),
      QUOTE(UIA_DataItemControlTypeId),
      QUOTE(UIA_DocumentControlTypeId),
      QUOTE(UIA_SplitButtonControlTypeId),
      QUOTE(UIA_WindowControlTypeId),
      QUOTE(UIA_PaneControlTypeId),
      QUOTE(UIA_HeaderControlTypeId),
      QUOTE(UIA_HeaderItemControlTypeId),
      QUOTE(UIA_TableControlTypeId),
      QUOTE(UIA_TitleBarControlTypeId),
      QUOTE(UIA_SeparatorControlTypeId),
      QUOTE(UIA_SemanticZoomControlTypeId),
      QUOTE(UIA_AppBarControlTypeId),
  };

  return GetNameForPlatformConstant(id_table, base::size(id_table), identifier);
}

CONTENT_EXPORT base::string16 UiaOrientationToString(int32_t identifier) {
  static const PlatformConstantToNameEntry id_table[] = {
      QUOTE(OrientationType_None), QUOTE(OrientationType_Horizontal),
      QUOTE(OrientationType_Vertical)};
  return GetNameForPlatformConstant(id_table, base::size(id_table), identifier);
}

CONTENT_EXPORT base::string16 UiaLiveSettingToString(int32_t identifier) {
  static const PlatformConstantToNameEntry id_table[] = {
      QUOTE(LiveSetting::Off), QUOTE(LiveSetting::Polite),
      QUOTE(LiveSetting::Assertive)};
  return GetNameForPlatformConstant(id_table, base::size(id_table), identifier);
}

CONTENT_EXPORT std::string BstrToUTF8(BSTR bstr) {
  base::string16 str16(bstr, SysStringLen(bstr));
  return base::UTF16ToUTF8(str16);
}

CONTENT_EXPORT std::string UiaIdentifierToStringUTF8(int32_t id) {
  return base::UTF16ToUTF8(UiaIdentifierToString(id));
}

CONTENT_EXPORT HWND GetHwndForProcess(base::ProcessId pid) {
  HwndWithProcId hwnd_with_proc_id(pid);
  EnumWindows(&EnumWindowsProcPid, (LPARAM)&hwnd_with_proc_id);
  return hwnd_with_proc_id.hwnd;
}
}  // namespace content
