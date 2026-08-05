/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   FontMenuItem.js
 */

'use strict';

/*
 *   @constructor MenuItem
 *
 *   @desc
 *       Wrapper object for a simple menu item in a popup menu
 *
 *   @param domNode
 *       The DOM element node that serves as the menu item container.
 *       The menuObj PopupMenu is responsible for checking that it has
 *       requisite metadata, e.g. role="menuitem".
 *
 *   @param menuObj
 *       The object that is a wrapper for the PopupMenu DOM element that
 *       contains the menu item DOM element. See PopupMenuAction.js
 */
var FontMenuItem = function (domNode, fontMenu) {
  this.domNode = domNode;
  this.fontMenu = fontMenu;
  this.font = '';

  this.keyCode = Object.freeze({
    TAB: 9,
    ENTER: 13,
    ESC: 27,
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

FontMenuItem.prototype.init = function () {
  this.domNode.setAttribute('tabindex', '-1');

  if (!this.domNode.getAttribute('role')) {
    this.domNode.setAttribute('role', 'menuitemradio');
  }

  this.font = this.domNode.textContent.trim().toLowerCase();

  this.domNode.addEventListener('keydown', this.handleKeydown.bind(this));
  this.domNode.addEventListener('click', this.handleClick.bind(this));
  this.domNode.addEventListener('focus', this.handleFocus.bind(this));
  this.domNode.addEventListener('blur', this.handleBlur.bind(this));
  this.domNode.addEventListener('mouseover', this.handleMouseover.bind(this));
  this.domNode.addEventListener('mouseout', this.handleMouseout.bind(this));
};

/* EVENT HANDLERS */

FontMenuItem.prototype.handleKeydown = function (event) {
  var flag = false,
    char = event.key;

  function isPrintableCharacter(str) {
    return str.length === 1 && str.match(/\S/);
  }

  if (event.ctrlKey || event.altKey || event.metaKey) {
    return;
  }

  if (event.shiftKey) {
    if (isPrintableCharacter(char)) {
      this.fontMenu.setFocusByFirstCharacter(this, char);
    }
  } else {
    switch (event.keyCode) {
      case this.keyCode.SPACE:
      case this.keyCode.ENTER:
        this.handleClick(event);
        flag = true;
        break;

      case this.keyCode.ESC:
        this.fontMenu.setFocusToController();
        this.fontMenu.close(true);
        flag = true;
        break;

      case this.keyCode.UP:
        this.fontMenu.setFocusToPreviousItem(this);
        flag = true;
        break;

      case this.keyCode.DOWN:
        this.fontMenu.setFocusToNextItem(this);
        flag = true;
        break;

      case this.keyCode.RIGHT:
        flag = true;
        break;

      case this.keyCode.LEFT:
        flag = true;
        break;

      case this.keyCode.HOME:
      case this.keyCode.PAGEUP:
        this.fontMenu.setFocusToFirstItem();
        flag = true;
        break;

      case this.keyCode.END:
      case this.keyCode.PAGEDOWN:
        this.fontMenu.setFocusToLastItem();
        flag = true;
        break;

      case this.keyCode.TAB:
        this.fontMenu.setFocusToController();
        this.fontMenu.close(true);
        break;

      default:
        if (isPrintableCharacter(char)) {
          this.fontMenu.setFocusByFirstCharacter(this, char);
        }
        break;
    }
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

FontMenuItem.prototype.handleClick = function () {
  this.fontMenu.setFontFamily(this, this.font);
  this.fontMenu.setFocusToController();
  this.fontMenu.close(true);
};

FontMenuItem.prototype.handleFocus = function () {
  this.fontMenu.setFocus();
};

FontMenuItem.prototype.handleBlur = function () {
  this.fontMenu.removeFocus();
};

FontMenuItem.prototype.handleMouseover = function () {
  this.fontMenu.hasHover = true;
  this.fontMenu.open();
};

FontMenuItem.prototype.handleMouseout = function () {
  this.fontMenu.hasHover = false;
  setTimeout(this.fontMenu.close.bind(this.fontMenu, false), 300);
};
