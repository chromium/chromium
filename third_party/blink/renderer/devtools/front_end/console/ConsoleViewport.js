/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @unrestricted
 */
Console.ConsoleViewport = class {
  /**
   * @param {!Console.ConsoleViewportProvider} provider
   */
  constructor(provider) {
    this.element = createElement('div');
    this.element.style.overflow = 'auto';
    this._topGapElement = this.element.createChild('div');
    this._topGapElement.style.height = '0px';
    this._topGapElement.style.color = 'transparent';
    this._contentElement = this.element.createChild('div');
    this._bottomGapElement = this.element.createChild('div');
    this._bottomGapElement.style.height = '0px';
    this._bottomGapElement.style.color = 'transparent';

    // Text content needed for range intersection checks in _updateSelectionModel.
    // Use Unicode ZERO WIDTH NO-BREAK SPACE, which avoids contributing any height to the element's layout overflow.
    this._topGapElement.textContent = '\uFEFF';
    this._bottomGapElement.textContent = '\uFEFF';

    UI.ARIAUtils.markAsHidden(this._topGapElement);
    UI.ARIAUtils.markAsHidden(this._bottomGapElement);

    this._provider = provider;
    this.element.addEventListener('scroll', this._onScroll.bind(this), false);
    this.element.addEventListener('copy', this._onCopy.bind(this), false);
    this.element.addEventListener('dragstart', this._onDragStart.bind(this), false);
    this._keyboardNavigationEnabled = Runtime.experiments.isEnabled('consoleKeyboardNavigation');
    if (this._keyboardNavigationEnabled) {
      this._contentElement.addEventListener('focusin', this._onFocusIn.bind(this), false);
      this._contentElement.addEventListener('focusout', this._onFocusOut.bind(this), false);
      this._contentElement.addEventListener('keydown', this._onKeyDown.bind(this), false);
    }
    this._virtualSelectedIndex = -1;
    this._contentElement.tabIndex = -1;

    this._firstActiveIndex = -1;
    this._lastActiveIndex = -1;
    this._renderedItems = [];
    this._anchorSelection = null;
    this._headSelection = null;
    this._itemCount = 0;
    this._cumulativeHeights = new Int32Array(0);
    this._muteCopyHandler = false;

    // Listen for any changes to descendants and trigger a refresh. This ensures
    // that items updated asynchronously will not break stick-to-bottom behavior
    // if they change the scroll height.
    this._observer = new MutationObserver(this.refresh.bind(this));
    this._observerConfig = {childList: true, subtree: true};
  }

  /**
   * @return {boolean}
   */
  stickToBottom() {
    return this._stickToBottom;
  }

  /**
   * @param {boolean} value
   */
  setStickToBottom(value) {
    this._stickToBottom = value;
    if (this._stickToBottom)
      this._observer.observe(this._contentElement, this._observerConfig);
    else
      this._observer.disconnect();
  }

  copyWithStyles() {
    this._muteCopyHandler = true;
    this.element.ownerDocument.execCommand('copy');
    this._muteCopyHandler = false;
  }

  /**
   * @param {!Event} event
   */
  _onCopy(event) {
    if (this._muteCopyHandler)
      return;
    const text = this._selectedText();
    if (!text)
      return;
    event.preventDefault();
    event.clipboardData.setData('text/plain', text);
  }

  /**
   * @param {!Event} event
   */
  _onFocusIn(event) {
    const renderedIndex = this._renderedItems.findIndex(item => item.element().isSelfOrAncestor(event.target));
    if (renderedIndex !== -1)
      this._virtualSelectedIndex = this._firstActiveIndex + renderedIndex;
    // Make default selection when moving from external (e.g. prompt) to the container.
    if (this._virtualSelectedIndex === -1 && this._isOutsideViewport(/** @type {?Element} */ (event.relatedTarget)) &&
        event.target === this._contentElement)
      this._virtualSelectedIndex = this._itemCount - 1;
    this._updateFocusedItem();
  }

  /**
   * @param {!Event} event
   */
  _onFocusOut(event) {
    // Remove selection when focus moves to external location (e.g. prompt).
    if (this._isOutsideViewport(/** @type {?Element} */ (event.relatedTarget)))
      this._virtualSelectedIndex = -1;
    this._updateFocusedItem();
  }

  /**
   * @param {?Element} element
   * @return {boolean}
   */
  _isOutsideViewport(element) {
    return !!element && !element.isSelfOrDescendant(this._contentElement);
  }

  /**
   * @param {!Event} event
   */
  _onDragStart(event) {
    const text = this._selectedText();
    if (!text)
      return false;
    event.dataTransfer.clearData();
    event.dataTransfer.setData('text/plain', text);
    event.dataTransfer.effectAllowed = 'copy';
    return true;
  }

  /**
   * @param {!Event} event
   */
  _onKeyDown(event) {
    if (UI.isEditing() || !this._itemCount || event.shiftKey)
      return;
    let isArrowUp = false;
    switch (event.key) {
      case 'ArrowUp':
        if (this._virtualSelectedIndex > 0) {
          isArrowUp = true;
          this._virtualSelectedIndex--;
        } else {
          return;
        }
        break;
      case 'ArrowDown':
        if (this._virtualSelectedIndex < this._itemCount - 1)
          this._virtualSelectedIndex++;
        else
          return;
        break;
      case 'Home':
        this._virtualSelectedIndex = 0;
        break;
      case 'End':
        this._virtualSelectedIndex = this._itemCount - 1;
        break;
      default:
        return;
    }
    event.consume(true);
    this.scrollItemIntoView(this._virtualSelectedIndex);
    this._updateFocusedItem(isArrowUp);
  }

  /**
   * @param {boolean=} focusLastChild
   */
  _updateFocusedItem(focusLastChild) {
    const selectedElement = this.renderedElementAt(this._virtualSelectedIndex);
    const changed = this._lastSelectedElement !== selectedElement;
    const containerHasFocus = this._contentElement === this.element.ownerDocument.deepActiveElement();
    if (this._lastSelectedElement && changed)
      this._lastSelectedElement.classList.remove('console-selected');
    if (selectedElement && (changed || containerHasFocus) && this.element.hasFocus()) {
      selectedElement.classList.add('console-selected');
      // Do not focus the message if something within holds focus (e.g. object).
      if (!selectedElement.hasFocus()) {
        if (focusLastChild)
          this._renderedItems[this._virtualSelectedIndex - this._firstActiveIndex].focusLastChildOrSelf();
        else
          focusWithoutScroll(selectedElement);
      }
    }
    if (this._itemCount && !this._contentElement.hasFocus())
      this._contentElement.tabIndex = 0;
    else
      this._contentElement.tabIndex = -1;
    this._lastSelectedElement = selectedElement;

    /**
     * @suppress {checkTypes}
     * @param {!Element} element
     */
    function focusWithoutScroll(element) {
      // TODO(luoe): Closure has an outdated typedef for Element.prototype.focus.
      element.focus({preventScroll: true});
    }
  }

  /**
   * @return {!Element}
   */
  contentElement() {
    return this._contentElement;
  }

  invalidate() {
    delete this._cachedProviderElements;
    this._itemCount = this._provider.itemCount();
    if (this._virtualSelectedIndex > this._itemCount - 1)
      this._virtualSelectedIndex = this._itemCount - 1;
    this._rebuildCumulativeHeights();
    this.refresh();
  }

  /**
   * @param {number} index
   * @return {?Console.ConsoleViewportElement}
   */
  _providerElement(index) {
    if (!this._cachedProviderElements)
      this._cachedProviderElements = new Array(this._itemCount);
    let element = this._cachedProviderElements[index];
    if (!element) {
      element = this._provider.itemElement(index);
      this._cachedProviderElements[index] = element;
    }
    return element;
  }

  _rebuildCumulativeHeights() {
    const firstActiveIndex = this._firstActiveIndex;
    const lastActiveIndex = this._lastActiveIndex;
    let height = 0;
    this._cumulativeHeights = new Int32Array(this._itemCount);
    for (let i = 0; i < this._itemCount; ++i) {
      if (firstActiveIndex <= i && i - firstActiveIndex < this._renderedItems.length && i <= lastActiveIndex)
        height += this._renderedItems[i - firstActiveIndex].element().offsetHeight;
      else
        height += this._provider.fastHeight(i);
      this._cumulativeHeights[i] = height;
    }
  }

  _rebuildCumulativeHeightsIfNeeded() {
    let totalCachedHeight = 0;
    let totalMeasuredHeight = 0;
    // Check whether current items in DOM have changed heights. Tolerate 1-pixel
    // error due to double-to-integer rounding errors.
    for (let i = 0; i < this._renderedItems.length; ++i) {
      const cachedItemHeight = this._cachedItemHeight(this._firstActiveIndex + i);
      const measuredHeight = this._renderedItems[i].element().offsetHeight;
      if (Math.abs(cachedItemHeight - measuredHeight) > 1) {
        this._rebuildCumulativeHeights();
        return;
      }
      totalMeasuredHeight += measuredHeight;
      totalCachedHeight += cachedItemHeight;
      if (Math.abs(totalCachedHeight - totalMeasuredHeight) > 1) {
        this._rebuildCumulativeHeights();
        return;
      }
    }
  }

  /**
   * @param {number} index
   * @return {number}
   */
  _cachedItemHeight(index) {
    return index === 0 ? this._cumulativeHeights[0] :
                         this._cumulativeHeights[index] - this._cumulativeHeights[index - 1];
  }

  /**
   * @param {?Selection} selection
   * @suppressGlobalPropertiesCheck
   */
  _isSelectionBackwards(selection) {
    if (!selection || !selection.rangeCount)
      return false;
    const range = document.createRange();
    range.setStart(selection.anchorNode, selection.anchorOffset);
    range.setEnd(selection.focusNode, selection.focusOffset);
    return range.collapsed;
  }

  /**
   * @param {number} itemIndex
   * @param {!Node} node
   * @param {number} offset
   * @return {!{item: number, node: !Node, offset: number}}
   */
  _createSelectionModel(itemIndex, node, offset) {
    return {item: itemIndex, node: node, offset: offset};
  }

  /**
   * @param {?Selection} selection
   */
  _updateSelectionModel(selection) {
    const range = selection && selection.rangeCount ? selection.getRangeAt(0) : null;
    if (!range || selection.isCollapsed || !this.element.hasSelection()) {
      this._headSelection = null;
      this._anchorSelection = null;
      return false;
    }

    let firstSelected = Number.MAX_VALUE;
    let lastSelected = -1;

    let hasVisibleSelection = false;
    for (let i = 0; i < this._renderedItems.length; ++i) {
      if (range.intersectsNode(this._renderedItems[i].element())) {
        const index = i + this._firstActiveIndex;
        firstSelected = Math.min(firstSelected, index);
        lastSelected = Math.max(lastSelected, index);
        hasVisibleSelection = true;
      }
    }
    if (hasVisibleSelection) {
      firstSelected =
          this._createSelectionModel(firstSelected, /** @type {!Node} */ (range.startContainer), range.startOffset);
      lastSelected =
          this._createSelectionModel(lastSelected, /** @type {!Node} */ (range.endContainer), range.endOffset);
    }
    const topOverlap = range.intersectsNode(this._topGapElement) && this._topGapElement._active;
    const bottomOverlap = range.intersectsNode(this._bottomGapElement) && this._bottomGapElement._active;
    if (!topOverlap && !bottomOverlap && !hasVisibleSelection) {
      this._headSelection = null;
      this._anchorSelection = null;
      return false;
    }

    if (!this._anchorSelection || !this._headSelection) {
      this._anchorSelection = this._createSelectionModel(0, this.element, 0);
      this._headSelection = this._createSelectionModel(this._itemCount - 1, this.element, this.element.children.length);
      this._selectionIsBackward = false;
    }

    const isBackward = this._isSelectionBackwards(selection);
    const startSelection = this._selectionIsBackward ? this._headSelection : this._anchorSelection;
    const endSelection = this._selectionIsBackward ? this._anchorSelection : this._headSelection;
    if (topOverlap && bottomOverlap && hasVisibleSelection) {
      firstSelected = firstSelected.item < startSelection.item ? firstSelected : startSelection;
      lastSelected = lastSelected.item > endSelection.item ? lastSelected : endSelection;
    } else if (!hasVisibleSelection) {
      firstSelected = startSelection;
      lastSelected = endSelection;
    } else if (topOverlap) {
      firstSelected = isBackward ? this._headSelection : this._anchorSelection;
    } else if (bottomOverlap) {
      lastSelected = isBackward ? this._anchorSelection : this._headSelection;
    }

    if (isBackward) {
      this._anchorSelection = lastSelected;
      this._headSelection = firstSelected;
    } else {
      this._anchorSelection = firstSelected;
      this._headSelection = lastSelected;
    }
    this._selectionIsBackward = isBackward;
    return true;
  }

  /**
   * @param {?Selection} selection
   */
  _restoreSelection(selection) {
    let anchorElement = null;
    let anchorOffset;
    if (this._firstActiveIndex <= this._anchorSelection.item && this._anchorSelection.item <= this._lastActiveIndex) {
      anchorElement = this._anchorSelection.node;
      anchorOffset = this._anchorSelection.offset;
    } else {
      if (this._anchorSelection.item < this._firstActiveIndex)
        anchorElement = this._topGapElement;
      else if (this._anchorSelection.item > this._lastActiveIndex)
        anchorElement = this._bottomGapElement;
      anchorOffset = this._selectionIsBackward ? 1 : 0;
    }

    let headElement = null;
    let headOffset;
    if (this._firstActiveIndex <= this._headSelection.item && this._headSelection.item <= this._lastActiveIndex) {
      headElement = this._headSelection.node;
      headOffset = this._headSelection.offset;
    } else {
      if (this._headSelection.item < this._firstActiveIndex)
        headElement = this._topGapElement;
      else if (this._headSelection.item > this._lastActiveIndex)
        headElement = this._bottomGapElement;
      headOffset = this._selectionIsBackward ? 0 : 1;
    }

    selection.setBaseAndExtent(anchorElement, anchorOffset, headElement, headOffset);
  }

  refresh() {
    this._observer.disconnect();
    this._innerRefresh();
    if (this._stickToBottom)
      this._observer.observe(this._contentElement, this._observerConfig);
  }

  _innerRefresh() {
    if (!this._visibleHeight())
      return;  // Do nothing for invisible controls.

    if (!this._itemCount) {
      for (let i = 0; i < this._renderedItems.length; ++i)
        this._renderedItems[i].willHide();
      this._renderedItems = [];
      this._contentElement.removeChildren();
      this._topGapElement.style.height = '0px';
      this._bottomGapElement.style.height = '0px';
      this._firstActiveIndex = -1;
      this._lastActiveIndex = -1;
      if (this._keyboardNavigationEnabled)
        this._updateFocusedItem();
      return;
    }

    const selection = this.element.getComponentSelection();
    const shouldRestoreSelection = this._updateSelectionModel(selection);

    const visibleFrom = this.element.scrollTop;
    const visibleHeight = this._visibleHeight();
    const activeHeight = visibleHeight * 2;
    this._rebuildCumulativeHeightsIfNeeded();

    // When the viewport is scrolled to the bottom, using the cumulative heights estimate is not
    // precise enough to determine next visible indices. This stickToBottom check avoids extra
    // calls to refresh in those cases.
    if (this._stickToBottom) {
      this._firstActiveIndex =
          Math.max(this._itemCount - Math.ceil(activeHeight / this._provider.minimumRowHeight()), 0);
      this._lastActiveIndex = this._itemCount - 1;
    } else {
      this._firstActiveIndex =
          Math.max(this._cumulativeHeights.lowerBound(visibleFrom + 1 - (activeHeight - visibleHeight) / 2), 0);
      // Proactively render more rows in case some of them will be collapsed without triggering refresh. @see crbug.com/390169
      this._lastActiveIndex = this._firstActiveIndex + Math.ceil(activeHeight / this._provider.minimumRowHeight()) - 1;
      this._lastActiveIndex = Math.min(this._lastActiveIndex, this._itemCount - 1);
    }

    const topGapHeight = this._cumulativeHeights[this._firstActiveIndex - 1] || 0;
    const bottomGapHeight =
        this._cumulativeHeights[this._cumulativeHeights.length - 1] - this._cumulativeHeights[this._lastActiveIndex];

    /**
     * @this {Console.ConsoleViewport}
     */
    function prepare() {
      this._topGapElement.style.height = topGapHeight + 'px';
      this._bottomGapElement.style.height = bottomGapHeight + 'px';
      this._topGapElement._active = !!topGapHeight;
      this._bottomGapElement._active = !!bottomGapHeight;
      this._contentElement.style.setProperty('height', '10000000px');
    }

    this._partialViewportUpdate(prepare.bind(this));
    this._contentElement.style.removeProperty('height');
    // Should be the last call in the method as it might force layout.
    if (shouldRestoreSelection)
      this._restoreSelection(selection);
    if (this._stickToBottom)
      this.element.scrollTop = 10000000;
  }

  /**
   * @param {function()} prepare
   */
  _partialViewportUpdate(prepare) {
    const itemsToRender = new Set();
    for (let i = this._firstActiveIndex; i <= this._lastActiveIndex; ++i)
      itemsToRender.add(this._providerElement(i));
    const willBeHidden = this._renderedItems.filter(item => !itemsToRender.has(item));
    for (let i = 0; i < willBeHidden.length; ++i)
      willBeHidden[i].willHide();
    prepare();
    let hadFocus = false;
    for (let i = 0; i < willBeHidden.length; ++i) {
      if (this._keyboardNavigationEnabled)
        hadFocus = hadFocus || willBeHidden[i].element().hasFocus();
      willBeHidden[i].element().remove();
    }

    const wasShown = [];
    let anchor = this._contentElement.firstChild;
    for (const viewportElement of itemsToRender) {
      const element = viewportElement.element();
      if (element !== anchor) {
        const shouldCallWasShown = !element.parentElement;
        if (shouldCallWasShown)
          wasShown.push(viewportElement);
        this._contentElement.insertBefore(element, anchor);
      } else {
        anchor = anchor.nextSibling;
      }
    }
    for (let i = 0; i < wasShown.length; ++i)
      wasShown[i].wasShown();
    this._renderedItems = Array.from(itemsToRender);

    if (this._keyboardNavigationEnabled) {
      if (hadFocus)
        this._contentElement.focus();
      this._updateFocusedItem();
    }
  }

  /**
   * @return {?string}
   */
  _selectedText() {
    this._updateSelectionModel(this.element.getComponentSelection());
    if (!this._headSelection || !this._anchorSelection)
      return null;

    let startSelection = null;
    let endSelection = null;
    if (this._selectionIsBackward) {
      startSelection = this._headSelection;
      endSelection = this._anchorSelection;
    } else {
      startSelection = this._anchorSelection;
      endSelection = this._headSelection;
    }

    const textLines = [];
    for (let i = startSelection.item; i <= endSelection.item; ++i) {
      const element = this._providerElement(i).element();
      const lineContent = element.childTextNodes().map(Components.Linkifier.untruncatedNodeText).join('');
      textLines.push(lineContent);
    }

    const endSelectionElement = this._providerElement(endSelection.item).element();
    if (endSelection.node && endSelection.node.isSelfOrDescendant(endSelectionElement)) {
      const itemTextOffset = this._textOffsetInNode(endSelectionElement, endSelection.node, endSelection.offset);
      textLines[textLines.length - 1] = textLines.peekLast().substring(0, itemTextOffset);
    }

    const startSelectionElement = this._providerElement(startSelection.item).element();
    if (startSelection.node && startSelection.node.isSelfOrDescendant(startSelectionElement)) {
      const itemTextOffset = this._textOffsetInNode(startSelectionElement, startSelection.node, startSelection.offset);
      textLines[0] = textLines[0].substring(itemTextOffset);
    }

    return textLines.join('\n');
  }

  /**
   * @param {!Element} itemElement
   * @param {!Node} selectionNode
   * @param {number} offset
   * @return {number}
   */
  _textOffsetInNode(itemElement, selectionNode, offset) {
    // If the selectionNode is not a TextNode, we may need to convert a child offset into a character offset.
    if (selectionNode.nodeType !== Node.TEXT_NODE) {
      if (offset < selectionNode.childNodes.length) {
        selectionNode = /** @type {!Node} */ (selectionNode.childNodes.item(offset));
        offset = 0;
      } else {
        offset = selectionNode.textContent.length;
      }
    }

    let chars = 0;
    let node = itemElement;
    while ((node = node.traverseNextNode(itemElement)) && node !== selectionNode) {
      if (node.nodeType !== Node.TEXT_NODE || node.parentElement.nodeName === 'STYLE' ||
          node.parentElement.nodeName === 'SCRIPT')
        continue;
      chars += Components.Linkifier.untruncatedNodeText(node).length;
    }
    // If the selected node text was truncated, treat any non-zero offset as the full length.
    const untruncatedContainerLength = Components.Linkifier.untruncatedNodeText(selectionNode).length;
    if (offset > 0 && untruncatedContainerLength !== selectionNode.textContent.length)
      offset = untruncatedContainerLength;
    return chars + offset;
  }

  /**
   * @param {!Event} event
   */
  _onScroll(event) {
    this.refresh();
  }

  /**
   * @return {number}
   */
  firstVisibleIndex() {
    if (!this._cumulativeHeights.length)
      return -1;
    this._rebuildCumulativeHeightsIfNeeded();
    return this._cumulativeHeights.lowerBound(this.element.scrollTop + 1);
  }

  /**
   * @return {number}
   */
  lastVisibleIndex() {
    if (!this._cumulativeHeights.length)
      return -1;
    this._rebuildCumulativeHeightsIfNeeded();
    const scrollBottom = this.element.scrollTop + this.element.clientHeight;
    const right = this._itemCount - 1;
    return this._cumulativeHeights.lowerBound(scrollBottom, undefined, undefined, right);
  }

  /**
   * @return {?Element}
   */
  renderedElementAt(index) {
    if (index === -1 || index < this._firstActiveIndex || index > this._lastActiveIndex)
      return null;
    return this._renderedItems[index - this._firstActiveIndex].element();
  }

  /**
   * @param {number} index
   * @param {boolean=} makeLast
   */
  scrollItemIntoView(index, makeLast) {
    const firstVisibleIndex = this.firstVisibleIndex();
    const lastVisibleIndex = this.lastVisibleIndex();
    if (index > firstVisibleIndex && index < lastVisibleIndex)
      return;
    // If the prompt is visible, then the last item must be fully on screen.
    if (index === lastVisibleIndex && this._cumulativeHeights[index] <= this.element.scrollTop + this._visibleHeight())
      return;
    if (makeLast)
      this.forceScrollItemToBeLast(index);
    else if (index <= firstVisibleIndex)
      this.forceScrollItemToBeFirst(index);
    else if (index >= lastVisibleIndex)
      this.forceScrollItemToBeLast(index);
  }

  /**
   * @param {number} index
   */
  forceScrollItemToBeFirst(index) {
    console.assert(index >= 0 && index < this._itemCount, 'Cannot scroll item at invalid index');
    this.setStickToBottom(false);
    this._rebuildCumulativeHeightsIfNeeded();
    this.element.scrollTop = index > 0 ? this._cumulativeHeights[index - 1] : 0;
    if (this.element.isScrolledToBottom())
      this.setStickToBottom(true);
    this.refresh();
    // After refresh, the item is in DOM, but may not be visible (items above were larger than expected).
    this.renderedElementAt(index).scrollIntoView(true /* alignTop */);
  }

  /**
   * @param {number} index
   */
  forceScrollItemToBeLast(index) {
    console.assert(index >= 0 && index < this._itemCount, 'Cannot scroll item at invalid index');
    this.setStickToBottom(false);
    this._rebuildCumulativeHeightsIfNeeded();
    this.element.scrollTop = this._cumulativeHeights[index] - this._visibleHeight();
    if (this.element.isScrolledToBottom())
      this.setStickToBottom(true);
    this.refresh();
    // After refresh, the item is in DOM, but may not be visible (items above were larger than expected).
    this.renderedElementAt(index).scrollIntoView(false /* alignTop */);
  }

  /**
   * @return {number}
   */
  _visibleHeight() {
    // Use offsetHeight instead of clientHeight to avoid being affected by horizontal scroll.
    return this.element.offsetHeight;
  }
};

/**
 * @interface
 */
Console.ConsoleViewportProvider = function() {};

Console.ConsoleViewportProvider.prototype = {
  /**
   * @param {number} index
   * @return {number}
   */
  fastHeight(index) {
    return 0;
  },

  /**
   * @return {number}
   */
  itemCount() {
    return 0;
  },

  /**
   * @return {number}
   */
  minimumRowHeight() {
    return 0;
  },

  /**
   * @param {number} index
   * @return {?Console.ConsoleViewportElement}
   */
  itemElement(index) {
    return null;
  }
};

/**
 * @interface
 */
Console.ConsoleViewportElement = function() {};
Console.ConsoleViewportElement.prototype = {
  willHide() {},

  wasShown() {},

  /**
   * @return {!Element}
   */
  element() {},
};
