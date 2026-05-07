// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Keep the following enums in sync with 'ui/accessibility/ax_enums.mojom'.
// They are kept here purely for extension docs generation.

// Possible events fired on an $(ref:automation.AutomationNode).
enum EventType {
  "accessKeyChanged",
  "activeDescendantChanged",
  "alert",
  // TODO(crbug.com/1464633) Fully remove ariaAttributeChangedDeprecated
  // starting in 122, because although it was removed in 118, it is still
  // present in earlier versions of LaCros.
  "ariaAttributeChangedDeprecated",
  "ariaCurrentChanged",
  "ariaNotificationsPosted",
  "atomicChanged",
  "autoCompleteChanged",
  "autocorrectionOccured",
  "autofillAvailabilityChanged",
  "blur",
  "busyChanged",
  "caretBoundsChanged",
  "checkedStateChanged",
  "checkedStateDescriptionChanged",
  "childrenChanged",
  "clicked",
  "collapsed",
  "controlsChanged",
  "defaultActionVerbChanged",
  "detailsChanged",
  "describedByChanged",
  "descriptionChanged",
  "documentSelectionChanged",
  "documentTitleChanged",
  "dropeffectChanged",
  "editableTextChanged",
  "enabledChanged",
  "endOfTest",
  "expanded",
  "expandedChanged",
  "flowFromChanged",
  "flowToChanged",
  "focus",
  "focusAfterMenuClose",
  "focusChanged",
  "focusContextDeprecated",
  "grabbedChanged",
  "grammarMarkerChanged",
  "haspopupChanged",
  "hide",
  "hierarchicalLevelChanged",
  "highlightMarkerChanged",
  "hitTestResult",
  "hover",
  "ignoredChanged",
  "imageAnnotationChanged",
  "imageFrameUpdated",
  "invalidStatusChanged",
  "keyShortcutsChanged",
  "labeledByChanged",
  "languageChanged",
  "layoutComplete",
  // fired when aria-busy goes false
  "layoutInvalidated",
  "liveRegionChanged",
  "liveRegionCreated",
  // fired on a node within a live region.
  "liveRegionNodeChanged",
  "liveRelevantChanged",
  "liveStatusChanged",
  "loadComplete",
  "loadStart",
  "locationChanged",
  "mediaStartedPlaying",
  "mediaStoppedPlaying",
  "menuEnd",
  "menuItemSelected",
  "menuListValueChangedDeprecated",
  "menuPopupEnd",
  "menuPopupStart",
  "menuStart",
  "mouseCanceled",
  "mouseDragged",
  "mouseMoved",
  "mousePressed",
  "mouseReleased",
  "multilineStateChanged",
  "multiselectableStateChanged",
  "nameChanged",
  "objectAttributeChanged",
  "orientationChanged",
  "parentChanged",
  "placeholderChanged",
  "positionInSetChanged",
  "rangeValueChanged",
  "rangeValueMaxChanged",
  "rangeValueMinChanged",
  "rangeValueStepChanged",
  "readonlyChanged",
  "relatedNodeChanged",
  "requiredStateChanged",
  "roleChanged",
  "rowCollapsed",
  "rowCountChanged",
  "rowExpanded",
  "scrollHorizontalPositionChanged",
  "scrollPositionChanged",
  "scrollVerticalPositionChanged",
  "scrolledToAnchor",
  "selectedChanged",
  "selectedChildrenChanged",
  "selectedValueChanged",
  "selection",
  "selectionAdd",
  "selectionRemove",
  "setSizeChanged",
  "show",
  "sortChanged",
  "spellingMarkerChanged",
  "stateChanged",
  "subtreeCreated",
  "textAttributeChanged",
  "textSelectionChanged",
  "textChanged",
  "tooltipClosed",
  "tooltipOpened",
  "treeChanged",
  "valueInSpinButtonDecremented",
  "valueInSpinButtonIncremented",
  "valueInTextFieldChanged",
  // Deprecated.
  "valueChanged",
  "windowActivated",
  "windowDeactivated",
  "windowVisibilityChanged"
};

// Describes the purpose of an $(ref:automation.AutomationNode).
enum RoleType {
  "abbr",
  "alert",
  "alertDialog",
  "application",
  "article",
  "audio",
  "banner",
  "blockquote",
  "button",
  "canvas",
  "caption",
  "caret",
  "cell",
  "checkBox",
  "client",
  "code",
  "colorWell",
  "column",
  "columnHeader",
  "comboBoxGrouping",
  "comboBoxMenuButton",
  "comboBoxSelect",
  "comment",
  "complementary",
  "contentDeletion",
  "contentInsertion",
  "contentInfo",
  "date",
  "dateTime",
  "definition",
  "descriptionList",
  "descriptionListDetailDeprecated",
  "descriptionListTermDeprecated",
  "desktop",
  "details",
  "dialog",
  "directoryDeprecated",
  "disclosureTriangle",
  "disclosureTriangleGrouped",
  // --------------------------------------------------------------
  // DPub Roles:
  // https://www.w3.org/TR/dpub-aam-1.0/#mapping_role_table
  "docAbstract",
  "docAcknowledgments",
  "docAfterword",
  "docAppendix",
  "docBackLink",
  "docBiblioEntry",
  "docBibliography",
  "docBiblioRef",
  "docChapter",
  "docColophon",
  "docConclusion",
  "docCover",
  "docCredit",
  "docCredits",
  "docDedication",
  "docEndnote",
  "docEndnotes",
  "docEpigraph",
  "docEpilogue",
  "docErrata",
  "docExample",
  "docFootnote",
  "docForeword",
  "docGlossary",
  "docGlossRef",
  "docIndex",
  "docIntroduction",
  "docNoteRef",
  "docNotice",
  "docPageBreak",
  "docPageFooter",
  "docPageHeader",
  "docPageList",
  "docPart",
  "docPreface",
  "docPrologue",
  "docPullquote",
  "docQna",
  "docSubtitle",
  "docTip",
  "docToc",
  // End DPub roles.
  // --------------------------------------------------------------
  "document",
  "embeddedObject",
  "emphasis",
  "feed",
  "figcaption",
  "figure",
  "footer",
  "form",
  "genericContainer",
  // --------------------------------------------------------------
  // ARIA Graphics module roles:
  // https://rawgit.com/w3c/graphics-aam/master/#mapping_role_table
  "graphicsDocument",
  "graphicsObject",
  "graphicsSymbol",
  // End ARIA Graphics module roles.
  // --------------------------------------------------------------
  "grid",
  "gridCell",
  "group",
  "header",
  "heading",
  "iframe",
  "iframePresentational",
  "image",
  "imeCandidate",
  "inlineTextBox",
  "inputTime",
  "keyboard",
  "labelText",
  "layoutTable",
  "layoutTableCell",
  "layoutTableRow",
  "legend",
  "lineBreak",
  "link",
  "list",
  "listBox",
  "listBoxOption",
  // Native
  "listGrid",
  "listItem",
  "listMarker",
  "log",
  "main",
  "mark",
  "marquee",
  "math",
  "mathMLFraction",
  "mathMLIdentifier",
  "mathMLMath",
  "mathMLMultiscripts",
  "mathMLNoneScript",
  "mathMLNumber",
  "mathMLOperator",
  "mathMLOver",
  "mathMLPrescriptDelimiter",
  "mathMLRoot",
  "mathMLRow",
  "mathMLSquareRoot",
  "mathMLStringLiteral",
  "mathMLSub",
  "mathMLSubSup",
  "mathMLSup",
  "mathMLTable",
  "mathMLTableCell",
  "mathMLTableRow",
  "mathMLText",
  "mathMLUnder",
  "mathMLUnderOver",
  "menu",
  "menuBar",
  "menuItem",
  "menuItemCheckBox",
  "menuItemRadio",
  "menuItemSeparator",
  "menuListOption",
  "menuListPopup",
  "meter",
  "navigation",
  "note",
  "pane",
  "paragraph",
  "pdfActionableHighlight",
  "pdfRoot",
  "pluginObject",
  "popUpButton",
  "portalDeprecated",
  "preDeprecated",
  "progressIndicator",
  "radioButton",
  "radioGroup",
  "region",
  "rootWebArea",
  "row",
  "rowGroup",
  "rowHeader",
  "ruby",
  "rubyAnnotation",
  "scrollBar",
  "scrollView",
  "search",
  "searchBox",
  "section",
  "sectionFooter",
  "sectionHeader",
  "sectionWithoutName",
  "slider",
  "spinButton",
  "splitter",
  "staticText",
  "status",
  "strong",
  "subscript",
  "suggestion",
  "superscript",
  "svgRoot",
  "switch",
  "tab",
  "tabList",
  "tabPanel",
  "table",
  "tableHeaderContainer",
  "term",
  "textField",
  "textFieldWithComboBox",
  "time",
  "timer",
  "titleBar",
  "toggleButton",
  "toolbar",
  "tooltip",
  "tree",
  "treeGrid",
  "treeItem",
  "unknown",
  "video",
  "webView",
  "window"
};

// Describes characteristics of an $(ref:automation.AutomationNode).
enum StateType {
  "autofillAvailable",
  "collapsed",
  "default",
  "editable",
  "expanded",
  "focusable",
  "focused",
  "horizontal",
  "hovered",
  "ignored",
  "invisible",
  "linked",
  "multiline",
  "multiselectable",
  "offscreen",
  "protected",
  "required",
  "richlyEditable",
  "vertical",
  "visited",
  "hasActions",
  "hasInterestFor"
};

// All possible actions that can be performed on automation nodes.
enum ActionType {
  "annotatePageImages",
  "blur",
  "clearAccessibilityFocus",
  "collapse",
  "customAction",
  "decrement",
  "doDefault",
  "expand",
  "focus",
  "getImageData",
  "getTextLocation",
  "hideTooltip",
  "hitTest",
  "increment",
  "internalInvalidateTree",
  "loadInlineTextBoxes",
  "longClick",
  "replaceRanges",
  "replaceSelectedText",
  "requestLayoutBasedAction",
  "resumeMedia",
  "scrollBackward",
  "scrollDown",
  "scrollForward",
  "scrollLeft",
  "scrollRight",
  "scrollUp",
  "scrollToMakeVisible",
  "scrollToPoint",
  "scrollToPositionAtRowColumn",
  "setAccessibilityFocus",
  "setScrollOffset",
  "setSelection",
  "setSequentialFocusNavigationStartingPoint",
  "setValue",
  "showContextMenu",
  "signalEndOfTest",
  "showTooltip",
  "stitchChildTree",
  "startDuckingMedia",
  "stopDuckingMedia",
  "suspendMedia"
};

// Possible changes to the automation tree. For any given atomic change
// to the tree, each node that's added, removed, or changed, will appear
// in exactly one TreeChange, with one of these types.
//
// nodeCreated means that this node was added to the tree and its parent is
// new as well, so it's just one node in a new subtree that was added.
enum TreeChangeType {
  // This node was added to the tree and its parent is new as well, so it's just
  // one node in a new subtree that was added.
  "nodeCreated",

  // This node was added to the tree but its parent was already in the tree, so
  // it's possibly the root of a new subtree - it does not mean that it
  // necessarily has children.
  "subtreeCreated",

  // This node changed.
  "nodeChanged",

  // This node's text (name) changed.
  "textChanged",

  // This node was removed.
  "nodeRemoved",

  // This subtree has finished an update.
  "subtreeUpdateEnd"
};

// Where the node's name is from.
enum NameFromType {
  "attribute",
  "attributeExplicitlyEmpty",
  "caption",
  "contents",
  "cssAltText",
  "interestFor",
  "placeholder",
  "popoverTarget",
  "prohibited",
  "prohibitedAndRedundant",
  "relatedElement",
  "title",
  "value"
};

enum DescriptionFromType {
  "ariaDescription",
  "attributeExplicitlyEmpty",
  "buttonLabel",
  "interestFor",
  "popoverTarget",
  "prohibitedNameRepair",
  "relatedElement",
  "rubyAnnotation",
  "summary",
  "svgDescElement",
  "tableCaption",
  "title"
};

// The input restriction for a object -- even non-controls can be disabled.
enum Restriction {
  "disabled",
  "readOnly"
};

// Availability and types for an interactive popup element.
enum HasPopup {
  "false",
  "true",
  "menu",
  "listbox",
  "tree",
  "grid",
  "dialog"
};

// Indicates the ARIA-current state.
enum AriaCurrentState {
  "false",
  "true",
  "page",
  "step",
  "location",
  "date",
  "time"
};

// Lists the values that `invalidState` can take on.
enum InvalidState {
  "false",
  "true"
};

// Describes possible actions when performing a do default action.
enum DefaultActionVerb {
  "activate",
  "check",
  "click",
  "clickAncestor",
  "clickInHitTest",
  "clickNotInHitTest",
  "jump",
  "open",
  "press",
  "select",
  "uncheck"
};

// Types of markers on text. See <code>AutomationNode.markerTypes</code>.
enum MarkerType {
  "spelling",
  "grammar",
  "textMatch",
  "activeSuggestion",
  "suggestion",
  "highlight"
};

// The following three enums are associated with an
// $(ref:automation.AutomationIntent).

// A command associated with an $(ref:automation.AutomationIntent).
enum IntentCommandType {
  "clearSelection",
  "delete",
  "dictate",
  "extendSelection",
  "format",
  "history",
  "insert",
  "marker",
  "moveSelection",
  "setSelection",
  "spinButtonIncrement",
  "spinButtonDecrement"
};

// The type of an input event associated with an
// $(ref:automation.AutomationIntent). It describes an edit command, e.g.
// IntentCommandType.insert, in more detail.
enum IntentInputEventType {
  // Insertion.
  "insertText",
  "insertLineBreak",
  "insertParagraph",
  "insertOrderedList",
  "insertUnorderedList",
  "insertHorizontalRule",
  "insertFromPaste",
  "insertFromDrop",
  "insertFromYank",
  "insertTranspose",
  "insertReplacementText",
  "insertCompositionText",
  "insertLink",
  // Deletion.
  "deleteWordBackward",
  "deleteWordForward",
  "deleteSoftLineBackward",
  "deleteSoftLineForward",
  "deleteHardLineBackward",
  "deleteHardLineForward",
  "deleteContentBackward",
  "deleteContentForward",
  "deleteByCut",
  "deleteByDrag",
  // History.
  "historyUndo",
  "historyRedo",
  // Formatting.
  "formatBold",
  "formatItalic",
  "formatUnderline",
  "formatStrikeThrough",
  "formatSuperscript",
  "formatSubscript",
  "formatJustifyCenter",
  "formatJustifyFull",
  "formatJustifyRight",
  "formatJustifyLeft",
  "formatIndent",
  "formatOutdent",
  "formatRemove",
  "formatSetBlockTextDirection"
};

// A text boundary associated with an $(ref:automation.AutomationIntent).
enum IntentTextBoundaryType {
  "character",
  "formatEnd",
  "formatStart",
  "formatStartOrEnd",
  "lineEnd",
  "lineStart",
  "lineStartOrEnd",
  "object",
  "pageEnd",
  "pageStart",
  "pageStartOrEnd",
  "paragraphEnd",
  "paragraphStart",
  "paragraphStartSkippingEmptyParagraphs",
  "paragraphStartOrEnd",
  "sentenceEnd",
  "sentenceStart",
  "sentenceStartOrEnd",
  "webPage",
  "wordEnd",
  "wordStart",
  "wordStartOrEnd"
};

// A move direction associated with an $(ref:automation.AutomationIntent).
enum IntentMoveDirectionType {
  "backward",
  "forward"
};

// A sort applied to a table row or column header.
enum SortDirectionType {
  "unsorted",
  "ascending",
  "descending",
  "other"
};

// A type of AutomationPosition.
enum PositionType {
  "null",
  "text",
  "tree"
};

dictionary Rect {
  required long left;
  required long top;
  required long width;
  required long height;
};

// Arguments for the find() and findAll() methods.
[nocompile] dictionary FindParams {
  RoleType role;

  // A map of $(ref:automation.StateType) to boolean, indicating for each
  // state whether it should be set or not. For example:
  // <code>{ StateType.disabled: false }</code> would only match if
  // <code>StateType.disabled</code> was <em>not</em> present in the node's
  // <code>state</code> object.
  object state;

  // A map of attribute name to expected value, for example
  // <code>{ name: 'Root directory', checkbox_mixed: true }</code>.
  // String attribute values may be specified as a regex, for example
  // <code>{ name: /stralia$/</code> }</code>.
  // Unless specifying a regex, the expected value must be an exact match
  // in type and value for the actual value. Thus, the type of expected value
  // must be one of:
  // <ul>
  // <li>string</li>
  // <li>integer</li>
  // <li>float</li>
  // <li>boolean</li>
  // </ul>
  object attributes;
};

// Arguments for the setDocumentSelection() function.
[nocompile] dictionary SetDocumentSelectionParams {
  // The node where the selection begins.
  [instanceOf=AutomationNode] required object anchorObject;

  // The offset in the anchor node where the selection begins.
  required long anchorOffset;

  // The node where the selection ends.
  [instanceOf=AutomationNode] required object focusObject;

  // The offset within the focus node where the selection ends.
  required long focusOffset;
};

[nocompile] dictionary AutomationIntent {
  // A command associated with this AutomationIntent.
  required IntentCommandType command;

  // A text boundary associated with this AutomationIntent.
  required IntentTextBoundaryType textBoundary;

  // A move direction associated with this AutomationIntent.
  IntentMoveDirectionType moveDirection;
};

callback StopPropagationCallback = undefined();

// An event in the Automation tree.
[nocompile] dictionary AutomationEvent {
  // The $(ref:automation.AutomationNode) to which the event was targeted.
  required AutomationNode target;

  // The type of the event.
  required EventType type;

  // The source of this event.
  required DOMString eventFrom;

  // Any mouse coordinates associated with this event.
  required long mouseX;
  required long mouseY;

  // A list of $(ref:automation.AutomationIntent)s associated with this event.
  required sequence<AutomationIntent> intents;

  // Stops this event from further processing except for any remaining
  // listeners on $(ref:automation.AutomationEvent.target).
  required StopPropagationCallback stopPropagation;
};

// A listener for events on an <code>AutomationNode</code>.
callback AutomationListener = undefined(AutomationEvent event);

// A change to the Automation tree.
[nocompile] dictionary TreeChange {
  // The $(ref:automation.AutomationNode) that changed.
  required AutomationNode target;

  // The type of change.
  required TreeChangeType type;
};

// Possible tree changes to listen to using addTreeChangeObserver.
// Note that listening to all tree changes can be expensive.
enum TreeChangeObserverFilter {
  "noTreeChanges",
  "liveRegionTreeChanges",
  "textMarkerChanges",
  "allTreeChanges"
};

// A listener for changes on the <code>AutomationNode</code> tree.
callback TreeChangeObserver = undefined(TreeChange treeChange);

// Callback called for actions with a response.
callback PerformActionCallback = undefined(boolean result);
callback PerformActionCallbackWithNode = undefined(AutomationNode node);
callback BoundsForRangeCallback = undefined(Rect bounds);

[nocompile] dictionary CustomAction {
  required long id;
  required DOMString description;
};

// A marker associated with an AutomationNode.
[nocompile] dictionary Marker {
  // The start offset within the text of the associated node.
  required long startOffset;

  // The end offset within the text of the associated node.
  required long endOffset;

  // A mapping of MarkerType to true or undefined indicating the marker types
  // for this marker.
  required object flags;
};

callback AutomationPositionIsNullPositionCallback = boolean();
callback AutomationPositionIsTreePositionCallback = boolean();
callback AutomationPositionIsTextPositionCallback = boolean();
callback AutomationPositionIsLeafTextPositionCallback = boolean();
callback AutomationPositionAtStartOfAnchorCallback = boolean();
callback AutomationPositionAtEndOfAnchorCallback = boolean();
callback AutomationPositionAtStartOfWordCallback = boolean();
callback AutomationPositionAtEndOfWordCallback = boolean();
callback AutomationPositionAtStartOfLineCallback = boolean();
callback AutomationPositionAtEndOfLineCallback = boolean();
callback AutomationPositionAtStartOfParagraphCallback = boolean();
callback AutomationPositionAtEndOfParagraphCallback = boolean();
callback AutomationPositionAtStartOfPageCallback = boolean();
callback AutomationPositionAtEndOfPageCallback = boolean();
callback AutomationPositionAtStartOfFormatCallback = boolean();
callback AutomationPositionAtEndOfFormatCallback = boolean();
callback AutomationPositionAtStartOfDocumentCallback = boolean();
callback AutomationPositionAtEndOfDocumentCallback = boolean();
callback AutomationPositionAsTreePositionCallback = undefined();
callback AutomationPositionAsTextPositionCallback = undefined();
callback AutomationPositionAsLeafTextPositionCallback = undefined();
callback AutomationPositionMoveToPositionAtStartOfAnchorCallback = undefined();
callback AutomationPositionMoveToPositionAtEndOfAnchorCallback = undefined();
callback AutomationPositionMoveToPositionAtStartOfDocumentCallback =
    undefined();
callback AutomationPositionMoveToPositionAtEndOfDocumentCallback = undefined();
callback AutomationPositionMoveToParentPositionCallback = undefined();
callback AutomationPositionMoveToNextLeafTreePositionCallback = undefined();
callback AutomationPositionMoveToPreviousLeafTreePositionCallback = undefined();
callback AutomationPositionMoveToNextLeafTextPositionCallback = undefined();
callback AutomationPositionMoveToPreviousLeafTextPositionCallback = undefined();
callback AutomationPositionMoveToNextCharacterPositionCallback = undefined();
callback AutomationPositionMoveToPreviousCharacterPositionCallback =
    undefined();
callback AutomationPositionMoveToNextWordStartPositionCallback = undefined();
callback AutomationPositionMoveToPreviousWordStartPositionCallback =
    undefined();
callback AutomationPositionMoveToNextWordEndPositionCallback = undefined();
callback AutomationPositionMoveToPreviousWordEndPositionCallback = undefined();
callback AutomationPositionMoveToNextLineStartPositionCallback = undefined();
callback AutomationPositionMoveToPreviousLineStartPositionCallback =
    undefined();
callback AutomationPositionMoveToNextLineEndPositionCallback = undefined();
callback AutomationPositionMoveToPreviousLineEndPositionCallback = undefined();
callback AutomationPositionMoveToNextFormatStartPositionCallback = undefined();
callback AutomationPositionMoveToPreviousFormatStartPositionCallback =
    undefined();
callback AutomationPositionMoveToNextFormatEndPositionCallback = undefined();
callback AutomationPositionMoveToPreviousFormatEndPositionCallback =
    undefined();
callback AutomationPositionMoveToNextParagraphStartPositionCallback =
    undefined();
callback AutomationPositionMoveToPreviousParagraphStartPositionCallback =
    undefined();
callback AutomationPositionMoveToNextParagraphEndPositionCallback = undefined();
callback AutomationPositionMoveToPreviousParagraphEndPositionCallback =
    undefined();
callback AutomationPositionMoveToNextPageStartPositionCallback = undefined();
callback AutomationPositionMoveToPreviousPageStartPositionCallback =
    undefined();
callback AutomationPositionMoveToNextPageEndPositionCallback = undefined();
callback AutomationPositionMoveToPreviousPageEndPositionCallback = undefined();
callback AutomationPositionMoveToNextAnchorPositionCallback = undefined();
callback AutomationPositionMoveToPreviousAnchorPositionCallback = undefined();
callback AutomationPositionMaxTextOffsetCallback = long();
callback AutomationPositionIsInLineBreakCallback = boolean();
callback AutomationPositionIsInTextObjectCallback = boolean();
callback AutomationPositionIsInWhiteSpaceCallback = boolean();
callback AutomationPositionIsValidCallback = boolean();
callback AutomationPositionGetTextCallback = DOMString();

// A position in the automation tree.
// See ui/accessibility/ax_position.h for documentation. All members need to be
// kept in sync with extensions/renderer/api/automation/automation_position.h.
// Some members there are kept private and not represented here.
[nocompile] dictionary AutomationPosition {
  AutomationNode node;
  required long childIndex;
  required long textOffset;
  required DOMString affinity;

  required AutomationPositionIsNullPositionCallback isNullPosition;
  required AutomationPositionIsTreePositionCallback isTreePosition;
  required AutomationPositionIsTextPositionCallback isTextPosition;
  required AutomationPositionIsLeafTextPositionCallback isLeafTextPosition;
  required AutomationPositionAtStartOfAnchorCallback atStartOfAnchor;
  required AutomationPositionAtEndOfAnchorCallback atEndOfAnchor;
  required AutomationPositionAtStartOfWordCallback atStartOfWord;
  required AutomationPositionAtEndOfWordCallback atEndOfWord;
  required AutomationPositionAtStartOfLineCallback atStartOfLine;
  required AutomationPositionAtEndOfLineCallback atEndOfLine;
  required AutomationPositionAtStartOfParagraphCallback atStartOfParagraph;
  required AutomationPositionAtEndOfParagraphCallback atEndOfParagraph;
  required AutomationPositionAtStartOfPageCallback atStartOfPage;
  required AutomationPositionAtEndOfPageCallback atEndOfPage;
  required AutomationPositionAtStartOfFormatCallback atStartOfFormat;
  required AutomationPositionAtEndOfFormatCallback atEndOfFormat;
  required AutomationPositionAtStartOfDocumentCallback atStartOfDocument;
  required AutomationPositionAtEndOfDocumentCallback atEndOfDocument;
  required AutomationPositionAsTreePositionCallback asTreePosition;
  required AutomationPositionAsTextPositionCallback asTextPosition;
  required AutomationPositionAsLeafTextPositionCallback asLeafTextPosition;
  required AutomationPositionMoveToPositionAtStartOfAnchorCallback
      moveToPositionAtStartOfAnchor;
  required AutomationPositionMoveToPositionAtEndOfAnchorCallback
      moveToPositionAtEndOfAnchor;
  required AutomationPositionMoveToPositionAtStartOfDocumentCallback
      moveToPositionAtStartOfDocument;
  required AutomationPositionMoveToPositionAtEndOfDocumentCallback
      moveToPositionAtEndOfDocument;
  required AutomationPositionMoveToParentPositionCallback moveToParentPosition;
  required AutomationPositionMoveToNextLeafTreePositionCallback
      moveToNextLeafTreePosition;
  required AutomationPositionMoveToPreviousLeafTreePositionCallback
      moveToPreviousLeafTreePosition;
  required AutomationPositionMoveToNextLeafTextPositionCallback
      moveToNextLeafTextPosition;
  required AutomationPositionMoveToPreviousLeafTextPositionCallback
      moveToPreviousLeafTextPosition;
  required AutomationPositionMoveToNextCharacterPositionCallback
      moveToNextCharacterPosition;
  required AutomationPositionMoveToPreviousCharacterPositionCallback
      moveToPreviousCharacterPosition;
  required AutomationPositionMoveToNextWordStartPositionCallback
      moveToNextWordStartPosition;
  required AutomationPositionMoveToPreviousWordStartPositionCallback
      moveToPreviousWordStartPosition;
  required AutomationPositionMoveToNextWordEndPositionCallback
      moveToNextWordEndPosition;
  required AutomationPositionMoveToPreviousWordEndPositionCallback
      moveToPreviousWordEndPosition;
  required AutomationPositionMoveToNextLineStartPositionCallback
      moveToNextLineStartPosition;
  required AutomationPositionMoveToPreviousLineStartPositionCallback
      moveToPreviousLineStartPosition;
  required AutomationPositionMoveToNextLineEndPositionCallback
      moveToNextLineEndPosition;
  required AutomationPositionMoveToPreviousLineEndPositionCallback
      moveToPreviousLineEndPosition;
  required AutomationPositionMoveToNextFormatStartPositionCallback
      moveToNextFormatStartPosition;
  required AutomationPositionMoveToPreviousFormatStartPositionCallback
      moveToPreviousFormatStartPosition;
  required AutomationPositionMoveToNextFormatEndPositionCallback
      moveToNextFormatEndPosition;
  required AutomationPositionMoveToPreviousFormatEndPositionCallback
      moveToPreviousFormatEndPosition;
  required AutomationPositionMoveToNextParagraphStartPositionCallback
      moveToNextParagraphStartPosition;
  required AutomationPositionMoveToPreviousParagraphStartPositionCallback
      moveToPreviousParagraphStartPosition;
  required AutomationPositionMoveToNextParagraphEndPositionCallback
      moveToNextParagraphEndPosition;
  required AutomationPositionMoveToPreviousParagraphEndPositionCallback
      moveToPreviousParagraphEndPosition;
  required AutomationPositionMoveToNextPageStartPositionCallback
      moveToNextPageStartPosition;
  required AutomationPositionMoveToPreviousPageStartPositionCallback
      moveToPreviousPageStartPosition;
  required AutomationPositionMoveToNextPageEndPositionCallback
      moveToNextPageEndPosition;
  required AutomationPositionMoveToPreviousPageEndPositionCallback
      moveToPreviousPageEndPosition;
  required AutomationPositionMoveToNextAnchorPositionCallback
      moveToNextAnchorPosition;
  required AutomationPositionMoveToPreviousAnchorPositionCallback
      moveToPreviousAnchorPosition;
  required AutomationPositionMaxTextOffsetCallback maxTextOffset;
  required AutomationPositionIsInLineBreakCallback isInLineBreak;
  required AutomationPositionIsInTextObjectCallback isInTextObject;
  required AutomationPositionIsInWhiteSpaceCallback isInWhiteSpace;
  required AutomationPositionIsValidCallback isValid;
  required AutomationPositionGetTextCallback getText;
};

callback AutomationNodeBoundsForRangeCallback =
    undefined(long startIndex, long endIndex, BoundsForRangeCallback callback);
callback AutomationNodeUnclippedBoundsForRangeCallback =
    undefined(long startIndex, long endIndex, BoundsForRangeCallback callback);
callback AutomationNodeDoDefaultCallback = undefined();
callback AutomationNodeFocusCallback = undefined();
callback AutomationNodeGetImageDataCallback =
    undefined(long maxWidth, long maxHeight);
callback AutomationNodeHitTestCallback =
    undefined(long x, long y, EventType eventToFire);
callback AutomationNodeHitTestWithReplyCallback =
    undefined(long x, long y, PerformActionCallbackWithNode callback);
callback AutomationNodeMakeVisibleCallback = undefined();
callback AutomationNodePerformCustomActionCallback =
    undefined(long customActionId);
callback AutomationNodePerformStandardActionCallback =
    undefined(ActionType actionType);
callback AutomationNodeReplaceSelectedTextCallback = undefined(DOMString value);
callback AutomationNodeSetAccessibilityFocusCallback = undefined();
callback AutomationNodeSetSelectionCallback =
    undefined(long startIndex, long endIndex);
callback AutomationNodeSetSequentialFocusNavigationStartingPointCallback =
    undefined();
callback AutomationNodeSetValueCallback = undefined(DOMString value);
callback AutomationNodeShowContextMenuCallback = undefined();
callback AutomationNodeResumeMediaCallback = undefined();
callback AutomationNodeStartDuckingMediaCallback = undefined();
callback AutomationNodeStopDuckingMediaCallback = undefined();
callback AutomationNodeSuspendMediaCallback = undefined();
callback AutomationNodeLongClickCallback = undefined();
callback AutomationNodeScrollBackwardCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollForwardCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollUpCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollDownCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollLeftCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollRightCallback =
    undefined(optional PerformActionCallback callback);
callback AutomationNodeScrollToPointCallback = undefined(long x, long y);
callback AutomationNodeSetScrollOffsetCallback = undefined(long x, long y);
callback AutomationNodeAddEventListenerCallback =
    undefined(EventType eventType,
              AutomationListener listener,
              boolean capture);
callback AutomationNodeRemoveEventListenerCallback =
    undefined(EventType eventType,
              AutomationListener listener,
              boolean capture);
callback AutomationNodeFindCallback = AutomationNode?(FindParams params);
callback AutomationNodeFindAllCallback = sequence<AutomationNode>(
    FindParams params);
callback AutomationNodeMatchesCallback = boolean(FindParams params);
callback AutomationNodeGetNextTextMatchCallback =
    AutomationNode?(DOMString searchStr, boolean backward);
callback AutomationNodeCreatePositionCallback = AutomationPosition?(
    PositionType type, long offset, optional boolean isUpstream);

// A single node in an Automation tree.
[nocompile] dictionary AutomationNode {
  // The root node of the tree containing this AutomationNode.
  AutomationNode root;

  // Whether this AutomationNode is a root node.
  required boolean isRootNode;

  // The role of this node.
  RoleType role;

  // The $(ref:automation.StateType)s describing this node.
  // <jsexterns>@type {Object<chrome.automation.StateType, boolean>}
  // </jsexterns>
  object state;

  // The rendered location (as a bounding box) of this node in global
  // screen coordinates.
  required Rect location;

  // Determines the location of the text within the node specified by
  // |startIndex| and |endIndex|, inclusively. Invokes |callback| with the
  // bounding rectangle, in screen coordinates. |callback| can be invoked
  // either synchronously or asynchronously. The bounds are clipped to
  // ancestors.
  required AutomationNodeBoundsForRangeCallback boundsForRange;

  // Determines the location of the text within the node specified by
  // |startIndex| and |endIndex|, inclusively. Invokes |callback| with the
  // bounding rectangle, in screen coordinates. |callback| can be invoked
  // either synchronously or asynchronously. The bounds are not clipped to
  // ancestors.
  required AutomationNodeUnclippedBoundsForRangeCallback unclippedBoundsForRange;

  // The location (as a bounding box) of this node in global screen
  // coordinates without applying any clipping from ancestors.
  Rect unclippedLocation;

  // The purpose of the node, other than the role, if any.
  DOMString description;

  // Description of the state of the checkbox.
  // Used only when the node is checkable.
  DOMString checkedStateDescription;

  // The placeholder for this text field, if any.
  DOMString placeholder;

  // The role description for this node.
  DOMString roleDescription;

  // The accessible name for this node, via the
  // <a href="http://www.w3.org/TR/wai-aria/#namecalculation">
  // Accessible Name Calculation</a> process.
  DOMString name;

  // Explains what will happen when the doDefault action is performed.
  DOMString doDefaultLabel;

  // Explains what will happen when the long click action is performed.
  DOMString longClickLabel;

  // The tooltip of the node, if any.
  DOMString tooltip;

  // The source of the name.
  NameFromType nameFrom;

  // The image annotation for image nodes, which may be a human-readable
  // string that is the contextualized annotation or a status string related
  // to annotations.
  DOMString imageAnnotation;

  // The value for this node: for example the <code>value</code> attribute of
  // an <code>&lt;input&gt; element.
  DOMString value;

  // The HTML id for this element, if this node is an HTML element.
  DOMString htmlId;

  // The HTML tag for this element, if this node is an HTML element.
  DOMString htmlTag;

  // The level of a heading or tree item.
  long hierarchicalLevel;

  // The current caret bounds in screen coordinates.
  Rect caretBounds;

  // The start and end index of each word in an inline text box.
  sequence<long> wordStarts;
  sequence<long> wordEnds;

  // The start indexes of each sentence within the node's name.
  sequence<long> sentenceStarts;

  // The end indexes of each sentence within the node's name. For most nodes,
  // the size of sentenceStarts array should be equal to the size of
  // sentenceEnds array. Two exceptions are (1) node at the begining of a
  // paragraph but the end of the node's sentences is in its following node.
  // Such a node has one more start index. (2) Node at the end of a paragraph
  // but the start of the node's sentences is in its previous node. Such a
  // node has one more end index. For example, <p><b>Hello</b> world.</p> has
  // two nodes. The first one has one start index (i.e., 0) but no end index.
  // The second node has one end index (i.e., 7) but no start index.
  sequence<long> sentenceEnds;

  // The start index of each word within the node's name. This is different
  // from wordStarts because it is not restricted to inline text boxes and can
  // be used for any type of element.
  sequence<long> nonInlineTextWordStarts;

  // The end index of each word within the node's name. This is different
  // from wordEnds because it is not restricted to inline text boxes and can
  // be used for any type of element.
  sequence<long> nonInlineTextWordEnds;

  // The nodes, if any, which this node is specified to control via
  // <a href="http://www.w3.org/TR/wai-aria/#aria-controls">
  // <code>aria-controls</code></a>.
  sequence<AutomationNode> controls;

  // The nodes, if any, which form a description for this node.
  sequence<AutomationNode> describedBy;

  // The nodes, if any, which may optionally be navigated to after this
  // one. See
  // <a href="http://www.w3.org/TR/wai-aria/#aria-flowto">
  // <code>aria-flowto</code></a>.
  sequence<AutomationNode> flowTo;

  // The nodes, if any, which form a label for this element. Generally, the
  // text from these elements will also be exposed as the element's accessible
  // name, via the $(ref:automation.AutomationNode.name) attribute.
  sequence<AutomationNode> labelledBy;

  // The node referred to by <code>aria-activedescendant</code>, where
  // applicable
  AutomationNode activeDescendant;

  // Reverse relationship for active descendant.
  sequence<AutomationNode> activeDescendantFor;

  // The target of an in-page link.
  AutomationNode inPageLinkTarget;

  // A node that provides more details about the current node.
  sequence<AutomationNode> details;

  // The nodes, if any, that provide an error message for the current node.
  sequence<AutomationNode> errorMessages;

  // Reverse relationship for details.
  sequence<AutomationNode> detailsFor;

  // Reverse relationship for errorMessage.
  sequence<AutomationNode> errorMessageFor;

  // Reverse relationship for controls.
  sequence<AutomationNode> controlledBy;

  // Reverse relationship for describedBy.
  sequence<AutomationNode> descriptionFor;

  // Reverse relationship for flowTo.
  sequence<AutomationNode> flowFrom;

  // Reverse relationship for labelledBy.
  sequence<AutomationNode> labelFor;

  // The column header nodes for a table cell.
  sequence<AutomationNode> tableCellColumnHeaders;

  // The row header nodes for a table cell.
  sequence<AutomationNode> tableCellRowHeaders;

  // An array of standard actions available on this node.
  sequence<ActionType> standardActions;

  // An array of custom actions.
  sequence<CustomAction> customActions;

  // The action taken by calling <code>doDefault</code>.
  DefaultActionVerb defaultActionVerb;

  //
  // Link attributes.
  //

  // The URL that this link will navigate to.
  DOMString url;

  //
  // Document attributes.
  //

  // The URL of this document.
  DOMString docUrl;

  // The title of this document.
  DOMString docTitle;

  // Whether this document has finished loading.
  boolean docLoaded;

  // The proportion (out of 1.0) that this doc has completed loading.
  double docLoadingProgress;

  //
  // Scrollable container attributes.
  //

  long scrollX;
  long scrollXMin;
  long scrollXMax;
  long scrollY;
  long scrollYMin;
  long scrollYMax;

  // Indicates whether this node is scrollable.
  boolean scrollable;

  //
  // Editable text field attributes.
  //

  // The character index of the start of the selection within this editable
  // text element; -1 if no selection.
  long textSelStart;

  // The character index of the end of the selection within this editable
  // text element; -1 if no selection.
  long textSelEnd;

  // An array of Marker objects for this node.
  sequence<Marker> markers;

  //
  // Tree selection attributes (available on root nodes only)
  //

  // If a selection is present, whether the anchor of the selection comes
  // after its focus in the accessibility tree.
  boolean isSelectionBackward;
  // The anchor node of the tree selection, if any.
  AutomationNode anchorObject;
  // The anchor offset of the tree selection, if any.
  long anchorOffset;
  // The affinity of the tree selection anchor, if any.
  DOMString anchorAffinity;
  // The focus node of the tree selection, if any.
  AutomationNode focusObject;
  // The focus offset of the tree selection, if any.
  long focusOffset;
  // The affinity of the tree selection focus, if any.
  DOMString focusAffinity;

  // The selection start node of the tree selection, if any.
  AutomationNode selectionStartObject;
  // The selection start offset of the tree selection, if any.
  long selectionStartOffset;
  // The affinity of the tree selection start, if any.
  DOMString selectionStartAffinity;
  // The selection end node of the tree selection, if any.
  AutomationNode selectionEndObject;
  // The selection end offset of the tree selection, if any.
  long selectionEndOffset;
  // The affinity of the tree selection end, if any.
  DOMString selectionEndAffinity;

  // Indicates that the node is marked user-select:none
  boolean notUserSelectableStyle;

  //
  // Range attributes.
  //

  // The current value for this range.
  double valueForRange;

  // The minimum possible value for this range.
  double minValueForRange;

  // The maximum possible value for this range.
  double maxValueForRange;

  //
  // List attributes.
  //

  // The 1-based index of an item in a set.
  long posInSet;

  // The number of items in a set;
  long setSize;

  //
  // Table attributes.
  //

  // The number of rows in this table as specified in the DOM.
  long tableRowCount;

  // The number of rows in this table as specified by the page author.
  long ariaRowCount;

  // The number of columns in this table as specified in the DOM.
  long tableColumnCount;

  // The number of columns in this table as specified by the page author.
  long ariaColumnCount;

  //
  // Table cell attributes.
  //

  // The zero-based index of the column that this cell is in as specified in
  // the DOM.
  long tableCellColumnIndex;

  // The ARIA column index as specified by the page author.
  long tableCellAriaColumnIndex;

  // The number of columns that this cell spans (default is 1).
  long tableCellColumnSpan;

  // The zero-based index of the row that this cell is in as specified in the
  // DOM.
  long tableCellRowIndex;

  // The ARIA row index as specified by the page author.
  long tableCellAriaRowIndex;

  // The number of rows that this cell spans (default is 1).
  long tableCellRowSpan;

  // The corresponding column header for this cell.
  AutomationNode tableColumnHeader;

  // The corresponding row header for this cell.
  AutomationNode tableRowHeader;

  // The column index of this column node.
  long tableColumnIndex;

  // The row index of this row node.
  long tableRowIndex;

  //
  // Live region attributes.
  //

  // The type of region if this is the root of a live region.
  // Possible values are 'polite' and 'assertive'.
  DOMString liveStatus;

  // The value of aria-relevant for a live region.
  DOMString liveRelevant;

  // The value of aria-atomic for a live region.
  boolean liveAtomic;

  // The value of aria-busy for a live region or any other element.
  boolean busy;

  // The type of live region if this node is inside a live region.
  DOMString containerLiveStatus;

  // The value of aria-relevant if this node is inside a live region.
  DOMString containerLiveRelevant;

  // The value of aria-atomic if this node is inside a live region.
  boolean containerLiveAtomic;

  // The value of aria-busy if this node is inside a live region.
  boolean containerLiveBusy;

  //
  // Miscellaneous attributes.
  //

  // Whether or not this node is a button.
  required boolean isButton;

  // Whether or not this node is a checkbox.
  required boolean isCheckBox;

  // Whether or not this node is a combobox.
  required boolean isComboBox;

  // Whether or not this node is an image.
  required boolean isImage;

  // Whether the node contains hidden nodes.
  required boolean hasHiddenOffscreenNodes;

  // Aria auto complete.
  DOMString autoComplete;

  // The name of the programmatic backing object.
  DOMString className;

  // Marks this subtree as modal.
  boolean modal;

  // The input type of a text field, such as "text" or "email".
  DOMString inputType;

  // The key that activates this widget.
  DOMString accessKey;

  // The value of the aria-invalid attribute, indicating the error type.
  DOMString ariaInvalidValue;

  // The CSS display attribute for this node, if applicable.
  DOMString display;

  // A data url with the contents of this object's image or thumbnail.
  DOMString imageDataUrl;

  // The author-provided language code for this subtree.
  DOMString language;

  // The detected language code for this subtree.
  DOMString detectedLanguage;

  // Indicates the availability and type of an interactive popup element.
  HasPopup hasPopup;

  // Input restriction, if any, such as readonly or disabled:
  // undefined - enabled control or other object that is not disabled
  // Restriction.DISABLED - disallows input in itself + any descendants
  // Restriction.READONLY - allow focus/selection but not input
  DOMString restriction;

  // Tri-state describing checkbox or radio button:
  // 'false' | 'true' | 'mixed'
  DOMString checked;

  // The inner html of this element. Only populated for math content.
  DOMString innerHtml;

  // The RGBA foreground color of this subtree, as an integer.
  long color;

  // The RGBA background color of this subtree, as an integer.
  long backgroundColor;

  // The RGBA color of an input element whose value is a color.
  long colorValue;

  // Indicates node text is subscript.
  required boolean subscript;

  // Indicates node text is superscript.
  required boolean superscript;

  // Indicates node text is bold.
  required boolean bold;

  // Indicates node text is italic.
  required boolean italic;

  // Indicates node text is underline.
  required boolean underline;

  // Indicates node text is line through.
  required boolean lineThrough;

  // Indicates whether this node is selected, unselected, or neither.
  boolean selected;

  // Indicates the font size of this node.
  long fontSize;

  // Indicates the font family.
  required DOMString fontFamily;

  // Indicates whether the object functions as a text field which exposes its
  // descendants. Use cases include the root of a content-editable region, an
  // ARIA textbox which isn't currently editable and which has interactive
  // descendants, and a <body> element that has "design-mode" set to "on".
  required boolean nonAtomicTextFieldRoot;

  // Indicates aria-current state.
  AriaCurrentState ariaCurrentState;

  // Indicates invalid-state.
  InvalidState invalidState;

  // The application id for a tree rooted at this node.
  DOMString appId;

  //
  // Walking the tree.
  //

  required sequence<AutomationNode> children;
  AutomationNode parent;
  AutomationNode firstChild;
  AutomationNode lastChild;
  AutomationNode previousSibling;
  AutomationNode nextSibling;
  AutomationNode previousOnLine;
  AutomationNode nextOnLine;
  AutomationNode previousFocus;
  AutomationNode nextFocus;
  AutomationNode previousWindowFocus;
  AutomationNode nextWindowFocus;

  // The index of this node in its parent node's list of children. If this is
  // the root node, this will be undefined.
  long indexInParent;

  // The sort direction of this node.
  required SortDirectionType sortDirection;

  // Explicitly set to true when this node is clickable.
  required boolean clickable;

  //
  // Actions.
  //

  // Does the default action based on this node's role. This is generally
  // the same action that would result from clicking the node such as
  // expanding a treeitem, toggling a checkbox, selecting a radiobutton,
  // or activating a button.
  required AutomationNodeDoDefaultCallback doDefault;

  // Places focus on this node.
  required AutomationNodeFocusCallback focus;

  // Request a data url for the contents of an image, optionally
  // resized.  Pass zero for maxWidth and/or maxHeight for the
  // original size.
  required AutomationNodeGetImageDataCallback getImageData;

  // Does a hit test of the given global screen coordinates, and fires
  // eventToFire on the resulting object.
  required AutomationNodeHitTestCallback hitTest;

  // Does a $(ref:automation.AutomationNode.hitTest), and receives a callback
  // with the resulting hit node.
  required AutomationNodeHitTestWithReplyCallback hitTestWithReply;

  // Scrolls this node to make it visible.
  required AutomationNodeMakeVisibleCallback makeVisible;

  // Performs custom action.
  required AutomationNodePerformCustomActionCallback performCustomAction;

  // Convenience method to perform a standard action supported by this node.
  // For actions requiring additional arguments, call the specific binding
  // e.g. <code>setSelection</code>.
  required AutomationNodePerformStandardActionCallback performStandardAction;

  // Replaces the selected text within a text field.
  required AutomationNodeReplaceSelectedTextCallback replaceSelectedText;

  // Sets accessibility focus. Accessibility focus is the node on which an
  // extension tracks a user's focus. This may be conveyed through a focus
  // ring or or speech output by the extension. Automation will dispatch more
  // events to the accessibility focus such as location changes.
  required AutomationNodeSetAccessibilityFocusCallback setAccessibilityFocus;

  // Sets selection within a text field.
  required AutomationNodeSetSelectionCallback setSelection;

  // Clears focus and sets this node as the starting point for the next
  // time the user presses Tab or Shift+Tab.
  required AutomationNodeSetSequentialFocusNavigationStartingPointCallback
      setSequentialFocusNavigationStartingPoint;

  // Sets the value of a text field.
  required AutomationNodeSetValueCallback setValue;

  // Show the context menu for this element, as if the user right-clicked.
  required AutomationNodeShowContextMenuCallback showContextMenu;

  // Resume playing any media within this tree.
  required AutomationNodeResumeMediaCallback resumeMedia;

  // Start ducking any media within this tree.
  required AutomationNodeStartDuckingMediaCallback startDuckingMedia;

  // Stop ducking any media within this tree.
  required AutomationNodeStopDuckingMediaCallback stopDuckingMedia;

  // Suspend any media playing within this tree.
  required AutomationNodeSuspendMediaCallback suspendMedia;

  // Simulates long click on node.
  required AutomationNodeLongClickCallback longClick;

  // Scrolls this scrollable container backward.
  required AutomationNodeScrollBackwardCallback scrollBackward;

  // Scrolls this scrollable container forward.
  required AutomationNodeScrollForwardCallback scrollForward;

  // Scrolls this scrollable container up.
  required AutomationNodeScrollUpCallback scrollUp;

  // Scrolls this scrollable container down.
  required AutomationNodeScrollDownCallback scrollDown;

  // Scrolls this scrollable container left.
  required AutomationNodeScrollLeftCallback scrollLeft;

  // Scrolls this scrollable container right.
  required AutomationNodeScrollRightCallback scrollRight;

  // Scrolls this scrollable container to the given point.
  required AutomationNodeScrollToPointCallback scrollToPoint;

  // Sets this scrollable container's scroll offset.
  required AutomationNodeSetScrollOffsetCallback setScrollOffset;

  // Adds a listener for the given event type and event phase.
  required AutomationNodeAddEventListenerCallback addEventListener;

  // Removes a listener for the given event type and event phase.
  required AutomationNodeRemoveEventListenerCallback removeEventListener;

  // Finds the first AutomationNode in this node's subtree which matches the
  // given search parameters.
  required AutomationNodeFindCallback find;

  // Finds all the AutomationNodes in this node's subtree which matches the
  // given search parameters.
  required AutomationNodeFindAllCallback findAll;

  // Returns whether this node matches the given $(ref:automation.FindParams).
  required AutomationNodeMatchesCallback matches;

  required AutomationNodeGetNextTextMatchCallback getNextTextMatch;

  // Creates a position object backed by Chrome's accessibility position support.
  required AutomationNodeCreatePositionCallback createPosition;
};

// The <code>chrome.automation</code> API allows developers to access the
// automation (accessibility) tree for the browser. The tree resembles the DOM
// tree, but only exposes the <em>semantic</em> structure of a page. It can be
// used to programmatically interact with a page by examining names, roles, and
// states, listening for events, and performing actions on nodes.
interface Automation {
  // Get the automation tree for the whole desktop which consists of all on
  // screen views. Note this API is currently only supported on Chrome OS.
  // |Returns|: Called when the <code>AutomationNode</code> for the page is
  // available.
  // |PromiseValue|: rootNode
  [nocompile, requiredCallback] static Promise<AutomationNode> getDesktop();

  // Get the automation node that currently has focus, globally. Will return
  // null if none of the nodes in any loaded trees have focus.
  // |Returns|: Called with the <code>AutomationNode</code> that currently has
  // focus.
  // |PromiseValue|: focusedNode
  [nocompile, requiredCallback] static Promise<AutomationNode> getFocus();

  // Get the automation node that currently has accessibility focus, globally.
  // Will return null if none of the nodes in any loaded trees have
  // accessibility focus.
  // |Returns|: Called with the <code>AutomationNode</code> that currently has
  // accessibility focus.
  // |PromiseValue|: focusedNode
  [nocompile, requiredCallback]
  static Promise<AutomationNode> getAccessibilityFocus();

  // Add a tree change observer. Tree change observers are static/global, they
  // listen to changes across all trees. Pass a filter to determine what
  // specific tree changes to listen to, and note that listnening to all
  // tree changes can be expensive.
  [nocompile]
  static undefined addTreeChangeObserver(
      TreeChangeObserverFilter filter, TreeChangeObserver observer);

  // Remove a tree change observer.
  [nocompile]
  static undefined removeTreeChangeObserver(TreeChangeObserver observer);

  // Sets the selection in a tree. This creates a selection in a single
  // tree (anchorObject and focusObject must have the same root).
  // Everything in the tree between the two node/offset pairs gets included
  // in the selection. The anchor is where the user started the selection,
  // while the focus is the point at which the selection gets extended
  // e.g. when dragging with a mouse or using the keyboard. For nodes with
  // the role staticText, the offset gives the character offset within
  // the value where the selection starts or ends, respectively.
  [nocompile] static undefined setDocumentSelection(
      SetDocumentSelectionParams params);
};

partial interface Browser {
  static attribute Automation automation;
};
