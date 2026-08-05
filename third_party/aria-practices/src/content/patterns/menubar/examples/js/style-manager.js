/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   TextStyling.js
 *
 *   Desc:   Styling functions for changing the style of an item
 */

'use strict';

/* exported StyleManager */

class StyleManager {
  constructor(node) {
    this.node = node;
    this.fontSize = 'medium';
  }

  setFontFamily(value) {
    this.node.style.fontFamily = value;
  }

  setTextDecoration(value) {
    this.node.style.textDecoration = value;
  }

  setTextAlign(value) {
    this.node.style.textAlign = value;
  }

  setFontSize(value) {
    this.fontSize = value;
    this.node.style.fontSize = value;
  }

  setColor(value) {
    this.node.style.color = value;
  }

  setBold(flag) {
    if (flag) {
      this.node.style.fontWeight = 'bold';
    } else {
      this.node.style.fontWeight = 'normal';
    }
  }

  setItalic(flag) {
    if (flag) {
      this.node.style.fontStyle = 'italic';
    } else {
      this.node.style.fontStyle = 'normal';
    }
  }

  fontSmaller() {
    switch (this.fontSize) {
      case 'small':
        this.setFontSize('x-small');
        break;

      case 'medium':
        this.setFontSize('small');
        break;

      case 'large':
        this.setFontSize('medium');
        break;

      case 'x-large':
        this.setFontSize('large');
        break;

      default:
        break;
    } // end switch
  }

  fontLarger() {
    switch (this.fontSize) {
      case 'x-small':
        this.setFontSize('small');
        break;

      case 'small':
        this.setFontSize('medium');
        break;

      case 'medium':
        this.setFontSize('large');
        break;

      case 'large':
        this.setFontSize('x-large');
        break;

      default:
        break;
    } // end switch
  }

  isMinFontSize() {
    return this.fontSize === 'x-small';
  }

  isMaxFontSize() {
    return this.fontSize === 'x-large';
  }

  getFontSize() {
    return this.fontSize;
  }

  setOption(option, value) {
    option = option.toLowerCase();
    if (typeof value === 'string') {
      value = value.toLowerCase();
    }

    switch (option) {
      case 'font-bold':
        this.setBold(value);
        break;

      case 'font-color':
        this.setColor(value);
        break;

      case 'font-family':
        this.setFontFamily(value);
        break;

      case 'font-smaller':
        this.fontSmaller();
        break;

      case 'font-larger':
        this.fontLarger();
        break;

      case 'font-size':
        this.setFontSize(value);
        break;

      case 'font-italic':
        this.setItalic(value);
        break;

      case 'text-align':
        this.setTextAlign(value);
        break;

      case 'text-decoration':
        this.setTextDecoration(value);
        break;

      default:
        break;
    } // end switch
  }
}
