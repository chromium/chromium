'use strict';
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

function initializeColorSuggestionPicker() {
  new ColorSuggestionPicker(main, global.params);
}

/**
 * @param {!Object} args
 * @return {?string} An error message, or null if the argument has no errors.
 */
function validateColorSuggestionPickerArguments(args) {
  if (!args.shouldShowColorSuggestionPicker)
    return 'Should not be showing the color suggestion picker.';
  if (!args.values)
    return 'No values.';
  if (!args.otherColorLabel)
    return 'No otherColorLabel.';
  return null;
}

function handleArgumentsTimeout() {
  if (global.argumentsReceived)
    return;
  const args = {values: DefaultColorPalette, otherColorLabel: 'Other...'};
  initialize(args);
}

class ColorSuggestionPicker extends Picker {
  constructor(element, config) {
    super(element, config);
    if (this.config_.values.length === 0)
      this.config_.values = DefaultColorPalette;
    this.container_ = null;
    this.layout_();
    document.body.addEventListener('keydown', this.handleKeyDown_.bind(this));
    this.element_.addEventListener(
        'mousemove', this.handleMouseMove_.bind(this));
    this.element_.addEventListener(
        'mousedown', this.handleMouseDown_.bind(this));
  }

  get getColorSwatchWidth_() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--color-swatch-width')
                      .replace('px', ''));
  }

  get getColorSwatchHeight_() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--color-swatch-height')
                      .replace('px', ''));
  }

  get getColorSwatchPadding_() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--color-swatch-padding')
                      .replace('px', ''));
  }

  get getColorSwatchBorderWidth_() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--color-swatch-border-width')
                      .replace('px', ''));
  }

  get getColorSwatchMargin_() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--color-swatch-margin')
                      .replace('px', ''));
  }

  get getSwatchBorderBoxWidth() {
    return (
        this.getColorSwatchWidth_ + this.getColorSwatchPadding_ * 2 +
        this.getColorSwatchBorderWidth_ * 2 + this.getColorSwatchMargin_ * 2);
  }

  get getSwatchBorderBoxHeight() {
    return (
        this.getColorSwatchHeight_ + this.getColorSwatchPadding_ * 2 +
        this.getColorSwatchBorderWidth_ * 2 + this.getColorSwatchMargin_ * 2);
  }

  get getSwatchesPerRow() {
    return 5;
  }

  get getSwatchesMaxRow() {
    return 3;
  }

  get getScrollbarWidth() {
    return Number(window.getComputedStyle(document.body)
                      .getPropertyValue('--scrollbar-width')
                      .replace('px', ''));
  }

  layout_() {
    const container = createElement('div', 'color-swatch-container');
    container.addEventListener(
        'click', this.handleSwatchClick_.bind(this), false);
    for (let i = 0; i < this.config_.values.length; ++i) {
      const swatch = createElement('button', 'color-swatch');
      swatch.dataset.index = i;
      swatch.dataset.value = this.config_.values[i];
      swatch.title = this.config_.values[i];
      swatch.style.backgroundColor = this.config_.values[i];
      container.appendChild(swatch);
    }
    let containerWidth = this.getSwatchBorderBoxWidth * this.getSwatchesPerRow;
    if (this.config_.values.length >
        this.getSwatchesPerRow * this.getSwatchesMaxRow)
      containerWidth += this.getScrollbarWidth;
    container.style.width = containerWidth + 'px';
    container.style.maxHeight =
        this.getSwatchBorderBoxHeight* this.getSwatchesMaxRow + 'px';
    this.element_.appendChild(container);
    const otherButton =
        createElement('button', 'other-color', this.config_.otherColorLabel);
    otherButton.addEventListener(
        'click', this.onOtherButtonClick_.bind(this), false);
    this.element_.appendChild(otherButton);
    this.container_ = container;
    this.otherButton_ = otherButton;
    const elementWidth = this.element_.offsetWidth;
    const elementHeight = this.element_.offsetHeight;
    resizeWindow(elementWidth, elementHeight);
  }

  onOtherButtonClick_() {
    const main = $('main');
    main.innerHTML = '';
    main.classList.remove('color-suggestion-picker-main');
    main.classList.add('color-picker-main');
    // Replace document.body with a deep clone to drop all event listeners.
    const oldBody = document.body;
    const newBody = oldBody.cloneNode(true);
    oldBody.parentElement.replaceChild(newBody, oldBody);
    initializeColorPicker();
  }

  selectColorAtIndex_(index) {
    index = Math.max(Math.min(this.container_.childNodes.length - 1, index), 0);
    this.container_.childNodes[index].focus();
  }

  handleMouseMove_(event) {
    if (event.target.classList.contains('color-swatch'))
      event.target.focus();
  }

  handleMouseDown_(event) {
    // Prevent blur.
    if (event.target.classList.contains('color-swatch'))
      event.preventDefault();
  }

  handleKeyDown_(event) {
    const key = event.key;
    if (key === 'Escape')
      this.handleCancel();
    else if (
        key == 'ArrowLeft' || key == 'ArrowUp' || key == 'ArrowRight' ||
        key == 'ArrowDown') {
      const selectedElement = document.activeElement;
      let index = 0;
      if (selectedElement.classList.contains('other-color')) {
        if (key != 'ArrowRight' && key != 'ArrowUp')
          return;
        index = this.container_.childNodes.length - 1;
      } else if (selectedElement.classList.contains('color-swatch')) {
        index = parseInt(selectedElement.dataset.index, 10);
        switch (key) {
          case 'ArrowLeft':
            index--;
            break;
          case 'ArrowRight':
            index++;
            break;
          case 'ArrowUp':
            index -= this.getSwatchesPerRow;
            break;
          case 'ArrowDown':
            index += this.getSwatchesPerRow;
            break;
        }
        if (index > this.container_.childNodes.length - 1) {
          this.otherButton_.focus();
          return;
        }
      }
      this.selectColorAtIndex_(index);
    }
    event.preventDefault();
  }

  handleSwatchClick_(event) {
    if (event.target.classList.contains('color-swatch'))
      this.submitValue(event.target.dataset.value);
  }
}
