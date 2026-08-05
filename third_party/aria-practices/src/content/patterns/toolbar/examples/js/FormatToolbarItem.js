/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   FontToolbarItem.js
 */

'use strict';

function FormatToolbarItem(domNode, toolbar) {
  this.domNode = domNode;
  this.toolbar = toolbar;
  this.buttonAction = '';
  this.value = '';
  this.popupLabelNode = null;
  this.hasHover = false;
  this.popupLabelDelay = 800;

  this.keyCode = Object.freeze({
    TAB: 9,
    ENTER: 13,
    ESC: 27,
    SPACE: 32,
    PAGEUP: 33,
    PAGEDOWN: 34,
    END: 35,
    HOME: 36,
    LEFT: 37,
    UP: 38,
    RIGHT: 39,
    DOWN: 40,
  });
}

FormatToolbarItem.prototype.init = function () {
  this.domNode.addEventListener('keydown', this.handleKeyDown.bind(this));
  this.domNode.addEventListener('click', this.handleClick.bind(this));
  this.domNode.addEventListener('focus', this.handleFocus.bind(this));
  this.domNode.addEventListener('blur', this.handleBlur.bind(this));
  this.domNode.addEventListener('mouseover', this.handleMouseOver.bind(this));
  this.domNode.addEventListener('mouseleave', this.handleMouseLeave.bind(this));

  document.body.addEventListener(
    'keydown',
    this.handleHideAllPopupLabels.bind(this)
  );

  if (this.domNode.classList.contains('bold')) {
    this.buttonAction = 'bold';
  }

  if (this.domNode.classList.contains('italic')) {
    this.buttonAction = 'italic';
  }

  if (this.domNode.classList.contains('underline')) {
    this.buttonAction = 'underline';
  }

  if (this.domNode.classList.contains('align-left')) {
    this.buttonAction = 'align';
    this.value = 'left';
    this.toolbar.alignItems.push(this);
  }

  if (this.domNode.classList.contains('align-center')) {
    this.buttonAction = 'align';
    this.value = 'center';
    this.toolbar.alignItems.push(this);
  }

  if (this.domNode.classList.contains('align-right')) {
    this.buttonAction = 'align';
    this.value = 'right';
    this.toolbar.alignItems.push(this);
  }
  if (this.domNode.classList.contains('nightmode')) {
    this.buttonAction = 'nightmode';
  }
  if (this.domNode.classList.contains('link')) {
    this.buttonAction = 'link';
  }
  if (this.domNode.classList.contains('copy')) {
    this.buttonAction = 'copy';
  }
  if (this.domNode.classList.contains('paste')) {
    this.buttonAction = 'paste';
  }
  if (this.domNode.classList.contains('cut')) {
    this.buttonAction = 'cut';
  }
  if (this.domNode.classList.contains('spinbutton')) {
    this.buttonAction = 'changeFontSize';
  }
  // Initialize any popup label

  this.popupLabelNode = this.domNode.querySelector('.popup-label');
  if (this.popupLabelNode) {
    var width = 8 * this.popupLabelNode.textContent.length;
    this.popupLabelNode.style.width = width + 'px';
    this.popupLabelNode.style.left =
      -1 * ((width - this.domNode.offsetWidth) / 2) - 5 + 'px';
  }
};

FormatToolbarItem.prototype.isPressed = function () {
  return this.domNode.getAttribute('aria-pressed') === 'true';
};

FormatToolbarItem.prototype.setPressed = function () {
  this.domNode.setAttribute('aria-pressed', 'true');
};

FormatToolbarItem.prototype.resetPressed = function () {
  this.domNode.setAttribute('aria-pressed', 'false');
};

FormatToolbarItem.prototype.setChecked = function () {
  this.domNode.setAttribute('aria-checked', 'true');
  this.domNode.checked = true;
};

FormatToolbarItem.prototype.resetChecked = function () {
  this.domNode.setAttribute('aria-checked', 'false');
  this.domNode.checked = false;
};

FormatToolbarItem.prototype.disable = function () {
  this.domNode.setAttribute('aria-disabled', 'true');
};

FormatToolbarItem.prototype.enable = function () {
  this.domNode.removeAttribute('aria-disabled');
};

FormatToolbarItem.prototype.showPopupLabel = function () {
  if (this.popupLabelNode) {
    this.toolbar.hidePopupLabels();
    this.popupLabelNode.classList.add('show');
  }
};

FormatToolbarItem.prototype.hidePopupLabel = function () {
  if (this.popupLabelNode && !this.hasHover) {
    this.popupLabelNode.classList.remove('show');
  }
};

// Events

FormatToolbarItem.prototype.handleHideAllPopupLabels = function (event) {
  switch (event.keyCode) {
    case this.keyCode.ESC:
      this.toolbar.hidePopupLabels();
      break;

    default:
      break;
  }
};

FormatToolbarItem.prototype.handleBlur = function () {
  this.toolbar.domNode.classList.remove('focus');

  if (this.domNode.classList.contains('nightmode')) {
    this.domNode.parentNode.classList.remove('focus');
  }
  this.hidePopupLabel();
};

FormatToolbarItem.prototype.handleFocus = function () {
  this.toolbar.domNode.classList.add('focus');

  if (this.domNode.classList.contains('nightmode')) {
    this.domNode.parentNode.classList.add('focus');
  }
  this.showPopupLabel();
};

FormatToolbarItem.prototype.handleMouseLeave = function () {
  this.hasHover = false;
  setTimeout(this.hidePopupLabel.bind(this), this.popupLabelDelay);
};

FormatToolbarItem.prototype.handleMouseOver = function () {
  this.showPopupLabel();
  this.hasHover = true;
};

FormatToolbarItem.prototype.handleKeyDown = function (event) {
  var flag = false;

  switch (event.keyCode) {
    case this.keyCode.ENTER:
    case this.keyCode.SPACE:
      if (
        this.buttonAction !== '' &&
        this.buttonAction !== 'bold' &&
        this.buttonAction !== 'italic' &&
        this.buttonAction !== 'underline'
      ) {
        this.toolbar.activateItem(this);
        if (this.buttonAction !== 'nightmode') {
          flag = true;
        }
      }
      break;

    case this.keyCode.RIGHT:
      this.toolbar.setFocusToNext(this);
      flag = true;
      break;

    case this.keyCode.LEFT:
      this.toolbar.setFocusToPrevious(this);
      flag = true;
      break;

    case this.keyCode.HOME:
      this.toolbar.setFocusToFirst(this);
      flag = true;
      break;

    case this.keyCode.END:
      this.toolbar.setFocusToLast(this);
      flag = true;
      break;

    case this.keyCode.UP:
      if (this.buttonAction === 'align') {
        if (this.domNode.classList.contains('align-left')) {
          this.toolbar.setFocusToLastAlignItem();
        } else {
          this.toolbar.setFocusToPrevious(this);
        }
        flag = true;
      }
      break;
    case this.keyCode.DOWN:
      if (this.buttonAction === 'align') {
        if (this.domNode.classList.contains('align-right')) {
          this.toolbar.setFocusToFirstAlignItem();
        } else {
          this.toolbar.setFocusToNext(this);
        }
        flag = true;
      }
      break;
    default:
      break;
  }

  if (flag) {
    event.stopPropagation();
    event.preventDefault();
  }
};

FormatToolbarItem.prototype.handleClick = function () {
  if (this.buttonAction == 'link') {
    return;
  }

  this.toolbar.setFocusItem(this);
  this.toolbar.activateItem(this);
};
