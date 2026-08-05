/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 */

'use strict';

/**
 * ARIA Menu Button example
 *
 * @function onload
 * @description  after page has loaded initialize all menu buttons based on the selector "[aria-haspopup][aria-controls]"
 */

window.addEventListener('load', function () {
  var menuButtons = document.querySelectorAll('[aria-haspopup][aria-controls]');

  [].forEach.call(menuButtons, function (menuButton) {
    if (
      (menuButton && menuButton.tagName.toLowerCase() === 'button') ||
      menuButton.getAttribute('role').toLowerCase() === 'button'
    ) {
      var mb = new aria.widget.MenuButton(menuButton);
      mb.initMenuButton();
    }
  });
});

/**
 * @namespace aria
 */

var aria = aria || {};

/* ---------------------------------------------------------------- */
/*                  ARIA Utils Namespace                        */
/* ---------------------------------------------------------------- */

/**
 * @class Menu
 * @memberof aria.Utils
 * @description  Computes absolute position of an element
 */

aria.Utils = aria.Utils || {};

aria.Utils.findPos = function (element) {
  var xPosition = 0;
  var yPosition = 0;

  while (element) {
    xPosition += element.offsetLeft - element.scrollLeft + element.clientLeft;
    yPosition += element.offsetTop - element.scrollTop + element.clientTop;
    element = element.offsetParent;
  }
  return { x: xPosition, y: yPosition };
};

/* ---------------------------------------------------------------- */
/*                  ARIA Widget Namespace                        */
/* ---------------------------------------------------------------- */

aria.widget = aria.widget || {};

/* ---------------------------------------------------------------- */
/*                  Menu Button Widget                           */
/* ---------------------------------------------------------------- */

/**
 * @class Menu
 * @memberof aria.Widget
 * @description  Creates a Menu Button widget using ARIA
 */

aria.widget.Menu = function (node, menuButton) {
  this.keyCode = Object.freeze({
    TAB: 9,
    RETURN: 13,
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

  // Check fo DOM element node
  if (typeof node !== 'object' || !node.getElementsByClassName) {
    return false;
  }

  this.menuNode = node;
  node.tabIndex = -1;

  this.menuButton = menuButton;

  this.firstMenuItem = false;
  this.lastMenuItem = false;
};

/**
 * @function initMenuButton
 * @memberof aria.widget.Menu
 * @description  Adds event handlers to button elements
 */

aria.widget.Menu.prototype.initMenu = function () {
  var self = this;

  var cn = this.menuNode.firstChild;

  while (cn) {
    if (cn.nodeType === Node.ELEMENT_NODE) {
      if (cn.getAttribute('role') === 'menuitem') {
        cn.tabIndex = -1;
        if (!this.firstMenuItem) {
          this.firstMenuItem = cn;
        }
        this.lastMenuItem = cn;

        var eventKeyDown = function (event) {
          self.eventKeyDown(event, self);
        };
        cn.addEventListener('keydown', eventKeyDown);

        var eventMouseClick = function (event) {
          self.eventMouseClick(event, self);
        };
        cn.addEventListener('click', eventMouseClick);

        var eventBlur = function (event) {
          self.eventBlur(event, self);
        };
        cn.addEventListener('blur', eventBlur);

        var eventFocus = function (event) {
          self.eventFocus(event, self);
        };
        cn.addEventListener('focus', eventFocus);
      }
    }
    cn = cn.nextSibling;
  }
};

/**
 * @function nextMenuItem
 * @memberof aria.widget.Menu
 * @description  Moves focus to next menuItem
 */

aria.widget.Menu.prototype.nextMenuItem = function (currentMenuItem) {
  var mi = currentMenuItem.nextSibling;

  while (mi) {
    if (
      mi.nodeType === Node.ELEMENT_NODE &&
      mi.getAttribute('role') === 'menuitem'
    ) {
      mi.focus();
      break;
    }
    mi = mi.nextSibling;
  }

  if (!mi && this.firstMenuItem) {
    this.firstMenuItem.focus();
  }
};

/**
 * @function previousMenuItem
 * @memberof aria.widget.Menu
 * @description  Moves focus to previous menuItem
 */

aria.widget.Menu.prototype.previousMenuItem = function (currentMenuItem) {
  var mi = currentMenuItem.previousSibling;

  while (mi) {
    if (
      mi.nodeType === Node.ELEMENT_NODE &&
      mi.getAttribute('role') === 'menuitem'
    ) {
      mi.focus();
      break;
    }
    mi = mi.previousSibling;
  }

  if (!mi && this.lastMenuItem) {
    this.lastMenuItem.focus();
  }
};

/**
 * @function eventKeyDown
 * @memberof aria.widget.Menu
 * @description  Keydown event handler for Menu Object
 *        NOTE: The menu parameter is needed to provide a reference to the specific
 *               menu
 */

aria.widget.Menu.prototype.eventKeyDown = function (event, menu) {
  var ct = event.currentTarget;
  var flag = false;

  switch (event.keyCode) {
    case menu.keyCode.SPACE:
    case menu.keyCode.RETURN:
      menu.eventMouseClick(event, menu);
      menu.menuButton.closeMenu(true);
      flag = true;
      break;

    case menu.keyCode.ESC:
      menu.menuButton.closeMenu(true);
      menu.menuButton.buttonNode.focus();
      flag = true;
      break;

    case menu.keyCode.UP:
    case menu.keyCode.LEFT:
      menu.previousMenuItem(ct);
      flag = true;
      break;

    case menu.keyCode.DOWN:
    case menu.keyCode.RIGHT:
      menu.nextMenuItem(ct);
      flag = true;
      break;

    case menu.keyCode.TAB:
      menu.menuButton.closeMenu(true, false);
      break;

    default:
      break;
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * @function eventMouseClick
 * @memberof aria.widget.Menu
 * @description  onclick event handler for Menu Object
 *        NOTE: The menu parameter is needed to provide a reference to the specific
 *               menu
 */

aria.widget.Menu.prototype.eventMouseClick = function (event, menu) {
  var clickedItemText = event.target.innerText;
  this.menuButton.buttonNode.innerText = clickedItemText;
  menu.menuButton.closeMenu(true);
};

/**
 * @param event
 * @param menu
 * @function eventBlur
 * @memberof aria.widget.Menu
 * @description eventBlur event handler for Menu Object
 * NOTE: The menu parameter is needed to provide a reference to the specific
 * menu
 */
aria.widget.Menu.prototype.eventBlur = function (event, menu) {
  menu.menuHasFocus = false;
  setTimeout(function () {
    if (!menu.menuHasFocus) {
      menu.menuButton.closeMenu(false, false);
    }
  }, 200);
};

/**
 * @param event
 * @param menu
 * @function eventFocus
 * @memberof aria.widget.Menu
 * @description eventFocus event handler for Menu Object
 * NOTE: The menu parameter is needed to provide a reference to the specific
 * menu
 */
aria.widget.Menu.prototype.eventFocus = function (event, menu) {
  menu.menuHasFocus = true;
};

/* ---------------------------------------------------------------- */
/*                  Menu Button Widget                           */
/* ---------------------------------------------------------------- */

/**
 * @class Menu Button
 * @memberof aria.Widget
 * @description  Creates a Menu Button widget using ARIA
 */

aria.widget.MenuButton = function (node) {
  this.keyCode = Object.freeze({
    TAB: 9,
    RETURN: 13,
    ESC: 27,
    SPACE: 32,
    UP: 38,
    DOWN: 40,
  });

  // Check fo DOM element node
  if (typeof node !== 'object' || !node.getElementsByClassName) {
    return false;
  }

  this.done = true;
  this.mouseInMouseButton = false;
  this.menuHasFocus = false;
  this.buttonNode = node;
  this.isLink = false;

  if (node.tagName.toLowerCase() === 'a') {
    var url = node.getAttribute('href');
    if (url && url.length && url.length > 0) {
      this.isLink = true;
    }
  }
};

/**
 * @function initMenuButton
 * @memberof aria.widget.MenuButton
 * @description  Adds event handlers to button elements
 */

aria.widget.MenuButton.prototype.initMenuButton = function () {
  var id = this.buttonNode.getAttribute('aria-controls');

  if (id) {
    this.menuNode = document.getElementById(id);

    if (this.menuNode) {
      this.menu = new aria.widget.Menu(this.menuNode, this);
      this.menu.initMenu();
      this.menuShowing = false;
    }
  }

  this.closeMenu(false, false);

  var self = this;

  var eventKeyDown = function (event) {
    self.eventKeyDown(event, self);
  };
  this.buttonNode.addEventListener('keydown', eventKeyDown);

  var eventMouseClick = function (event) {
    self.eventMouseClick(event, self);
  };
  this.buttonNode.addEventListener('click', eventMouseClick);
};

/**
 * @function openMenu
 * @memberof aria.widget.MenuButton
 * @description  Opens the menu
 */

aria.widget.MenuButton.prototype.openMenu = function () {
  if (this.menuNode) {
    this.menuNode.style.display = 'block';
    this.menuShowing = true;
  }
};

/**
 * @function closeMenu
 * @memberof aria.widget.MenuButton
 * @description  Close the menu
 */

aria.widget.MenuButton.prototype.closeMenu = function (force, focusMenuButton) {
  if (typeof force !== 'boolean') {
    force = false;
  }
  if (typeof focusMenuButton !== 'boolean') {
    focusMenuButton = true;
  }

  if (
    force ||
    (!this.mouseInMenuButton &&
      this.menuNode &&
      !this.menu.mouseInMenu &&
      !this.menu.menuHasFocus)
  ) {
    this.menuNode.style.display = 'none';
    if (focusMenuButton) {
      this.buttonNode.focus();
    }
    this.menuShowing = false;
  }
};

/**
 * @function toggleMenu
 * @memberof aria.widget.MenuButton
 * @description  Close or open the menu depending on current state
 */

aria.widget.MenuButton.prototype.toggleMenu = function () {
  if (this.menuNode) {
    if (this.menuNode.style.display === 'block') {
      this.menuNode.style.display = 'none';
    } else {
      this.menuNode.style.display = 'block';
    }
  }
};

/**
 * @function moveFocusToFirstMenuItem
 * @memberof aria.widget.MenuButton
 * @description  Move keyboard focus to first menu item
 */

aria.widget.MenuButton.prototype.moveFocusToFirstMenuItem = function () {
  if (this.menu.firstMenuItem) {
    this.openMenu();
    this.menu.firstMenuItem.focus();
  }
};

/**
 * @function moveFocusToLastMenuItem
 * @memberof aria.widget.MenuButton
 * @description  Move keyboard focus to last menu item
 */

aria.widget.MenuButton.prototype.moveFocusToLastMenuItem = function () {
  if (this.menu.lastMenuItem) {
    this.openMenu();
    this.menu.lastMenuItem.focus();
  }
};

/**
 * @function eventKeyDown
 * @memberof aria.widget.MenuButton
 * @description  Keydown event handler for MenuButton Object
 *        NOTE: The menuButton parameter is needed to provide a reference to the specific
 *               menuButton
 */

aria.widget.MenuButton.prototype.eventKeyDown = function (event, menuButton) {
  var flag = false;

  switch (event.keyCode) {
    case menuButton.keyCode.SPACE:
      menuButton.moveFocusToFirstMenuItem();
      flag = true;
      break;

    case menuButton.keyCode.RETURN:
      menuButton.moveFocusToFirstMenuItem();
      flag = true;
      break;

    case menuButton.keyCode.UP:
      if (this.menuShowing) {
        menuButton.moveFocusToLastMenuItem();
        flag = true;
      }
      break;

    case menuButton.keyCode.DOWN:
      if (this.menuShowing) {
        menuButton.moveFocusToFirstMenuItem();
        flag = true;
      }
      break;

    case menuButton.keyCode.TAB:
      menuButton.closeMenu(true, false);
      break;

    default:
      break;
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * @param event
 * @param menuButton
 * @function eventMouseClick
 * @memberof aria.widget.MenuButton
 * @description Click event handler for MenuButton Object
 * NOTE: The menuButton parameter is needed to provide a reference to the specific
 * menuButton
 */
aria.widget.MenuButton.prototype.eventMouseClick = function (
  event,
  menuButton
) {
  menuButton.moveFocusToFirstMenuItem();
};
