/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   quantity-spinbutton.js
 */

'use strict';

class SpinButton {
  constructor(el) {
    this.el = el;
    this.id = el.id;
    this.controls = Array.from(
      document.querySelectorAll(`button[aria-controls="${this.id}"]`)
    );
    this.output = document.querySelector(`output[for="${this.id}"]`);
    this.timer = null;
    this.setBounds();
    el.addEventListener('input', () => this.setValue(el.value, true));
    el.addEventListener('blur', () => {
      if (el.value === '' && this.hasMin) {
        this.setValue(this.min, true);
      }
    });
    el.addEventListener('keydown', (e) => this.handleKey(e));
    this.controls.forEach((btn) =>
      btn.addEventListener('click', () => this.handleClick(btn))
    );
    this.setValue(el.value);
  }

  clamp(n) {
    return Math.min(Math.max(n, this.min), this.max);
  }

  parseValue(raw) {
    const s = String(raw).trim();
    if (!s) return null;
    const n = parseInt(s.replace(/[^\d-]/g, ''), 10);
    return isNaN(n) ? null : n;
  }

  setBounds() {
    const el = this.el;
    this.hasMin = el.hasAttribute('aria-valuemin');
    this.hasMax = el.hasAttribute('aria-valuemax');
    this.min = this.hasMin
      ? +el.getAttribute('aria-valuemin')
      : Number.MIN_SAFE_INTEGER;
    this.max = this.hasMax
      ? +el.getAttribute('aria-valuemax')
      : Number.MAX_SAFE_INTEGER;
  }

  isValid(val) {
    if (val === '' || val === null) return true;
    const numVal = +val;
    return !(
      (this.hasMin && numVal < this.min) ||
      (this.hasMax && numVal > this.max)
    );
  }

  snapValue(val, currentVal) {
    let result = val;

    if (this.hasMin) result = Math.max(result, this.min);
    if (this.hasMax) result = Math.min(result, this.max);
    if (
      (result < currentVal && val > currentVal) ||
      (result > currentVal && val < currentVal)
    ) {
      return currentVal;
    }
    return result;
  }

  setValue(raw, fromInput = false) {
    let val = typeof raw === 'number' ? raw : this.parseValue(raw);

    if (!fromInput) {
      const currentVal = +this.el.value || 0;
      val = this.snapValue(val, currentVal);
    }

    this.el.value = val;
    this.el.ariaValueNow = val;
    this.el.ariaInvalid = !this.isValid(val) ? 'true' : null;
    this.updateButtonStates();
  }

  updateButtonStates() {
    const val = +this.el.value;
    this.controls.forEach((btn) => {
      const op = btn.getAttribute('data-spinbutton-operation');
      btn.ariaDisabled = (
        op === 'decrement' ? val <= this.min : val >= this.max
      )
        ? 'true'
        : null;
    });
  }

  announce() {
    if (!this.output) return;
    this.output.textContent = this.el.value;
    clearTimeout(this.timer);
    this.timer = setTimeout(() => {
      this.output.textContent = '';
      this.timer = null;
    }, this.output.dataset.selfDestruct || 1000);
  }

  handleKey(e) {
    let v = +this.el.value || 0;
    if (e.key === 'ArrowUp' || e.key === 'ArrowDown') {
      e.preventDefault();
      this.setValue(v + (e.key === 'ArrowUp' ? 1 : -1));
    } else if (e.key === 'Home') {
      e.preventDefault();
      this.setValue(this.min);
    } else if (e.key === 'End') {
      e.preventDefault();
      this.setValue(this.max);
    }
  }

  handleClick(btn) {
    const dir =
      btn.getAttribute('data-spinbutton-operation') === 'decrement' ? -1 : 1;
    this.setValue((+this.el.value || 0) + dir);
    this.announce();
  }
}

window.addEventListener('load', () =>
  document
    .querySelectorAll('[role="spinbutton"]')
    .forEach((el) => new SpinButton(el))
);
