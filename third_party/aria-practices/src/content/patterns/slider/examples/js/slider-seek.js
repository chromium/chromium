/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   slider-valuetext.js
 *
 *   Desc:   Slider widgets using aria-valuetext that implements ARIA Authoring Practices
 */

'use strict';

/*
 *   Desc: Slider widget that implements ARIA Authoring Practices
 */

class SliderSeek {
  constructor(domNode) {
    this.domNode = domNode;

    this.isMoving = false;

    this.svgNode = domNode.querySelector('svg');
    this.svgPoint = this.svgNode.createSVGPoint();

    this.railNode = domNode.querySelector('.rail');
    this.sliderNode = domNode.querySelector('[role=slider]');
    this.sliderValueNode = this.sliderNode.querySelector('.value');
    this.sliderFocusNode = this.sliderNode.querySelector('.focus-ring');
    this.sliderThumbNode = this.sliderNode.querySelector('.thumb');

    this.valueLabelNodes = domNode.querySelectorAll('.value-label');

    // Dimensions of the slider focus ring, thumb and rail

    this.railHeight = parseInt(this.railNode.getAttribute('height'));
    this.railWidth = parseInt(this.railNode.getAttribute('width'));
    this.railY = parseInt(this.railNode.getAttribute('y'));
    this.railX = parseInt(this.railNode.getAttribute('x'));

    this.thumbWidth = parseInt(this.sliderThumbNode.getAttribute('width'));
    this.thumbHeight = parseInt(this.sliderThumbNode.getAttribute('height'));

    this.focusHeight = parseInt(this.sliderFocusNode.getAttribute('height'));
    this.focusWidth = parseInt(this.sliderFocusNode.getAttribute('width'));

    this.thumbY = this.railY + this.railHeight / 2 - this.thumbHeight / 2;
    this.sliderThumbNode.setAttribute('y', this.thumbY);

    this.focusY = this.railY + this.railHeight / 2 - this.focusHeight / 2;
    this.sliderFocusNode.setAttribute('y', this.focusY);

    this.railNode.setAttribute('y', this.railY);
    this.railNode.setAttribute('x', this.railX);
    this.railNode.setAttribute('height', this.railHeight);
    this.railNode.setAttribute('width', this.railWidth);

    // define possible slider positions

    this.svgNode.addEventListener('click', this.onRailClick.bind(this));
    this.sliderNode.addEventListener(
      'keydown',
      this.onSliderKeydown.bind(this)
    );

    this.sliderNode.addEventListener(
      'pointerdown',
      this.onSliderPointerDown.bind(this)
    );

    // bind a pointermove event handler to move pointer
    this.svgNode.addEventListener('pointermove', this.onPointerMove.bind(this));

    // bind a pointerup event handler to stop tracking pointer movements
    document.addEventListener('pointerup', this.onPointerUp.bind(this));

    this.sliderNode.addEventListener('focus', this.onSliderFocus.bind(this));
    this.sliderNode.addEventListener('blur', this.onSliderBlur.bind(this));

    let deltaPosition = this.railWidth / (this.valueLabelNodes.length - 1);

    let position = this.railX;

    this.positions = [];
    this.textValues = [];

    let maxTextWidth = this.getWidthFromLabelText();
    let textHeight = this.getHeightFromLabelText();

    for (let i = 0; i < this.valueLabelNodes.length; i++) {
      let valueLabelNode = this.valueLabelNodes[i];

      let textNode = valueLabelNode.querySelector('text');

      let w = maxTextWidth + 2;
      let x = position - w / 2;
      let y = this.thumbY + this.thumbHeight;

      x = x + (maxTextWidth - textNode.getBoundingClientRect().width) / 2;
      y = y + textHeight;

      textNode.setAttribute('x', x);
      textNode.setAttribute('y', y);

      this.textValues.push(valueLabelNode.getAttribute('data-value'));

      this.positions.push(position);
      position += deltaPosition;
    }

    // temporarily show slider value to allow width calc onload
    this.sliderValueNode.setAttribute('style', 'display: block');
    this.moveSliderTo(this.getValue());
    this.sliderValueNode.removeAttribute('style');

    // Include total time in aria-valuetext when loaded
    this.sliderNode.setAttribute(
      'aria-valuetext',
      this.getValueTextMinutesSeconds(this.getValue(), true)
    );
  }

  getWidthFromLabelText() {
    let width = 0;
    for (let i = 0; i < this.valueLabelNodes.length; i++) {
      let textNode = this.valueLabelNodes[i].querySelector('text');
      if (textNode) {
        width = Math.max(width, textNode.getBoundingClientRect().width);
      }
    }
    return width;
  }

  getHeightFromLabelText() {
    let height = 0;
    let textNode = this.valueLabelNodes[0].querySelector('text');
    if (textNode) {
      height = textNode.getBoundingClientRect().height;
    }
    return height;
  }

  // Get point in global SVG space
  getSVGPoint(event) {
    this.svgPoint.x = event.clientX;
    this.svgPoint.y = event.clientY;
    return this.svgPoint.matrixTransform(this.svgNode.getScreenCTM().inverse());
  }

  getValue() {
    return parseInt(this.sliderNode.getAttribute('aria-valuenow'));
  }

  getValueMin() {
    return parseInt(this.sliderNode.getAttribute('aria-valuemin'));
  }

  getValueMax() {
    return parseInt(this.sliderNode.getAttribute('aria-valuemax'));
  }

  isInRange(value) {
    let valueMin = this.getValueMin();
    let valueMax = this.getValueMax();
    return value <= valueMax && value >= valueMin;
  }

  getValueMinutesSeconds(value) {
    let minutes = parseInt(value / 60);
    let seconds = value % 60;

    if (seconds < 10) {
      seconds = '0' + seconds;
    }
    return minutes + ':' + seconds;
  }

  getValueTextMinutesSeconds(value, flag) {
    if (typeof flag !== 'boolean') {
      flag = false;
    }

    let minutes = parseInt(value / 60);
    let seconds = value % 60;

    let valuetext = '';
    let minutesLabel = 'Minutes';
    let secondsLabel = 'Seconds';

    if (minutes === 1) {
      minutesLabel = 'Minute';
    }

    if (minutes > 0) {
      valuetext += minutes + ' ' + minutesLabel;
    }

    if (seconds === 1) {
      secondsLabel = 'Second';
    }

    if (seconds > 0) {
      if (minutes > 0) {
        valuetext += ' ';
      }
      valuetext += seconds + ' ' + secondsLabel;
    }

    if (minutes === 0 && seconds === 0) {
      valuetext += '0 ' + secondsLabel;
    }

    if (flag) {
      let maxValue = parseInt(this.sliderNode.getAttribute('aria-valuemax'));
      valuetext += ' of ' + this.getValueTextMinutesSeconds(maxValue);
    }

    return valuetext;
  }

  moveSliderTo(value) {
    let valueMax, valueMin, pos, width;

    valueMin = this.getValueMin();
    valueMax = this.getValueMax();

    value = Math.min(Math.max(value, valueMin), valueMax);

    this.sliderNode.setAttribute('aria-valuenow', value);

    this.sliderValueNode.textContent = this.getValueMinutesSeconds(value);

    width = this.sliderValueNode.getBoundingClientRect().width;

    this.sliderNode.setAttribute(
      'aria-valuetext',
      this.getValueTextMinutesSeconds(value)
    );

    pos =
      this.railX +
      Math.round(((value - valueMin) * this.railWidth) / (valueMax - valueMin));

    // move the SVG focus ring and thumb elements
    this.sliderFocusNode.setAttribute('x', pos - this.focusWidth / 2);
    this.sliderThumbNode.setAttribute('x', pos - this.thumbWidth / 2);
    this.sliderValueNode.setAttribute('x', pos - width / 2);
  }

  onSliderKeydown(event) {
    var flag = false;
    var value = this.getValue();
    var valueMin = this.getValueMin();
    var valueMax = this.getValueMax();

    switch (event.key) {
      case 'ArrowLeft':
      case 'ArrowDown':
        this.moveSliderTo(value - 1);
        flag = true;
        break;

      case 'ArrowRight':
      case 'ArrowUp':
        this.moveSliderTo(value + 1);
        flag = true;
        break;

      case 'PageDown':
        this.moveSliderTo(value - 15);
        flag = true;
        break;

      case 'PageUp':
        this.moveSliderTo(value + 15);
        flag = true;
        break;

      case 'Home':
        this.moveSliderTo(valueMin);
        flag = true;
        break;

      case 'End':
        this.moveSliderTo(valueMax);
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.preventDefault();
      event.stopPropagation();
    }
  }

  onSliderFocus() {
    this.domNode.classList.add('focus');
  }

  onSliderBlur() {
    this.domNode.classList.remove('focus');
    // Include total time in aria-valuetext
    this.sliderNode.setAttribute(
      'aria-valuetext',
      this.getValueTextMinutesSeconds(this.getValue(), true)
    );
  }

  onRailClick(event) {
    var x = this.getSVGPoint(event).x;
    var min = this.getValueMin();
    var max = this.getValueMax();
    var diffX = x - this.railX;
    var value = Math.round((diffX * (max - min)) / this.railWidth);
    this.moveSliderTo(value);

    event.preventDefault();
    event.stopPropagation();

    // Set focus to the clicked handle
    this.sliderNode.focus();
  }

  onSliderPointerDown(event) {
    this.isMoving = true;

    event.preventDefault();
    event.stopPropagation();

    // Set focus to the clicked handle
    this.sliderNode.focus();
  }

  onPointerMove(event) {
    if (this.isMoving) {
      var x = this.getSVGPoint(event).x;
      var min = this.getValueMin();
      var max = this.getValueMax();
      var diffX = x - this.railX;
      var value = Math.round((diffX * (max - min)) / this.railWidth);
      this.moveSliderTo(value);

      event.preventDefault();
      event.stopPropagation();
    }
  }

  onPointerUp() {
    this.isMoving = false;
  }
}

window.addEventListener('load', function () {
  let sliders = document.querySelectorAll('.slider-seek');
  for (let i = 0; i < sliders.length; i++) {
    new SliderSeek(sliders[i]);
  }
});
