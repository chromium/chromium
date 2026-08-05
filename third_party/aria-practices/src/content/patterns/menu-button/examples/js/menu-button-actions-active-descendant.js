/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   menu-button-actives-active-descendant.js
 *
 *   Desc:   Creates a menu button that opens a menu of actions using aria-activedescendants
 */

'use strict';

class MenuButtonActionsActiveDescendant {
  constructor(domNode, performMenuAction) {
    this.domNode = domNode;
    this.performMenuAction = performMenuAction;
    this.buttonNode = domNode.querySelector('button');
    this.menuNode = domNode.querySelector('[role="menu"]');
    this.currentMenuitem = {};
    this.menuitemNodes = [];
    this.firstMenuitem = false;
    this.lastMenuitem = false;
    this.firstChars = [];

    this.buttonNode.addEventListener(
      'keydown',
      this.onButtonKeydown.bind(this)
    );

    this.buttonNode.addEventListener('click', this.onButtonClick.bind(this));

    this.menuNode.addEventListener('keydown', this.onMenuKeydown.bind(this));

    var nodes = domNode.querySelectorAll('[role="menuitem"]');

    for (var i = 0; i < nodes.length; i++) {
      var menuitem = nodes[i];
      this.menuitemNodes.push(menuitem);
      menuitem.tabIndex = -1;
      this.firstChars.push(menuitem.textContent.trim()[0].toLowerCase());

      menuitem.addEventListener('click', this.onMenuitemClick.bind(this));

      menuitem.addEventListener(
        'mouseover',
        this.onMenuitemMouseover.bind(this)
      );

      if (!this.firstMenuitem) {
        this.firstMenuitem = menuitem;
      }
      this.lastMenuitem = menuitem;
    }

    domNode.addEventListener('focusin', this.onFocusin.bind(this));
    domNode.addEventListener('focusout', this.onFocusout.bind(this));

    window.addEventListener(
      'mousedown',
      this.onBackgroundMousedown.bind(this),
      true
    );
  }

  setFocusToMenuitem(newMenuitem) {
    for (var i = 0; i < this.menuitemNodes.length; i++) {
      var menuitem = this.menuitemNodes[i];
      if (menuitem === newMenuitem) {
        this.currentMenuitem = newMenuitem;
        menuitem.classList.add('focus');
        this.menuNode.setAttribute('aria-activedescendant', newMenuitem.id);
      } else {
        menuitem.classList.remove('focus');
      }
    }
  }

  setFocusToFirstMenuitem() {
    this.setFocusToMenuitem(this.firstMenuitem);
  }

  setFocusToLastMenuitem() {
    this.setFocusToMenuitem(this.lastMenuitem);
  }

  setFocusToPreviousMenuitem() {
    var newMenuitem, index;

    if (this.currentMenuitem === this.firstMenuitem) {
      newMenuitem = this.lastMenuitem;
    } else {
      index = this.menuitemNodes.indexOf(this.currentMenuitem);
      newMenuitem = this.menuitemNodes[index - 1];
    }

    this.setFocusToMenuitem(newMenuitem);

    return newMenuitem;
  }

  setFocusToNextMenuitem() {
    var newMenuitem, index;

    if (this.currentMenuitem === this.lastMenuitem) {
      newMenuitem = this.firstMenuitem;
    } else {
      index = this.menuitemNodes.indexOf(this.currentMenuitem);
      newMenuitem = this.menuitemNodes[index + 1];
    }
    this.setFocusToMenuitem(newMenuitem);

    return newMenuitem;
  }

  setFocusByFirstCharacter(char) {
    var start, index;

    if (char.length > 1) {
      return;
    }

    char = char.toLowerCase();

    // Get start index for search based on position of currentItem
    start = this.menuitemNodes.indexOf(this.currentMenuitem) + 1;
    if (start >= this.menuitemNodes.length) {
      start = 0;
    }

    // Check remaining slots in the menu
    index = this.firstChars.indexOf(char, start);

    // If not found in remaining slots, check from beginning
    if (index === -1) {
      index = this.firstChars.indexOf(char, 0);
    }

    // If match was found...
    if (index > -1) {
      this.setFocusToMenuitem(this.menuitemNodes[index]);
    }
  }

  // Utilities

  getIndexFirstChars(startIndex, char) {
    for (var i = startIndex; i < this.firstChars.length; i++) {
      if (char === this.firstChars[i]) {
        return i;
      }
    }
    return -1;
  }

  // Popup menu methods

  openPopup() {
    this.menuNode.style.display = 'block';
    this.buttonNode.setAttribute('aria-expanded', 'true');
    this.menuNode.focus();
    this.setFocusToFirstMenuitem();
  }

  closePopup() {
    if (this.isOpen()) {
      this.buttonNode.setAttribute('aria-expanded', 'false');
      this.menuNode.setAttribute('aria-activedescendant', '');
      for (let i = 0; i < this.menuitemNodes.length; i++) {
        this.menuitemNodes[i].classList.remove('focus');
      }
      this.menuNode.style.display = 'none';
      this.buttonNode.focus();
    }
  }

  isOpen() {
    return this.buttonNode.getAttribute('aria-expanded') === 'true';
  }

  // Menu event handlers

  onFocusin() {
    this.domNode.classList.add('focus');
  }

  onFocusout() {
    this.domNode.classList.remove('focus');
  }

  onButtonKeydown(event) {
    var key = event.key,
      flag = false;

    switch (key) {
      case ' ':
      case 'Enter':
      case 'ArrowDown':
      case 'Down':
        this.openPopup();
        this.setFocusToFirstMenuitem();
        flag = true;
        break;

      case 'Esc':
      case 'Escape':
        this.closePopup();
        flag = true;
        break;

      case 'Up':
      case 'ArrowUp':
        this.openPopup();
        this.setFocusToLastMenuitem();
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onButtonClick(event) {
    if (this.isOpen()) {
      this.closePopup();
    } else {
      this.openPopup();
    }
    event.stopPropagation();
    event.preventDefault();
  }

  onMenuKeydown(event) {
    var key = event.key,
      flag = false;

    function isPrintableCharacter(str) {
      return str.length === 1 && str.match(/\S/);
    }

    if (event.ctrlKey || event.altKey || event.metaKey) {
      return;
    }

    if (event.shiftKey) {
      if (isPrintableCharacter(key)) {
        this.setFocusByFirstCharacter(key);
        flag = true;
      }

      if (event.key === 'Tab') {
        this.closePopup();
        flag = true;
      }
    } else {
      switch (key) {
        case ' ':
        case 'Enter':
          this.closePopup();
          this.performMenuAction(this.currentMenuitem);
          flag = true;
          break;

        case 'Esc':
        case 'Escape':
          this.closePopup();
          flag = true;
          break;

        case 'Up':
        case 'ArrowUp':
          this.setFocusToPreviousMenuitem();
          flag = true;
          break;

        case 'ArrowDown':
        case 'Down':
          this.setFocusToNextMenuitem();
          flag = true;
          break;

        case 'Home':
        case 'PageUp':
          this.setFocusToFirstMenuitem();
          flag = true;
          break;

        case 'End':
        case 'PageDown':
          this.setFocusToLastMenuitem();
          flag = true;
          break;

        case 'Tab':
          this.closePopup();
          break;

        default:
          if (isPrintableCharacter(key)) {
            this.setFocusByFirstCharacter(key);
            flag = true;
          }
          break;
      }
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onMenuitemMouseover(event) {
    var tgt = event.currentTarget;
    this.setFocusToMenuitem(tgt);
  }

  onMenuitemClick(event) {
    var tgt = event.currentTarget;
    this.closePopup();
    this.performMenuAction(tgt);
  }

  onBackgroundMousedown(event) {
    if (!this.domNode.contains(event.target)) {
      if (this.isOpen()) {
        this.closePopup();
      }
    }
  }
}

// Initialize menu buttons

window.addEventListener('load', function () {
  document.getElementById('action_output').value = 'none';

  function performMenuAction(node) {
    document.getElementById('action_output').value = node.textContent.trim();
  }

  var menuButtons = document.querySelectorAll('.menu-button-actions');
  for (var i = 0; i < menuButtons.length; i++) {
    new MenuButtonActionsActiveDescendant(menuButtons[i], performMenuAction);
  }
});
