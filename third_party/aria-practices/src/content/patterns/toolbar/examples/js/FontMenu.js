/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   FontMenu.js
 */

/* global FontMenuItem */

'use strict';

var FontMenu = function (domNode, controllerObj) {
  var msgPrefix = 'FontMenu constructor argument domNode ';

  // Check whether domNode is a DOM element
  if (!(domNode instanceof Element)) {
    throw new TypeError(msgPrefix + 'is not a DOM Element.');
  }

  // Check whether domNode has child elements
  if (domNode.childElementCount === 0) {
    throw new Error(msgPrefix + 'has no element children.');
  }

  // Check whether domNode child elements are A elements
  var childElement = domNode.firstElementChild;

  while (childElement) {
    var menuitem = childElement.firstElementChild;

    if (menuitem && menuitem === 'A') {
      throw new Error(
        msgPrefix + 'Cannot have descendant elements are A elements.'
      );
    }
    childElement = childElement.nextElementSibling;
  }

  this.domNode = domNode;
  this.controller = controllerObj;

  this.menuitems = []; // See PopupMenu init method
  this.firstChars = []; // See PopupMenu init method

  this.firstItem = null; // See PopupMenu init method
  this.lastItem = null; // See PopupMenu init method

  this.hasFocus = false; // See MenuItem handleFocus, handleBlur
  this.hasHover = false; // See PopupMenu handleMouseover, handleMouseout
};

/*
 *   @method FontMenu.prototype.init
 *
 *   @desc
 *       Add domNode event listeners for mouseover and mouseout. Traverse
 *       domNode children to configure each menuitem and populate menuitems
 *       array. Initialize firstItem and lastItem properties.
 */
FontMenu.prototype.init = function () {
  var menuitemElements, menuitemElement, menuItem, textContent, numItems;

  // Configure the domNode itself
  this.domNode.setAttribute('tabindex', '-1');

  this.domNode.addEventListener('mouseover', this.handleMouseover.bind(this));
  this.domNode.addEventListener('mouseout', this.handleMouseout.bind(this));

  // Traverse the element children of domNode: configure each with
  // menuitem role behavior and store reference in menuitems array.
  menuitemElements = this.domNode.querySelectorAll('[role="menuitemradio"]');

  for (var i = 0; i < menuitemElements.length; i++) {
    menuitemElement = menuitemElements[i];
    menuItem = new FontMenuItem(menuitemElement, this);
    menuItem.init();
    this.menuitems.push(menuItem);
    textContent = menuitemElement.textContent.trim();
    this.firstChars.push(textContent.substring(0, 1).toLowerCase());
  }

  // Use populated menuitems array to initialize firstItem and lastItem.
  numItems = this.menuitems.length;
  if (numItems > 0) {
    this.firstItem = this.menuitems[0];
    this.lastItem = this.menuitems[numItems - 1];
  }
};

/* EVENT HANDLERS */

FontMenu.prototype.handleMouseover = function () {
  this.hasHover = true;
};

FontMenu.prototype.handleMouseout = function () {
  this.hasHover = false;
  setTimeout(this.close.bind(this, false), 300);
};

/* FOCUS MANAGEMENT METHODS */

FontMenu.prototype.setFocusToController = function (command) {
  if (typeof command !== 'string') {
    command = '';
  }

  if (command === 'previous') {
    this.controller.toolbar.setFocusToPrevious(this.controller.toolbarItem);
  } else {
    if (command === 'next') {
      this.controller.toolbar.setFocusToNext(this.controller.toolbarItem);
    } else {
      this.controller.domNode.focus();
    }
  }
};

FontMenu.prototype.setFontFamily = function (menuitem, font) {
  for (var i = 0; i < this.menuitems.length; i++) {
    var mi = this.menuitems[i];
    mi.domNode.setAttribute('aria-checked', mi === menuitem);
  }
  this.controller.setFontFamily(font);
};

FontMenu.prototype.setFocusToFirstItem = function () {
  this.firstItem.domNode.focus();
};

FontMenu.prototype.setFocusToLastItem = function () {
  this.lastItem.domNode.focus();
};

FontMenu.prototype.setFocusToPreviousItem = function (currentItem) {
  var index;

  if (currentItem === this.firstItem) {
    this.lastItem.domNode.focus();
  } else {
    index = this.menuitems.indexOf(currentItem);
    this.menuitems[index - 1].domNode.focus();
  }
};

FontMenu.prototype.setFocusToNextItem = function (currentItem) {
  var index;

  if (currentItem === this.lastItem) {
    this.firstItem.domNode.focus();
  } else {
    index = this.menuitems.indexOf(currentItem);
    this.menuitems[index + 1].domNode.focus();
  }
};

FontMenu.prototype.setFocusToCheckedItem = function () {
  for (var index = 0; index < this.menuitems.length; index++) {
    if (this.menuitems[index].domNode.getAttribute('aria-checked') === 'true') {
      this.menuitems[index].domNode.focus();
    }
  }
};

FontMenu.prototype.setFocusByFirstCharacter = function (currentItem, char) {
  var start, index;

  char = char.toLowerCase();

  // Get start index for search based on position of currentItem
  start = this.menuitems.indexOf(currentItem) + 1;
  if (start === this.menuitems.length) {
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
    this.menuitems[index].domNode.focus();
  }
};

FontMenu.prototype.getIndexFirstChars = function (startIndex, char) {
  for (var i = startIndex; i < this.firstChars.length; i++) {
    if (char === this.firstChars[i]) {
      return i;
    }
  }
  return -1;
};

/* Focus methods */

FontMenu.prototype.setFocus = function () {
  this.hasFocus = true;
  this.domNode.classList.add('focus');
  this.controller.toolbar.domNode.classList.add('focus');
};

FontMenu.prototype.removeFocus = function () {
  this.hasFocus = false;
  this.domNode.classList.remove('focus');
  this.controller.toolbar.domNode.classList.remove('focus');
  setTimeout(this.close.bind(this, false), 300);
};

/* MENU DISPLAY METHODS */

FontMenu.prototype.isOpen = function () {
  return this.controller.domNode.getAttribute('aria-expanded') === 'true';
};

FontMenu.prototype.open = function () {
  // Get bounding rectangle of controller object's DOM node
  var rect = this.controller.domNode.getBoundingClientRect();

  // Set CSS properties
  this.domNode.style.display = 'block';
  this.domNode.style.position = 'absolute';
  this.domNode.style.top = rect.height - 1 + 'px';
  this.domNode.style.left = '0px';
  this.domNode.style.zIndex = 100;

  // Set aria-expanded attribute
  this.controller.domNode.setAttribute('aria-expanded', 'true');
};

FontMenu.prototype.close = function (force) {
  if (typeof force !== 'boolean') {
    force = false;
  }

  if (
    force ||
    (!this.hasFocus && !this.hasHover && !this.controller.hasHover)
  ) {
    this.domNode.style.display = 'none';
    this.controller.domNode.setAttribute('aria-expanded', 'false');
  }
};
