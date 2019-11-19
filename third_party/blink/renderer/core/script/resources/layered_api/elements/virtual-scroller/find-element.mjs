/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Utilities for binary searching by layed-out pixed offset in a
 * list of elements.
 * @package
 */

/** Symbols for use with @see findElement */
export const BIAS_LOW = Symbol('BIAS_LOW');
export const BIAS_HIGH = Symbol('BIAS_HIGH');

function getBound(elements, edgeIndex) {
  const element = elements[Math.floor(edgeIndex / 2)];
  const rect = element.getBoundingClientRect();
  return edgeIndex % 2 ? rect.bottom : rect.top;
}

/**
 * Does the actual work of binary searching. This searches amongst the 2*N edges
 * of the N elements. Returns the index of an edge found, 2i is the low edge of
 * the ith element, 2i+1 is the high edge of the ith element. If |bias| is low
 * then we find the index of the lowest edge >= offset. Otherwise we find index
 * of the highest edge > offset.
 */
function findEdgeIndex(elements, offset, bias) {
  let low = 0;
  let high = elements.length * 2 - 1;
  while (low < high) {
    const i = Math.floor((low + high) / 2);
    const bound = getBound(elements, i);
    if (bias === BIAS_LOW) {
      if (bound < offset) {
        low = i + 1;
      } else {
        high = i;
      }
    } else {
      if (offset < bound) {
        high = i;
      } else {
        low = i + 1;
      }
    }
  }
  return low;
}

/**
 * Binary searches inside the array |elements| to find an element containing or
 * nearest to |offset| (based on @see Element#getBoundingClientRect()). Assumes
 * that the elements are already sorted in increasing pixel order. |bias|
 * controls what happens if |offset| is not contained within any element or if
 * |offset| is contained with 2 elements (this only happens if there is no
 * margin between the elements). If |bias| is BIAS_LOW, then this selects the
 * lower element nearest |offset|, otherwise it selects the higher element.
 *
 * Returns null if |offset| is not within any element.
 *
 * @param {!Element[]} elements An array of Elements in display order,
 *     i.e. the pixel offsets of later element are higher than those of earlier
 *     elements.
 * @param {!number} offset The target offset in pixels to search for.
 * @param {!Symbol} bias Controls whether we prefer a higher or lower element
 *     when there is a choice between two elements.
 */
export function findElement(elements, offset, bias) {
  if (elements.length === 0) {
    return null;
  }
  // Check if the offset is outside the range entirely.
  if (offset < getBound(elements, 0) ||
      offset > getBound(elements, elements.length * 2 - 1)) {
    return null;
  }

  let edgeIndex = findEdgeIndex(elements, offset, bias);

  // Fix up edge cases.
  if (bias === BIAS_LOW) {
    // bound(0)..bound(edgeIndex) < offset <= bound(edgeIndex+1) ...
    // If we bias low and we got a low edge and we weren't exactly on the edge
    // then we want to select the element that's lower.
    if (edgeIndex % 2 === 0) {
      const bound = getBound(elements, edgeIndex);
      if (offset < bound) {
        edgeIndex--;
      }
    }
  } else {
    // bound(0)..bound(edgeIndex - 1) <= offset < bound(edgeIndex) ...
    // If we bias high and we got a low edge, we need to check if we were
    // exactly on the edge of the previous element.
    if (edgeIndex % 2 === 0) {
      const bound = getBound(elements, edgeIndex - 1);
      if (offset === bound) {
        edgeIndex--;
      }
    }
  }
  return elements[Math.floor(edgeIndex / 2)];
}
