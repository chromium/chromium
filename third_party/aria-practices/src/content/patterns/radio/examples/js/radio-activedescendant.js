/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:  radio-activedescendant.js
 *
 *   Desc:  Radio group widget using aria-activedescendant that implements ARIA Authoring Practices
 */

'use strict';

class RadioGroupActiveDescendant {
  constructor(groupNode) {
    this.groupNode = groupNode;

    this.radioButtons = [];

    this.firstRadioButton = null;
    this.lastRadioButton = null;

    this.groupNode.addEventListener('keydown', this.handleKeydown.bind(this));
    this.groupNode.addEventListener('focus', this.handleFocus.bind(this));
    this.groupNode.addEventListener('blur', this.handleBlur.bind(this));

    // initialize
    if (!this.groupNode.getAttribute('role')) {
      this.groupNode.setAttribute('role', 'radiogroup');
    }

    var rbs = this.groupNode.querySelectorAll('[role=radio]');

    for (var i = 0; i < rbs.length; i++) {
      var rb = rbs[i];
      rb.addEventListener('click', this.handleClick.bind(this));
      this.radioButtons.push(rb);
      if (!this.firstRadioButton) {
        this.firstRadioButton = rb;
      }
      this.lastRadioButton = rb;
    }
    this.groupNode.tabIndex = 0;
  }

  isRadioInView(radio) {
    var bounding = radio.getBoundingClientRect();
    return (
      bounding.top >= 0 &&
      bounding.left >= 0 &&
      bounding.bottom <=
        (window.innerHeight || document.documentElement.clientHeight) &&
      bounding.right <=
        (window.innerWidth || document.documentElement.clientWidth)
    );
  }

  setChecked(currentItem) {
    for (var i = 0; i < this.radioButtons.length; i++) {
      var rb = this.radioButtons[i];
      rb.setAttribute('aria-checked', 'false');
      rb.classList.remove('focus');
    }
    currentItem.setAttribute('aria-checked', 'true');
    currentItem.classList.add('focus');
    this.groupNode.setAttribute('aria-activedescendant', currentItem.id);
    if (!this.isRadioInView(currentItem)) {
      currentItem.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
    this.groupNode.focus();
  }

  setCheckedToPreviousItem(currentItem) {
    var index;

    if (currentItem === this.firstRadioButton) {
      this.setChecked(this.lastRadioButton);
    } else {
      index = this.radioButtons.indexOf(currentItem);
      this.setChecked(this.radioButtons[index - 1]);
    }
  }

  setCheckedToNextItem(currentItem) {
    var index;

    if (currentItem === this.lastRadioButton) {
      this.setChecked(this.firstRadioButton);
    } else {
      index = this.radioButtons.indexOf(currentItem);
      this.setChecked(this.radioButtons[index + 1]);
    }
  }

  getCurrentRadioButton() {
    var id = this.groupNode.getAttribute('aria-activedescendant');
    if (!id) {
      this.groupNode.setAttribute(
        'aria-activedescendant',
        this.firstRadioButton.id
      );
      return this.firstRadioButton;
    }
    for (var i = 0; i < this.radioButtons.length; i++) {
      var rb = this.radioButtons[i];
      if (rb.id === id) {
        return rb;
      }
    }
    this.groupNode.setAttribute(
      'aria-activedescendant',
      this.firstRadioButton.id
    );
    return this.firstRadioButton;
  }

  // Event Handlers

  handleKeydown(event) {
    var flag = false;

    var currentItem = this.getCurrentRadioButton();
    switch (event.key) {
      case ' ':
        this.setChecked(currentItem);
        flag = true;
        break;

      case 'Up':
      case 'ArrowUp':
      case 'Left':
      case 'ArrowLeft':
        this.setCheckedToPreviousItem(currentItem);
        flag = true;
        break;

      case 'Down':
      case 'ArrowDown':
      case 'Right':
      case 'ArrowRight':
        this.setCheckedToNextItem(currentItem);
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  handleClick(event) {
    this.setChecked(event.currentTarget);
  }

  handleFocus() {
    var currentItem = this.getCurrentRadioButton();
    if (!this.isRadioInView(currentItem)) {
      currentItem.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
    }
    currentItem.classList.add('focus');
  }

  handleBlur() {
    var currentItem = this.getCurrentRadioButton();
    currentItem.classList.remove('focus');
  }
}

// Initialize radio button group using aria-activedescendant
window.addEventListener('load', function () {
  var radios = document.querySelectorAll('.radiogroup-activedescendant');
  for (var i = 0; i < radios.length; i++) {
    new RadioGroupActiveDescendant(radios[i]);
  }
});
