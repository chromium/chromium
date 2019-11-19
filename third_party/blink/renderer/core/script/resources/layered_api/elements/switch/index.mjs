// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as reflection from '../internal/reflection.mjs';

import * as face from './face_utils.mjs';
import * as style from './style.mjs';
import {SwitchTrack} from './track.mjs';

const generateStyleSheet = style.styleSheetFactory();
const generateMaterialStyleSheet = style.materialStyleSheetFactory();

// https://github.com/tkent-google/std-switch/issues/2
const STATE_ATTR = 'on';

function parentOrHostElement(element) {
  const parent = element.parentNode;
  if (!parent) {
    return null;
  }
  if (parent.nodeType === Node.ELEMENT_NODE) {
    return parent;
  }
  if (parent.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
    return parent.host;
  }
  return null;
}

function shouldUsePlatformTheme(element) {
  for (; element; element = parentOrHostElement(element)) {
    const themeValue = element.getAttribute('theme');
    if (themeValue === 'match-platform') {
      return true;
    } else if (themeValue === 'platform-agnostic') {
      return false;
    }
  }
  return false;
}

export class StdSwitchElement extends HTMLElement {
  // TODO(tkent): The following should be |static fooBar = value;|
  // after enabling babel-eslint.
  static get formAssociated() {
    return true;
  }
  static get observedAttributes() {
    return [STATE_ATTR];
  }

  #internals;
  #track;
  #containerElement;
  #inUserAction = false;
  #shadowRoot;

  constructor() {
    super();
    if (new.target !== StdSwitchElement) {
      throw new TypeError(
          'Illegal constructor: StdSwitchElement is not ' +
          'extensible for now');
    }
    this.#internals = this.attachInternals();
    this.#internals.setFormValue('off');
    this.#initializeDOM();

    this.addEventListener('click', this.#onClick);
    this.addEventListener('keypress', this.#onKeyPress);
  }

  attributeChangedCallback(attrName, oldValue, newValue) {
    if (attrName === STATE_ATTR) {
      this.#internals.setFormValue(newValue !== null ? 'on' : 'off');
      this.#track.value = newValue !== null;
      if (this.#internals.ariaChecked !== undefined) {
        this.#internals.ariaChecked = newValue !== null ? 'true' : 'false';
      } else {
        // TODO(tkent): Remove this when we ship AOM.
        this.setAttribute('aria-checked', newValue !== null ? 'true' : 'false');
      }
      if (!this.#inUserAction) {
        for (const element of this.#containerElement.querySelectorAll('*')) {
          style.unmarkTransition(element);
        }
      }
    }
  }

  connectedCallback() {
    // The element might have been disconnected when the callback is invoked.
    if (!this.isConnected) {
      return;
    }

    // TODO(tkent): We should not add tabindex attribute.
    // https://github.com/w3c/webcomponents/issues/762
    if (!this.hasAttribute('tabindex')) {
      this.setAttribute('tabindex', '0');
    }

    if (this.#internals.role !== undefined) {
      this.#internals.role = 'switch';
    } else {
      // TODO(tkent): Remove this when we ship AOM.
      if (!this.hasAttribute('role')) {
        this.setAttribute('role', 'switch');
      }
    }

    if (shouldUsePlatformTheme(this)) {
      // TODO(tkent): Should we apply Cocoa-like on macOS and Fluent-like
      // on Windows?
      this.#shadowRoot.adoptedStyleSheets =
          [generateStyleSheet(), generateMaterialStyleSheet()];
    } else {
      this.#shadowRoot.adoptedStyleSheets = [generateStyleSheet()];
    }
  }

  formResetCallback() {
    this.on = this.defaultOn;
  }

  #initializeDOM = () => {
    const factory = this.ownerDocument;
    const root = this.attachShadow({mode: 'closed'});
    this.#containerElement = factory.createElement('span');
    this.#containerElement.id = 'container';
    // Shadow elements should be invisible for a11y technologies.
    this.#containerElement.setAttribute('aria-hidden', 'true');
    root.appendChild(this.#containerElement);

    this.#track = new SwitchTrack(factory);
    this.#containerElement.appendChild(this.#track.element);
    this.#track.value = this.on;

    const thumbElement =
        this.#containerElement.appendChild(factory.createElement('span'));
    thumbElement.id = 'thumb';
    thumbElement.part.add('thumb');

    this.#shadowRoot = root;
  };

  #onClick = () => {
    for (const element of this.#containerElement.querySelectorAll('*')) {
      style.markTransition(element);
    }
    this.#inUserAction = true;
    try {
      this.on = !this.on;
    } finally {
      this.#inUserAction = false;
    }
    this.dispatchEvent(new Event('input', {bubbles: true}));
    this.dispatchEvent(new Event('change', {bubbles: true}));
  };

  #onKeyPress = event => {
    if (event.code === 'Space') {
      // Do not scroll the page.
      event.preventDefault();
      this.#onClick(event);
    }
  };

  // -------- Boilerplate code for form-associated custom elements --------
  // They can't be in face_utils.mjs because private fields are available
  // only in the class.
  get form() {
    return this.#internals.form;
  }
  get willValidate() {
    return this.#internals.willValidate;
  }
  get validity() {
    return this.#internals.validity;
  }
  get validationMessage() {
    return this.#internals.validationMessage;
  }
  get labels() {
    return this.#internals.labels;
  }
  checkValidity() {
    return this.#internals.checkValidity();
  }
  reportValidity() {
    return this.#internals.reportValidity();
  }
  setCustomValidity(error) {
    if (error === undefined) {
      throw new TypeError('Too few arguments');
    }
    this.#internals.setValidity({customError: true}, error);
  }
}

reflection.installBool(StdSwitchElement.prototype, STATE_ATTR);
reflection.installBool(
    StdSwitchElement.prototype, 'default' + STATE_ATTR,
    'default' + STATE_ATTR.charAt(0).toUpperCase() + STATE_ATTR.substring(1));
face.installProperties(StdSwitchElement.prototype);

// This is necessary for anyObject.toString.call(switchInstance).
Object.defineProperty(StdSwitchElement.prototype, Symbol.toStringTag, {
  configurable: true,
  enumerable: false,
  value: 'StdSwitchElement',
  writable: false,
});

customElements.define('std-switch', StdSwitchElement);
delete StdSwitchElement.formAssociated;
delete StdSwitchElement.observedAttributes;
delete StdSwitchElement.prototype.attributeChangedCallback;
delete StdSwitchElement.prototype.connectedCallback;
delete StdSwitchElement.prototype.formResetCallback;
