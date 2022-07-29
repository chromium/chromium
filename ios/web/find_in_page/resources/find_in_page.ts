// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A Section contains the info of one TEXT node in the |allText_|. The node's
 * textContent is [begin, end) of |allText_|.
 */
class Section {
  begin: number;
  end: number;
  node: Node;

  /**
   * @param {number} begin Beginning index of |node|.textContent in |allText_|.
   * @param {number} end Ending index of |node|.textContent in |allText_|.
   * @param {Node} node The TEXT Node of this section.
   */
  constructor(begin: number, end: number, node: Node) {
    this.begin = begin;
    this.end = end;
    this.node = node;
  }
}

export {Section}
