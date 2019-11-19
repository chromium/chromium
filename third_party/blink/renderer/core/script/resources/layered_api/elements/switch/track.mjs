// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export class SwitchTrack {
  #value;
  #trackElement;
  #fillElement;
  #slotElement;

  /**
   * @param {!Document} factory A factory for elements created for this track.
   */
  constructor(factory) {
    this.#value = false;
    this.#initializeDOM(factory);
  }

  /**
   * @return {!Element}
   */
  get element() {
    return this.#trackElement;
  }

  /**
   * @param {Boolean} newValue
   */
  set value(newValue) {
    const oldValue = this.#value;
    this.#value = Boolean(newValue);

    const bar = this.#fillElement;
    if (bar) {
      bar.style.inlineSize = this.#value ? '100%' : '0%';
      if (oldValue !== this.#value) {
        this.#addSlot();
      }
    }
  }

  /**
   * @param {!Document} factory A factory for elements created for this track.
   */
  #initializeDOM = factory => {
    this.#trackElement = factory.createElement('div');
    this.#trackElement.id = 'track';
    this.#trackElement.part.add('track');
    this.#fillElement = factory.createElement('span');
    this.#fillElement.id = 'trackFill';
    this.#fillElement.part.add('track-fill');
    this.#trackElement.appendChild(this.#fillElement);
    this.#slotElement = factory.createElement('slot');
    this.#addSlot();
  };

  /**
   * Add the <slot>
   *   - next to _fillElement if _value is true
   *   - as a child of _fillElement if _value is false
   * This behavior is helpful to show text in the track.
   */
  #addSlot = () => {
    if (this.#value) {
      this.#fillElement.appendChild(this.#slotElement);
    } else {
      this.#trackElement.appendChild(this.#slotElement);
    }
  };
}
