'use strict';
/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   slider-color-viewer.js
 *
 *   Desc:   ColorViewerSliders widget that implements ARIA Authoring Practices
 */

// Create ColorViewerSliders that contains value, valuemin, valuemax, and valuenow
class ColorViewerSliders {
  constructor(domNode) {
    this.domNode = domNode;

    this.pointerSlider = false;

    this.sliders = {};

    this.svgWidth = 310;
    this.svgHeight = 50;
    this.borderWidth = 2;

    this.valueY = 20;

    this.railX = 15;
    this.railY = 26;
    this.railWidth = 275;
    this.railHeight = 14;

    this.thumbHeight = this.railHeight;
    this.thumbWidth = this.thumbHeight;
    this.rectRadius = this.railHeight / 4;

    this.focusY = this.borderWidth;
    this.focusWidth = 36;
    this.focusHeight = 48;

    this.initSliderRefs(this.sliders, 'red');
    this.initSliderRefs(this.sliders, 'green');
    this.initSliderRefs(this.sliders, 'blue');

    document.body.addEventListener(
      'pointerup',
      this.onThumbPointerUp.bind(this)
    );

    this.colorBoxNode = domNode.querySelector('.color-box');
    this.colorValueHexNode = domNode.querySelector('input.color-value-hex');
    this.colorValueRGBNode = domNode.querySelector('input.color-value-rgb');
  }

  initSliderRefs(sliderRef, color) {
    sliderRef[color] = {};
    var n = this.domNode.querySelector('.color-slider.' + color);
    sliderRef[color].sliderNode = n;

    sliderRef[color].svgNode = n.querySelector('svg');
    sliderRef[color].svgNode.setAttribute('width', this.svgWidth);
    sliderRef[color].svgNode.setAttribute('height', this.svgHeight);
    sliderRef[color].svgPoint = sliderRef[color].svgNode.createSVGPoint();

    sliderRef[color].valueNode = n.querySelector('.value');
    sliderRef[color].valueNode.setAttribute('y', this.valueY);

    sliderRef[color].thumbNode = n.querySelector('.thumb');
    sliderRef[color].thumbNode.setAttribute('width', this.thumbWidth);
    sliderRef[color].thumbNode.setAttribute('height', this.thumbHeight);
    sliderRef[color].thumbNode.setAttribute('y', this.railY);
    sliderRef[color].thumbNode.setAttribute('rx', this.rectRadius);

    sliderRef[color].focusNode = n.querySelector('.focus');
    sliderRef[color].focusNode.setAttribute(
      'width',
      this.focusWidth - this.borderWidth
    );
    sliderRef[color].focusNode.setAttribute(
      'height',
      this.focusHeight - this.borderWidth
    );
    sliderRef[color].focusNode.setAttribute('y', this.focusY);
    sliderRef[color].focusNode.setAttribute('rx', this.rectRadius);

    sliderRef[color].railNode = n.querySelector('.color-slider .rail');
    sliderRef[color].railNode.setAttribute('x', this.railX);
    sliderRef[color].railNode.setAttribute('y', this.railY);
    sliderRef[color].railNode.setAttribute('width', this.railWidth);
    sliderRef[color].railNode.setAttribute('height', this.railHeight);
    sliderRef[color].railNode.setAttribute('rx', this.rectRadius);

    sliderRef[color].fillNode = n.querySelector('.color-slider .fill');
    sliderRef[color].fillNode.setAttribute('x', this.railX);
    sliderRef[color].fillNode.setAttribute('y', this.railY);
    sliderRef[color].fillNode.setAttribute('width', this.railWidth);
    sliderRef[color].fillNode.setAttribute('height', this.railHeight);
    sliderRef[color].fillNode.setAttribute('rx', this.rectRadius);
  }

  // Initialize slider
  init() {
    for (var slider in this.sliders) {
      if (this.sliders[slider].sliderNode.tabIndex != 0) {
        this.sliders[slider].sliderNode.tabIndex = 0;
      }

      this.sliders[slider].railNode.addEventListener(
        'click',
        this.onRailClick.bind(this)
      );

      this.sliders[slider].sliderNode.addEventListener(
        'keydown',
        this.onSliderKeyDown.bind(this)
      );

      this.sliders[slider].sliderNode.addEventListener(
        'pointerdown',
        this.onThumbPointerDown.bind(this)
      );

      this.sliders[slider].valueNode.addEventListener(
        'keydown',
        this.onSliderKeyDown.bind(this)
      );

      this.sliders[slider].valueNode.addEventListener(
        'pointerdown',
        this.onThumbPointerDown.bind(this)
      );

      this.sliders[slider].sliderNode.addEventListener(
        'pointermove',
        this.onThumbPointerMove.bind(this)
      );

      this.moveSliderTo(
        this.sliders[slider],
        this.getValueNow(this.sliders[slider])
      );
    }
  }

  // Get point in global SVG space
  getSVGPoint(slider, event) {
    slider.svgPoint.x = event.clientX;
    slider.svgPoint.y = event.clientY;
    return slider.svgPoint.matrixTransform(
      slider.svgNode.getScreenCTM().inverse()
    );
  }

  getSlider(domNode) {
    if (!domNode.classList.contains('color-slider')) {
      if (domNode.tagName.toLowerCase() === 'rect') {
        domNode = domNode.parentNode.parentNode;
      } else {
        domNode = domNode.parentNode.querySelector('.color-slider');
      }
    }

    if (this.sliders.red.sliderNode === domNode) {
      return this.sliders.red;
    }

    if (this.sliders.green.sliderNode === domNode) {
      return this.sliders.green;
    }

    return this.sliders.blue;
  }

  getValueMin(slider) {
    return parseInt(slider.sliderNode.getAttribute('aria-valuemin'));
  }

  getValueNow(slider) {
    return parseInt(slider.sliderNode.getAttribute('aria-valuenow'));
  }

  getValueMax(slider) {
    return parseInt(slider.sliderNode.getAttribute('aria-valuemax'));
  }

  moveSliderTo(slider, value) {
    var pos, offsetX, valueWidth;
    var valueMin = this.getValueMin(slider);
    var valueNow = this.getValueNow(slider);
    var valueMax = this.getValueMax(slider);

    value = Math.min(Math.max(value, valueMin), valueMax);

    valueNow = value;
    slider.sliderNode.setAttribute('aria-valuenow', value);

    offsetX = Math.round(
      (valueNow * (this.railWidth - this.thumbWidth)) / (valueMax - valueMin)
    );

    pos = this.railX + offsetX;

    slider.thumbNode.setAttribute('x', pos);
    slider.fillNode.setAttribute('width', offsetX + this.rectRadius);

    slider.valueNode.textContent = valueNow;
    valueWidth = slider.valueNode.getBBox().width;

    pos = this.railX + offsetX - (valueWidth - this.thumbWidth) / 2;
    slider.valueNode.setAttribute('x', pos);

    pos = this.railX + offsetX - (this.focusWidth - this.thumbWidth) / 2;
    slider.focusNode.setAttribute('x', pos);

    this.updateColorBox();
  }

  onSliderKeyDown(event) {
    var flag = false;

    var slider = this.getSlider(event.currentTarget);

    var valueMin = this.getValueMin(slider);
    var valueNow = this.getValueNow(slider);
    var valueMax = this.getValueMax(slider);

    switch (event.key) {
      case 'Left':
      case 'ArrowLeft':
      case 'Down':
      case 'ArrowDown':
        this.moveSliderTo(slider, valueNow - 1);
        flag = true;
        break;

      case 'Right':
      case 'ArrowRight':
      case 'Up':
      case 'ArrowUp':
        this.moveSliderTo(slider, valueNow + 1);
        flag = true;
        break;

      case 'PageDown':
        this.moveSliderTo(slider, valueNow - 10);
        flag = true;
        break;

      case 'PageUp':
        this.moveSliderTo(slider, valueNow + 10);
        flag = true;
        break;

      case 'Home':
        this.moveSliderTo(slider, valueMin);
        flag = true;
        break;

      case 'End':
        this.moveSliderTo(slider, valueMax);
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

  onThumbPointerDown(event) {
    this.pointerSlider = this.getSlider(event.currentTarget);

    // Set focus to the clicked on
    this.pointerSlider.sliderNode.focus();

    event.preventDefault();
    event.stopPropagation();
  }

  onThumbPointerUp() {
    this.pointerSlider = false;
  }

  onThumbPointerMove(event) {
    if (
      this.pointerSlider &&
      this.pointerSlider.sliderNode.contains(event.target)
    ) {
      let x = this.getSVGPoint(this.pointerSlider, event).x;
      let min = this.getValueMin(this.pointerSlider);
      let max = this.getValueMax(this.pointerSlider);
      let diffX = x - this.railX;
      let value = Math.round((diffX * (max - min)) / this.railWidth);
      this.moveSliderTo(this.pointerSlider, value);

      event.preventDefault();
      event.stopPropagation();
    }
  }

  // handle click event on the rail
  onRailClick(event) {
    var slider = this.getSlider(event.currentTarget);

    var x = this.getSVGPoint(slider, event).x;
    var min = this.getValueMin(slider);
    var max = this.getValueMax(slider);
    var diffX = x - this.railX;
    var value = Math.round((diffX * (max - min)) / this.railWidth);
    this.moveSliderTo(slider, value);

    event.preventDefault();
    event.stopPropagation();

    // Set focus to the clicked handle
    slider.sliderNode.focus();
  }

  getColorHex() {
    var r = parseInt(
      this.sliders.red.sliderNode.getAttribute('aria-valuenow')
    ).toString(16);
    var g = parseInt(
      this.sliders.green.sliderNode.getAttribute('aria-valuenow')
    ).toString(16);
    var b = parseInt(
      this.sliders.blue.sliderNode.getAttribute('aria-valuenow')
    ).toString(16);

    if (r.length === 1) {
      r = '0' + r;
    }
    if (g.length === 1) {
      g = '0' + g;
    }
    if (b.length === 1) {
      b = '0' + b;
    }

    return '#' + r + g + b;
  }

  getColorRGB() {
    var r = this.sliders.red.sliderNode.getAttribute('aria-valuenow');
    var g = this.sliders.green.sliderNode.getAttribute('aria-valuenow');
    var b = this.sliders.blue.sliderNode.getAttribute('aria-valuenow');

    return r + ', ' + g + ', ' + b;
  }

  updateColorBox() {
    if (this.colorBoxNode) {
      this.colorBoxNode.style.backgroundColor = this.getColorHex();
    }

    if (this.colorValueHexNode) {
      this.colorValueHexNode.value = this.getColorHex();
    }

    if (this.colorValueRGBNode) {
      this.colorValueRGBNode.value = this.getColorRGB();
    }
  }
}
// Initialize ColorViewerSliders on the page
window.addEventListener('load', function () {
  var cps = document.querySelectorAll('.color-viewer-sliders');
  for (let i = 0; i < cps.length; i++) {
    let s = new ColorViewerSliders(cps[i]);
    s.init();
  }
});
