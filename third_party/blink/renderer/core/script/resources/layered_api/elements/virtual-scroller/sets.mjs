/**
 * Copyright 2019 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * @fileoverview Utility functions for set operations.
 * @package
 */

/*
 * Returns the set of elements in |a| that are not in |b|.
 *
 * @param {!Set} a A set of elements.
 * @param {!Set} b A set of elements.
 */
export function difference(a, b) {
  const result = new Set();
  for (const element of a) {
    if (!b.has(element)) {
      result.add(element);
    }
  }
  return result;
}

/**
 * Callback applying to an item in a set.
 *
 * @callback applyToDiff
 * @param {any} item
 */

/**
 * Calculates the difference between |oldSet| and |newSet| and applies
 * |deletedFunc| or |addedFunc| to the elements that were deleted or added.
 *
 * Returns the number of elements operated on.
 *
 * @param {!Set} oldSet A set of elements.
 * @param {!Set} newSet A set of elements.
 * @param {applyToDiff} deletedFun A function to be applied to deleted elements.
 * @param {applyToDiff} addedFun A function to be applied to added elements.
 */
export function applyToDiffs(oldSet, newSet, deletedFunc, addedFunc) {
  const deleted = difference(oldSet, newSet);
  const added = difference(newSet, oldSet);
  deleted.forEach(deletedFunc);
  added.forEach(addedFunc);
  return deleted.size + added.size;
}
