/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   SpinButton.js
 */

'use strict';

// Create SpinButton that contains value, valuemin, valuemax, and valuenow
var SpinButton = function (domNode, toolbar) {
  this.domNode = domNode;
  this.toolbar = toolbar;

  this.valueDomNode = domNode.querySelector('.value');
  this.increaseDomNode = domNode.querySelector('.increase');
  this.decreaseDomNode = domNode.querySelector('.decrease');

  this.valueMin = 8;
  this.valueMax = 40;
  this.valueNow = 12;
  this.valueText = this.valueNow + ' Point';

  this.keyCode = Object.freeze({
    UP: 38,
    DOWN: 40,
    PAGEUP: 33,
    PAGEDOWN: 34,
    END: 35,
    HOME: 36,
  });
};

// Initialize slider
SpinButton.prototype.init = function () {
  if (this.domNode.getAttribute('aria-valuemin')) {
    this.valueMin = parseInt(this.domNode.getAttribute('aria-valuemin'));
  }

  if (this.domNode.getAttribute('aria-valuemax')) {
    this.valueMax = parseInt(this.domNode.getAttribute('aria-valuemax'));
  }

  if (this.domNode.getAttribute('aria-valuenow')) {
    this.valueNow = parseInt(this.domNode.getAttribute('aria-valuenow'));
  }

  this.setValue(this.valueNow);

  this.domNode.addEventListener('keydown', this.handleKeyDown.bind(this));

  this.increaseDomNode.addEventListener(
    'click',
    this.handleIncreaseClick.bind(this)
  );
  this.decreaseDomNode.addEventListener(
    'click',
    this.handleDecreaseClick.bind(this)
  );
};

SpinButton.prototype.setValue = function (value) {
  if (value > this.valueMax) {
    value = this.valueMax;
  }

  if (value < this.valueMin) {
    value = this.valueMin;
  }

  this.valueNow = value;
  this.valueText = value + ' Point';

  this.domNode.setAttribute('aria-valuenow', this.valueNow);
  this.domNode.setAttribute('aria-valuetext', this.valueText);

  if (this.valueDomNode) {
    this.valueDomNode.innerHTML = this.valueText;
  }

  this.toolbar.changeFontSize(value);
};

SpinButton.prototype.handleKeyDown = function (event) {
  var flag = false;

  switch (event.keyCode) {
    case this.keyCode.DOWN:
      this.setValue(this.valueNow - 1);
      flag = true;
      break;

    case this.keyCode.UP:
      this.setValue(this.valueNow + 1);
      flag = true;
      break;

    case this.keyCode.PAGEDOWN:
      this.setValue(this.valueNow - 5);
      flag = true;
      break;

    case this.keyCode.PAGEUP:
      this.setValue(this.valueNow + 5);
      flag = true;
      break;

    case this.keyCode.HOME:
      this.setValue(this.valueMin);
      flag = true;
      break;

    case this.keyCode.END:
      this.setValue(this.valueMax);
      flag = true;
      break;

    default:
      break;
  }

  if (flag) {
    event.preventDefault();
    event.stopPropagation();
  }
};

SpinButton.prototype.handleIncreaseClick = function (event) {
  this.setValue(this.valueNow + 1);

  event.preventDefault();
  event.stopPropagation();
};

SpinButton.prototype.handleDecreaseClick = function (event) {
  this.setValue(this.valueNow - 1);

  event.preventDefault();
  event.stopPropagation();
};
