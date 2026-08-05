/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   menubar-navigation.js
 *
 *   Desc:   Creates a menubar of hierarchical set of links
 */

'use strict';

class NavigationContentGenerator {
  constructor(siteURL, siteName) {
    this.siteName = siteName;
    this.siteURL = siteURL;
    this.fillerTextSentences = [];

    this.fillerTextSentences.push(
      'The content on this page is associated with the <a href="$linkURL">$linkName</a> link for <a href="$siteURL">$siteName</a>.'
    );
  }

  renderParagraph(linkURL, linkName) {
    var content = '';
    this.fillerTextSentences.forEach(
      (s) =>
        (content += s
          .replace('$siteName', this.siteName)
          .replace('$siteURL', this.siteURL)
          .replace('$linkName', linkName)
          .replace('$linkURL', linkURL))
    );
    return content;
  }
}

class MenubarNavigation {
  constructor(domNode) {
    var linkURL, linkTitle;

    this.domNode = domNode;

    this.menuitems = [];
    this.popups = [];
    this.menuitemGroups = {};
    this.menuOrientation = {};
    this.isPopup = {};
    this.isPopout = {};
    this.openPopups = false;

    this.firstChars = {}; // see Menubar init method
    this.firstMenuitem = {}; // see Menubar init method
    this.lastMenuitem = {}; // see Menubar init method

    this.initMenu(domNode, 0);

    domNode.addEventListener('focusin', this.onMenubarFocusin.bind(this));
    domNode.addEventListener('focusout', this.onMenubarFocusout.bind(this));

    window.addEventListener(
      'pointerdown',
      this.onBackgroundPointerdown.bind(this),
      true
    );

    domNode.querySelector('[role=menuitem]').tabIndex = 0;

    // Initial content for page
    if (location.href.split('#').length > 1) {
      linkURL = location.href;
      linkTitle = getLinkNameFromURL(location.href);
    } else {
      linkURL = location.href + '#home';
      linkTitle = 'Home';
    }

    this.contentGenerator = new NavigationContentGenerator(
      '#home',
      'Mythical University'
    );
    this.updateContent(linkURL, linkTitle, false);

    function getLinkNameFromURL(url) {
      function capitalize(str) {
        return str.charAt(0).toUpperCase() + str.slice(1);
      }

      var name = url.split('#')[1];
      if (typeof name === 'string') {
        name = name.split('-').map(capitalize).join(' ');
      } else {
        name = 'Home';
      }
      return name;
    }
  }

  getParentMenuitem(menuitem) {
    var node = menuitem.parentNode;
    if (node) {
      node = node.parentNode;
      if (node) {
        node = node.previousElementSibling;
        if (node) {
          if (node.getAttribute('role') === 'menuitem') {
            return node;
          }
        }
      }
    }
    return false;
  }

  updateContent(linkURL, linkName, moveFocus) {
    var h1Node, paraNodes, pathNode;

    if (typeof moveFocus !== 'boolean') {
      moveFocus = true;
    }

    // Update content area
    h1Node = document.querySelector('.page .main h1');
    if (h1Node) {
      h1Node.textContent = linkName;
      h1Node.tabIndex = -1;
      if (moveFocus) {
        h1Node.focus();
      }
    }
    paraNodes = document.querySelectorAll('.page .main p');
    paraNodes.forEach(
      (p) =>
        (p.innerHTML = this.contentGenerator.renderParagraph(linkURL, linkName))
    );

    // Update aria-current
    this.menuitems.forEach((item) => {
      item.removeAttribute('aria-current');
      item.classList.remove('aria-current-path');
      item.title = '';
    });

    this.menuitems.forEach((item) => {
      if (item.href === linkURL) {
        item.setAttribute('aria-current', 'page');
        pathNode = this.getParentMenuitem(item);
        while (pathNode) {
          pathNode.classList.add('aria-current-path');
          pathNode.title = 'Contains current page link';
          pathNode = this.getParentMenuitem(pathNode);
        }
      }
    });
  }

  getMenuitems(domNode, depth) {
    var nodes = [];

    var initMenu = this.initMenu.bind(this);
    var popups = this.popups;

    function findMenuitems(node) {
      var role, flag;

      while (node) {
        flag = true;
        role = node.getAttribute('role');

        if (role) {
          role = role.trim().toLowerCase();
        }

        switch (role) {
          case 'menu':
            node.tabIndex = -1;
            initMenu(node, depth + 1);
            flag = false;
            break;

          case 'menuitem':
            if (node.getAttribute('aria-haspopup') === 'true') {
              popups.push(node);
            }
            nodes.push(node);
            break;

          default:
            break;
        }

        if (
          flag &&
          node.firstElementChild &&
          node.firstElementChild.tagName !== 'svg'
        ) {
          findMenuitems(node.firstElementChild);
        }
        node = node.nextElementSibling;
      }
    }
    findMenuitems(domNode.firstElementChild);
    return nodes;
  }

  initMenu(menu, depth) {
    var menuitems, menuitem, role;

    var menuId = this.getMenuId(menu);

    menuitems = this.getMenuitems(menu, depth);
    this.menuOrientation[menuId] = this.getMenuOrientation(menu);

    this.isPopup[menuId] = menu.getAttribute('role') === 'menu' && depth === 1;
    this.isPopout[menuId] = menu.getAttribute('role') === 'menu' && depth > 1;

    this.menuitemGroups[menuId] = [];
    this.firstChars[menuId] = [];
    this.firstMenuitem[menuId] = null;
    this.lastMenuitem[menuId] = null;

    for (var i = 0; i < menuitems.length; i++) {
      menuitem = menuitems[i];
      role = menuitem.getAttribute('role');

      if (role.indexOf('menuitem') < 0) {
        continue;
      }

      menuitem.tabIndex = -1;
      this.menuitems.push(menuitem);
      this.menuitemGroups[menuId].push(menuitem);
      this.firstChars[menuId].push(
        menuitem.textContent.trim().toLowerCase()[0]
      );

      menuitem.addEventListener('keydown', this.onKeydown.bind(this));
      menuitem.addEventListener('click', this.onMenuitemClick.bind(this), {
        capture: true,
      });

      menuitem.addEventListener(
        'pointerover',
        this.onMenuitemPointerover.bind(this)
      );

      if (!this.firstMenuitem[menuId]) {
        if (this.hasPopup(menuitem)) {
          menuitem.tabIndex = 0;
        }
        this.firstMenuitem[menuId] = menuitem;
      }
      this.lastMenuitem[menuId] = menuitem;
    }
  }

  setFocusToMenuitem(menuId, newMenuitem) {
    this.closePopupAll(newMenuitem);

    if (this.menuitemGroups[menuId]) {
      this.menuitemGroups[menuId].forEach(function (item) {
        if (item === newMenuitem) {
          item.tabIndex = 0;
          newMenuitem.focus();
        } else {
          item.tabIndex = -1;
        }
      });
    }
  }

  setFocusToFirstMenuitem(menuId) {
    this.setFocusToMenuitem(menuId, this.firstMenuitem[menuId]);
  }

  setFocusToLastMenuitem(menuId) {
    this.setFocusToMenuitem(menuId, this.lastMenuitem[menuId]);
  }

  setFocusToPreviousMenuitem(menuId, currentMenuitem) {
    var newMenuitem, index;

    if (currentMenuitem === this.firstMenuitem[menuId]) {
      newMenuitem = this.lastMenuitem[menuId];
    } else {
      index = this.menuitemGroups[menuId].indexOf(currentMenuitem);
      newMenuitem = this.menuitemGroups[menuId][index - 1];
    }

    this.setFocusToMenuitem(menuId, newMenuitem);

    return newMenuitem;
  }

  setFocusToNextMenuitem(menuId, currentMenuitem) {
    var newMenuitem, index;

    if (currentMenuitem === this.lastMenuitem[menuId]) {
      newMenuitem = this.firstMenuitem[menuId];
    } else {
      index = this.menuitemGroups[menuId].indexOf(currentMenuitem);
      newMenuitem = this.menuitemGroups[menuId][index + 1];
    }
    this.setFocusToMenuitem(menuId, newMenuitem);

    return newMenuitem;
  }

  setFocusByFirstCharacter(menuId, currentMenuitem, char) {
    var start, index;

    char = char.toLowerCase();

    // Get start index for search based on position of currentItem
    start = this.menuitemGroups[menuId].indexOf(currentMenuitem) + 1;
    if (start >= this.menuitemGroups[menuId].length) {
      start = 0;
    }

    // Check remaining slots in the menu
    index = this.getIndexFirstChars(menuId, start, char);

    // If not found in remaining slots, check from beginning
    if (index === -1) {
      index = this.getIndexFirstChars(menuId, 0, char);
    }

    // If match was found...
    if (index > -1) {
      this.setFocusToMenuitem(menuId, this.menuitemGroups[menuId][index]);
    }
  }

  // Utilities

  getIndexFirstChars(menuId, startIndex, char) {
    for (var i = startIndex; i < this.firstChars[menuId].length; i++) {
      if (char === this.firstChars[menuId][i]) {
        return i;
      }
    }
    return -1;
  }

  isPrintableCharacter(str) {
    return str.length === 1 && str.match(/\S/);
  }

  getIdFromAriaLabel(node) {
    var id = node.getAttribute('aria-label');
    if (id) {
      id = id.trim().toLowerCase().replace(' ', '-').replace('/', '-');
    }
    return id;
  }

  getMenuOrientation(node) {
    var orientation = node.getAttribute('aria-orientation');

    if (!orientation) {
      var role = node.getAttribute('role');

      switch (role) {
        case 'menubar':
          orientation = 'horizontal';
          break;

        case 'menu':
          orientation = 'vertical';
          break;

        default:
          break;
      }
    }

    return orientation;
  }

  getMenuId(node) {
    var id = false;
    var role = node.getAttribute('role');

    while (node && role !== 'menu' && role !== 'menubar') {
      node = node.parentNode;
      if (node) {
        role = node.getAttribute('role');
      }
    }

    if (node) {
      id = role + '-' + this.getIdFromAriaLabel(node);
    }

    return id;
  }

  getMenu(menuitem) {
    var menu = menuitem;
    var role = menuitem.getAttribute('role');

    while (menu && role !== 'menu' && role !== 'menubar') {
      menu = menu.parentNode;
      if (menu) {
        role = menu.getAttribute('role');
      }
    }

    return menu;
  }

  // Popup menu methods

  isAnyPopupOpen() {
    for (var i = 0; i < this.popups.length; i++) {
      if (this.popups[i].getAttribute('aria-expanded') === 'true') {
        return true;
      }
    }
    return false;
  }

  setMenubarDataExpanded(value) {
    this.domNode.setAttribute('data-menubar-item-expanded', value);
  }

  isMenubarDataExpandedTrue() {
    return this.domNode.getAttribute('data-menubar-item-expanded') === 'true';
  }

  openPopup(menuId, menuitem) {
    // set aria-expanded attribute
    var popupMenu = menuitem.nextElementSibling;

    if (popupMenu) {
      var rect = menuitem.getBoundingClientRect();

      // Set CSS properties
      if (this.isPopup[menuId]) {
        popupMenu.parentNode.style.position = 'relative';
        popupMenu.style.display = 'block';
        popupMenu.style.position = 'absolute';
        popupMenu.style.left = rect.width + 10 + 'px';
        popupMenu.style.top = '0px';
        popupMenu.style.zIndex = 100;
      } else {
        popupMenu.style.display = 'block';
        popupMenu.style.position = 'absolute';
        popupMenu.style.left = '0px';
        popupMenu.style.top = rect.height + 8 + 'px';
        popupMenu.style.zIndex = 100;
      }

      menuitem.setAttribute('aria-expanded', 'true');
      this.setMenubarDataExpanded('true');
      return this.getMenuId(popupMenu);
    }

    return false;
  }

  closePopout(menuitem) {
    var menu,
      menuId = this.getMenuId(menuitem),
      cmi = menuitem;

    while (this.isPopup[menuId] || this.isPopout[menuId]) {
      menu = this.getMenu(cmi);
      cmi = menu.previousElementSibling;
      menuId = this.getMenuId(cmi);
      menu.style.display = 'none';
    }
    cmi.focus();
    return cmi;
  }

  closePopup(menuitem) {
    var menu,
      menuId = this.getMenuId(menuitem),
      cmi = menuitem;

    if (this.isMenubar(menuId)) {
      if (this.isOpen(menuitem)) {
        menuitem.setAttribute('aria-expanded', 'false');
        menuitem.nextElementSibling.style.display = 'none';
      }
    } else {
      menu = this.getMenu(menuitem);
      cmi = menu.previousElementSibling;
      cmi.setAttribute('aria-expanded', 'false');
      cmi.focus();
      menu.style.display = 'none';
    }

    return cmi;
  }

  doesNotContain(popup, menuitem) {
    if (menuitem) {
      return !popup.nextElementSibling.contains(menuitem);
    }
    return true;
  }

  closePopupAll(menuitem) {
    if (typeof menuitem !== 'object') {
      menuitem = false;
    }
    for (var i = 0; i < this.popups.length; i++) {
      var popup = this.popups[i];
      if (this.doesNotContain(popup, menuitem) && this.isOpen(popup)) {
        var cmi = popup.nextElementSibling;
        if (cmi) {
          popup.setAttribute('aria-expanded', 'false');
          cmi.style.display = 'none';
        }
      }
    }
  }

  hasPopup(menuitem) {
    return menuitem.getAttribute('aria-haspopup') === 'true';
  }

  isOpen(menuitem) {
    return menuitem.getAttribute('aria-expanded') === 'true';
  }

  isMenubar(menuId) {
    return !this.isPopup[menuId] && !this.isPopout[menuId];
  }

  isMenuHorizontal(menuitem) {
    return this.menuOrientation[menuitem] === 'horizontal';
  }

  hasFocus() {
    return this.domNode.classList.contains('focus');
  }

  // Menu event handlers

  onMenubarFocusin() {
    // if the menubar or any of its menus has focus, add styling hook for hover
    this.domNode.classList.add('focus');
  }

  onMenubarFocusout() {
    // remove styling hook for hover on menubar item
    this.domNode.classList.remove('focus');
  }

  onKeydown(event) {
    var tgt = event.currentTarget,
      key = event.key,
      flag = false,
      menuId = this.getMenuId(tgt),
      id,
      popupMenuId,
      mi;

    switch (key) {
      case ' ':
      case 'Enter':
        if (this.hasPopup(tgt)) {
          this.openPopups = true;
          popupMenuId = this.openPopup(menuId, tgt);
          this.setFocusToFirstMenuitem(popupMenuId);
        } else {
          if (tgt.href !== '#') {
            this.closePopupAll();
            this.updateContent(tgt.href, tgt.textContent.trim());
            this.setMenubarDataExpanded('false');
          }
        }
        flag = true;
        break;

      case 'Esc':
      case 'Escape':
        this.openPopups = false;
        mi = this.closePopup(tgt);
        id = this.getMenuId(mi);
        this.setMenubarDataExpanded('false');
        flag = true;
        break;

      case 'Up':
      case 'ArrowUp':
        if (this.isMenuHorizontal(menuId)) {
          if (this.hasPopup(tgt)) {
            this.openPopups = true;
            popupMenuId = this.openPopup(menuId, tgt);
            this.setFocusToLastMenuitem(popupMenuId);
          }
        } else {
          this.setFocusToPreviousMenuitem(menuId, tgt);
        }
        flag = true;
        break;

      case 'ArrowDown':
      case 'Down':
        if (this.isMenuHorizontal(menuId)) {
          if (this.hasPopup(tgt)) {
            this.openPopups = true;
            popupMenuId = this.openPopup(menuId, tgt);
            this.setFocusToFirstMenuitem(popupMenuId);
          }
        } else {
          this.setFocusToNextMenuitem(menuId, tgt);
        }
        flag = true;
        break;

      case 'Left':
      case 'ArrowLeft':
        if (this.isMenuHorizontal(menuId)) {
          mi = this.setFocusToPreviousMenuitem(menuId, tgt);
          if (this.isAnyPopupOpen() || this.isMenubarDataExpandedTrue()) {
            this.openPopup(menuId, mi);
          }
        } else {
          if (this.isPopout[menuId]) {
            mi = this.closePopup(tgt);
            id = this.getMenuId(mi);
            mi = this.setFocusToMenuitem(id, mi);
          } else {
            mi = this.closePopup(tgt);
            id = this.getMenuId(mi);
            mi = this.setFocusToPreviousMenuitem(id, mi);
            this.openPopup(id, mi);
          }
        }
        flag = true;
        break;

      case 'Right':
      case 'ArrowRight':
        if (this.isMenuHorizontal(menuId)) {
          mi = this.setFocusToNextMenuitem(menuId, tgt);
          if (this.isAnyPopupOpen() || this.isMenubarDataExpandedTrue()) {
            this.openPopup(menuId, mi);
          }
        } else {
          if (this.hasPopup(tgt)) {
            popupMenuId = this.openPopup(menuId, tgt);
            this.setFocusToFirstMenuitem(popupMenuId);
          } else {
            mi = this.closePopout(tgt);
            id = this.getMenuId(mi);
            mi = this.setFocusToNextMenuitem(id, mi);
            this.openPopup(id, mi);
          }
        }
        flag = true;
        break;

      case 'Home':
      case 'PageUp':
        this.setFocusToFirstMenuitem(menuId, tgt);
        flag = true;
        break;

      case 'End':
      case 'PageDown':
        this.setFocusToLastMenuitem(menuId, tgt);
        flag = true;
        break;

      case 'Tab':
        this.openPopups = false;
        this.setMenubarDataExpanded('false');
        this.closePopup(tgt);
        break;

      default:
        if (this.isPrintableCharacter(key)) {
          this.setFocusByFirstCharacter(menuId, tgt, key);
          flag = true;
        }
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onMenuitemClick(event) {
    var tgt = event.currentTarget;
    var menuId = this.getMenuId(tgt);

    if (this.hasPopup(tgt)) {
      if (this.isOpen(tgt)) {
        this.closePopup(tgt);
      } else {
        this.closePopupAll(tgt);
        this.openPopup(menuId, tgt);
      }
    } else {
      this.updateContent(tgt.href, tgt.textContent.trim());
      this.closePopupAll();
    }
    event.stopPropagation();
    event.preventDefault();
  }

  onMenuitemPointerover(event) {
    var tgt = event.currentTarget;
    var menuId = this.getMenuId(tgt);

    if (this.hasFocus()) {
      this.setFocusToMenuitem(menuId, tgt);
    }

    if (this.isAnyPopupOpen() || this.hasFocus()) {
      this.closePopupAll(tgt);
      if (this.hasPopup(tgt)) {
        this.openPopup(menuId, tgt);
      }
    }
  }

  onBackgroundPointerdown(event) {
    if (!this.domNode.contains(event.target)) {
      this.closePopupAll();
    }
  }
}

// Initialize menubar editor

window.addEventListener('load', function () {
  var menubarNavs = document.querySelectorAll('.menubar-navigation');
  for (var i = 0; i < menubarNavs.length; i++) {
    new MenubarNavigation(menubarNavs[i]);
  }
});
