'use strict';
// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = {argumentsReceived: false, params: null, picker: null};

/**
 * @param {Event} event
 */
function handleMessage(event) {
  window.removeEventListener('message', handleMessage);
  initialize(JSON.parse(event.data));
  global.argumentsReceived = true;
}

/**
 * @param {!Object} args
 */
function initialize(args) {
  global.params = args;
  const main = $('main');
  main.innerHTML = '';
  global.picker = new ListPicker(main, args);
}

function handleArgumentsTimeout() {
  if (global.argumentsReceived)
    return;
  initialize({});
}

class ListPicker extends Picker {
  /**
   * @param {!Element} element
   * @param {!Object} config
   */
  constructor(element, config) {
    super(element, config);
    this._selectElement = createElement('select');
    this._selectElement.size = 20;
    this._element.appendChild(this._selectElement);
    this._delayedChildrenConfig = null;
    this._delayedChildrenConfigIndex = 0;
    this._layout();
    this._selectElement.addEventListener(
        'mouseup', this._handleMouseUp.bind(this));
    this._selectElement.addEventListener(
        'touchstart', this._handleTouchStart.bind(this));
    this._selectElement.addEventListener(
        'keydown', this._handleKeyDown.bind(this));
    this._selectElement.addEventListener(
        'change', this._handleChange.bind(this));
    window.addEventListener('message', this._handleWindowMessage.bind(this));
    window.addEventListener(
        'mousemove', this._handleWindowMouseMove.bind(this));
    window.addEventListener(
        'mouseover', this._handleWindowMouseOver.bind(this));
    this._handleWindowTouchMoveBound = this._handleWindowTouchMove.bind(this);
    this._handleWindowTouchEndBound = this._handleWindowTouchEnd.bind(this);
    this._handleTouchSelectModeScrollBound =
        this._handleTouchSelectModeScroll.bind(this);
    this.lastMousePositionX = Infinity;
    this.lastMousePositionY = Infinity;
    this._selectionSetByMouseHover = false;

    this._trackingTouchId = null;

    this._handleWindowDidHide();
    this._selectElement.focus();
    this._selectElement.value = this._config.selectedIndex;
  }

  _handleWindowDidHide() {
    this._fixWindowSize();
    const selectedOption =
        this._selectElement.options[this._selectElement.selectedIndex];
    if (selectedOption)
      selectedOption.scrollIntoView(false);
    window.removeEventListener('didHide', this._handleWindowDidHideBound);
  }

  _handleWindowMessage(event) {
    eval(event.data);
    if (window.updateData.type === 'update') {
      this._config.baseStyle = window.updateData.baseStyle;
      this._config.children = window.updateData.children;
      const prev_children_count = this._selectElement.children.length;
      this._update();
      if (this._config.anchorRectInScreen.x !==
              window.updateData.anchorRectInScreen.x ||
          this._config.anchorRectInScreen.y !==
              window.updateData.anchorRectInScreen.y ||
          this._config.anchorRectInScreen.width !==
              window.updateData.anchorRectInScreen.width ||
          this._config.anchorRectInScreen.height !==
              window.updateData.anchorRectInScreen.height ||
          prev_children_count !== window.updateData.children.length) {
        this._config.anchorRectInScreen = window.updateData.anchorRectInScreen;
        this._fixWindowSize();
      }
    }
    delete window.updateData;
  }

  // This should be matched to the border width of the internal listbox
  // SELECT. See list_picker.css and html.css.
  static ListboxSelectBorder = 1;

  _handleWindowMouseMove(event) {
    const visibleTop = ListPicker.ListboxSelectBorder;
    const visibleBottom =
        this._selectElement.offsetHeight - ListPicker.ListboxSelectBorder;
    const optionBounds = event.target.getBoundingClientRect();
    if (optionBounds.height >= 1.0) {
      // If the height of the visible part of event.target is less than 1px,
      // ignore this event because it may be an error by sub-pixel layout.
      if (optionBounds.top < visibleTop) {
        if (optionBounds.bottom - visibleTop < 1.0)
          return;
      } else if (optionBounds.bottom > visibleBottom) {
        if (visibleBottom - optionBounds.top < 1.0)
          return;
      }
    }
    this.lastMousePositionX = event.clientX;
    this.lastMousePositionY = event.clientY;
    this._selectionSetByMouseHover = true;
    // Prevent the select element from firing change events for mouse input.
    event.preventDefault();
  }

  _handleWindowMouseOver(event) {
    this._highlightOption(event.target);
  }

  _handleMouseUp(event) {
    if (event.target.tagName !== 'OPTION')
      return;
    window.pagePopupController.setValueAndClosePopup(
        0, this._selectElement.value);
  }

  _handleTouchStart(event) {
    if (this._trackingTouchId !== null)
      return;
    // Enter touch select mode. In touch select mode the highlight follows the
    // finger and on touchend the highlighted item is selected.
    const touch = event.touches[0];
    this._trackingTouchId = touch.identifier;
    this._highlightOption(touch.target);
    this._selectionSetByMouseHover = false;
    this._selectElement.addEventListener(
        'scroll', this._handleTouchSelectModeScrollBound);
    window.addEventListener('touchmove', this._handleWindowTouchMoveBound);
    window.addEventListener('touchend', this._handleWindowTouchEndBound);
  }

  _handleTouchSelectModeScroll(event) {
    this._exitTouchSelectMode();
  }

  _exitTouchSelectMode(event) {
    this._trackingTouchId = null;
    this._selectElement.removeEventListener(
        'scroll', this._handleTouchSelectModeScrollBound);
    window.removeEventListener('touchmove', this._handleWindowTouchMoveBound);
    window.removeEventListener('touchend', this._handleWindowTouchEndBound);
  }

  _handleWindowTouchMove(event) {
    if (this._trackingTouchId === null)
      return;
    const touch = this._getTouchForId(event.touches, this._trackingTouchId);
    if (!touch)
      return;
    this._highlightOption(
        document.elementFromPoint(touch.clientX, touch.clientY));
    this._selectionSetByMouseHover = false;
  }

  _handleWindowTouchEnd(event) {
    if (this._trackingTouchId === null)
      return;
    const touch =
        this._getTouchForId(event.changedTouches, this._trackingTouchId);
    if (!touch)
      return;
    const target = document.elementFromPoint(touch.clientX, touch.clientY);
    if (target.tagName === 'OPTION' && !target.disabled)
      window.pagePopupController.setValueAndClosePopup(
          0, this._selectElement.value);
    this._exitTouchSelectMode();
  }

  _getTouchForId(touchList, id) {
    for (let i = 0; i < touchList.length; i++) {
      if (touchList[i].identifier === id)
        return touchList[i];
    }
    return null;
  }

  _highlightOption(target) {
    if (target.tagName !== 'OPTION' || target.selected || target.disabled)
      return;
    const savedScrollTop = this._selectElement.scrollTop;
    // TODO(tkent): Updating HTMLOptionElement::selected is not efficient. We
    // should optimize it, or use an alternative way.
    target.selected = true;
    this._selectElement.scrollTop = savedScrollTop;
  }

  _handleChange(event) {
    window.pagePopupController.setValue(this._selectElement.value);
    this._selectionSetByMouseHover = false;
  }

  _handleKeyDown(event) {
    const key = event.key;
    if (key === 'Escape') {
      window.pagePopupController.closePopup();
      event.preventDefault();
    } else if (key === 'Tab' || key === 'Enter') {
      window.pagePopupController.setValueAndClosePopup(
          0, this._selectElement.value);
      event.preventDefault();
    } else if (event.altKey && (key === 'ArrowDown' || key === 'ArrowUp')) {
      // We need to add a delay here because, if we do it immediately the key
      // press event will be handled by HTMLSelectElement and this popup will
      // be reopened.
      setTimeout(function() {
        window.pagePopupController.closePopup();
      }, 0);
      event.preventDefault();
    }
  }

  _fixWindowSize() {
    this._selectElement.style.height = '';
    const scale = this._config.scaleFactor;
    const maxHeight = this._selectElement.offsetHeight;
    const noScrollHeight =
        (this._calculateScrollHeight() + ListPicker.ListboxSelectBorder * 2);
    const scrollbarWidth = getScrollbarWidth();
    const elementOffsetWidth = this._selectElement.offsetWidth;
    let desiredWindowHeight = noScrollHeight;
    let desiredWindowWidth = elementOffsetWidth;
    // If we already have a vertical scrollbar, subtract it out, it will get
    // re-added below.
    if (this._selectElement.scrollHeight > this._selectElement.clientHeight)
      desiredWindowWidth -= scrollbarWidth;
    let expectingScrollbar = false;
    if (desiredWindowHeight > maxHeight) {
      desiredWindowHeight = maxHeight;
      // Setting overflow to auto does not increase width for the scrollbar
      // so we need to do it manually.
      desiredWindowWidth += scrollbarWidth;
      expectingScrollbar = true;
    }
    // Screen coordinate for anchorRectInScreen and windowRect is DIP.
    desiredWindowWidth = Math.max(
        this._config.anchorRectInScreen.width * scale, desiredWindowWidth);
    let windowRect = adjustWindowRect(
        desiredWindowWidth / scale, desiredWindowHeight / scale,
        elementOffsetWidth / scale, 0);
    // If the available screen space is smaller than maxHeight, we will get
    // an unexpected scrollbar.
    if (!expectingScrollbar && windowRect.height < noScrollHeight / scale) {
      desiredWindowWidth = windowRect.width * scale + scrollbarWidth;
      windowRect = adjustWindowRect(
          desiredWindowWidth / scale, windowRect.height, windowRect.width,
          windowRect.height);
    }
    this._selectElement.style.width = (windowRect.width * scale) + 'px';
    this._selectElement.style.height = (windowRect.height * scale) + 'px';
    this._element.style.height = (windowRect.height * scale) + 'px';
    setWindowRect(windowRect);
  }

  _calculateScrollHeight() {
    // Element.scrollHeight returns an integer value but this calculate the
    // actual fractional value.
    // TODO(tkent): This can be too large? crbug.com/579863
    let top = Infinity;
    let bottom = -Infinity;
    for (let i = 0; i < this._selectElement.children.length; i++) {
      const rect = this._selectElement.children[i].getBoundingClientRect();
      // Skip hidden elements.
      if (rect.width === 0 && rect.height === 0)
        continue;
      top = Math.min(top, rect.top);
      bottom = Math.max(bottom, rect.bottom);
    }
    return Math.max(bottom - top, 0);
  }

  _listItemCount() {
    return this._selectElement.querySelectorAll('option,optgroup,hr').length;
  }

  _layout() {
    if (this._config.isRTL)
      this._element.classList.add('rtl');
    this._selectElement.style.backgroundColor =
        this._config.baseStyle.backgroundColor;
    this._selectElement.style.color = this._config.baseStyle.color;
    this._selectElement.style.textTransform =
        this._config.baseStyle.textTransform;
    this._selectElement.style.fontSize = this._config.baseStyle.fontSize + 'px';
    this._selectElement.style.fontFamily = this._config.baseStyle.fontFamily;
    this._selectElement.style.fontStyle = this._config.baseStyle.fontStyle;
    this._selectElement.style.fontVariant = this._config.baseStyle.fontVariant;
    if (this._config.baseStyle.textAlign)
      this._selectElement.style.textAlign = this._config.baseStyle.textAlign;
    this._updateChildren(this._selectElement, this._config);
  }

  _update() {
    const scrollPosition = this._selectElement.scrollTop;
    const oldValue = this._selectElement.value;
    this._layout();
    this._selectElement.value = this._config.selectedIndex;
    this._selectElement.scrollTop = scrollPosition;
    let optionUnderMouse = null;
    if (this._selectionSetByMouseHover) {
      const elementUnderMouse = document.elementFromPoint(
          this.lastMousePositionX, this.lastMousePositionY);
      optionUnderMouse =
          elementUnderMouse && elementUnderMouse.closest('option');
    }
    if (optionUnderMouse)
      optionUnderMouse.selected = true;
    else
      this._selectElement.value = oldValue;
    this._selectElement.scrollTop = scrollPosition;
    this.dispatchEvent('didUpdate');
  }

  static DelayedLayoutThreshold = 1000;

  /**
   * @param {!Element} parent Select element or optgroup element.
   * @param {!Object} config
   */
  _updateChildren(parent, config) {
    let outOfDateIndex = 0;
    let fragment = null;
    const inGroup = parent.tagName === 'OPTGROUP';
    let lastListIndex = -1;
    const limit =
        Math.max(this._config.selectedIndex, ListPicker.DelayedLayoutThreshold);
    let i;
    for (i = 0; i < config.children.length; ++i) {
      if (!inGroup && lastListIndex >= limit)
        break;
      const childConfig = config.children[i];
      const item =
          this._findReusableItem(parent, childConfig, outOfDateIndex) ||
          this._createItemElement(childConfig);
      this._configureItem(item, childConfig, inGroup);
      lastListIndex = item.value ? Number(item.value) : -1;
      if (outOfDateIndex < parent.children.length) {
        parent.insertBefore(item, parent.children[outOfDateIndex]);
      } else {
        if (!fragment)
          fragment = document.createDocumentFragment();
        fragment.appendChild(item);
      }
      outOfDateIndex++;
    }
    if (fragment) {
      parent.appendChild(fragment);
    } else {
      const unused = parent.children.length - outOfDateIndex;
      for (let j = 0; j < unused; j++) {
        parent.removeChild(parent.lastElementChild);
      }
    }
    if (i < config.children.length) {
      // We don't bind |config.children| and |i| to _updateChildrenLater
      // because config.children can get invalid before _updateChildrenLater
      // is called.
      this._delayedChildrenConfig = config.children;
      this._delayedChildrenConfigIndex = i;
      // Needs some amount of delay to kick the first paint.
      setTimeout(this._updateChildrenLater.bind(this), 100);
    }
  }

  _updateChildrenLater(timeStamp) {
    if (!this._delayedChildrenConfig)
      return;
    const fragment = document.createDocumentFragment();
    const startIndex = this._delayedChildrenConfigIndex;
    for (;
         this._delayedChildrenConfigIndex < this._delayedChildrenConfig.length;
         ++this._delayedChildrenConfigIndex) {
      const childConfig =
          this._delayedChildrenConfig[this._delayedChildrenConfigIndex];
      const item = this._createItemElement(childConfig);
      this._configureItem(item, childConfig, false);
      fragment.appendChild(item);
    }
    this._selectElement.appendChild(fragment);
    this._selectElement.classList.add('wrap');
    this._delayedChildrenConfig = null;
  }

  _findReusableItem(parent, config, startIndex) {
    if (startIndex >= parent.children.length)
      return null;
    let tagName = 'OPTION';
    if (config.type === 'optgroup')
      tagName = 'OPTGROUP';
    else if (config.type === 'separator')
      tagName = 'HR';
    for (let i = startIndex; i < parent.children.length; i++) {
      const child = parent.children[i];
      if (tagName === child.tagName) {
        return child;
      }
    }
    return null;
  }

  _createItemElement(config) {
    let element;
    if (!config.type || config.type === 'option')
      element = createElement('option');
    else if (config.type === 'optgroup')
      element = createElement('optgroup');
    else if (config.type === 'separator')
      element = createElement('hr');
    return element;
  }

  _applyItemStyle(element, styleConfig) {
    if (!styleConfig)
      return;
    const style = element.style;
    style.visibility = styleConfig.visibility ? styleConfig.visibility : '';
    style.display = styleConfig.display ? styleConfig.display : '';
    style.direction = styleConfig.direction ? styleConfig.direction : '';
    style.unicodeBidi = styleConfig.unicodeBidi ? styleConfig.unicodeBidi : '';
    style.color = styleConfig.color ? styleConfig.color : '';
    style.backgroundColor =
        styleConfig.backgroundColor ? styleConfig.backgroundColor : '';
    style.colorScheme = styleConfig.colorScheme ? styleConfig.colorScheme : '';
    style.fontSize =
        styleConfig.fontSize !== undefined ? styleConfig.fontSize + 'px' : '';
    style.fontWeight = styleConfig.fontWeight ? styleConfig.fontWeight : '';
    style.fontFamily = styleConfig.fontFamily ? styleConfig.fontFamily : '';
    style.fontStyle = styleConfig.fontStyle ? styleConfig.fontStyle : '';
    style.fontVariant = styleConfig.fontVariant ? styleConfig.fontVariant : '';
    style.textTransform =
        styleConfig.textTransform ? styleConfig.textTransform : '';
    style.textAlign = styleConfig.textAlign ? styleConfig.textAlign : '';
  }

  _configureItem(element, config, inGroup) {
    if (!config.type || config.type === 'option') {
      element.label = config.label;
      element.value = config.value;
      if (config.title)
        element.title = config.title;
      else
        element.removeAttribute('title');
      element.disabled = !!config.disabled
      if (config.ariaLabel)
      element.setAttribute('aria-label', config.ariaLabel);
      else element.removeAttribute('aria-label');
      element.style.paddingInlineStart = this._config.paddingStart + 'px';
      if (inGroup) {
        element.style.marginInlineStart = (-this._config.paddingStart) + 'px';
        // Should be synchronized with padding-end in list_picker.css.
        element.style.marginInlineEnd = '-2px';
      }
    } else if (config.type === 'optgroup') {
      element.label = config.label;
      element.title = config.title;
      element.disabled = config.disabled;
      element.setAttribute('aria-label', config.ariaLabel);
      this._updateChildren(element, config);
      element.style.paddingInlineStart = this._config.paddingStart + 'px';
    } else if (config.type === 'separator') {
      element.title = config.title;
      element.disabled = config.disabled;
      element.setAttribute('aria-label', config.ariaLabel);
      if (inGroup) {
        element.style.marginInlineStart = (-this._config.paddingStart) + 'px';
        // Should be synchronized with padding-end in list_picker.css.
        element.style.marginInlineEnd = '-2px';
      }
    }
    this._applyItemStyle(element, config.style);
  }
}

if (window.dialogArguments) {
  initialize(dialogArguments);
} else {
  window.addEventListener('message', handleMessage);
  window.setTimeout(handleArgumentsTimeout, 1000);
}
