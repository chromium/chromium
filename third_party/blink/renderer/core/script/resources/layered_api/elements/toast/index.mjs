/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview This file defines the class for the Standard Toast LAPI
 * and the accompanying showToast() function.
 * EXPLAINER: https://github.com/jackbsteinberg/std-toast
 * TEST PATH: /chromium/src/third_party/blink/web_tests/external/wpt/std-toast/*
 * @package
 */

import * as reflection from '../internal/reflection.mjs';

const DEFAULT_DURATION = 3000;
const TYPES = new Set(['success', 'warning', 'error']);

function styleSheetFactory() {
  let stylesheet;
  return () => {
    if (!stylesheet) {
      stylesheet = new CSSStyleSheet();
      stylesheet.replaceSync(`
        :host {
          position: fixed;
          bottom: 1em;
          right: 1em;
          border: solid;
          padding: 1em;
          background: white;
          color: black;
          z-index: 1;
        }

        :host(:not([open])) {
          display: none;
        }

        .default-closebutton {
          user-select: none;
        }

        :host([type=success i]) {
          border-color: green;
        }

        :host([type=warning i]) {
          border-color: orange;
        }

        :host([type=error i]) {
          border-color: red;
        }
      `);
      // TODO(jacksteinberg): use offset-block-end: / offset-inline-end: over
      // bottom: / right: when implemented http://crbug.com/538475.
    }
    return stylesheet;
  };
}

const generateStyleSheet = styleSheetFactory();

export class StdToastElement extends HTMLElement {
  static observedAttributes = ['open', 'closebutton'];
  #shadow = this.attachShadow({mode: 'closed'});
  #timeoutID;
  #actionSlot;
  #closeButtonElement;
  #setCloseTimeout = duration => {
    clearTimeout(this.#timeoutID);

    if (duration === Infinity) {
      this.#timeoutID = null;
    } else {
      this.#timeoutID = setTimeout(() => {
        this.removeAttribute('open');
      }, duration);
    }
  };

  constructor(message) {
    super();

    this.#shadow.adoptedStyleSheets = [generateStyleSheet()];

    this.#shadow.appendChild(document.createElement('slot'));

    this.#actionSlot = document.createElement('slot');
    this.#actionSlot.setAttribute('name', 'action');
    this.#shadow.appendChild(this.#actionSlot);

    this.#closeButtonElement = document.createElement('button');
    this.#closeButtonElement.setAttribute('part', 'closebutton');
    setDefaultCloseButton(this.#closeButtonElement);
    this.#shadow.appendChild(this.#closeButtonElement);

    this.#closeButtonElement.addEventListener('click', () => {
      this.hide();
    });

    if (message !== undefined) {
      this.textContent = message;
    }
  }

  connectedCallback() {
    if (!this.hasAttribute('role')) {
      this.setAttribute('role', 'status');
    }
    // TODO(jacksteinberg): use https://github.com/whatwg/html/pull/4658
    // when implemented
  }

  get action() {
    return this.#actionSlot.assignedNodes().length !== 0 ?
        this.#actionSlot.assignedNodes()[0] :
        null;
  }

  set action(val) {
    const previousAction = this.action;
    if (val !== null) {
      if (!isElement(val)) {
        throw new TypeError('Invalid argument: must be type Element');
      }

      val.setAttribute('slot', 'action');
      this.insertBefore(val, previousAction);
    }

    if (previousAction !== null) {
      previousAction.remove();
    }
  }

  get closeButton() {
    if (this.hasAttribute('closebutton')) {
      const closeAttr = this.getAttribute('closebutton');
      return closeAttr === '' ? true : closeAttr;
    }
    return false;
  }

  set closeButton(val) {
    if (val === true) {
      this.setAttribute('closebutton', '');
    } else if (val === false) {
      this.removeAttribute('closebutton');
    } else {
      this.setAttribute('closebutton', val);
    }
  }

  get type() {
    const typeAttr = this.getAttribute('type');
    if (typeAttr === null) {
      return '';
    }

    const typeAttrLower = typeAttr.toLowerCase();

    if (TYPES.has(typeAttrLower)) {
      return typeAttrLower;
    }

    return '';
  }

  set type(val) {
    this.setAttribute('type', val);
  }

  show({duration = DEFAULT_DURATION} = {}) {
    if (duration <= 0) {
      throw new RangeError(
          `Invalid Argument: duration must be greater ` +
          `than 0 [${duration} given]`);
    }

    this.setAttribute('open', '');
    this.#setCloseTimeout(duration);
  }

  hide() {
    this.removeAttribute('open');
  }

  toggle(force) {
    this.toggleAttribute('open', force);
  }

  attributeChangedCallback(name, oldValue, newValue) {
    switch (name) {
      case 'open':
        if (newValue !== null && oldValue === null) {
          this.dispatchEvent(new Event('show'));
        } else if (newValue === null) {
          this.dispatchEvent(new Event('hide'));
          this.#setCloseTimeout(Infinity);
        }
        break;
      case 'closebutton':
        if (newValue !== null) {
          if (newValue === '') {
            setDefaultCloseButton(this.#closeButtonElement);
          } else {
            replaceDefaultCloseButton(this.#closeButtonElement, newValue);
          }
        }
        // if newValue === null we do nothing, since CSS will hide the button
        break;
    }
  }
}

reflection.installBool(StdToastElement.prototype, 'open');

customElements.define('std-toast', StdToastElement);

delete StdToastElement.prototype.attributeChangedCallback;
delete StdToastElement.prototype.observedAttributes;
delete StdToastElement.prototype.connectedCallback;

export function showToast(message, options = {}) {
  const toast = new StdToastElement(message);

  const {action, closeButton, type, ...showOptions} = options;

  if (isElement(action)) {
    toast.action = action;
  } else if (action !== undefined) {
    const actionButton = document.createElement('button');

    // Unlike String(), this performs the desired JavaScript ToString operation.
    // https://gist.github.com/domenic/82adbe7edc4a33a70f42f255479cec39
    actionButton.textContent = `${action}`;

    actionButton.setAttribute('slot', 'action');
    toast.appendChild(actionButton);
  }

  if (closeButton !== undefined) {
    toast.closeButton = closeButton;
  }

  if (type !== undefined) {
    toast.type = type;
  }

  document.body.append(toast);
  toast.show(showOptions);

  return toast;
}

const idGetter = Object.getOwnPropertyDescriptor(Element.prototype, 'id').get;
function isElement(value) {
  try {
    idGetter.call(value);
    return true;
  } catch {
    return false;
  }
}

function setDefaultCloseButton(closeButton) {
  closeButton.setAttribute('aria-label', 'close');
  closeButton.setAttribute('class', 'default-closebutton');
  closeButton.textContent = '×';
}

function replaceDefaultCloseButton(closeButton, value) {
  closeButton.textContent = value;
  closeButton.removeAttribute('aria-label');
  closeButton.removeAttribute('class');
}
