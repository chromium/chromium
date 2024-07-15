'use strict';
// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var global = {argumentsReceived: false, params: null, picker: null};

const DELAYED_LAYOUT_THRESHOLD = 1000;

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

/**
 * @param {!Element} parent
 * @param {!Array} optionBounds
 */
function buildOptionBoundsArray(parent, optionBounds) {
  // The optionBounds.length check prevents us from doing so many
  // getBoundingClientRect() calls that the picker hangs for 10+ seconds.
  for (let i = 0; i < parent.children.length &&
       optionBounds.length < DELAYED_LAYOUT_THRESHOLD;
       i++) {
    const child = parent.children[i];
    if (child.tagName === 'OPTION') {
      optionBounds[child.index] = child.getBoundingClientRect();
    } else if (child.tagName === 'OPTGROUP') {
      buildOptionBoundsArray(child, optionBounds)
    }
  }
}

class ListPicker extends Picker {
  /**
   * @param {!Element} element
   * @param {!Object} config
   */
  constructor(element, config) {
    super(element, config);
    this.selectElement_ = createElement('select');
    this.selectElement_.size = 20;
    this.element_.appendChild(this.selectElement_);
    this.delayedChildrenConfig_ = null;
    this.delayedChildrenConfigIndex = 0;
    this.layout_();
    this.selectElement_.addEventListener(
        'mouseup', this.handleMouseUp_.bind(this));
    this.selectElement_.addEventListener(
        'touchstart', this.handleTouchStart_.bind(this));
    this.selectElement_.addEventListener(
        'keydown', this.handleKeyDown_.bind(this));
    this.selectElement_.addEventListener(
        'change', this.handleChange_.bind(this));
    window.addEventListener('message', this.handleWindowMessage_.bind(this));
    window.addEventListener(
        'mousemove', this.handleWindowMouseMove_.bind(this));
    window.addEventListener(
        'mouseover', this.handleWindowMouseOver_.bind(this));
    this.handleWindowTouchMoveBound_ = this.handleWindowTouchMove_.bind(this);
    this.handleWindowTouchEndBound_ = this.handleWindowTouchEnd_.bind(this);
    this.handleTouchSelectModeScrollBound_ =
        this.handleTouchSelectModeScroll_.bind(this);
    this.lastMousePositionX_ = Infinity;
    this.lastMousePositionY_ = Infinity;
    this.selectionSetByMouseHover_ = false;

    this.trackingTouchId_ = null;

    this.handleWindowDidHide_();
    this.selectElement_.focus();
    this.selectElement_.value = this.config_.selectedIndex;
  }

  handleWindowDidHide_() {
    this.fixWindowSize_();
    const selectedOption =
        this.selectElement_.options[this.selectElement_.selectedIndex];
    if (selectedOption)
      selectedOption.scrollIntoView(false);
    window.removeEventListener('didHide', this.handleWindowDidHideBound_);
  }

  handleWindowMessage_(event) {
    eval(event.data);
    if (window.updateData.type === 'update') {
      this.config_.baseStyle = window.updateData.baseStyle;
      this.config_.children = window.updateData.children;
      const prevChildrenCount = this.selectElement_.children.length;
      this.update_();
      if (this.config_.anchorRectInScreen.x !==
              window.updateData.anchorRectInScreen.x ||
          this.config_.anchorRectInScreen.y !==
              window.updateData.anchorRectInScreen.y ||
          this.config_.anchorRectInScreen.width !==
              window.updateData.anchorRectInScreen.width ||
          this.config_.anchorRectInScreen.height !==
              window.updateData.anchorRectInScreen.height ||
          prevChildrenCount !== window.updateData.children.length) {
        this.config_.anchorRectInScreen = window.updateData.anchorRectInScreen;
        this.fixWindowSize_();
      }
    }
    delete window.updateData;
  }

  // This should be matched to the border width of the internal listbox
  // SELECT. See list_picker.css and html.css.
  static LISTBOX_SELECT_BORDER = 1;

  handleWindowMouseMove_(event) {
    const visibleTop = ListPicker.LISTBOX_SELECT_BORDER;
    const visibleBottom =
        this.selectElement_.offsetHeight - ListPicker.LISTBOX_SELECT_BORDER;
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
    this.lastMousePositionX_ = event.clientX;
    this.lastMousePositionY_ = event.clientY;
    this.selectionSetByMouseHover_ = true;
    // Prevent the select element from firing change events for mouse input.
    event.preventDefault();
  }

  handleWindowMouseOver_(event) {
    this.highlightOption_(event.target);
  }

  handleMouseUp_(event) {
    if (event.target.tagName !== 'OPTION')
      return;
    window.pagePopupController.setValueAndClosePopup(
        0, this.selectElement_.value);
  }

  handleTouchStart_(event) {
    if (this.trackingTouchId_ !== null)
      return;
    // Enter touch select mode. In touch select mode the highlight follows the
    // finger and on touchend the highlighted item is selected.
    const touch = event.touches[0];
    this.trackingTouchId_ = touch.identifier;
    this.highlightOption_(touch.target);
    this.selectionSetByMouseHover_ = false;
    this.selectElement_.addEventListener(
        'scroll', this.handleTouchSelectModeScrollBound_);
    window.addEventListener('touchmove', this.handleWindowTouchMoveBound_);
    window.addEventListener('touchend', this.handleWindowTouchEndBound_);
  }

  handleTouchSelectModeScroll_(event) {
    this.exitTouchSelectMode_();
  }

  exitTouchSelectMode_(event) {
    this.trackingTouchId_ = null;
    this.selectElement_.removeEventListener(
        'scroll', this.handleTouchSelectModeScrollBound_);
    window.removeEventListener('touchmove', this.handleWindowTouchMoveBound_);
    window.removeEventListener('touchend', this.handleWindowTouchEndBound_);
  }

  handleWindowTouchMove_(event) {
    if (this.trackingTouchId_ === null)
      return;
    const touch = this.getTouchForId_(event.touches, this.trackingTouchId_);
    if (!touch)
      return;
    this.highlightOption_(
        document.elementFromPoint(touch.clientX, touch.clientY));
    this.selectionSetByMouseHover_ = false;
  }

  handleWindowTouchEnd_(event) {
    if (this.trackingTouchId_ === null)
      return;
    const touch =
        this.getTouchForId_(event.changedTouches, this.trackingTouchId_);
    if (!touch)
      return;
    const target = document.elementFromPoint(touch.clientX, touch.clientY);
    if (target.tagName === 'OPTION' && !target.disabled)
      window.pagePopupController.setValueAndClosePopup(
          0, this.selectElement_.value);
    this.exitTouchSelectMode_();
  }

  getTouchForId_(touchList, id) {
    for (let i = 0; i < touchList.length; i++) {
      if (touchList[i].identifier === id)
        return touchList[i];
    }
    return null;
  }

  highlightOption_(target) {
    if (target.tagName !== 'OPTION' || target.selected || target.disabled)
      return;
    const savedScrollTop = this.selectElement_.scrollTop;
    // TODO(tkent): Updating HTMLOptionElement::selected is not efficient. We
    // should optimize it, or use an alternative way.
    target.selected = true;
    this.selectElement_.scrollTop = savedScrollTop;
  }

  handleChange_(event) {
    window.pagePopupController.setValue(this.selectElement_.value);
    this.selectionSetByMouseHover_ = false;
  }

  handleKeyDown_(event) {
    const key = event.key;
    if (key === 'Escape') {
      window.pagePopupController.closePopup();
      event.preventDefault();
    } else if (key === 'Tab' || key === 'Enter') {
      window.pagePopupController.setValueAndClosePopup(
          0, this.selectElement_.value);
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

  fixWindowSize_() {
    this.selectElement_.style.height = '';
    const scale = this.config_.scaleFactor;
    const maxHeight = this.selectElement_.offsetHeight;
    const noScrollHeight =
        (this.calculateScrollHeight_() + ListPicker.LISTBOX_SELECT_BORDER * 2);
    const scrollbarWidth = getScrollbarWidth();
    const elementOffsetWidth = this.selectElement_.offsetWidth;
    let desiredWindowHeight = noScrollHeight;
    let desiredWindowWidth = elementOffsetWidth;
    // If we already have a vertical scrollbar, subtract it out, it will get
    // re-added below.
    if (this.selectElement_.scrollHeight > this.selectElement_.clientHeight)
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
        this.config_.anchorRectInScreen.width * scale, desiredWindowWidth);
    let windowRect = adjustWindowRect(
        desiredWindowWidth / scale, desiredWindowHeight / scale,
        elementOffsetWidth / scale, 0, /*allowOverlapWithAnchor=*/ false);
    // If the available screen space is smaller than maxHeight, we will get
    // an unexpected scrollbar.
    if (!expectingScrollbar && windowRect.height < noScrollHeight / scale) {
      desiredWindowWidth = windowRect.width * scale + scrollbarWidth;
      windowRect = adjustWindowRect(
          desiredWindowWidth / scale, windowRect.height, windowRect.width,
          windowRect.height);
    }
    this.selectElement_.style.width = (windowRect.width * scale) + 'px';
    this.selectElement_.style.height = (windowRect.height * scale) + 'px';
    this.element_.style.height = (windowRect.height * scale) + 'px';
    setWindowRect(windowRect);
  }

  calculateScrollHeight_() {
    // Element.scrollHeight returns an integer value but this calculate the
    // actual fractional value.
    // TODO(tkent): This can be too large? crbug.com/579863
    let top = Infinity;
    let bottom = -Infinity;
    for (let i = 0; i < this.selectElement_.children.length; i++) {
      const rect = this.selectElement_.children[i].getBoundingClientRect();
      // Skip hidden elements.
      if (rect.width === 0 && rect.height === 0)
        continue;
      top = Math.min(top, rect.top);
      bottom = Math.max(bottom, rect.bottom);
    }
    return Math.max(bottom - top, 0);
  }

  listItemCount_() {
    return this.selectElement_.querySelectorAll('option,optgroup,hr').length;
  }

  layout_() {
    if (this.config_.isRTL)
      this.element_.classList.add('rtl');
    this.selectElement_.style.backgroundColor =
        this.config_.baseStyle.backgroundColor;
    this.selectElement_.style.color = this.config_.baseStyle.color;
    this.selectElement_.style.textTransform =
        this.config_.baseStyle.textTransform;
    this.selectElement_.style.fontSize = this.config_.baseStyle.fontSize + 'px';
    this.selectElement_.style.fontFamily = this.config_.baseStyle.fontFamily;
    this.selectElement_.style.fontStyle = this.config_.baseStyle.fontStyle;
    this.selectElement_.style.fontVariant = this.config_.baseStyle.fontVariant;
    if (this.config_.baseStyle.textAlign)
      this.selectElement_.style.textAlign = this.config_.baseStyle.textAlign;
    this.updateChildren_(this.selectElement_, this.config_);
    this.setMenuListOptionsBoundsInAXTree_();
  }

  update_() {
    const scrollPosition = this.selectElement_.scrollTop;
    const oldValue = this.selectElement_.value;
    this.layout_();
    this.selectElement_.value = this.config_.selectedIndex;
    this.selectElement_.scrollTop = scrollPosition;
    let optionUnderMouse = null;
    if (this.selectionSetByMouseHover_) {
      const elementUnderMouse = document.elementFromPoint(
          this.lastMousePositionX_, this.lastMousePositionY_);
      optionUnderMouse =
          elementUnderMouse && elementUnderMouse.closest('option');
    }
    if (optionUnderMouse)
      optionUnderMouse.selected = true;
    else
      this.selectElement_.value = oldValue;
    this.selectElement_.scrollTop = scrollPosition;
    this.dispatchEvent('didUpdate');
  }

  /**
   * @param {!Element} parent Select element or optgroup element.
   * @param {!Object} config
   */
  updateChildren_(parent, config) {
    let outOfDateIndex = 0;
    let fragment = null;
    const inGroup = parent.tagName === 'OPTGROUP';
    let lastListIndex = -1;
    const limit = Math.max(
        this.config_.selectedIndex, ListPicker.DELAYED_LAYOUT_THRESHOLD);
    let i;
    for (i = 0; i < config.children.length; ++i) {
      if (!inGroup && lastListIndex >= limit)
        break;
      const childConfig = config.children[i];
      const item =
          this.findReusableItem_(parent, childConfig, outOfDateIndex) ||
          this.createItemElement_(childConfig);
      this.configureItem_(item, childConfig, inGroup);
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
      // We don't bind |config.children| and |i| to updateChildrenLater_
      // because config.children can get invalid before updateChildrenLater_
      // is called.
      this.delayedChildrenConfig_ = config.children;
      this.delayedChildrenConfigIndex = i;
      // Needs some amount of delay to kick the first paint.
      setTimeout(this.updateChildrenLater_.bind(this), 100);
    }
  }

  updateChildrenLater_(timeStamp) {
    if (!this.delayedChildrenConfig_)
      return;
    const fragment = document.createDocumentFragment();
    const startIndex = this.delayedChildrenConfigIndex;
    for (; this.delayedChildrenConfigIndex < this.delayedChildrenConfig_.length;
         ++this.delayedChildrenConfigIndex) {
      const childConfig =
          this.delayedChildrenConfig_[this.delayedChildrenConfigIndex];
      const item = this.createItemElement_(childConfig);
      this.configureItem_(item, childConfig, false);
      fragment.appendChild(item);
    }
    this.selectElement_.appendChild(fragment);
    this.selectElement_.classList.add('wrap');
    this.delayedChildrenConfig_ = null;
    this.setMenuListOptionsBoundsInAXTree_(true);
  }

  findReusableItem_(parent, config, startIndex) {
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

  createItemElement_(config) {
    let element;
    if (!config.type || config.type === 'option')
      element = createElement('option');
    else if (config.type === 'optgroup')
      element = createElement('optgroup');
    else if (config.type === 'separator')
      element = createElement('hr');
    return element;
  }

  applyItemStyle_(element, styleConfig) {
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

  configureItem_(element, config, inGroup) {
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
      element.style.paddingInlineStart = this.config_.paddingStart + 'px';
      if (inGroup) {
        const extraPaddingForOptionInsideOptgroup = 20;
        element.style.paddingInlineStart = Number(this.config_.paddingStart) +
            extraPaddingForOptionInsideOptgroup + 'px';
        element.style.marginInlineStart = (-this.config_.paddingStart) + 'px';
        // Should be synchronized with padding-end in list_picker.css.
        element.style.marginInlineEnd = '-2px';
      }
    } else if (config.type === 'optgroup') {
      element.label = config.label;
      element.title = config.title;
      element.disabled = config.disabled;
      element.setAttribute('aria-label', config.ariaLabel);
      this.updateChildren_(element, config);
      element.style.paddingInlineStart = this.config_.paddingStart + 'px';
    } else if (config.type === 'separator') {
      element.title = config.title;
      element.disabled = config.disabled;
      element.setAttribute('aria-label', config.ariaLabel);
      if (inGroup) {
        element.style.marginInlineStart = (-this.config_.paddingStart) + 'px';
        // Should be synchronized with padding-end in list_picker.css.
        element.style.marginInlineEnd = '-2px';
      }
    }
    this.applyItemStyle_(element, config.style);
  }

  setMenuListOptionsBoundsInAXTree_(childrenUpdated = false) {
    var optionBounds = [];
    buildOptionBoundsArray(this.selectElement_, optionBounds);
    window.pagePopupController.setMenuListOptionsBoundsInAXTree(
        optionBounds, childrenUpdated);
  }
}

if (window.dialogArguments) {
  initialize(dialogArguments);
} else {
  window.addEventListener('message', handleMessage);
  window.setTimeout(handleArgumentsTimeout, 1000);
}
