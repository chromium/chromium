/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   slider-multithumb.js
 *
 *   Desc:   A dual slider widget that implements ARIA Authoring Practices
 */

'use strict';

class SliderMultithumb {
  constructor(domNode) {
    this.isMoving = false;
    this.movingSliderNode = false;

    this.domNode = domNode;

    this.svgNode = domNode.querySelector('svg');
    this.svgPoint = this.svgNode.createSVGPoint();

    this.railNode = domNode.querySelector('.rail rect');
    this.rangeNode = domNode.querySelector('.range rect');

    this.minSliderPosition = 0;
    this.maxSliderPosition = 0;

    this.minSliderNode = domNode.querySelector('[role=slider].minimum');
    this.maxSliderNode = domNode.querySelector('[role=slider].maximum');

    this.minSliderValueNode = this.minSliderNode.querySelector('.value');
    this.maxSliderValueNode = this.maxSliderNode.querySelector('.value');

    this.minSliderFocusNode = this.minSliderNode.querySelector('.focus-ring');
    this.maxSliderFocusNode = this.maxSliderNode.querySelector('.focus-ring');

    this.minSliderThumbNode = this.minSliderNode.querySelector('.thumb');
    this.maxSliderThumbNode = this.maxSliderNode.querySelector('.thumb');

    // Dimensions of the slider focus ring, thumb and rail

    this.svgWidth = 360;
    this.svgHeight = 100;

    this.valueTop = 24;
    this.valueHeight = this.minSliderValueNode.getBoundingClientRect().height;

    this.railHeight = 6;
    this.railWidth = 300;
    this.railY = 42;
    this.railX = 10;

    this.thumbTop = 31;
    this.thumbHeight = 28;
    this.thumbWidth = 28;
    this.thumb2Width = 2 * this.thumbWidth;
    this.thumbMiddle = this.thumbTop + this.thumbHeight / 2;
    this.thumbBottom = this.thumbTop + this.thumbHeight;

    this.focusOffset = 8;
    this.focusY = this.valueTop - this.valueHeight - this.focusOffset + 2;
    this.focusWidth = this.thumbWidth + 2 * this.focusOffset;
    this.focusHeight = this.thumbBottom - this.focusY + this.focusOffset + 2;
    this.focusRadius = this.focusWidth / 8;

    this.svgNode.setAttribute('width', this.svgWidth);
    this.svgNode.setAttribute('height', this.svgHeight);

    this.minSliderFocusNode.setAttribute('y', Math.max(2, this.focusY));
    this.maxSliderFocusNode.setAttribute('y', Math.max(2, this.focusY));
    this.minSliderFocusNode.setAttribute('width', this.focusWidth);
    this.maxSliderFocusNode.setAttribute('width', this.focusWidth);
    this.minSliderFocusNode.setAttribute('height', this.focusHeight);
    this.maxSliderFocusNode.setAttribute('height', this.focusHeight);
    this.minSliderFocusNode.setAttribute('rx', this.focusRadius);
    this.maxSliderFocusNode.setAttribute('rx', this.focusRadius);

    this.minSliderValueNode.setAttribute('y', this.valueTop);
    this.maxSliderValueNode.setAttribute('y', this.valueTop);

    this.railNode.setAttribute('y', this.railY);
    this.railNode.setAttribute('x', this.railX);
    this.railNode.setAttribute('height', this.railHeight);
    this.railNode.setAttribute('width', this.railWidth + this.thumbWidth);
    this.railNode.setAttribute('rx', this.railHeight / 2);

    this.rangeNode.setAttribute('y', this.railY);
    this.rangeNode.setAttribute('x', this.railX / 2);
    this.rangeNode.setAttribute('height', this.railHeight);
    this.rangeNode.setAttribute('width', 0);

    this.sliderMinValue = this.getValueMin(this.minSliderNode);
    this.sliderMaxValue = this.getValueMax(this.maxSliderNode);
    this.sliderDiffValue = this.sliderMaxValue - this.sliderMinValue;

    this.minSliderRight = 0;
    this.maxSliderLeft = this.railWidth;

    this.minSliderNode.addEventListener(
      'keydown',
      this.onSliderKeydown.bind(this)
    );
    this.minSliderNode.addEventListener(
      'pointerdown',
      this.onSliderPointerdown.bind(this)
    );

    this.minSliderNode.addEventListener('focus', this.onSliderFocus.bind(this));
    this.minSliderNode.addEventListener('blur', this.onSliderBlur.bind(this));

    this.maxSliderNode.addEventListener(
      'keydown',
      this.onSliderKeydown.bind(this)
    );
    this.maxSliderNode.addEventListener(
      'pointerdown',
      this.onSliderPointerdown.bind(this)
    );

    // bind a pointermove event handler to move pointer
    document.addEventListener('pointermove', this.onPointermove.bind(this));

    // bind a pointerup event handler to stop tracking pointer movements
    document.addEventListener('pointerup', this.onPointerup.bind(this));

    this.maxSliderNode.addEventListener('focus', this.onSliderFocus.bind(this));
    this.maxSliderNode.addEventListener('blur', this.onSliderBlur.bind(this));

    this.railNode.addEventListener('click', this.onRailClick.bind(this));

    this.moveSliderTo(this.minSliderNode, this.getValue(this.minSliderNode));
    this.moveSliderTo(this.maxSliderNode, this.getValue(this.maxSliderNode));
  }

  getSVGPoint(event) {
    this.svgPoint.x = event.clientX;
    this.svgPoint.y = event.clientY;
    return this.svgPoint.matrixTransform(this.svgNode.getScreenCTM().inverse());
  }

  getValue(sliderNode) {
    return parseInt(sliderNode.getAttribute('aria-valuenow'));
  }

  getValueMin(sliderNode) {
    return parseInt(sliderNode.getAttribute('aria-valuemin'));
  }

  getValueMax(sliderNode) {
    return parseInt(sliderNode.getAttribute('aria-valuemax'));
  }

  isMinSlider(sliderNode) {
    return this.minSliderNode === sliderNode;
  }

  isInRange(sliderNode, value) {
    const valueMin = this.getValueMin(sliderNode);
    const valueMax = this.getValueMax(sliderNode);
    return value <= valueMax && value >= valueMin;
  }

  isOutOfRange(value) {
    const valueMin = this.getValueMin(this.minSliderNode);
    const valueMax = this.getValueMax(this.maxSliderNode);
    return value > valueMax || value < valueMin;
  }

  getXFromThumb(node) {
    const points = node.getAttribute('points').split(',');
    return parseInt(points[0]);
  }

  moveSliderTo(sliderNode, value) {
    let valueMax;
    let valueMin;
    let pos;
    let points = '';
    let x;
    let width;

    if (this.isMinSlider(sliderNode)) {
      valueMin = this.getValueMin(this.minSliderNode);
      valueMax = this.getValueMax(this.minSliderNode);
    } else {
      valueMin = this.getValueMin(this.maxSliderNode);
      valueMax = this.getValueMax(this.maxSliderNode);
    }

    value = Math.min(Math.max(value, valueMin), valueMax);

    sliderNode.setAttribute('aria-valuenow', value);
    const dollarValue = `$${value}`;

    pos = this.railX;
    pos += Math.round(
      (value * (this.railWidth - this.thumbWidth)) /
        (this.sliderMaxValue - this.sliderMinValue)
    );

    if (this.isMinSlider(sliderNode)) {
      this.minSliderPosition = pos;

      // update ARIA attributes
      this.minSliderValueNode.textContent = dollarValue;
      this.maxSliderNode.setAttribute('aria-valuemin', value);

      // move the SVG focus ring and thumb elements
      x = pos - this.focusOffset - 1;
      this.minSliderFocusNode.setAttribute('x', x);

      points = `${pos},${this.thumbTop}`;
      points += ` ${pos + this.thumbWidth},${this.thumbMiddle}`;
      points += ` ${pos},${this.thumbBottom}`;
      this.minSliderThumbNode.setAttribute('points', points);

      // Position value
      width = this.minSliderValueNode.getBoundingClientRect().width;
      pos += (this.thumbWidth - width) / 2;
      if (pos + width > this.maxSliderLeft - 2) {
        pos = this.maxSliderLeft - width - 2;
      }
      this.minSliderValueNode.setAttribute('x', pos);
      this.minSliderRight = pos;
    } else {
      // update label and ARIA attributes
      this.maxSliderPosition = pos;

      this.maxSliderValueNode.textContent = dollarValue;
      this.minSliderNode.setAttribute('aria-valuemax', value);

      // move the SVG focus ring and thumb elements
      x = pos + this.thumbWidth - this.focusOffset + 1;
      this.maxSliderFocusNode.setAttribute('x', x);

      points = `${pos + this.thumbWidth},${this.thumbMiddle}`;
      points += ` ${pos + this.thumb2Width},${this.thumbTop}`;
      points += ` ${pos + this.thumb2Width},${this.thumbBottom}`;
      this.maxSliderThumbNode.setAttribute('points', points);

      width = Math.max(
        0,
        this.maxSliderValueNode.getBoundingClientRect().width
      );
      pos = pos + this.thumbWidth + (this.thumbWidth - width) / 2;
      if (pos - width < this.minSliderRight + 2) {
        pos = this.minSliderRight + width + 2;
      }

      this.maxSliderValueNode.setAttribute('x', pos);
      this.maxSliderLeft = pos;
    }

    // Set range rect

    x = this.getXFromThumb(this.minSliderThumbNode) + this.thumbWidth / 2;
    width =
      this.getXFromThumb(this.maxSliderThumbNode) - x + this.thumbWidth / 2;
    this.rangeNode.setAttribute('x', x);
    this.rangeNode.setAttribute('width', width);
  }

  onSliderKeydown(event) {
    let flag = false;
    const sliderNode = event.currentTarget;
    const value = this.getValue(sliderNode);
    const valueMin = this.getValueMin(sliderNode);
    const valueMax = this.getValueMax(sliderNode);

    switch (event.key) {
      case 'ArrowLeft':
      case 'ArrowDown':
        this.moveSliderTo(sliderNode, value - 1);
        flag = true;
        break;

      case 'ArrowRight':
      case 'ArrowUp':
        this.moveSliderTo(sliderNode, value + 1);
        flag = true;
        break;

      case 'PageDown':
        this.moveSliderTo(sliderNode, value - 10);
        flag = true;
        break;

      case 'PageUp':
        this.moveSliderTo(sliderNode, value + 10);
        flag = true;
        break;

      case 'Home':
        this.moveSliderTo(sliderNode, valueMin);
        flag = true;
        break;

      case 'End':
        this.moveSliderTo(sliderNode, valueMax);
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

  onSliderFocus(event) {
    event.currentTarget.classList.add('focus');
    this.svgNode.classList.add('active');
  }

  onSliderBlur(event) {
    event.currentTarget.classList.remove('focus');
    this.svgNode.classList.remove('active');
  }

  onSliderPointerdown(event) {
    this.isMoving = true;
    this.movingSliderNode = event.currentTarget;
    this.isMinSliderMoving = this.isMinSlider(event.currentTarget);

    event.preventDefault();
    event.stopPropagation();

    // Set focus to the clicked handle
    this.movingSliderNode.focus();
  }

  onPointermove(event) {
    if (
      this.isMoving &&
      this.movingSliderNode &&
      this.domNode.contains(event.target)
    ) {
      let x = this.getSVGPoint(event).x - this.railX;
      if (this.isMinSliderMoving) {
        x = Math.max(0, x - this.thumbWidth / 3);
      } else {
        x = Math.max(0, x - (5 * this.thumbWidth) / 3);
      }
      x = Math.min(x, this.railWidth - this.thumbWidth);
      const value = Math.round(
        (x * this.sliderDiffValue) / (this.railWidth - this.thumbWidth)
      );
      this.moveSliderTo(this.movingSliderNode, value);

      event.preventDefault();
      event.stopPropagation();
    }
  }

  onPointerup() {
    this.isMoving = false;
    this.movingSliderNode = false;
  }

  // handle click event on the rail
  onRailClick(event) {
    const x = this.getSVGPoint(event).x - this.railX;
    const diffMin = x - this.minSliderPosition - (3 * this.thumbWidth) / 2;
    const diffMax = this.maxSliderPosition - x;
    const sliderNode =
      (x < this.minSliderPosition || diffMin < diffMax) &&
      x < this.maxSliderPosition
        ? this.minSliderNode
        : this.maxSliderNode;

    let p = Math.max(0, x - this.thumbWidth);
    p = Math.min(p, this.railWidth - this.thumbWidth);
    const value = Math.round(
      (p * this.sliderDiffValue) / (this.railWidth - this.thumbWidth)
    );

    this.moveSliderTo(sliderNode, value);

    event.preventDefault();
    event.stopPropagation();
  }
}

// Initialize Multithumb Slider widgets on the page
window.addEventListener('load', () => {
  const slidersMultithumb = document.querySelectorAll('.slider-multithumb');

  for (let i = 0; i < slidersMultithumb.length; i++) {
    new SliderMultithumb(slidersMultithumb[i]);
  }
});
