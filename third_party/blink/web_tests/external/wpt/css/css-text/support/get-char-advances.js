'use strict';

/**
 * Returns an array of advances for all characters in the descendants
 * of the specified element.
 *
 * Technically speaking, advances and glyph bounding boxes are different things,
 * and advances are not exposed. This function computes approximate values from
 * bounding boxes.
 */
function getCharAdvances(element) {
  const range = document.createRange();
  const advances = [];
  let origin = undefined;
  let blockEnd = -1;
  function walk(element) {
    for (const node of element.childNodes) {
      const nodeType = node.nodeType;
      if (nodeType === Node.TEXT_NODE) {
        const text = node.nodeValue;
        for (let i = 0; i < text.length; ++i) {
          range.setStart(node, i);
          range.setEnd(node, i + 1);
          const bounds = range.getBoundingClientRect();
          // Check if this is on the same line.
          if (bounds.top >= blockEnd) {
            origin = undefined;
            blockEnd = bounds.bottom;
          }
          // For the first character, the left bound is closest to the origin.
          if (origin === undefined) origin = bounds.left;
          // The right bound is a good approximation of the next origin.
          advances.push(bounds.right - origin);
          origin = bounds.right;
        }
      } else if (nodeType === Node.ELEMENT_NODE) {
        walk(node);
      }
    }
  }
  walk(element);
  return advances;
}
