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
    this._isFocusByMouse = false;
    this._containerElement = null;
    this._setColors();
    this._layout();
    this._fixWindowSize();
    this._handleBodyKeyDownBound = this._handleBodyKeyDown.bind(this);
    document.body.addEventListener('keydown', this._handleBodyKeyDownBound);
    this._element.addEventListener(
        'mouseout', this._handleMouseOut.bind(this), false);
  }

  static NumberOfVisibleEntries = 20;
  // An entry needs to be at least this many pixels visible for it to be a visible entry.
  static VisibleEntryThresholdHeight = 4;
  static ActionNames = {
    OpenCalendarPicker: 'openCalendarPicker',
  };
  static ListEntryClass = 'suggestion-list-entry';

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

  Padding() {
    return Number(
        window.getComputedStyle(document.querySelector('.suggestion-list'))
            .getPropertyValue('padding')
            .replace('px', ''));
  }

  _setColors() {
    let text = '.' + SuggestionPicker.ListEntryClass + ':focus {\
          background-color: ' +
        this._config.suggestionHighlightColor + ';\
          color: ' +
        this._config.suggestionHighlightTextColor + '; }';
    text += '.' + SuggestionPicker.ListEntryClass +
        ':focus .label { color: ' + this._config.suggestionHighlightTextColor +
        '; }';
    document.head.appendChild(createElement('style', null, text));
  }

  cleanup() {
    document.body.removeEventListener(
        'keydown', this._handleBodyKeyDownBound, false);
  }

  /**
   * @param {!string} title
   * @param {!string} label
   * @param {!string} value
   * @return {!Element}
   */
  _createSuggestionEntryElement(title, label, value) {
    const entryElement = createElement('li', SuggestionPicker.ListEntryClass);
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
        'mouseover', this._handleEntryMouseOver.bind(this), false);
    return entryElement;
  }

  /**
   * @param {!string} title
   * @param {!string} actionName
   * @return {!Element}
   */
  _createActionEntryElement(title, actionName) {
    const entryElement = createElement('li', SuggestionPicker.ListEntryClass);
    entryElement.tabIndex = 0;
    entryElement.dataset.action = actionName;
    const content = createElement('span', 'content');
    entryElement.appendChild(content);
    const titleElement = createElement('span', 'title', title);
    content.appendChild(titleElement);
    entryElement.addEventListener(
        'mouseover', this._handleEntryMouseOver.bind(this), false);
    return entryElement;
  }

  /**
   * @return {!number}
   */
  _measureMaxContentWidth() {
    // To measure the required width, we first set the class to "measuring-width" which
    // left aligns all the content including label.
    this._containerElement.classList.add('measuring-width');
    let maxContentWidth = 0;
    const contentElements =
        this._containerElement.getElementsByClassName('content');
    for (let i = 0; i < contentElements.length; ++i) {
      maxContentWidth = Math.max(
          maxContentWidth, contentElements[i].getBoundingClientRect().width);
    }
    this._containerElement.classList.remove('measuring-width');
    return maxContentWidth;
  }

  _fixWindowSize() {
    const ListBorder = 2;
    const ListPadding = 2 * this.Padding();
    const zoom = this._config.zoomFactor;
    let desiredWindowWidth =
        (this._measureMaxContentWidth() + ListBorder + ListPadding) * zoom;
    if (typeof this._config.inputWidth === 'number')
      desiredWindowWidth =
          Math.max(this._config.inputWidth, desiredWindowWidth);
    let totalHeight = ListBorder + ListPadding;
    let maxHeight = 0;
    let entryCount = 0;
    for (let i = 0; i < this._containerElement.childNodes.length; ++i) {
      const node = this._containerElement.childNodes[i];
      if (node.classList.contains(SuggestionPicker.ListEntryClass))
        entryCount++;
      totalHeight += node.offsetHeight;
      if (maxHeight === 0 &&
          entryCount == SuggestionPicker.NumberOfVisibleEntries)
        maxHeight = totalHeight;
    }
    let desiredWindowHeight = totalHeight * zoom;
    if (maxHeight !== 0 && totalHeight > maxHeight * zoom) {
      this._containerElement.style.maxHeight =
          maxHeight - ListBorder - ListPadding + 'px';
      desiredWindowWidth += getScrollbarWidth() * zoom;
      desiredWindowHeight = maxHeight * zoom;
      this._containerElement.style.overflowY = 'scroll';
    }
    const windowRect = adjustWindowRect(
        desiredWindowWidth, desiredWindowHeight, desiredWindowWidth, 0);
    this._containerElement.style.height =
        windowRect.height / zoom - ListBorder - ListPadding + 'px';
    setWindowRect(windowRect);
  }

  _layout() {
    if (this._config.isRTL)
      this._element.classList.add('rtl');
    if (this._config.isLocaleRTL)
      this._element.classList.add('locale-rtl');
    this._containerElement = createElement('ul', 'suggestion-list');
    if (global.params.isBorderTransparent) {
      this._containerElement.style.borderColor = 'transparent';
    }
    this._containerElement.addEventListener(
        'click', this._handleEntryClick.bind(this), false);
    for (let i = 0; i < this._config.suggestionValues.length; ++i) {
      this._containerElement.appendChild(this._createSuggestionEntryElement(
          this._config.localizedSuggestionValues[i],
          this._config.suggestionLabels[i], this._config.suggestionValues[i]));
    }
    if (this._config.showOtherDateEntry) {
      // Add "Other..." entry
      const otherEntry = this._createActionEntryElement(
          this._config.otherDateLabel,
          SuggestionPicker.ActionNames.OpenCalendarPicker);
      this._containerElement.appendChild(otherEntry);
    }
    this._element.appendChild(this._containerElement);
  }

  /**
   * @param {!Element} entry
   */
  selectEntry(entry) {
    if (typeof entry.dataset.value !== 'undefined') {
      this.submitValue(entry.dataset.value);
    } else if (
        entry.dataset.action ===
        SuggestionPicker.ActionNames.OpenCalendarPicker) {
      window.addEventListener(
          'didHide', SuggestionPicker._handleWindowDidHide, false);
      hideWindow();
    }
  }

  static _handleWindowDidHide() {
    openCalendarPicker();
    window.removeEventListener(
        'didHide', SuggestionPicker._handleWindowDidHide);
  }

  /**
   * @param {!Event} event
   */
  _handleEntryClick(event) {
    const entry = enclosingNodeOrSelfWithClass(
        event.target, SuggestionPicker.ListEntryClass);
    if (!entry)
      return;
    this.selectEntry(entry);
    event.preventDefault();
  }

  /**
   * @return {?Element}
   */
  _findFirstVisibleEntry() {
    const scrollTop = this._containerElement.scrollTop;
    const childNodes = this._containerElement.childNodes;
    for (let i = 0; i < childNodes.length; ++i) {
      const node = childNodes[i];
      if (node.nodeType !== Node.ELEMENT_NODE ||
          !node.classList.contains(SuggestionPicker.ListEntryClass))
        continue;
      if (node.offsetTop + node.offsetHeight - scrollTop >
          SuggestionPicker.VisibleEntryThresholdHeight)
        return node;
    }
    return null;
  }

  /**
   * @return {?Element}
   */
  _findLastVisibleEntry() {
    const scrollBottom =
        this._containerElement.scrollTop + this._containerElement.offsetHeight;
    const childNodes = this._containerElement.childNodes;
    for (let i = childNodes.length - 1; i >= 0; --i) {
      const node = childNodes[i];
      if (node.nodeType !== Node.ELEMENT_NODE ||
          !node.classList.contains(SuggestionPicker.ListEntryClass))
        continue;
      if (scrollBottom - node.offsetTop >
          SuggestionPicker.VisibleEntryThresholdHeight)
        return node;
    }
    return null;
  }

  /**
   * @param {!Event} event
   */
  _handleBodyKeyDown(event) {
    let eventHandled = false;
    const key = event.key;
    if (key === 'Escape') {
      this.handleCancel();
      eventHandled = true;
    } else if (key == 'ArrowUp') {
      if (document.activeElement &&
          document.activeElement.classList.contains(
              SuggestionPicker.ListEntryClass)) {
        for (let node = document.activeElement.previousElementSibling; node;
             node = node.previousElementSibling) {
          if (node.classList.contains(SuggestionPicker.ListEntryClass)) {
            this._isFocusByMouse = false;
            node.focus();
            break;
          }
        }
      } else {
        this._element
            .querySelector(
                '.' + SuggestionPicker.ListEntryClass + ':last-child')
            .focus();
      }
      eventHandled = true;
    } else if (key == 'ArrowDown') {
      if (document.activeElement &&
          document.activeElement.classList.contains(
              SuggestionPicker.ListEntryClass)) {
        for (let node = document.activeElement.nextElementSibling; node;
             node = node.nextElementSibling) {
          if (node.classList.contains(SuggestionPicker.ListEntryClass)) {
            this._isFocusByMouse = false;
            node.focus();
            break;
          }
        }
      } else {
        this._element
            .querySelector(
                '.' + SuggestionPicker.ListEntryClass + ':first-child')
            .focus();
      }
      eventHandled = true;
    } else if (key === 'Enter') {
      this.selectEntry(document.activeElement);
      eventHandled = true;
    } else if (key === 'PageUp') {
      this._containerElement.scrollTop -= this._containerElement.clientHeight;
      // Scrolling causes mouseover event to be called and that tries to move the focus too.
      // To prevent flickering we won't focus if the current focus was caused by the mouse.
      if (!this._isFocusByMouse)
        this._findFirstVisibleEntry().focus();
      eventHandled = true;
    } else if (key === 'PageDown') {
      this._containerElement.scrollTop += this._containerElement.clientHeight;
      if (!this._isFocusByMouse)
        this._findLastVisibleEntry().focus();
      eventHandled = true;
    }
    if (eventHandled)
      event.preventDefault();
  }

  /**
   * @param {!Event} event
   */
  _handleEntryMouseOver(event) {
    const entry = enclosingNodeOrSelfWithClass(
        event.target, SuggestionPicker.ListEntryClass);
    if (!entry)
      return;
    this._isFocusByMouse = true;
    entry.focus();
    event.preventDefault();
  }

  /**
   * @param {!Event} event
   */
  _handleMouseOut(event) {
    if (!document.activeElement.classList.contains(
            SuggestionPicker.ListEntryClass))
      return;
    this._isFocusByMouse = false;
    document.activeElement.blur();
    event.preventDefault();
  }
}
