'use strict';
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

class SuggestionPicker extends Picker {
  /**
   * @param {!Element} element
   * @param {!Object} config
   */
  constructor(element, config) {
    super(element, config);
    this.isFocusByMouse_ = false;
    this.containerElement_ = null;
    this.setColors_();
    this.layout_();
    this.fixWindowSize_();
    this.handleBodyKeyDownBound_ = this.handleBodyKeyDown_.bind(this);
    document.body.addEventListener('keydown', this.handleBodyKeyDownBound_);
    this.element_.addEventListener(
        'mouseout', this.handleMouseOut_.bind(this), false);
  }

  static NUMBER_OF_VISIBLE_ENTRIES = 20;
  // An entry needs to be at least this many pixels visible for it to be a visible entry.
  static VISIBLE_ENTRY_THRESHOLD_HEIGHT = 4;
  static ActionNames = {
    OPEN_CALENDAR_PICKER: 'openCalendarPicker',
  };
  static LIST_ENTRY_CLASS = 'suggestion-list-entry';

  static validateConfig(config) {
    if (config.showOtherDateEntry && !config.otherDateLabel)
      return 'No otherDateLabel.';
    if (config.suggestionHighlightColor && !config.suggestionHighlightColor)
      return 'No suggestionHighlightColor.';
    if (config.suggestionHighlightTextColor &&
        !config.suggestionHighlightTextColor)
      return 'No suggestionHighlightTextColor.';
    if (config.suggestionValues.length !==
        config.localizedSuggestionValues.length)
      return 'localizedSuggestionValues.length must equal suggestionValues.length.';
    if (config.suggestionValues.length !== config.suggestionLabels.length)
      return 'suggestionLabels.length must equal suggestionValues.length.';
    if (typeof config.inputWidth === 'undefined')
      return 'No inputWidth.';
    return null;
  }

  padding_() {
    return Number(
        window.getComputedStyle(document.querySelector('.suggestion-list'))
            .getPropertyValue('padding')
            .replace('px', ''));
  }

  setColors_() {
    let text = '.' + SuggestionPicker.LIST_ENTRY_CLASS + ':focus {\
          background-color: ' +
        this.config_.suggestionHighlightColor + ';\
          color: ' +
        this.config_.suggestionHighlightTextColor + '; }';
    text += '.' + SuggestionPicker.LIST_ENTRY_CLASS +
        ':focus .label { color: ' + this.config_.suggestionHighlightTextColor +
        '; }';
    document.head.appendChild(createElement('style', null, text));
  }

  cleanup() {
    document.body.removeEventListener(
        'keydown', this.handleBodyKeyDownBound_, false);
  }

  /**
   * @param {!string} title
   * @param {!string} label
   * @param {!string} value
   * @return {!Element}
   */
  createSuggestionEntryElement_(title, label, value) {
    const entryElement = createElement('li', SuggestionPicker.LIST_ENTRY_CLASS);
    entryElement.tabIndex = 0;
    entryElement.dataset.value = value;
    const content = createElement('span', 'content');
    entryElement.appendChild(content);
    const titleElement = createElement('span', 'title', title);
    content.appendChild(titleElement);
    if (label) {
      const labelElement = createElement('span', 'label', label);
      content.appendChild(labelElement);
    }
    entryElement.addEventListener(
        'mouseover', this.handleEntryMouseOver_.bind(this), false);
    return entryElement;
  }

  /**
   * @param {!string} title
   * @param {!string} actionName
   * @return {!Element}
   */
  createActionEntryElement_(title, actionName) {
    const entryElement = createElement('li', SuggestionPicker.LIST_ENTRY_CLASS);
    entryElement.tabIndex = 0;
    entryElement.dataset.action = actionName;
    const content = createElement('span', 'content');
    entryElement.appendChild(content);
    const titleElement = createElement('span', 'title', title);
    content.appendChild(titleElement);
    entryElement.addEventListener(
        'mouseover', this.handleEntryMouseOver_.bind(this), false);
    return entryElement;
  }

  /**
   * @return {!number}
   */
  measureMaxContentWidth_() {
    // To measure the required width, we first set the class to "measuring-width" which
    // left aligns all the content including label.
    this.containerElement_.classList.add('measuring-width');
    let maxContentWidth = 0;
    const contentElements =
        this.containerElement_.getElementsByClassName('content');
    for (let i = 0; i < contentElements.length; ++i) {
      maxContentWidth = Math.max(
          maxContentWidth, contentElements[i].getBoundingClientRect().width);
    }
    this.containerElement_.classList.remove('measuring-width');
    return maxContentWidth;
  }

  fixWindowSize_() {
    const ListBorder = 2;
    const ListPadding = 2 * this.padding_();
    const zoom = this.config_.zoomFactor;
    let desiredWindowWidth =
        (this.measureMaxContentWidth_() + ListBorder + ListPadding) * zoom;
    if (typeof this.config_.inputWidth === 'number')
      desiredWindowWidth =
          Math.max(this.config_.inputWidth, desiredWindowWidth);
    let totalHeight = ListBorder + ListPadding;
    let maxHeight = 0;
    let entryCount = 0;
    for (let i = 0; i < this.containerElement_.childNodes.length; ++i) {
      const node = this.containerElement_.childNodes[i];
      if (node.classList.contains(SuggestionPicker.LIST_ENTRY_CLASS))
        entryCount++;
      totalHeight += node.offsetHeight;
      if (maxHeight === 0 &&
          entryCount == SuggestionPicker.NUMBER_OF_VISIBLE_ENTRIES)
        maxHeight = totalHeight;
    }
    let desiredWindowHeight = totalHeight * zoom;
    if (maxHeight !== 0 && totalHeight > maxHeight * zoom) {
      this.containerElement_.style.maxHeight =
          maxHeight - ListBorder - ListPadding + 'px';
      desiredWindowWidth += getScrollbarWidth() * zoom;
      desiredWindowHeight = maxHeight * zoom;
      this.containerElement_.style.overflowY = 'scroll';
    }
    const windowRect = adjustWindowRect(
        desiredWindowWidth, desiredWindowHeight, desiredWindowWidth, 0,
        /*allowOverlapWithAnchor=*/ false);
    this.containerElement_.style.height =
        windowRect.height / zoom - ListBorder - ListPadding + 'px';
    setWindowRect(windowRect);
  }

  layout_() {
    if (this.config_.isRTL)
      this.element_.classList.add('rtl');
    if (this.config_.isLocaleRTL)
      this.element_.classList.add('locale-rtl');
    this.containerElement_ = createElement('ul', 'suggestion-list');
    if (global.params.isBorderTransparent) {
      this.containerElement_.style.borderColor = 'transparent';
    }
    this.containerElement_.addEventListener(
        'click', this.handleEntryClick_.bind(this), false);
    for (let i = 0; i < this.config_.suggestionValues.length; ++i) {
      this.containerElement_.appendChild(this.createSuggestionEntryElement_(
          this.config_.localizedSuggestionValues[i],
          this.config_.suggestionLabels[i], this.config_.suggestionValues[i]));
    }
    if (this.config_.showOtherDateEntry) {
      // Add "Other..." entry
      const otherEntry = this.createActionEntryElement_(
          this.config_.otherDateLabel,
          SuggestionPicker.ActionNames.OPEN_CALENDAR_PICKER);
      this.containerElement_.appendChild(otherEntry);
    }
    this.element_.appendChild(this.containerElement_);
  }

  /**
   * @param {!Element} entry
   */
  selectEntry_(entry) {
    if (typeof entry.dataset.value !== 'undefined') {
      this.submitValue(entry.dataset.value);
    } else if (
        entry.dataset.action ===
        SuggestionPicker.ActionNames.OPEN_CALENDAR_PICKER) {
      window.addEventListener(
          'didHide', SuggestionPicker.handleWindowDidHide_, false);
      hideWindow();
    }
  }

  static handleWindowDidHide_() {
    openCalendarPicker();
    window.removeEventListener('didHide', SuggestionPicker.handleWindowDidHide_);
  }

  /**
   * @param {!Event} event
   */
  handleEntryClick_(event) {
    const entry = enclosingNodeOrSelfWithClass(
        event.target, SuggestionPicker.LIST_ENTRY_CLASS);
    if (!entry)
      return;
    this.selectEntry_(entry);
    event.preventDefault();
  }

  /**
   * @return {?Element}
   */
  findFirstVisibleEntry_() {
    const scrollTop = this.containerElement_.scrollTop;
    const childNodes = this.containerElement_.childNodes;
    for (let i = 0; i < childNodes.length; ++i) {
      const node = childNodes[i];
      if (node.nodeType !== Node.ELEMENT_NODE ||
          !node.classList.contains(SuggestionPicker.LIST_ENTRY_CLASS))
        continue;
      if (node.offsetTop + node.offsetHeight - scrollTop >
          SuggestionPicker.VISIBLE_ENTRY_THRESHOLD_HEIGHT)
        return node;
    }
    return null;
  }

  /**
   * @return {?Element}
   */
  findLastVisibleEntry_() {
    const scrollBottom =
        this.containerElement_.scrollTop + this.containerElement_.offsetHeight;
    const childNodes = this.containerElement_.childNodes;
    for (let i = childNodes.length - 1; i >= 0; --i) {
      const node = childNodes[i];
      if (node.nodeType !== Node.ELEMENT_NODE ||
          !node.classList.contains(SuggestionPicker.LIST_ENTRY_CLASS))
        continue;
      if (scrollBottom - node.offsetTop >
          SuggestionPicker.VISIBLE_ENTRY_THRESHOLD_HEIGHT)
        return node;
    }
    return null;
  }

  /**
   * @param {!Event} event
   */
  handleBodyKeyDown_(event) {
    let eventHandled = false;
    const key = event.key;
    if (key === 'Escape') {
      this.handleCancel();
      eventHandled = true;
    } else if (key == 'ArrowUp') {
      if (document.activeElement &&
          document.activeElement.classList.contains(
              SuggestionPicker.LIST_ENTRY_CLASS)) {
        for (let node = document.activeElement.previousElementSibling; node;
             node = node.previousElementSibling) {
          if (node.classList.contains(SuggestionPicker.LIST_ENTRY_CLASS)) {
            this.isFocusByMouse_ = false;
            node.focus();
            break;
          }
        }
      } else {
        this.element_
            .querySelector(
                '.' + SuggestionPicker.LIST_ENTRY_CLASS + ':last-child')
            .focus();
      }
      eventHandled = true;
    } else if (key == 'ArrowDown') {
      if (document.activeElement &&
          document.activeElement.classList.contains(
              SuggestionPicker.LIST_ENTRY_CLASS)) {
        for (let node = document.activeElement.nextElementSibling; node;
             node = node.nextElementSibling) {
          if (node.classList.contains(SuggestionPicker.LIST_ENTRY_CLASS)) {
            this.isFocusByMouse_ = false;
            node.focus();
            break;
          }
        }
      } else {
        this.element_
            .querySelector(
                '.' + SuggestionPicker.LIST_ENTRY_CLASS + ':first-child')
            .focus();
      }
      eventHandled = true;
    } else if (key === 'Enter') {
      this.selectEntry_(document.activeElement);
      eventHandled = true;
    } else if (key === 'PageUp') {
      this.containerElement_.scrollTop -= this.containerElement_.clientHeight;
      // Scrolling causes mouseover event to be called and that tries to move the focus too.
      // To prevent flickering we won't focus if the current focus was caused by the mouse.
      if (!this.isFocusByMouse_)
        this.findFirstVisibleEntry_().focus();
      eventHandled = true;
    } else if (key === 'PageDown') {
      this.containerElement_.scrollTop += this.containerElement_.clientHeight;
      if (!this.isFocusByMouse_)
        this.findLastVisibleEntry_().focus();
      eventHandled = true;
    }
    if (eventHandled)
      event.preventDefault();
  }

  /**
   * @param {!Event} event
   */
  handleEntryMouseOver_(event) {
    const entry = enclosingNodeOrSelfWithClass(
        event.target, SuggestionPicker.LIST_ENTRY_CLASS);
    if (!entry)
      return;
    this.isFocusByMouse_ = true;
    entry.focus();
    event.preventDefault();
  }

  /**
   * @param {!Event} event
   */
  handleMouseOut_(event) {
    if (!document.activeElement.classList.contains(
            SuggestionPicker.LIST_ENTRY_CLASS))
      return;
    this.isFocusByMouse_ = false;
    document.activeElement.blur();
    event.preventDefault();
  }
}
