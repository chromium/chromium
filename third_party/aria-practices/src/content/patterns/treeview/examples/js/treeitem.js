/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   Treeitem.js
 *
 *   Desc:   Treeitem widget that implements ARIA Authoring Practices
 *           for a tree being used as a file viewer
 */

'use strict';

/*
 *   @constructor
 *
 *   @desc
 *       Treeitem object for representing the state and user interactions for a
 *       treeItem widget
 *
 *   @param node
 *       An element with the role=tree attribute
 */

var Treeitem = function (node, treeObj, group) {
  // Check whether node is a DOM element
  if (typeof node !== 'object') {
    return;
  }

  node.tabIndex = -1;
  this.tree = treeObj;
  this.groupTreeitem = group;
  this.domNode = node;
  this.label = node.textContent.trim();

  if (node.getAttribute('aria-label')) {
    this.label = node.getAttribute('aria-label').trim();
  }

  this.isExpandable = false;
  this.isVisible = false;
  this.inGroup = false;

  if (group) {
    this.inGroup = true;
  }

  var elem = node.firstElementChild;

  while (elem) {
    if (elem.tagName.toLowerCase() == 'ul') {
      elem.setAttribute('role', 'group');
      this.isExpandable = true;
      break;
    }

    elem = elem.nextElementSibling;
  }

  this.keyCode = Object.freeze({
    RETURN: 13,
    SPACE: 32,
    PAGEUP: 33,
    PAGEDOWN: 34,
    END: 35,
    HOME: 36,
    LEFT: 37,
    UP: 38,
    RIGHT: 39,
    DOWN: 40,
  });
};

Treeitem.prototype.init = function () {
  this.domNode.tabIndex = -1;

  if (!this.domNode.getAttribute('role')) {
    this.domNode.setAttribute('role', 'treeitem');
  }

  this.domNode.addEventListener('keydown', this.handleKeydown.bind(this));
  this.domNode.addEventListener('click', this.handleClick.bind(this));
  this.domNode.addEventListener('focus', this.handleFocus.bind(this));
  this.domNode.addEventListener('blur', this.handleBlur.bind(this));

  if (!this.isExpandable) {
    this.domNode.addEventListener('mouseover', this.handleMouseOver.bind(this));
    this.domNode.addEventListener('mouseout', this.handleMouseOut.bind(this));
  }
};

Treeitem.prototype.isExpanded = function () {
  if (this.isExpandable) {
    return this.domNode.getAttribute('aria-expanded') === 'true';
  }

  return false;
};

/* EVENT HANDLERS */

Treeitem.prototype.handleKeydown = function (event) {
  var flag = false,
    char = event.key;

  function isPrintableCharacter(str) {
    return str.length === 1 && str.match(/\S/);
  }

  function printableCharacter(item) {
    if (char == '*') {
      item.tree.expandAllSiblingItems(item);
      flag = true;
    } else {
      if (isPrintableCharacter(char)) {
        item.tree.setFocusByFirstCharacter(item, char);
        flag = true;
      }
    }
  }

  if (event.altKey || event.ctrlKey || event.metaKey) {
    return;
  }

  if (event.shift) {
    if (isPrintableCharacter(char)) {
      printableCharacter(this);
    }
  } else {
    switch (event.keyCode) {
      case this.keyCode.RETURN:
      case this.keyCode.SPACE:
        var treeitem = event.currentTarget;
        var label = treeitem.getAttribute('aria-label');
        if (!label) {
          var child = treeitem.firstElementChild;
          label = child ? child.innerText : treeitem.innerText;
        }
        document.getElementById('last_action').value = label.trim();

        if (!this.isExpandable) this.tree.setFocusToItem(this);
        this.tree.setSelectedToItem(this);
        flag = true;
        break;

      case this.keyCode.UP:
        this.tree.setFocusToPreviousItem(this);
        flag = true;
        break;

      case this.keyCode.DOWN:
        this.tree.setFocusToNextItem(this);
        flag = true;
        break;

      case this.keyCode.RIGHT:
        if (this.isExpandable) {
          if (this.isExpanded()) {
            this.tree.setFocusToNextItem(this);
          } else {
            this.tree.expandTreeitem(this);
          }
        }
        flag = true;
        break;

      case this.keyCode.LEFT:
        if (this.isExpandable && this.isExpanded()) {
          this.tree.collapseTreeitem(this);
          flag = true;
        } else {
          if (this.inGroup) {
            this.tree.setFocusToParentItem(this);
            flag = true;
          }
        }
        break;

      case this.keyCode.HOME:
        this.tree.setFocusToFirstItem();
        flag = true;
        break;

      case this.keyCode.END:
        this.tree.setFocusToLastItem();
        flag = true;
        break;

      default:
        if (isPrintableCharacter(char)) {
          printableCharacter(this);
        }
        break;
    }
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

Treeitem.prototype.handleClick = function (event) {
  if (this.isExpandable) {
    if (this.isExpanded()) {
      this.tree.collapseTreeitem(this);
    } else {
      this.tree.expandTreeitem(this);
    }
    event.stopPropagation();
  } else {
    this.tree.setFocusToItem(this);
  }
  this.tree.setSelectedToItem(this);
};

Treeitem.prototype.handleFocus = function () {
  var node = this.domNode;
  if (this.isExpandable) {
    node = node.firstElementChild;
  }
  node.classList.add('focus');
};

Treeitem.prototype.handleBlur = function () {
  var node = this.domNode;
  if (this.isExpandable) {
    node = node.firstElementChild;
  }
  node.classList.remove('focus');
};

Treeitem.prototype.handleMouseOver = function (event) {
  event.currentTarget.classList.add('hover');
};

Treeitem.prototype.handleMouseOut = function (event) {
  event.currentTarget.classList.remove('hover');
};
