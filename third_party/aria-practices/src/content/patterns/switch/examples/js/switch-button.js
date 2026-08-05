/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:  switch.js
 *
 *   Desc:  Switch widget that implements ARIA Authoring Practices
 */

'use strict';

class ButtonSwitch {
  constructor(domNode) {
    this.switchNode = domNode;
    this.switchNode.addEventListener('click', () => this.toggleStatus());

    // Set background color for the SVG container Rect
    var color = getComputedStyle(this.switchNode).getPropertyValue(
      'background-color'
    );
    var containerNode = this.switchNode.querySelector('rect.container');
    containerNode.setAttribute('fill', color);
  }

  // Switch state of a switch
  toggleStatus() {
    const currentState =
      this.switchNode.getAttribute('aria-checked') === 'true';
    const newState = String(!currentState);

    this.switchNode.setAttribute('aria-checked', newState);
  }
}

// Initialize switches
window.addEventListener('load', function () {
  // Initialize the Switch component on all matching DOM nodes
  Array.from(document.querySelectorAll('button[role^=switch]')).forEach(
    (element) => new ButtonSwitch(element)
  );
});
