// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const AutomationEvent = require('automationEvent').AutomationEvent;
const automationInternal = getInternalApi('automationInternal');
const AutomationTreeCache = require('automationTreeCache').AutomationTreeCache;
const exceptionHandler = require('uncaught_exception_handler');

const natives = requireNative('automationInternal');

const IsInteractPermitted = natives.IsInteractPermitted;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The id of the root node.
 */
const GetRootID = natives.GetRootID;

/**
 * Similar to above, but may move to ancestor roots if the current tree
 * has multiple roots.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {{treeID: string, nodeID: number}}
 */
const GetPublicRoot = natives.GetPublicRoot;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The title of the document.
 */
const GetDocTitle = natives.GetDocTitle;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The url of the document.
 */
const GetDocURL = natives.GetDocURL;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?boolean} True if the document has finished loading.
 */
const GetDocLoaded = natives.GetDocLoaded;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The loading progress, from 0.0 to 1.0 (fully loaded).
 */
const GetDocLoadingProgress = natives.GetDocLoadingProgress;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {boolean} Whether the selection's anchor comes after its focus in the
 *     accessibility tree.
 */
const GetIsSelectionBackward = natives.GetIsSelectionBackward;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the selection anchor object.
 */
const GetAnchorObjectID = natives.GetAnchorObjectID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The selection anchor offset.
 */
const GetAnchorOffset = natives.GetAnchorOffset;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The selection anchor affinity.
 */
const GetAnchorAffinity = natives.GetAnchorAffinity;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the selection focus object.
 */
const GetFocusObjectID = natives.GetFocusObjectID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The selection focus offset.
 */
const GetFocusOffset = natives.GetFocusOffset;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The selection focus affinity.
 */
const GetFocusAffinity = natives.GetFocusAffinity;

/**
 * The start of the selection always comes before its end in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the object at the start of the
 *     selection.
 */
const GetSelectionStartObjectID = natives.GetSelectionStartObjectID;

/**
 * The start of the selection always comes before its end in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The offset at the start of the selection.
 */
const GetSelectionStartOffset = natives.GetSelectionStartOffset;

/**
 * The start of the selection always comes before its end in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The affinity at the start of the selection.
 */
const GetSelectionStartAffinity = natives.GetSelectionStartAffinity;

/**
 * The end of the selection always comes after its start in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The ID of the object at the end of the selection.
 */
const GetSelectionEndObjectID = natives.GetSelectionEndObjectID;

/**
 * The end of the selection always comes after its start in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?number} The offset at the end of the selection.
 */
const GetSelectionEndOffset = natives.GetSelectionEndOffset;

/**
 * The end of the selection always comes after its start in the accessibility
 * tree.
 * @param {string} axTreeID The id of the accessibility tree.
 * @return {?string} The affinity at the end of the selection.
 */
const GetSelectionEndAffinity = natives.GetSelectionEndAffinity;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The id of the node's parent, or undefined if it's the
 *    root of its tree or if the tree or node wasn't found.
 */
const GetParentID = natives.GetParentID;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The number of children of the node, or undefined if
 *     the tree or node wasn't found.
 */
const GetChildCount = natives.GetChildCount;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {number} childIndex An index of a child of this node.
 * @return {?number} The id of the child at the given index, or undefined
 *     if the tree or node or child at that index wasn't found.
 */
const GetChildIDAtIndex = natives.GetChildIDAtIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The ids of the children of the node, or undefined
 *     if the tree or node wasn't found.
 */
const GetChildIds = natives.GetChildIDs;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?number} The index of this node in its parent, or undefined if
 *     the tree or node or node parent wasn't found.
 */
const GetIndexInParent = natives.GetIndexInParent;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Object} An object with a string key for every state flag set,
 *     or undefined if the tree or node or node parent wasn't found.
 */
const GetState = natives.GetState;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The restriction, one of
 * "disabled", "readOnly" or undefined if enabled or other object not disabled
 */
const GetRestriction = natives.GetRestriction;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The checked state, as undefined, "true", "false" or "mixed".
 */
const GetChecked = natives.GetChecked;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The role of the node, or undefined if the tree or
 *     node wasn't found.
 */
const GetRole = natives.GetRole;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?automation.Rect} The location of the node, or undefined if
 *     the tree or node wasn't found.
 */
const GetLocation = natives.GetLocation;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {number} startIndex The start index of the range.
 * @param {number} endIndex The end index of the range.
 * @param {boolean} clipped Whether the bounds are clipped to ancestors.
 * @return {?automation.Rect} The bounding box of the subrange of this node,
 *     or the location if there are no subranges, or undefined if
 *     the tree or node wasn't found.
 */
const GetBoundsForRange = natives.GetBoundsForRange;

/**
 * @param {number} left The left location of the text range.
 * @param {number} top The top location of the text range.
 * @param {number} width The width of text range.
 * @param {number} height The height of the text range.
 * @param {number} requestID The request id associated with the query
 *    for this range.
 * @return {?automation.Rect} The bounding box of the subrange of this node,
 *     specified by arguments provided to the function.
 */
const ComputeGlobalBounds = natives.ComputeGlobalBounds;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?automation.Rect} The unclipped location of the node, or
 * undefined if the tree or node wasn't found.
 */
const GetUnclippedLocation = natives.GetUnclippedLocation;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>} The text offset where each line starts, or an empty
 *     array if this node has no text content, or undefined if the tree or node
 *     was not found.
 */
const GetLineStartOffsets = natives.GetLineStartOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of the node.
 * @return {?string} The computed name of this node.
 */
const GetName = natives.GetName;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of a string attribute.
 * @return {?string} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
const GetStringAttribute = natives.GetStringAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?boolean} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
const GetBoolAttribute = natives.GetBoolAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
const GetIntAttribute = natives.GetIntAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The ids of nodes who have a relationship pointing
 *     to |nodeID| (a reverse relationship).
 */
const GetIntAttributeReverseRelations = natives.GetIntAttributeReverseRelations;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?number} The value of this attribute, or undefined if the tree,
 *     node, or attribute wasn't found.
 */
const GetFloatAttribute = natives.GetFloatAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The value of this attribute, or undefined
 *     if the tree, node, or attribute wasn't found.
 */
const GetIntListAttribute = natives.GetIntListAttribute;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} attr The name of an attribute.
 * @return {?Array<number>} The ids of nodes who have a relationship pointing
 *     to |nodeID| (a reverse relationship).
 */
const GetIntListAttributeReverseRelations =
    natives.GetIntListAttributeReverseRelations;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.NameFromType} The source of the node's name.
 */
const GetNameFrom = natives.GetNameFrom;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.DescriptionFromType} The node description source.
 */
const GetDescriptionFrom = natives.GetDescriptionFrom;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?string} The image annotation status, which may
 *     include the annotation itself if completed successfully.
 */
const GetImageAnnotation = natives.GetImageAnnotation;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetBold = natives.GetBold;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetItalic = natives.GetItalic;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetUnderline = natives.GetUnderline;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetLineThrough = natives.GetLineThrough;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetIsButton = natives.GetIsButton;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetIsCheckBox = natives.GetIsCheckBox;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetIsComboBox = natives.GetIsComboBox;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {boolean}
 */
const GetIsImage = natives.GetIsImage;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<automation.CustomAction>} List of custom actions of the
 *     node.
 */
const GetCustomActions = natives.GetCustomActions;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<string>} List of standard actions of the node.
 */
const GetStandardActions = natives.GetStandardActions;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.NameFromType} The source of the node's name.
 */
const GetDefaultActionVerb = natives.GetDefaultActionVerb;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.HasPopup}
 */
const GetHasPopup = natives.GetHasPopup;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.AriaCurrentState}
 */
const GetAriaCurrentState = natives.GetAriaCurrentState;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {automation.InvalidState}
 */
const GetInvalidState = natives.GetInvalidState;


/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} searchStr
 * @param {boolean} backward
 * @return {{treeId: string, nodeId: number}}
 */
const GetNextTextMatch = natives.GetNextTextMatch;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<number>} A list of column header ids.
 */
const GetTableCellColumnHeaders = natives.GetTableCellColumnHeaders;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {?Array<number>} A list of row header ids.
 */
const GetTableCellRowHeaders = natives.GetTableCellRowHeaders;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} Column index for this cell.
 */
const GetTableCellColumnIndex = natives.GetTableCellColumnIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} Row index for this cell.
 */
const GetTableCellRowIndex = natives.GetTableCellRowIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} Column index for this cell.
 */
const GetTableCellAriaColumnIndex = natives.GetTableCellAriaColumnIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} Row index for this cell.
 */
const GetTableCellAriaRowIndex = natives.GetTableCellAriaRowIndex;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} column count for this cell's table. 0 if not in a table.
 */
const GetTableColumnCount = natives.GetTableColumnCount;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {number} Row count for this cell's table. 0 if not in a table.
 */
const GetTableRowCount = natives.GetTableRowCount;

/**
 * @param {string} axTreeId The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} Detected language for this node.
 */
const GetDetectedLanguage = natives.GetDetectedLanguage;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>}
 */
const GetWordStartOffsets = natives.GetWordStartOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>}
 */
const GetWordEndOffsets = natives.GetWordEndOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>}
 */
const GetSentenceStartOffsets = natives.GetSentenceStartOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {!Array<number>}
 */
const GetSentenceEndOffsets = natives.GetSentenceEndOffsets;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 */
const SetAccessibilityFocus = natives.SetAccessibilityFocus;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} eventType
 */
const EventListenerAdded = natives.EventListenerAdded;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {string} eventType
 */
const EventListenerRemoved = natives.EventListenerRemoved;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {Array}
 */
const GetMarkers = natives.GetMarkers;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @param {!automation.PositionType} type
 * @param {number} offset
 * @param {boolean} isUpstream
 * @return {!Object}
 */
const CreateAutomationPosition = natives.CreateAutomationPosition;

/**
 * @param {string} axTreeID The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} The sort direction.
 */
const GetSortDirection = natives.GetSortDirection;

/**
 * @param {string} axTreeId The id of the accessibility tree.
 * @param {number} nodeID The id of a node.
 * @return {string} .
 */
const GetValue = natives.GetValue;

const logging = requireNative('logging');
const utils = require('utils');

/**
 * A single node in the Automation tree.
 * @param {AutomationRootNodeImpl} root The root of the tree.
 * @constructor
 */
function AutomationNodeImpl(root) {
  this.rootImpl_ = root;
  this.listeners_ = {__proto__: null};
}

AutomationNodeImpl.prototype = {
  __proto__: null,

  /** @private {string} */
  treeID_: '',

  /** @private {number} */
  id_: -1,

  /** @private {boolean} */
  isRootNode_: false,

  get treeID() {
    return this.treeID_;
  },

  get id() {
    return this.id_;
  },

  detach: function() {
    this.rootImpl_ = null;
    this.listeners_ = {__proto__: null};
  },

  get isRootNode() {
    return this.isRootNode_;
  },

  get root() {
    const info = GetPublicRoot(this.treeID);
    if (!info) {
      return null;
    }
    return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId) ||
        null;
  },

  get parent() {
    const info = GetParentID(this.treeID, this.id);
    if (info) {
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
    }
  },

  get state() {
    return GetState(this.treeID, this.id) || {};
  },

  get role() {
    return GetRole(this.treeID, this.id);
  },

  get restriction() {
    return GetRestriction(this.treeID, this.id);
  },

  get checked() {
    return GetChecked(this.treeID, this.id);
  },

  get caretBounds() {
    const data = GetIntListAttribute(this.treeID, this.id, 'caretBounds');
    if (!data) {
      return;
    }

    if (data.length !== 4) {
      throw Error('Internal encoding error for caret bounds.');
    }

    return {left: data[0], top: data[1], width: data[2], height: data[3]};
  },

  get location() {
    return GetLocation(this.treeID, this.id);
  },

  boundsForRange: function(startIndex, endIndex, callback) {
    this.boundsForRangeInternal_(
        startIndex, endIndex, true /* clipped */, callback);
  },

  unclippedBoundsForRange: function(startIndex, endIndex, callback) {
    this.boundsForRangeInternal_(
        startIndex, endIndex, false /* clipped */, callback);
  },

  boundsForRangeInternal_: function(startIndex, endIndex, clipped, callback) {
    const errorMessage = clipped ?
        'Error with bounds for range callback' :
        'Error with unclipped bounds for range callback';

    if (!this.rootImpl_) {
      return;
    }

    // Not yet initialized.
    if (this.rootImpl_.treeID_ === undefined || this.id === undefined) {
      return;
    }

    if (!callback) {
      return;
    }

    if (!GetBoolAttribute(this.treeID, this.id, 'supportsTextLocation')) {
      try {
        callback(GetBoundsForRange(
            this.treeID, this.id, startIndex, endIndex, clipped /* clipped */));
        return;
      } catch (e) {
        logging.WARNING(errorMessage + e);
      }
      return;
    }

    this.performAction_(
        'getTextLocation', {startIndex: startIndex, endIndex: endIndex},
        callback);
    return;
  },

  get sortDirection() {
    return GetSortDirection(this.treeID, this.id);
  },

  get value() {
    return GetValue(this.treeID, this.id);
  },

  get unclippedLocation() {
    let result = GetUnclippedLocation(this.treeID, this.id);
    if (result === undefined) {
      result = GetLocation(this.treeID, this.id);
    }
    return result;
  },

  get indexInParent() {
    return GetIndexInParent(this.treeID, this.id);
  },

  get lineStartOffsets() {
    return GetLineStartOffsets(this.treeID, this.id);
  },

  get childTree() {
    const childTreeID = GetStringAttribute(this.treeID, this.id, 'childTreeId');
    if (childTreeID) {
      return AutomationRootNodeImpl.get(childTreeID);
    }
  },

  get firstChild() {
    if (GetChildCount(this.treeID, this.id) == 0) {
      return undefined;
    }
    const info = GetChildIDAtIndex(this.treeID, this.id, 0);
    if (info) {
      const child =
          AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);

      // A child with an app id should always be in a different tree.
      if (child.appId && this.treeID === info.treeId) {
        return;
      }

      return child;
    }
  },

  get lastChild() {
    const count = GetChildCount(this.treeID, this.id);
    if (count == 0) {
      return;
    }

    const info = GetChildIDAtIndex(this.treeID, this.id, count - 1);
    if (info) {
      const child =
          AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);

      // A child with an app id should always be in a different tree.
      if (child.appId && this.treeID === info.treeId) {
        return;
      }

      return child;
    }
  },

  get children() {
    const info = GetChildIds(this.treeID, this.id);
    if (!info) {
      return [];
    }

    const children = [];
    for (let i = 0; i < info.nodeIds.length; ++i) {
      const childID = info.nodeIds[i];
      const child =
          AutomationRootNodeImpl.getNodeFromTree(info.treeId, childID);

      // A child with an app id should always be in a different tree.
      if (child.appId && this.treeID === info.treeId) {
        continue;
      }

      if (child) {
        Array.prototype.push.call(children, child);
      }
    }
    return children;
  },

  get previousSibling() {
    let parent = this.parent;
    if (!parent) {
      return undefined;
    }
    parent = privates(parent).impl;
    const indexInParent = GetIndexInParent(this.treeID, this.id);
    const info = GetChildIDAtIndex(parent.treeID, parent.id, indexInParent - 1);
    if (info) {
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
    }
  },

  get nextSibling() {
    let parent = this.parent;
    if (!parent) {
      return undefined;
    }
    parent = privates(parent).impl;
    const indexInParent = GetIndexInParent(this.treeID, this.id);
    const info = GetChildIDAtIndex(parent.treeID, parent.id, indexInParent + 1);
    if (info) {
      return AutomationRootNodeImpl.getNodeFromTree(info.treeId, info.nodeId);
    }
  },

  get nameFrom() {
    return GetNameFrom(this.treeID, this.id);
  },

  get name() {
    return GetName(this.treeID, this.id);
  },

  get descriptionFrom() {
    return GetDescriptionFrom(this.treeID, this.id);
  },

  get imageAnnotation() {
    return GetImageAnnotation(this.treeID, this.id);
  },

  get bold() {
    return GetBold(this.treeID, this.id);
  },

  get italic() {
    return GetItalic(this.treeID, this.id);
  },

  get underline() {
    return GetUnderline(this.treeID, this.id);
  },

  get lineThrough() {
    return GetLineThrough(this.treeID, this.id);
  },

  get isButton() {
    return GetIsButton(this.treeID, this.id);
  },

  get isCheckBox() {
    return GetIsCheckBox(this.treeID, this.id);
  },

  get isComboBox() {
    return GetIsComboBox(this.treeID, this.id);
  },

  get isImage() {
    return GetIsImage(this.treeID, this.id);
  },

  get detectedLanguage() {
    return GetDetectedLanguage(this.treeID, this.id);
  },

  get customActions() {
    return GetCustomActions(this.treeID, this.id);
  },

  get standardActions() {
    return GetStandardActions(this.treeID, this.id);
  },

  get defaultActionVerb() {
    return GetDefaultActionVerb(this.treeID, this.id);
  },

  get hasPopup() {
    return GetHasPopup(this.treeID, this.id);
  },

  get ariaCurrentState() {
    return GetAriaCurrentState(this.treeID, this.id);
  },

  get invalidState() {
    return GetInvalidState(this.treeID, this.id);
  },

  get tableCellColumnHeaders() {
    const ids = GetTableCellColumnHeaders(this.treeID, this.id);
    if (ids && this.rootImpl_) {
      const result = [];
      for (let i = 0; i < ids.length; i++) {
        result.push(this.rootImpl_.get(ids[i]));
      }
      return result;
    }
  },

  get tableCellRowHeaders() {
    const ids = GetTableCellRowHeaders(this.treeID, this.id);
    if (ids && this.rootImpl_) {
      const result = [];
      for (let i = 0; i < ids.length; i++) {
        result.push(this.rootImpl_.get(ids[i]));
      }
      return result;
    }
  },

  get tableCellColumnIndex() {
    return GetTableCellColumnIndex(this.treeID, this.id);
  },

  get tableCellRowIndex() {
    return GetTableCellRowIndex(this.treeID, this.id);
  },


  get tableCellAriaColumnIndex() {
    return GetTableCellAriaColumnIndex(this.treeID, this.id);
  },

  get tableCellAriaRowIndex() {
    return GetTableCellAriaRowIndex(this.treeID, this.id);
  },

  get tableColumnCount() {
    return GetTableColumnCount(this.treeID, this.id);
  },

  get tableRowCount() {
    return GetTableRowCount(this.treeID, this.id);
  },

  get nonInlineTextWordStarts() {
    return GetWordStartOffsets(this.treeID, this.id);
  },

  get nonInlineTextWordEnds() {
    return GetWordEndOffsets(this.treeID, this.id);
  },

  get sentenceStarts() {
    return GetSentenceStartOffsets(this.treeID, this.id);
  },

  get sentenceEnds() {
    return GetSentenceEndOffsets(this.treeID, this.id);
  },

  get markers() {
    return GetMarkers(this.treeID, this.id);
  },

  createPosition: function(type, offset, opt_isUpstream) {
    const nativePosition = CreateAutomationPosition(
        this.treeID, this.id, type, offset, Boolean(opt_isUpstream));

    // Attach a getter for the node, which is only available in js.
    Object.defineProperty(nativePosition, 'node', {
      get: function() {
        const tree =
            AutomationTreeCache.idToAutomationRootNode[nativePosition.treeID];
        if (!tree) {
          return null;
        }

        return privates(tree).impl.get(nativePosition.anchorID);
      },
    });

    return nativePosition;
  },

  doDefault: function() {
    this.performAction_('doDefault');
  },

  focus: function() {
    this.performAction_('focus');
  },

  getImageData: function(maxWidth, maxHeight) {
    this.performAction_('getImageData',
                        { maxWidth: maxWidth,
                          maxHeight: maxHeight });
  },

  hitTest: function(x, y, eventToFire) {
    // Set an empty callback to trigger onActionResult.
    const callback = () => {};
    this.hitTestInternal(x, y, eventToFire, callback);
  },

  hitTestWithReply: function(x, y, opt_callback) {
    this.hitTestInternal(x, y, 'hitTestResult', opt_callback);
  },

  hitTestInternal: function(x, y, eventToFire, opt_callback) {
    // Convert from global to tree-relative coordinates.
    const location = GetLocation(this.treeID, GetRootID(this.treeID));
    this.performAction_('hitTest',
                        { x: Math.floor(x - location.left),
                          y: Math.floor(y - location.top),
                          eventToFire: eventToFire },
                        opt_callback);
  },

  makeVisible: function() {
    this.performAction_('scrollToMakeVisible');
  },

  performCustomAction: function(customActionId) {
    this.performAction_('customAction', { customActionID: customActionId });
  },

  performStandardAction: function(action) {
    const standardActions = GetStandardActions(this.treeID, this.id);
    if (!standardActions ||
        !standardActions.find(item => action == item)) {
      throw Error('Inapplicable action for node: ' + action);
    }
    this.performAction_(action);
  },

  replaceSelectedText: function(value) {
    if (this.state.editable) {
      this.performAction_('replaceSelectedText', { value: value});
    }
  },

  resumeMedia: function() {
    this.performAction_('resumeMedia');
  },

  scrollBackward: function(opt_callback) {
    this.performAction_('scrollBackward', {}, opt_callback);
  },

  scrollForward: function(opt_callback) {
    this.performAction_('scrollForward', {}, opt_callback);
  },

  scrollUp: function(opt_callback) {
    this.performAction_('scrollUp', {}, opt_callback);
  },

  scrollDown: function(opt_callback) {
    this.performAction_('scrollDown', {}, opt_callback);
  },

  scrollLeft: function(opt_callback) {
    this.performAction_('scrollLeft', {}, opt_callback);
  },

  scrollRight: function(opt_callback) {
    this.performAction_('scrollRight', {}, opt_callback);
  },

  scrollToPoint: function(x, y) {
    this.performAction_('scrollToPoint', {x, y});
  },

  scrollToPositionAtRowColumn: function(row, column) {
    this.performAction_('scrollToPositionAtRowColumn', {row, column});
  },

  setScrollOffset: function(x, y) {
    this.performAction_('setScrollOffset', {x, y});
  },

  setAccessibilityFocus: function() {
    SetAccessibilityFocus(this.treeID, this.id);
  },

  setSelection: function(startIndex, endIndex) {
    if (this.state.editable) {
      this.performAction_('setSelection', {
        focusNodeID: this.id,
        anchorOffset: startIndex,
        focusOffset: endIndex,
      });
    }
  },

  setSequentialFocusNavigationStartingPoint: function() {
    this.performAction_('setSequentialFocusNavigationStartingPoint');
  },

  setValue: function(value) {
    if (this.state.editable) {
      this.performAction_('setValue', { value: value});
    }
  },

  showContextMenu: function() {
    this.performAction_('showContextMenu');
  },

  startDuckingMedia: function() {
    this.performAction_('startDuckingMedia');
  },

  stopDuckingMedia: function() {
    this.performAction_('stopDuckingMedia');
  },

  suspendMedia: function() {
    this.performAction_('suspendMedia');
  },
  longClick: function() {
    this.performAction_('longClick');
  },

  find: function(params) {
    return this.findInternal_(params);
  },

  findAll: function(params) {
    return this.findInternal_(params, []);
  },

  matches: function(params) {
    return this.matchInternal_(params);
  },

  getNextTextMatch: function(searchStr, backward) {
    const info = GetNextTextMatch(this.treeID, this.id, searchStr, backward);

    if (!info) {
      return;
    }

    const impl = privates(AutomationRootNodeImpl.get(info.treeId)).impl;
    if (impl) {
      return impl.get(info.nodeId);
    }
  },

  addEventListener: function(eventType, callback, capture) {
    this.removeEventListener(eventType, callback);
    if (!this.listeners_[eventType]) {
      this.listeners_[eventType] = [];
    }

    // Calling EventListenerAdded will also validate the args
    // and throw an exception it's not a valid event type, so no invalid event
    // type/listener gets enqueued.
    EventListenerAdded(this.treeID, this.id, eventType);

    Array.prototype.push.call(this.listeners_[eventType], {
      __proto__: null,
      callback: callback,
      capture: !!capture,
    });
  },

  // TODO(dtseng/aboxhall): Check this impl against spec.
  removeEventListener: function(eventType, callback) {
    if (this.listeners_[eventType]) {
      const listeners = this.listeners_[eventType];
      for (let i = 0; i < listeners.length; i++) {
        if (callback === listeners[i].callback) {
          Array.prototype.splice.call(listeners, i, 1);
        }
      }

      if (listeners.length == 0) {
        EventListenerRemoved(this.treeID, this.id, eventType);
      }
    }
  },

  toJSON: function() {
    return {
      treeID: this.treeID,
      id: this.id,
      role: this.role,
      attributes: this.attributes,
    };
  },

  dispatchEvent: function(
      eventType, eventFrom, eventFromAction, mouseX, mouseY, intents) {
    const path = [];
    let parent = this.parent;
    while (parent) {
      Array.prototype.push.call(path, parent);
      parent = parent.parent;
    }

    const event = new AutomationEvent(
        eventType, this.wrapper, eventFrom, eventFromAction, mouseX, mouseY,
        intents);

    // Dispatch the event through the propagation path in three phases:
    // - capturing: starting from the root and going down to the target's parent
    // - targeting: dispatching the event on the target itself
    // - bubbling: starting from the target's parent, going back up to the root.
    // At any stage, a listener may call stopPropagation() on the event, which
    // will immediately stop event propagation through this path.
    if (this.dispatchEventAtCapturing_(event, path)) {
      if (this.dispatchEventAtTargeting_(event, path)) {
        this.dispatchEventAtBubbling_(event, path);
      }
    }
  },

  toString: function() {
    let parentID = GetParentID(this.treeID, this.id);
    parentID = parentID ? parentID.nodeId : null;
    const childTreeID = GetStringAttribute(this.treeID, this.id, 'childTreeId');
    const count = GetChildCount(this.treeID, this.id);
    const childIDs = [];
    for (let i = 0; i < count; ++i) {
      const childID = GetChildIDAtIndex(this.treeID, this.id, i).nodeId;
      Array.prototype.push.call(childIDs, childID);
    }
    const name = GetName(this.treeID, this.id);

    let result = 'node id=' + this.id + ' role=' + this.role +
        ' state=' + JSON.stringify(this.state) + ' parentID=' + parentID +
        ' childIds=' + JSON.stringify(childIDs);
    if (childTreeID) {
      result += ' childTreeID=' + childTreeID;
    }
    if (name) {
      result += ' name=' + name;
    }
    if (this.className) {
      result += ' className=' + this.className;
    }
    return result;
  },

  dispatchEventAtCapturing_: function(event, path) {
    event.eventPhase = Event.CAPTURING_PHASE;
    for (let i = path.length - 1; i >= 0; i--) {
      this.fireEventListeners_(path[i], event);
      if (event.propagationStopped) {
        return false;
      }
    }
    return true;
  },

  dispatchEventAtTargeting_: function(event) {
    event.eventPhase = Event.AT_TARGET;
    this.fireEventListeners_(this.wrapper, event);
    return !event.propagationStopped;
  },

  dispatchEventAtBubbling_: function(event, path) {
    event.eventPhase = Event.BUBBLING_PHASE;
    for (let i = 0; i < path.length; i++) {
      this.fireEventListeners_(path[i], event);
      if (event.propagationStopped) {
        return false;
      }
    }
    return true;
  },

  fireEventListeners_: function(node, event) {
    const nodeImpl = privates(node).impl;
    if (!nodeImpl.rootImpl_) {
      return;
    }

    const originalListeners = nodeImpl.listeners_[event.type];
    if (!originalListeners) {
      return;
    }

    // Make a copy of the original listeners since calling any of them can cause
    // the list to be modified.
    const listeners = [];
    for (let i = 0; i < originalListeners.length; i++) {
      listeners.push(originalListeners[i]);
    }

    const eventPhase = event.eventPhase;
    for (let i = 0; i < listeners.length; i++) {
      if (eventPhase == Event.CAPTURING_PHASE && !listeners[i].capture) {
        continue;
      }
      if (eventPhase == Event.BUBBLING_PHASE && listeners[i].capture) {
        continue;
      }

      try {
        listeners[i].callback(event);
      } catch (e) {
        exceptionHandler.handle('Error in event handler for ' + event.type +
            ' during phase ' + eventPhase, e);
      }
    }
  },

  performAction_: function(actionType, opt_args, opt_callback) {
    if (!this.rootImpl_) {
      return;
    }

    // Not yet initialized.
    if (this.rootImpl_.treeID === undefined || this.id === undefined) {
      return;
    }

    // Check permissions.
    if (!IsInteractPermitted()) {
      throw new Error(
          actionType + ' requires {"desktop": true} in the ' +
          '"automation" manifest key.');
    }

    let requestID = -1;
    if (opt_callback) {
      requestID = this.rootImpl_.addActionResultCallback(
          actionType, opt_args, opt_callback);
    }

    automationInternal.performAction(
        {
          treeID: this.rootImpl_.treeID,
          automationNodeID: this.id,
          actionType: actionType,
          requestID: requestID,
        },
        opt_args || {});
  },

  findInternal_: function(params, opt_results) {
    let result = null;
    this.forAllDescendants_(function(node) {
      if (privates(node).impl.matchInternal_(params)) {
        if (opt_results) {
          Array.prototype.push.call(opt_results, node);
        } else {
          result = node;
        }
        return !opt_results;
      }
    });
    if (opt_results) {
      return opt_results;
    }
    return result;
  },

  /**
   * Executes a closure for all of this node's descendants, in pre-order.
   * Early-outs if the closure returns true.
   * @param {Function(AutomationNode):boolean} closure Closure to be executed
   *     for each node. Return true to early-out the traversal.
   */
  forAllDescendants_: function(closure) {
    const stack = this.wrapper.children.reverse();
    while (stack.length > 0) {
      const node = stack.pop();
      if (closure(node)) {
        return;
      }

      const children = node.children;
      for (let i = children.length - 1; i >= 0; i--) {
        stack.push(children[i]);
      }
    }
  },

  matchInternal_: function(params) {
    if (Object.keys(params).length === 0) {
      return false;
    }

    if ('role' in params && this.role != params.role) {
      return false;
    }

    if ('state' in params) {
      for (const state in params.state) {
        if (params.state[state] != (state in this.state)) {
          return false;
        }
      }
    }
    if ('attributes' in params) {
      for (const attribute in params.attributes) {
        const attrValue = params.attributes[attribute];
        if (typeof attrValue != 'object') {
          if (this[attribute] !== attrValue) {
            return false;
          }
        } else if (attrValue instanceof $RegExp.self) {
          if (typeof this[attribute] != 'string') {
            return false;
          }
          if (!attrValue.test(this[attribute])) {
            return false;
          }
        } else {
          // TODO(aboxhall): handle intlist case.
          return false;
        }
      }
    }
    return true;
  },
};

const stringAttributes = [
  'accessKey',
  'appId',
  'ariaCellColumnIndexText',
  'ariaCellRowIndexText',
  'autoComplete',
  'checkedStateDescription',
  'className',
  'containerLiveRelevant',
  'containerLiveStatus',
  'description',
  'display',
  'doDefaultLabel',
  'fontFamily',
  'htmlId',
  'htmlTag',
  'imageDataUrl',
  'inputType',
  'language',
  'liveRelevant',
  'liveStatus',
  'longClickLabel',
  'mathContent',
  'placeholder',
  'roleDescription',
  'tooltip',
  'url',
];

const boolAttributes = [
  'busy',
  'clickable',
  'containerLiveAtomic',
  'containerLiveBusy',
  'hasHiddenOffscreenNodes',
  'nonAtomicTextFieldRoot',
  'liveAtomic',
  'modal',
  'notUserSelectableStyle',
  'scrollable',
  'selected',
  'supportsTextLocation',
];

const intAttributes = [
  'backgroundColor',
  'color',
  'colorValue',
  'hierarchicalLevel',
  'posInSet',
  'scrollX',
  'scrollXMax',
  'scrollXMin',
  'scrollY',
  'scrollYMax',
  'scrollYMin',
  'setSize',
  'tableCellColumnSpan',
  'tableCellRowSpan',
  'ariaColumnCount',
  'ariaRowCount',
  'textSelEnd',
  'textSelStart',
];

// Int attribute, relation property to expose, reverse relation to expose.
const nodeRefAttributes = [
  ['activedescendantId', 'activeDescendant', 'activeDescendantFor'],
  ['inPageLinkTargetId', 'inPageLinkTarget', null],
  ['nextFocusId', 'nextFocus', null],
  ['nextOnLineId', 'nextOnLine', null],
  ['nextWindowFocusId', 'nextWindowFocus', null],
  ['previousFocusId', 'previousFocus', null],
  ['previousOnLineId', 'previousOnLine', null],
  ['previousWindowFocusId', 'previousWindowFocus', null],
  ['tableColumnHeaderId', 'tableColumnHeader', null],
  ['tableHeaderId', 'tableHeader', null],
  ['tableRowHeaderId', 'tableRowHeader', null],
];

const intListAttributes = ['wordEnds', 'wordStarts'];

// Intlist attribute, relation property to expose, reverse relation to expose.
const nodeRefListAttributes = [
  ['controlsIds', 'controls', 'controlledBy'],
  ['describedbyIds', 'describedBy', 'descriptionFor'],
  ['detailsIds', 'details', 'detailsFor'],
  ['errorMessageIds', 'errorMessage', 'errorMessageFor'],
  ['flowtoIds', 'flowTo', 'flowFrom'],
  ['labelledbyIds', 'labelledBy', 'labelFor'],
];

const floatAttributes =
    ['fontSize', 'maxValueForRange', 'minValueForRange', 'valueForRange'];

const publicAttributes = [];

Array.prototype.forEach.call(stringAttributes, function(attributeName) {
  Array.prototype.push.call(publicAttributes, attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetStringAttribute(this.treeID, this.id, attributeName);
    },
  });
});

Array.prototype.forEach.call(boolAttributes, function(attributeName) {
  Array.prototype.push.call(publicAttributes, attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetBoolAttribute(this.treeID, this.id, attributeName);
    },
  });
});

Array.prototype.forEach.call(intAttributes, function(attributeName) {
  Array.prototype.push.call(publicAttributes, attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetIntAttribute(this.treeID, this.id, attributeName);
    },
  });
});

Array.prototype.forEach.call(nodeRefAttributes, function(params) {
  const srcAttributeName = params[0];
  const dstAttributeName = params[1];
  const dstReverseAttributeName = params[2];
  Array.prototype.push.call(publicAttributes, dstAttributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    __proto__: null,
    get: function() {
      const id = GetIntAttribute(this.treeID, this.id, srcAttributeName);
      if (id && this.rootImpl_) {
        return this.rootImpl_.get(id);
      } else {
        return undefined;
      }
    },
  });
  if (dstReverseAttributeName) {
    Array.prototype.push.call(publicAttributes, dstReverseAttributeName);
    Object.defineProperty(
        AutomationNodeImpl.prototype, dstReverseAttributeName, {
          __proto__: null,
          get: function() {
            const ids = GetIntAttributeReverseRelations(
                this.treeID, this.id, srcAttributeName);
            if (!ids || !this.rootImpl_) {
              return undefined;
            }
            const result = [];
            for (let i = 0; i < ids.length; ++i) {
              const node = this.rootImpl_.get(ids[i]);
              if (node) {
                Array.prototype.push.call(result, node);
              }
            }
            return result;
          },
        });
  }
});

Array.prototype.forEach.call(intListAttributes, function(attributeName) {
  Array.prototype.push.call(publicAttributes, attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetIntListAttribute(this.treeID, this.id, attributeName);
    },
  });
});

Array.prototype.forEach.call(nodeRefListAttributes, function(params) {
  const srcAttributeName = params[0];
  const dstAttributeName = params[1];
  const dstReverseAttributeName = params[2];
  Array.prototype.push.call(publicAttributes, dstAttributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, dstAttributeName, {
    __proto__: null,
    get: function() {
      const ids = GetIntListAttribute(this.treeID, this.id, srcAttributeName);
      if (!ids || !this.rootImpl_) {
        return undefined;
      }
      const result = [];
      for (let i = 0; i < ids.length; ++i) {
        const node = this.rootImpl_.get(ids[i]);
        if (node) {
          Array.prototype.push.call(result, node);
        }
      }
      return result;
    },
  });
  if (dstReverseAttributeName) {
    Array.prototype.push.call(publicAttributes, dstReverseAttributeName);
    Object.defineProperty(
        AutomationNodeImpl.prototype, dstReverseAttributeName, {
          __proto__: null,
          get: function() {
            const ids = GetIntListAttributeReverseRelations(
                this.treeID, this.id, srcAttributeName);
            if (!ids || !this.rootImpl_) {
              return undefined;
            }
            const result = [];
            for (let i = 0; i < ids.length; ++i) {
              const node = this.rootImpl_.get(ids[i]);
              if (node) {
                Array.prototype.push.call(result, node);
              }
            }
            return result;
          },
        });
  }
});

Array.prototype.forEach.call(floatAttributes, function(attributeName) {
  Array.prototype.push.call(publicAttributes, attributeName);
  Object.defineProperty(AutomationNodeImpl.prototype, attributeName, {
    __proto__: null,
    get: function() {
      return GetFloatAttribute(this.treeID, this.id, attributeName);
    },
  });
});

/**
 * AutomationRootNode.
 *
 * An AutomationRootNode is the javascript end of an AXTree living in the
 * browser. AutomationRootNode handles unserializing incremental updates from
 * the source AXTree. Each update contains node data that form a complete tree
 * after applying the update.
 *
 * A brief note about ids used through this class. The source AXTree assigns
 * unique ids per node and we use these ids to build a hash to the actual
 * AutomationNode object.
 * Thus, tree traversals amount to a lookup in our hash.
 *
 * The tree itself is identified by the accessibility tree id of the
 * renderer widget host.
 * @constructor
 */
function AutomationRootNodeImpl(treeID) {
  $Function.call(AutomationNodeImpl, this, this);
  this.treeID_ = treeID;
  this.axNodeDataCache_ = {__proto__: null};
}

utils.defineProperty(AutomationRootNodeImpl, 'get', function(treeID) {
  const result = AutomationTreeCache.idToAutomationRootNode[treeID];
  return result || undefined;
});

utils.defineProperty(AutomationRootNodeImpl, 'getOrCreate', function(treeID) {
  if (AutomationTreeCache.idToAutomationRootNode[treeID]) {
    return AutomationTreeCache.idToAutomationRootNode[treeID];
  }
  const result = new AutomationRootNode(treeID);
  AutomationTreeCache.idToAutomationRootNode[treeID] = result;
  return result;
});

utils.defineProperty(
    AutomationRootNodeImpl, 'getNodeFromTree', function(treeId, nodeId) {
      const tree = AutomationRootNodeImpl.get(treeId);
      if (!tree) {
        return;
      }
      const impl = privates(tree).impl;
      if (impl) {
        return impl.get(nodeId);
      }
    });

utils.defineProperty(AutomationRootNodeImpl, 'destroy', function(treeID) {
  delete AutomationTreeCache.idToAutomationRootNode[treeID];
});

utils.defineProperty(AutomationRootNodeImpl, 'destroyAll', function() {
  AutomationTreeCache.idToAutomationRootNode = {};
});

/**
 * A counter keeping track of IDs to use for mapping action requests to
 * their callback function.
 */
AutomationRootNodeImpl.actionRequestCounter = 0;

/**
 * A map from a request ID to the corresponding callback function to call
 * when the action response event is received.
 */
AutomationRootNodeImpl.actionRequestIDToCallback = {};

AutomationRootNodeImpl.prototype = {
  __proto__: AutomationNodeImpl.prototype,

  /**
   * @type {boolean}
   * @private
   */
  isRootNode_: true,

  /**
   * A map from id to AutomationNode.
   * @type {Object<number, AutomationNode>}
   * @private
   */
  axNodeDataCache_: null,

  get id_() {
    const result = GetRootID(this.treeID);

    // Don't return undefined, because the id is often passed directly
    // as an argument to a native binding that expects only a valid number.
    if (result === undefined) {
      return -1;
    }

    return result;
  },

  get docUrl() {
    return GetDocURL(this.treeID);
  },

  get docTitle() {
    return GetDocTitle(this.treeID);
  },

  get docLoaded() {
    return GetDocLoaded(this.treeID);
  },

  get docLoadingProgress() {
    return GetDocLoadingProgress(this.treeID);
  },

  get isSelectionBackward() {
    return GetIsSelectionBackward(this.treeID);
  },

  get anchorObject() {
    const id = GetAnchorObjectID(this.treeID);
    if (id && id != -1) {
      return this.get(id);
    }
    return undefined;
  },

  get anchorOffset() {
    const id = GetAnchorObjectID(this.treeID);
    if (id && id != -1) {
      return GetAnchorOffset(this.treeID);
    }
    return undefined;
  },

  get anchorAffinity() {
    const id = GetAnchorObjectID(this.treeID);
    if (id && id != -1) {
      return GetAnchorAffinity(this.treeID);
    }
    return undefined;
  },

  get focusObject() {
    const id = GetFocusObjectID(this.treeID);
    if (id && id != -1) {
      return this.get(id);
    }
    return undefined;
  },

  get focusOffset() {
    const id = GetFocusObjectID(this.treeID);
    if (id && id != -1) {
      return GetFocusOffset(this.treeID);
    }
    return undefined;
  },

  get focusAffinity() {
    const id = GetFocusObjectID(this.treeID);
    if (id && id != -1) {
      return GetFocusAffinity(this.treeID);
    }
    return undefined;
  },

  get selectionStartObject() {
    const id = GetSelectionStartObjectID(this.treeID);
    if (id && id != -1) {
      return this.get(id);
    }
    return undefined;
  },

  get selectionStartOffset() {
    const id = GetSelectionStartObjectID(this.treeID);
    if (id && id != -1) {
      return GetSelectionStartOffset(this.treeID);
    }
    return undefined;
  },

  get selectionStartAffinity() {
    const id = GetSelectionStartObjectID(this.treeID);
    if (id && id != -1) {
      return GetSelectionStartAffinity(this.treeID);
    }
    return undefined;
  },

  get selectionEndObject() {
    const id = GetSelectionEndObjectID(this.treeID);
    if (id && id != -1) {
      return this.get(id);
    }
    return undefined;
  },

  get selectionEndOffset() {
    const id = GetSelectionEndObjectID(this.treeID);
    if (id && id != -1) {
      return GetSelectionEndOffset(this.treeID);
    }
    return undefined;
  },

  get selectionEndAffinity() {
    const id = GetSelectionEndObjectID(this.treeID);
    if (id && id != -1) {
      return GetSelectionEndAffinity(this.treeID);
    }
    return undefined;
  },

  get: function(id) {
    if (id == undefined) {
      return undefined;
    }

    if (id == this.id) {
      return this.wrapper;
    }

    let obj = this.axNodeDataCache_[id];
    if (obj) {
      return obj;
    }

    // Validate the backing AXTree has the specified node.
    if (!GetRole(this.treeID, id)) {
      return;
    }

    obj = new AutomationNode(this);
    privates(obj).impl.treeID_ = this.treeID;
    privates(obj).impl.id_ = id;
    this.axNodeDataCache_[id] = obj;

    return obj;
  },

  remove: function(id) {
    if (this.axNodeDataCache_[id]) {
      privates(this.axNodeDataCache_[id]).impl.detach();
    }
    delete this.axNodeDataCache_[id];
  },

  destroy: function() {
    for (const id in this.axNodeDataCache_) {
      this.remove(id);
    }
    this.detach();
  },

  onAccessibilityEvent: function(eventParams) {
    const targetNode = this.get(eventParams.targetID);
    if (targetNode) {
      if (eventParams.actionRequestID != -1 &&
          this.onActionResult(eventParams.actionRequestID, targetNode)) {
        return;
      }

      const targetNodeImpl = privates(targetNode).impl;
      targetNodeImpl.dispatchEvent(
          eventParams.eventType, eventParams.eventFrom,
          eventParams.eventFromAction, eventParams.mouseX, eventParams.mouseY,
          eventParams.intents);
    } else {
      logging.WARNING(
          'Got ' + eventParams.eventType + ' event on unknown node: ' +
          eventParams.targetID + '; this: ' + this.id);
    }
  },

  addActionResultCallback: function(actionType, opt_args, callback) {
    AutomationRootNodeImpl
        .actionRequestIDToCallback[++AutomationRootNodeImpl
                                         .actionRequestCounter] = {
      actionType,
      opt_args,
      callback,
    };
    return AutomationRootNodeImpl.actionRequestCounter;
  },

  onGetTextLocationResult: function(textLocationParams) {
    const requestID = textLocationParams.requestID;
    if (requestID in AutomationRootNodeImpl.actionRequestIDToCallback) {
      const callback =
          AutomationRootNodeImpl.actionRequestIDToCallback[requestID].callback;
      try {
        if (textLocationParams.result) {
          callback(ComputeGlobalBounds(
              this.treeID, textLocationParams.nodeID, textLocationParams.left,
              textLocationParams.top, textLocationParams.width,
              textLocationParams.height));
        } else {
          callback(undefined);
        }
      } catch (e) {
        logging.WARNING('Error with onGetTextLocationResult callback:' + e);
      }
      delete AutomationRootNodeImpl.actionRequestIDToCallback[requestID];
    }
  },

  onActionResult: function(requestID, result) {
    if (requestID in AutomationRootNodeImpl.actionRequestIDToCallback) {
      const data = AutomationRootNodeImpl.actionRequestIDToCallback[requestID];
      if (data.actionType.indexOf('hitTest') === 0 && result &&
          result.role === 'window' && result.className &&
          result.className.indexOf('ExoSurface') === 0) {
        // Search for a node containing app id, which indicates Lacros.
        function findApp(node) {
          // Exit early if we've crossed roots from |result|.
          if (result.root !== node.root) {
            return null;
          }

          // This node is actually in a different backing C++ tree though at
          // this internal js layer, we merge the trees so that it is rooted to
          // the desktop tree (same as |result|).
          if (node.appId) {
            return node;
          }

          for (const child of node.children) {
            const found = findApp(child);
            if (found) {
              return found;
            }
          }

          return null;
        }

        // The hit test |result| node is not quite what we need to start
        // searching in. Find the topmost ExoShell surface.
        while (result.parent && result.parent.className &&
               result.parent.className.indexOf('ExoShellSurface') === 0) {
          result = result.parent;
        }

        const appNode = findApp(result);
        if (appNode) {
          delete AutomationRootNodeImpl.actionRequestIDToCallback[requestID];

          // Repost the hit test on |appNode|.
          privates(appNode).impl.performAction_(
              data.actionType, data.opt_args, data.callback);
          return true;
        }
      }

      data.callback(result);
      delete AutomationRootNodeImpl.actionRequestIDToCallback[requestID];
      return false;
    }
  },

  toString: function() {
    function toStringInternal(nodeImpl, indent) {
      if (!nodeImpl) {
        return '';
      }
      let output = '';
      if (nodeImpl.isRootNode) {
        output += indent + 'tree id=' + nodeImpl.treeID + '\n';
      }
      output += indent +
        $Function.call(AutomationNodeImpl.prototype.toString, nodeImpl) + '\n';
      indent += '  ';
      const children = nodeImpl.children;
      for (let i = 0; i < children.length; ++i) {
        output += toStringInternal(privates(children[i]).impl, indent);
      }
      return output;
    }
    return toStringInternal(this, '');
  },
};

function AutomationNode() {
  privates(AutomationNode).constructPrivate(this, arguments);
}
utils.expose(AutomationNode, AutomationNodeImpl, {
  functions: [
    'addEventListener',
    'boundsForRange',
    'createPosition',
    'doDefault',
    'find',
    'findAll',
    'focus',
    'getImageData',
    'getNextTextMatch',
    'hitTest',
    'hitTestWithReply',
    'languageAnnotationForStringAttribute',
    'makeVisible',
    'matches',
    'performCustomAction',
    'performStandardAction',
    'removeEventListener',
    'replaceSelectedText',
    'resumeMedia',
    'scrollBackward',
    'scrollDown',
    'scrollForward',
    'scrollLeft',
    'scrollRight',
    'scrollToPoint',
    'scrollToPositionAtRowColumn',
    'scrollUp',
    'setAccessibilityFocus',
    'setScrollOffset',
    'setSelection',
    'setSequentialFocusNavigationStartingPoint',
    'setValue',
    'showContextMenu',
    'startDuckingMedia',
    'stopDuckingMedia',
    'suspendMedia',
    'longClick',
    'toString',
    'unclippedBoundsForRange',
  ],
  readonly: Array.prototype.concat.call(
      publicAttributes,
      [
        'ariaCurrentState',
        'bold',
        'caretBounds',
        'checked',
        'children',
        'customActions',
        'defaultActionVerb',
        'descriptionFrom',
        'detectedLanguage',
        'firstChild',
        'hasPopup',
        'imageAnnotation',
        'indexInParent',
        'invalidState',
        'isButton',
        'isCheckBox',
        'isComboBox',
        'isImage',
        'isRootNode',
        'italic',
        'lastChild',
        'lineStartOffsets',
        'lineThrough',
        'location',
        'markers',
        'name',
        'nameFrom',
        'nextSibling',
        'nonInlineTextWordEnds',
        'nonInlineTextWordStarts',
        'parent',
        'previousSibling',
        'restriction',
        'role',
        'root',
        'sortDirection',
        'standardActions',
        'state',
        'sentenceStarts',
        'sentenceEnds',
        'tableCellAriaColumnIndex',
        'tableCellAriaRowIndex',
        'tableCellColumnHeaders',
        'tableCellColumnIndex',
        'tableCellRowHeaders',
        'tableCellRowIndex',
        'tableColumnCount',
        'tableRowCount',
        'unclippedLocation',
        'underline',
        'value',
      ]),
});

function AutomationRootNode() {
  privates(AutomationRootNode).constructPrivate(this, arguments);
}
utils.expose(AutomationRootNode, AutomationRootNodeImpl, {
  superclass: AutomationNode,
  readonly: [
    'docTitle',
    'docUrl',
    'docLoaded',
    'docLoadingProgress',
    'isSelectionBackward',
    'anchorObject',
    'anchorOffset',
    'anchorAffinity',
    'focusObject',
    'focusOffset',
    'focusAffinity',
    'selectionStartObject',
    'selectionStartOffset',
    'selectionStartAffinity',
    'selectionEndObject',
    'selectionEndOffset',
    'selectionEndAffinity',
  ],
});

utils.defineProperty(AutomationRootNode, 'get', function(treeID) {
  return AutomationRootNodeImpl.get(treeID);
});

utils.defineProperty(AutomationRootNode, 'getOrCreate', function(treeID) {
  return AutomationRootNodeImpl.getOrCreate(treeID);
});

utils.defineProperty(AutomationRootNode, 'destroy', function(treeID) {
  AutomationRootNodeImpl.destroy(treeID);
});

utils.defineProperty(AutomationRootNode, 'destroyAll', function() {
  AutomationRootNodeImpl.destroyAll();
});

exports.$set('AutomationNode', AutomationNode);
exports.$set('AutomationRootNode', AutomationRootNode);
