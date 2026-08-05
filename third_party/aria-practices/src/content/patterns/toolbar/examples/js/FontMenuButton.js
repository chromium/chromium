/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   FontMenuButton.js
 */

/* global FontMenu */

'use strict';

function FontMenuButton(node, toolbar, toolbarItem) {
  this.domNode = node;
  this.fontMenu = false;
  this.toolbar = toolbar;
  this.toolbarItem = toolbarItem;

  this.buttonAction = 'font-family';
  this.value = '';

  this.keyCode = Object.freeze({
    TAB: 9,
    ENTER: 13,
    ESC: 27,
    SPACE: 32,
    UP: 38,
    DOWN: 40,
  });
}

FontMenuButton.prototype.init = function () {
  var id = this.domNode.getAttribute('aria-controls');

  if (id) {
    var node = document.getElementById(id);

    if (node) {
      this.fontMenu = new FontMenu(node, this);
      this.fontMenu.init();
    }
  }

  this.domNode.addEventListener('keydown', this.handleKeyDown.bind(this));
  this.domNode.addEventListener('click', this.handleClick.bind(this));
};

FontMenuButton.prototype.handleKeyDown = function (event) {
  var flag = false;

  switch (event.keyCode) {
    case this.keyCode.SPACE:
    case this.keyCode.ENTER:
    case this.keyCode.DOWN:
    case this.keyCode.UP:
      this.fontMenu.open();
      this.fontMenu.setFocusToCheckedItem();
      flag = true;
      break;

    default:
      break;
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

FontMenuButton.prototype.handleClick = function () {
  if (this.fontMenu.isOpen()) {
    this.fontMenu.close();
  } else {
    this.fontMenu.open();
  }
};

FontMenuButton.prototype.setFontFamily = function (font) {
  this.value = font;
  this.domNode.innerHTML = font.toUpperCase() + '<span></span>';
  this.domNode.style.fontFamily = font;
  this.domNode.setAttribute('aria-label', 'Font: ' + font);
  this.toolbar.activateItem(this);
};
