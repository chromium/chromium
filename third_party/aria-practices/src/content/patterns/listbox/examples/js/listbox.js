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
 *  Listbox object representing the state and interactions for a listbox widget
 * @param listboxNode
 *  The DOM node pointing to the listbox
 */

aria.Listbox = class Listbox {
  constructor(listboxNode) {
    this.listboxNode = listboxNode;
    this.activeDescendant = this.listboxNode.getAttribute(
      'aria-activedescendant'
    );
    this.multiselectable = this.listboxNode.hasAttribute(
      'aria-multiselectable'
    );
    this.moveUpDownEnabled = false;
    this.siblingList = null;
    this.startRangeIndex = 0;
    this.upButton = null;
    this.downButton = null;
    this.moveButton = null;
    this.keysSoFar = '';
    this.handleFocusChange = function () {};
    this.handleItemChange = function () {};
    this.registerEvents();
  }

  registerEvents() {
    this.listboxNode.addEventListener('focus', this.setupFocus.bind(this));
    this.listboxNode.addEventListener('keydown', this.checkKeyPress.bind(this));
    this.listboxNode.addEventListener('click', this.checkClickItem.bind(this));

    if (this.multiselectable) {
      this.listboxNode.addEventListener(
        'mousedown',
        this.checkMouseDown.bind(this)
      );
    }
  }

  setupFocus() {
    if (this.activeDescendant) {
      const listitem = document.getElementById(this.activeDescendant);
      listitem.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    } else {
      this.focusFirstItem();
    }
  }

  focusFirstItem() {
    var firstItem = this.listboxNode.querySelector('[role="option"]');

    if (firstItem) {
      this.focusItem(firstItem);
    }
  }

  focusLastItem() {
    const itemList = this.listboxNode.querySelectorAll('[role="option"]');

    if (itemList.length) {
      this.focusItem(itemList[itemList.length - 1]);
    }
  }

  checkKeyPress(evt) {
    const lastActiveId = this.activeDescendant;
    const allOptions = this.listboxNode.querySelectorAll('[role="option"]');
    const currentItem =
      document.getElementById(this.activeDescendant) || allOptions[0];
    let nextItem = currentItem;

    if (!currentItem) {
      return;
    }

    switch (evt.key) {
      case 'PageUp':
      case 'PageDown':
        evt.preventDefault();
        if (this.moveUpDownEnabled) {
          if (evt.key === 'PageUp') {
            this.moveUpItems();
          } else {
            this.moveDownItems();
          }
        }

        break;
      case 'ArrowUp':
      case 'ArrowDown':
        evt.preventDefault();
        if (!this.activeDescendant) {
          // focus first option if no option was previously focused, and perform no other actions
          this.focusItem(currentItem);
          break;
        }

        if (this.moveUpDownEnabled && evt.altKey) {
          evt.preventDefault();
          if (evt.key === 'ArrowUp') {
            this.moveUpItems();
          } else {
            this.moveDownItems();
          }
          this.updateScroll();
          return;
        }

        if (evt.key === 'ArrowUp') {
          nextItem = this.findPreviousOption(currentItem);
        } else {
          nextItem = this.findNextOption(currentItem);
        }

        if (nextItem && this.multiselectable && event.shiftKey) {
          this.selectRange(this.startRangeIndex, nextItem);
        }

        if (nextItem) {
          this.focusItem(nextItem);
        }

        break;

      case 'Home':
        evt.preventDefault();
        this.focusFirstItem();

        if (this.multiselectable && evt.shiftKey && evt.ctrlKey) {
          this.selectRange(this.startRangeIndex, 0);
        }
        break;

      case 'End':
        evt.preventDefault();
        this.focusLastItem();

        if (this.multiselectable && evt.shiftKey && evt.ctrlKey) {
          this.selectRange(this.startRangeIndex, allOptions.length - 1);
        }
        break;

      case 'Shift':
        this.startRangeIndex = this.getElementIndex(currentItem, allOptions);
        break;

      case ' ':
        evt.preventDefault();
        this.toggleSelectItem(nextItem);
        break;

      case 'Backspace':
      case 'Delete':
      case 'Enter':
        if (!this.moveButton) {
          return;
        }

        var keyshortcuts = this.moveButton.getAttribute('aria-keyshortcuts');
        if (evt.key === 'Enter' && keyshortcuts.indexOf('Enter') === -1) {
          return;
        }
        if (
          (evt.key === 'Backspace' || evt.key === 'Delete') &&
          keyshortcuts.indexOf('Delete') === -1
        ) {
          return;
        }

        evt.preventDefault();

        var nextUnselected = nextItem.nextElementSibling;
        while (nextUnselected) {
          if (nextUnselected.getAttribute('aria-selected') != 'true') {
            break;
          }
          nextUnselected = nextUnselected.nextElementSibling;
        }
        if (!nextUnselected) {
          nextUnselected = nextItem.previousElementSibling;
          while (nextUnselected) {
            if (nextUnselected.getAttribute('aria-selected') != 'true') {
              break;
            }
            nextUnselected = nextUnselected.previousElementSibling;
          }
        }

        this.moveItems();

        if (!this.activeDescendant && nextUnselected) {
          this.focusItem(nextUnselected);
        }
        break;

      case 'A':
      case 'a':
        // handle control + A
        if (evt.ctrlKey || evt.metaKey) {
          if (this.multiselectable) {
            this.selectRange(0, allOptions.length - 1);
          }
          evt.preventDefault();
          break;
        }
      // fall through
      default:
        if (evt.key.length === 1) {
          const itemToFocus = this.findItemToFocus(evt.key.toLowerCase());
          if (itemToFocus) {
            this.focusItem(itemToFocus);
          }
        }
        break;
    }

    if (this.activeDescendant !== lastActiveId) {
      this.updateScroll();
    }
  }

  findItemToFocus(character) {
    const itemList = this.listboxNode.querySelectorAll('[role="option"]');
    let searchIndex = 0;

    if (!this.keysSoFar) {
      for (let i = 0; i < itemList.length; i++) {
        if (itemList[i].getAttribute('id') == this.activeDescendant) {
          searchIndex = i;
        }
      }
    }

    this.keysSoFar += character;
    this.clearKeysSoFarAfterDelay();

    let nextMatch = this.findMatchInRange(
      itemList,
      searchIndex + 1,
      itemList.length
    );

    if (!nextMatch) {
      nextMatch = this.findMatchInRange(itemList, 0, searchIndex);
    }
    return nextMatch;
  }

  /* Return the index of the passed element within the passed array, or null if not found */
  getElementIndex(option, options) {
    const allOptions = Array.prototype.slice.call(options); // convert to array
    const optionIndex = allOptions.indexOf(option);

    return typeof optionIndex === 'number' ? optionIndex : null;
  }

  /* Return the next listbox option, if it exists; otherwise, returns null */
  findNextOption(currentOption) {
    const allOptions = Array.prototype.slice.call(
      this.listboxNode.querySelectorAll('[role="option"]')
    ); // get options array
    const currentOptionIndex = allOptions.indexOf(currentOption);
    let nextOption = null;

    if (currentOptionIndex > -1 && currentOptionIndex < allOptions.length - 1) {
      nextOption = allOptions[currentOptionIndex + 1];
    }

    return nextOption;
  }

  /* Return the previous listbox option, if it exists; otherwise, returns null */
  findPreviousOption(currentOption) {
    const allOptions = Array.prototype.slice.call(
      this.listboxNode.querySelectorAll('[role="option"]')
    ); // get options array
    const currentOptionIndex = allOptions.indexOf(currentOption);
    let previousOption = null;

    if (currentOptionIndex > -1 && currentOptionIndex > 0) {
      previousOption = allOptions[currentOptionIndex - 1];
    }

    return previousOption;
  }

  clearKeysSoFarAfterDelay() {
    if (this.keyClear) {
      clearTimeout(this.keyClear);
      this.keyClear = null;
    }
    this.keyClear = setTimeout(
      function () {
        this.keysSoFar = '';
        this.keyClear = null;
      }.bind(this),
      500
    );
  }

  findMatchInRange(list, startIndex, endIndex) {
    // Find the first item starting with the keysSoFar substring, searching in
    // the specified range of items
    for (let n = startIndex; n < endIndex; n++) {
      const label = list[n].innerText;
      if (label && label.toLowerCase().indexOf(this.keysSoFar) === 0) {
        return list[n];
      }
    }
    return null;
  }

  checkClickItem(evt) {
    if (evt.target.getAttribute('role') !== 'option') {
      return;
    }

    this.focusItem(evt.target);
    this.toggleSelectItem(evt.target);
    this.updateScroll();

    if (this.multiselectable && evt.shiftKey) {
      this.selectRange(this.startRangeIndex, evt.target);
    }
  }

  /**
   * Prevent text selection on shift + click for multi-select listboxes
   *
   * @param evt
   */
  checkMouseDown(evt) {
    if (
      this.multiselectable &&
      evt.shiftKey &&
      evt.target.getAttribute('role') === 'option'
    ) {
      evt.preventDefault();
    }
  }

  /**
   * @description
   *  Toggle the aria-selected value
   * @param element
   *  The element to select
   */
  toggleSelectItem(element) {
    if (this.multiselectable) {
      element.setAttribute(
        'aria-selected',
        element.getAttribute('aria-selected') === 'true' ? 'false' : 'true'
      );

      this.updateMoveButton();
    }
  }

  /**
   * @description
   *  Defocus the specified item
   * @param element
   *  The element to defocus
   */
  defocusItem(element) {
    if (!element) {
      return;
    }
    if (!this.multiselectable) {
      element.removeAttribute('aria-selected');
    }
    element.classList.remove('focused');
  }

  /**
   * @description
   *  Focus on the specified item
   * @param element
   *  The element to focus
   */
  focusItem(element) {
    this.defocusItem(document.getElementById(this.activeDescendant));
    if (!this.multiselectable) {
      element.setAttribute('aria-selected', 'true');
    }
    element.classList.add('focused');
    this.listboxNode.setAttribute('aria-activedescendant', element.id);
    this.activeDescendant = element.id;

    if (!this.multiselectable) {
      this.updateMoveButton();
    }

    this.checkUpDownButtons();
    this.handleFocusChange(element);
  }

  /**
   * Helper function to check if a number is within a range; no side effects.
   *
   * @param index
   * @param start
   * @param end
   * @returns {boolean}
   */
  checkInRange(index, start, end) {
    const rangeStart = start < end ? start : end;
    const rangeEnd = start < end ? end : start;

    return index >= rangeStart && index <= rangeEnd;
  }

  /**
   * Select a range of options
   *
   * @param start
   * @param end
   */
  selectRange(start, end) {
    // get start/end indices
    const allOptions = this.listboxNode.querySelectorAll('[role="option"]');
    const startIndex =
      typeof start === 'number'
        ? start
        : this.getElementIndex(start, allOptions);
    const endIndex =
      typeof end === 'number' ? end : this.getElementIndex(end, allOptions);

    for (let index = 0; index < allOptions.length; index++) {
      const selected = this.checkInRange(index, startIndex, endIndex);
      allOptions[index].setAttribute('aria-selected', selected + '');
    }

    this.updateMoveButton();
  }

  /**
   * Check for selected options and update moveButton, if applicable
   */
  updateMoveButton() {
    if (!this.moveButton) {
      return;
    }

    if (this.listboxNode.querySelector('[aria-selected="true"]')) {
      this.moveButton.setAttribute('aria-disabled', 'false');
    } else {
      this.moveButton.setAttribute('aria-disabled', 'true');
    }
  }

  /**
   * Check if the selected option is in view, and scroll if not
   */
  updateScroll() {
    const selectedOption = document.getElementById(this.activeDescendant);
    if (selectedOption) {
      const scrollBottom =
        this.listboxNode.clientHeight + this.listboxNode.scrollTop;
      const elementBottom =
        selectedOption.offsetTop + selectedOption.offsetHeight;
      if (elementBottom > scrollBottom) {
        this.listboxNode.scrollTop =
          elementBottom - this.listboxNode.clientHeight;
      } else if (selectedOption.offsetTop < this.listboxNode.scrollTop) {
        this.listboxNode.scrollTop = selectedOption.offsetTop;
      }
      selectedOption.scrollIntoView({ block: 'nearest', inline: 'nearest' });
    }
  }

  /**
   * @description
   *  Enable/disable the up/down arrows based on the activeDescendant.
   */
  checkUpDownButtons() {
    const activeElement = document.getElementById(this.activeDescendant);

    if (!this.moveUpDownEnabled) {
      return;
    }

    if (!activeElement) {
      this.upButton.setAttribute('aria-disabled', 'true');
      this.downButton.setAttribute('aria-disabled', 'true');
      return;
    }

    if (this.upButton) {
      if (activeElement.previousElementSibling) {
        this.upButton.setAttribute('aria-disabled', false);
      } else {
        this.upButton.setAttribute('aria-disabled', 'true');
      }
    }

    if (this.downButton) {
      if (activeElement.nextElementSibling) {
        this.downButton.setAttribute('aria-disabled', false);
      } else {
        this.downButton.setAttribute('aria-disabled', 'true');
      }
    }
  }

  /**
   * @description
   *  Add the specified items to the listbox. Assumes items are valid options.
   * @param items
   *  An array of items to add to the listbox
   */
  addItems(items) {
    if (!items || !items.length) {
      return;
    }

    items.forEach(
      function (item) {
        this.defocusItem(item);
        this.toggleSelectItem(item);
        this.listboxNode.append(item);
      }.bind(this)
    );

    if (!this.activeDescendant) {
      this.focusItem(items[0]);
    }

    this.handleItemChange('added', items);
  }

  /**
   * @description
   *  Remove all of the selected items from the listbox; Removes the focused items
   *  in a single select listbox and the items with aria-selected in a multi
   *  select listbox.
   * @returns {Array}
   *  An array of items that were removed from the listbox
   */
  deleteItems() {
    let itemsToDelete;

    if (this.multiselectable) {
      itemsToDelete = this.listboxNode.querySelectorAll(
        '[aria-selected="true"]'
      );
    } else if (this.activeDescendant) {
      itemsToDelete = [document.getElementById(this.activeDescendant)];
    }

    if (!itemsToDelete || !itemsToDelete.length) {
      return [];
    }

    itemsToDelete.forEach(
      function (item) {
        item.remove();

        if (item.id === this.activeDescendant) {
          this.clearActiveDescendant();
        }
      }.bind(this)
    );

    this.handleItemChange('removed', itemsToDelete);

    return itemsToDelete;
  }

  clearActiveDescendant() {
    this.activeDescendant = null;
    this.listboxNode.setAttribute('aria-activedescendant', null);

    this.updateMoveButton();
    this.checkUpDownButtons();
  }

  /**
   * @description
   *  Shifts the currently focused item up on the list. No shifting occurs if the
   *  item is already at the top of the list.
   */
  moveUpItems() {
    if (!this.activeDescendant) {
      return;
    }

    const currentItem = document.getElementById(this.activeDescendant);
    const previousItem = currentItem.previousElementSibling;

    if (previousItem) {
      this.listboxNode.insertBefore(currentItem, previousItem);
      this.handleItemChange('moved_up', [currentItem]);
    }

    this.checkUpDownButtons();
  }

  /**
   * @description
   *  Shifts the currently focused item down on the list. No shifting occurs if
   *  the item is already at the end of the list.
   */
  moveDownItems() {
    if (!this.activeDescendant) {
      return;
    }

    var currentItem = document.getElementById(this.activeDescendant);
    var nextItem = currentItem.nextElementSibling;

    if (nextItem) {
      this.listboxNode.insertBefore(nextItem, currentItem);
      this.handleItemChange('moved_down', [currentItem]);
    }

    this.checkUpDownButtons();
  }

  /**
   * @description
   *  Delete the currently selected items and add them to the sibling list.
   */
  moveItems() {
    if (!this.siblingList) {
      return;
    }

    var itemsToMove = this.deleteItems();
    this.siblingList.addItems(itemsToMove);
  }

  /**
   * @description
   *  Enable Up/Down controls to shift items up and down.
   * @param upButton
   *   Up button to trigger up shift
   * @param downButton
   *   Down button to trigger down shift
   */
  enableMoveUpDown(upButton, downButton) {
    this.moveUpDownEnabled = true;
    this.upButton = upButton;
    this.downButton = downButton;
    upButton.addEventListener('click', this.moveUpItems.bind(this));
    downButton.addEventListener('click', this.moveDownItems.bind(this));
  }

  /**
   * @description
   *  Enable Move controls. Moving removes selected items from the current
   *  list and adds them to the sibling list.
   * @param button
   *   Move button to trigger delete
   * @param siblingList
   *   Listbox to move items to
   */
  setupMove(button, siblingList) {
    this.siblingList = siblingList;
    this.moveButton = button;
    button.addEventListener('click', this.moveItems.bind(this));
  }

  setHandleItemChange(handlerFn) {
    this.handleItemChange = handlerFn;
  }

  setHandleFocusChange(focusChangeHandler) {
    this.handleFocusChange = focusChangeHandler;
  }
};
