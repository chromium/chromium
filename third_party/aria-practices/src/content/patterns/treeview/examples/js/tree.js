/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   Tree.js
 *
 *   Desc:   Tree widget that implements ARIA Authoring Practices
 *           for a tree being used as a file viewer
 */

/* global Treeitem */

'use strict';

/**
 * ARIA Treeview example
 *
 * @function onload
 * @description  after page has loaded initialize all treeitems based on the role=treeitem
 */

window.addEventListener('load', function () {
  var trees = document.querySelectorAll('[role="tree"]');

  for (var i = 0; i < trees.length; i++) {
    var t = new Tree(trees[i]);
    t.init();
  }
});

/*
 *   @constructor
 *
 *   @desc
 *       Tree item object for representing the state and user interactions for a
 *       tree widget
 *
 *   @param node
 *       An element with the role=tree attribute
 */

var Tree = function (node) {
  // Check whether node is a DOM element
  if (typeof node !== 'object') {
    return;
  }

  this.domNode = node;

  this.treeitems = [];
  this.firstChars = [];

  this.firstTreeitem = null;
  this.lastTreeitem = null;
  this.selectedItem = null;
};

Tree.prototype.init = function () {
  function findTreeitems(node, tree, group) {
    var elem = node.firstElementChild;
    var ti = group;

    while (elem) {
      if (elem.tagName.toLowerCase() === 'li') {
        ti = new Treeitem(elem, tree, group);
        ti.init();
        tree.treeitems.push(ti);
        tree.firstChars.push(ti.label.substring(0, 1).toLowerCase());
      }

      if (elem.firstElementChild) {
        findTreeitems(elem, tree, ti);
      }

      elem = elem.nextElementSibling;
    }
  }

  // initialize pop up menus
  if (!this.domNode.getAttribute('role')) {
    this.domNode.setAttribute('role', 'tree');
  }

  findTreeitems(this.domNode, this, false);

  this.updateVisibleTreeitems();

  this.firstTreeitem.domNode.tabIndex = 0;
};

Tree.prototype.setSelectedToItem = function (treeitem) {
  if (this.selectedItem) {
    this.selectedItem.domNode.setAttribute('aria-selected', 'false');
  }
  treeitem.domNode.setAttribute('aria-selected', 'true');
  this.selectedItem = treeitem;
};

Tree.prototype.setFocusToItem = function (treeitem) {
  for (var i = 0; i < this.treeitems.length; i++) {
    var ti = this.treeitems[i];

    if (ti === treeitem) {
      ti.domNode.tabIndex = 0;
      ti.domNode.focus();
    } else {
      ti.domNode.tabIndex = -1;
    }
  }
};

Tree.prototype.setFocusToNextItem = function (currentItem) {
  var nextItem = false;

  for (var i = this.treeitems.length - 1; i >= 0; i--) {
    var ti = this.treeitems[i];
    if (ti === currentItem) {
      break;
    }
    if (ti.isVisible) {
      nextItem = ti;
    }
  }

  if (nextItem) {
    this.setFocusToItem(nextItem);
  }
};

Tree.prototype.setFocusToPreviousItem = function (currentItem) {
  var prevItem = false;

  for (var i = 0; i < this.treeitems.length; i++) {
    var ti = this.treeitems[i];
    if (ti === currentItem) {
      break;
    }
    if (ti.isVisible) {
      prevItem = ti;
    }
  }

  if (prevItem) {
    this.setFocusToItem(prevItem);
  }
};

Tree.prototype.setFocusToParentItem = function (currentItem) {
  if (currentItem.groupTreeitem) {
    this.setFocusToItem(currentItem.groupTreeitem);
  }
};

Tree.prototype.setFocusToFirstItem = function () {
  this.setFocusToItem(this.firstTreeitem);
};

Tree.prototype.setFocusToLastItem = function () {
  this.setFocusToItem(this.lastTreeitem);
};

Tree.prototype.expandTreeitem = function (currentItem) {
  if (currentItem.isExpandable) {
    currentItem.domNode.setAttribute('aria-expanded', true);
    this.updateVisibleTreeitems();
  }
};

Tree.prototype.expandAllSiblingItems = function (currentItem) {
  for (var i = 0; i < this.treeitems.length; i++) {
    var ti = this.treeitems[i];

    if (ti.groupTreeitem === currentItem.groupTreeitem && ti.isExpandable) {
      this.expandTreeitem(ti);
    }
  }
};

Tree.prototype.collapseTreeitem = function (currentItem) {
  var groupTreeitem = false;

  if (currentItem.isExpanded()) {
    groupTreeitem = currentItem;
  } else {
    groupTreeitem = currentItem.groupTreeitem;
  }

  if (groupTreeitem) {
    groupTreeitem.domNode.setAttribute('aria-expanded', false);
    this.updateVisibleTreeitems();
    this.setFocusToItem(groupTreeitem);
  }
};

Tree.prototype.updateVisibleTreeitems = function () {
  this.firstTreeitem = this.treeitems[0];

  for (var i = 0; i < this.treeitems.length; i++) {
    var ti = this.treeitems[i];

    var parent = ti.domNode.parentNode;

    ti.isVisible = true;

    while (parent && parent !== this.domNode) {
      if (parent.getAttribute('aria-expanded') == 'false') {
        ti.isVisible = false;
      }
      parent = parent.parentNode;
    }

    if (ti.isVisible) {
      this.lastTreeitem = ti;
    }
  }
};

Tree.prototype.setFocusByFirstCharacter = function (currentItem, char) {
  var start, index;

  char = char.toLowerCase();

  // Get start index for search based on position of currentItem
  start = this.treeitems.indexOf(currentItem) + 1;
  if (start === this.treeitems.length) {
    start = 0;
  }

  // Check remaining slots in the menu
  index = this.getIndexFirstChars(start, char);

  // If not found in remaining slots, check from beginning
  if (index === -1) {
    index = this.getIndexFirstChars(0, char);
  }

  // If match was found...
  if (index > -1) {
    this.setFocusToItem(this.treeitems[index]);
  }
};

Tree.prototype.getIndexFirstChars = function (startIndex, char) {
  for (var i = startIndex; i < this.firstChars.length; i++) {
    if (this.treeitems[i].isVisible) {
      if (char === this.firstChars[i]) {
        return i;
      }
    }
  }
  return -1;
};
