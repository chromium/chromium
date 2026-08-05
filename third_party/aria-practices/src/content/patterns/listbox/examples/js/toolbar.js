/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

'use strict';

/**
 * @namespace aria
 * @description
 * The aria namespace is used to support sharing class definitions between example files
 * without causing eslint errors for undefined classes
 */
var aria = aria || {};

/**
 * @class
 * @description
 *  Toolbar object representing the state and interactions for a toolbar widget
 * @param toolbarNode
 *  The DOM node pointing to the toolbar
 */

aria.Toolbar = class Toolbar {
  constructor(toolbarNode) {
    this.toolbarNode = toolbarNode;
    this.items = this.toolbarNode.querySelectorAll('.toolbar-item');
    this.selectedItem = this.toolbarNode.querySelector('.selected');
    this.registerEvents();
  }

  /**
   * @description
   *  Register events for the toolbar interactions
   */
  registerEvents() {
    this.toolbarNode.addEventListener(
      'keydown',
      this.checkFocusChange.bind(this)
    );
    this.toolbarNode.addEventListener('click', this.checkClickItem.bind(this));
  }

  /**
   * @description
   *  Handle various keyboard commands to move focus:
   *    LEFT:  Previous button
   *    RIGHT: Next button
   *    HOME:  First button
   *    END:   Last button
   * @param evt
   *  The keydown event object
   */
  checkFocusChange(evt) {
    let nextIndex, nextItem;

    // Do not move focus if any modifier keys pressed
    if (!evt.shiftKey && !evt.metaKey && !evt.altKey && !evt.ctrlKey) {
      switch (evt.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          nextIndex = Array.prototype.indexOf.call(
            this.items,
            this.selectedItem
          );
          nextIndex = evt.key === 'ArrowLeft' ? nextIndex - 1 : nextIndex + 1;
          nextIndex = Math.max(Math.min(nextIndex, this.items.length - 1), 0);

          nextItem = this.items[nextIndex];
          break;

        case 'End':
          nextItem = this.items[this.items.length - 1];
          break;

        case 'Home':
          nextItem = this.items[0];
          break;
      }

      if (nextItem) {
        this.selectItem(nextItem);
        this.focusItem(nextItem);
        evt.stopPropagation();
        evt.preventDefault();
      }
    }
  }

  /**
   * @description
   *  Selects a toolbar item if it is clicked
   * @param evt
   *  The click event object
   */
  checkClickItem(evt) {
    if (evt.target.classList.contains('toolbar-item')) {
      this.selectItem(evt.target);
    }
  }

  /**
   * @description
   *  Deselect the specified item
   * @param element
   *  The item to deselect
   */
  deselectItem(element) {
    element.classList.remove('selected');
    element.setAttribute('aria-selected', 'false');
    element.setAttribute('tabindex', '-1');
  }

  /**
   * @description
   *  Deselect the currently selected item and select the specified item
   * @param element
   *  The item to select
   */
  selectItem(element) {
    this.deselectItem(this.selectedItem);
    element.classList.add('selected');
    element.setAttribute('aria-selected', 'true');
    element.setAttribute('tabindex', '0');
    this.selectedItem = element;
  }

  /**
   * @description
   *  Focus on the specified item
   * @param element
   *  The item to focus on
   */
  focusItem(element) {
    element.focus();
  }
};
