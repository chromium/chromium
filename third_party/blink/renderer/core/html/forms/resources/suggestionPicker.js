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

/**
 * @constructor
 * @param {!Element} element
 * @param {!Object} config
 */
function SuggestionPicker(element, config) {
  Picker.call(this, element, config);
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
SuggestionPicker.prototype = Object.create(Picker.prototype);

SuggestionPicker.NumberOfVisibleEntries = 20;

// An entry needs to be at least this many pixels visible for it to be a visible entry.
SuggestionPicker.VisibleEntryThresholdHeight = 4;

SuggestionPicker.ActionNames = {
  OpenCalendarPicker: 'openCalendarPicker'
};

SuggestionPicker.ListEntryClass = 'suggestion-list-entry';

SuggestionPicker.validateConfig = function(config) {
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
};

Object.defineProperty(SuggestionPicker, 'Padding', {
  get: function() {
    return Number(
        window.getComputedStyle(document.querySelector('.suggestion-list'))
            .getPropertyValue('padding')
            .replace('px', ''));
  }
});

SuggestionPicker.prototype._setColors = function() {
  var text = '.' + SuggestionPicker.ListEntryClass + ':focus {\
        background-color: ' +
      this._config.suggestionHighlightColor + ';\
        color: ' +
      this._config.suggestionHighlightTextColor + '; }';
  text += '.' + SuggestionPicker.ListEntryClass +
      ':focus .label { color: ' + this._config.suggestionHighlightTextColor +
      '; }';
  document.head.appendChild(createElement('style', null, text));
};

SuggestionPicker.prototype.cleanup = function() {
  document.body.removeEventListener(
      'keydown', this._handleBodyKeyDownBound, false);
};

/**
 * @param {!string} title
 * @param {!string} label
 * @param {!string} value
 * @return {!Element}
 */
SuggestionPicker.prototype._createSuggestionEntryElement = function(
    title, label, value) {
  var entryElement = createElement('li', SuggestionPicker.ListEntryClass);
  entryElement.tabIndex = 0;
  entryElement.dataset.value = value;
  var content = createElement('span', 'content');
  entryElement.appendChild(content);
  var titleElement = createElement('span', 'title', title);
  content.appendChild(titleElement);
  if (label) {
    var labelElement = createElement('span', 'label', label);
    content.appendChild(labelElement);
  }
  entryElement.addEventListener(
      'mouseover', this._handleEntryMouseOver.bind(this), false);
  return entryElement;
};

/**
 * @param {!string} title
 * @param {!string} actionName
 * @return {!Element}
 */
SuggestionPicker.prototype._createActionEntryElement = function(
    title, actionName) {
  var entryElement = createElement('li', SuggestionPicker.ListEntryClass);
  entryElement.tabIndex = 0;
  entryElement.dataset.action = actionName;
  var content = createElement('span', 'content');
  entryElement.appendChild(content);
  var titleElement = createElement('span', 'title', title);
  content.appendChild(titleElement);
  entryElement.addEventListener(
      'mouseover', this._handleEntryMouseOver.bind(this), false);
  return entryElement;
};

/**
* @return {!number}
*/
SuggestionPicker.prototype._measureMaxContentWidth = function() {
  // To measure the required width, we first set the class to "measuring-width" which
  // left aligns all the content including label.
  this._containerElement.classList.add('measuring-width');
  var maxContentWidth = 0;
  var contentElements =
      this._containerElement.getElementsByClassName('content');
  for (var i = 0; i < contentElements.length; ++i) {
    maxContentWidth = Math.max(
        maxContentWidth, contentElements[i].getBoundingClientRect().width);
  }
  this._containerElement.classList.remove('measuring-width');
  return maxContentWidth;
};

SuggestionPicker.prototype._fixWindowSize = function() {
  var ListBorder = 2;
  const ListPadding = 2 * SuggestionPicker.Padding;
  var zoom = this._config.zoomFactor;
  var desiredWindowWidth =
      (this._measureMaxContentWidth() + ListBorder + ListPadding) * zoom;
  if (typeof this._config.inputWidth === 'number')
    desiredWindowWidth = Math.max(this._config.inputWidth, desiredWindowWidth);
  var totalHeight = ListBorder + ListPadding;
  var maxHeight = 0;
  var entryCount = 0;
  for (var i = 0; i < this._containerElement.childNodes.length; ++i) {
    var node = this._containerElement.childNodes[i];
    if (node.classList.contains(SuggestionPicker.ListEntryClass))
      entryCount++;
    totalHeight += node.offsetHeight;
    if (maxHeight === 0 &&
        entryCount == SuggestionPicker.NumberOfVisibleEntries)
      maxHeight = totalHeight;
  }
  var desiredWindowHeight = totalHeight * zoom;
  if (maxHeight !== 0 && totalHeight > maxHeight * zoom) {
    this._containerElement.style.maxHeight =
        (maxHeight - ListBorder - ListPadding) + 'px';
    desiredWindowWidth += getScrollbarWidth() * zoom;
    desiredWindowHeight = maxHeight * zoom;
    this._containerElement.style.overflowY = 'scroll';
  }
  var windowRect = adjustWindowRect(
      desiredWindowWidth, desiredWindowHeight, desiredWindowWidth, 0);
  this._containerElement.style.height =
      (windowRect.height / zoom - ListBorder - ListPadding) + 'px';
  setWindowRect(windowRect);
};

SuggestionPicker.prototype._layout = function() {
  if (this._config.isRTL)
    this._element.classList.add('rtl');
  if (this._config.isLocaleRTL)
    this._element.classList.add('locale-rtl');
  this._containerElement = createElement('ul', 'suggestion-list');
  this._containerElement.addEventListener(
      'click', this._handleEntryClick.bind(this), false);
  for (var i = 0; i < this._config.suggestionValues.length; ++i) {
    this._containerElement.appendChild(this._createSuggestionEntryElement(
        this._config.localizedSuggestionValues[i],
        this._config.suggestionLabels[i], this._config.suggestionValues[i]));
  }
  if (this._config.showOtherDateEntry) {
    // Add separator
    if (!global.params.isFormControlsRefreshEnabled) {
      var separator = createElement('div', 'separator');
      this._containerElement.appendChild(separator);
    }

    // Add "Other..." entry
    var otherEntry = this._createActionEntryElement(
        this._config.otherDateLabel,
        SuggestionPicker.ActionNames.OpenCalendarPicker);
    this._containerElement.appendChild(otherEntry);
  }
  this._element.appendChild(this._containerElement);
};

/**
 * @param {!Element} entry
 */
SuggestionPicker.prototype.selectEntry = function(entry) {
  if (typeof entry.dataset.value !== 'undefined') {
    this.submitValue(entry.dataset.value);
  } else if (
      entry.dataset.action ===
      SuggestionPicker.ActionNames.OpenCalendarPicker) {
    window.addEventListener(
        'didHide', SuggestionPicker._handleWindowDidHide, false);
    hideWindow();
  }
};

SuggestionPicker._handleWindowDidHide = function() {
  openCalendarPicker();
  window.removeEventListener('didHide', SuggestionPicker._handleWindowDidHide);
};

/**
 * @param {!Event} event
 */
SuggestionPicker.prototype._handleEntryClick = function(event) {
  var entry = enclosingNodeOrSelfWithClass(
      event.target, SuggestionPicker.ListEntryClass);
  if (!entry)
    return;
  this.selectEntry(entry);
  event.preventDefault();
};

/**
 * @return {?Element}
 */
SuggestionPicker.prototype._findFirstVisibleEntry = function() {
  var scrollTop = this._containerElement.scrollTop;
  var childNodes = this._containerElement.childNodes;
  for (var i = 0; i < childNodes.length; ++i) {
    var node = childNodes[i];
    if (node.nodeType !== Node.ELEMENT_NODE ||
        !node.classList.contains(SuggestionPicker.ListEntryClass))
      continue;
    if (node.offsetTop + node.offsetHeight - scrollTop >
        SuggestionPicker.VisibleEntryThresholdHeight)
      return node;
  }
  return null;
};

/**
 * @return {?Element}
 */
SuggestionPicker.prototype._findLastVisibleEntry = function() {
  var scrollBottom =
      this._containerElement.scrollTop + this._containerElement.offsetHeight;
  var childNodes = this._containerElement.childNodes;
  for (var i = childNodes.length - 1; i >= 0; --i) {
    var node = childNodes[i];
    if (node.nodeType !== Node.ELEMENT_NODE ||
        !node.classList.contains(SuggestionPicker.ListEntryClass))
      continue;
    if (scrollBottom - node.offsetTop >
        SuggestionPicker.VisibleEntryThresholdHeight)
      return node;
  }
  return null;
};

/**
 * @param {!Event} event
 */
SuggestionPicker.prototype._handleBodyKeyDown = function(event) {
  var eventHandled = false;
  var key = event.key;
  if (key === 'Escape') {
    this.handleCancel();
    eventHandled = true;
  } else if (key == 'ArrowUp') {
    if (document.activeElement &&
        document.activeElement.classList.contains(
            SuggestionPicker.ListEntryClass)) {
      for (var node = document.activeElement.previousElementSibling; node;
           node = node.previousElementSibling) {
        if (node.classList.contains(SuggestionPicker.ListEntryClass)) {
          this._isFocusByMouse = false;
          node.focus();
          break;
        }
      }
    } else {
      this._element
          .querySelector('.' + SuggestionPicker.ListEntryClass + ':last-child')
          .focus();
    }
    eventHandled = true;
  } else if (key == 'ArrowDown') {
    if (document.activeElement &&
        document.activeElement.classList.contains(
            SuggestionPicker.ListEntryClass)) {
      for (var node = document.activeElement.nextElementSibling; node;
           node = node.nextElementSibling) {
        if (node.classList.contains(SuggestionPicker.ListEntryClass)) {
          this._isFocusByMouse = false;
          node.focus();
          break;
        }
      }
    } else {
      this._element
          .querySelector('.' + SuggestionPicker.ListEntryClass + ':first-child')
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
};

/**
 * @param {!Event} event
 */
SuggestionPicker.prototype._handleEntryMouseOver = function(event) {
  var entry = enclosingNodeOrSelfWithClass(
      event.target, SuggestionPicker.ListEntryClass);
  if (!entry)
    return;
  this._isFocusByMouse = true;
  entry.focus();
  event.preventDefault();
};

/**
 * @param {!Event} event
 */
SuggestionPicker.prototype._handleMouseOut = function(event) {
  if (!document.activeElement.classList.contains(
          SuggestionPicker.ListEntryClass))
    return;
  this._isFocusByMouse = false;
  document.activeElement.blur();
  event.preventDefault();
};
