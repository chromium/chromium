/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   radio-rating.js
 *
 *   Desc:   Radio group widget that implements ARIA Authoring Practices
 */

'use strict';

class RatingRadioGroup {
  constructor(groupNode) {
    this.groupNode = groupNode;

    this.radioButtons = [];

    this.firstRadioButton = null;
    this.lastRadioButton = null;

    var rbs = this.groupNode.querySelectorAll('[role=radio]');

    for (var i = 0; i < rbs.length; i++) {
      var rb = rbs[i];

      rb.tabIndex = -1;
      rb.setAttribute('aria-checked', 'false');

      rb.addEventListener('keydown', this.handleKeydown.bind(this));
      rb.addEventListener('click', this.handleClick.bind(this));

      this.radioButtons.push(rb);

      if (!this.firstRadioButton) {
        this.firstRadioButton = rb;
      }
      this.lastRadioButton = rb;
    }

    var value = groupNode.getAttribute('data-rating-value');
    var index = parseInt(value);

    if (value && index >= 0 && index < this.radioButtons.length) {
      this.radioButtons[index].tabIndex = 0;
    } else {
      value = this.firstRadioButton.getAttribute('data-rating-value');
      groupNode.getAttribute('data-rating-value', value);
      this.firstRadioButton.tabIndex = 0;
    }
  }

  setChecked(currentItem) {
    this.groupNode.tabIndex = -1;
    for (var i = 0; i < this.radioButtons.length; i++) {
      var rb = this.radioButtons[i];
      rb.setAttribute('aria-checked', 'false');
      rb.tabIndex = -1;
    }
    this.currentValue = currentItem.getAttribute('data-rating');
    this.groupNode.setAttribute('data-rating-value', this.currentValue);

    currentItem.setAttribute('aria-checked', 'true');
    currentItem.tabIndex = 0;
    currentItem.focus();
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

  /* EVENT HANDLERS */

  handleKeydown(event) {
    var tgt = event.currentTarget,
      flag = false;

    switch (event.key) {
      case ' ':
        this.setChecked(tgt);
        flag = true;
        break;

      case 'Up':
      case 'ArrowUp':
      case 'Left':
      case 'ArrowLeft':
        this.setCheckedToPreviousItem(tgt);
        flag = true;
        break;

      case 'Down':
      case 'ArrowDown':
      case 'Right':
      case 'ArrowRight':
        this.setCheckedToNextItem(tgt);
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
}

// Initialize radio button group

window.addEventListener('load', function () {
  var radios = document.querySelectorAll('.rating-radio');
  for (var i = 0; i < radios.length; i++) {
    new RatingRadioGroup(radios[i]);
  }
});
