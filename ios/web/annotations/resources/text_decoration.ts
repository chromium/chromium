// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles one node decoration on the web page.
 */

import {HTMLElementWithSymbolIndex, NodeWithSymbolIndex, TextWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';

// Tags for on an `Element` part of an applied `TextDecoration`.
const originalNodeDecorationId = Symbol('originalNodeDecorationId');
const replacementNodeDecorationId = Symbol('replacementNodeDecorationId');

// Tags for on a CHROME_ANNOTATION id.
const annotationUniqueId = Symbol('annotationUniqueId');
// Tags for on a CHROME_ANNOTATION type.
const annotationType = Symbol('annotationType');
// Tags for on a CHROME_ANNOTATION original full text.
const annotationFullText = Symbol('annotationFullText');
// Tags for on a CHROME_ANNOTATION external data string.
const annotationExternalData = Symbol('annotationExternalData');

type ReplacementWithSymbolIndex =
    NodeWithSymbolIndex|HTMLElementWithSymbolIndex;

// Creates a CHROME_ANNOTATION `HTMLElement` with `textContent` as text.
// Attaches `id`, `type`, `data` and `fullText` to `annotationUniqueId`,
// `annotationType`, `annotationExternalData` and `annotationFullText`,
// respectively.
function createChromeAnnotation(
    id: number, textContent: string, type: string, fullText: string,
    data: string): HTMLElementWithSymbolIndex {
  const element =
      document.createElement('chrome_annotation') as HTMLElementWithSymbolIndex;
  element[annotationUniqueId] = id;
  element[annotationType] = type;
  element[annotationExternalData] = data;
  element[annotationFullText] = fullText;
  // Use textContent not innerText, since setting innerText will cause
  // the text to be parsed and '\n' to be upgraded to <br>.
  element.textContent = textContent;
  return element;
}

// Creates a <span>> with a single space. Attaches `id` and `type`.
function createSpace(id: number, type: string): HTMLElementWithSymbolIndex {
  const element = document.createElement('span') as HTMLElementWithSymbolIndex;
  element[annotationUniqueId] = id;
  element[annotationType] = type;
  element.textContent = ' ';
  return element;
}

// Returns `true` if given `node` is either an original node or a replacement
// node.
function isDecorationNode(node: NodeWithSymbolIndex): boolean {
  return !!node[originalNodeDecorationId] ||
      !!node[replacementNodeDecorationId];
}

// A `TextDecoration` is an `originalTextNode` Node and the list of `Node`s that
// make up a similar replacement. Not all `replacements` nodes have to come from
// the same annotation like, for example, a long paragraph with a date and an
// address in it. Note that either `originalTextNode` or `replacements` are live
// in the DOM at any time. If `apply` was called, it's `replacements` and nodes
// are tagged with `originalNodeDecorationId` and `replacementNodeDecorationId`.
class TextDecoration {
  // An `originalTextNode` Node and the list of `Node`s that make up a similar
  // replacement. Note that `originalTextNode` is a text node, but replacements
  // can be any node type (in reality a CHROME_ANNOTATION or a text node).
  constructor(
      public id: number, public originalTextNode: TextWithSymbolIndex,
      public replacements: ReplacementWithSymbolIndex[]) {}

  // Replaces `original` with `replacements`. Applies `id` to all nodes.
  apply(): void {
    const parentNode = this.originalTextNode.parentNode;
    if (!parentNode) {
      return;
    }
    this.originalTextNode[originalNodeDecorationId] = this.id;
    for (const replacement of this.replacements) {
      replacement[replacementNodeDecorationId] = this.id;
      parentNode.insertBefore(replacement, this.originalTextNode);
    }
    parentNode.removeChild(this.originalTextNode);
  }

  // Restores the `original` node. Removes `id` from all nodes.
  restore(): void {
    const parentNode = this.replacements[0]!.parentNode;
    if (!parentNode) {
      return;
    }
    delete this.originalTextNode[originalNodeDecorationId];
    parentNode.insertBefore(this.originalTextNode, this.replacements[0]!);
    for (const replacement of this.replacements) {
      delete replacement[replacementNodeDecorationId];
      parentNode.removeChild(replacement);
    }
  }

  // `true` if this decoration is applied.
  get live(): boolean {
    return !!this.originalTextNode[originalNodeDecorationId];
  }

  // Returns number of replacement nodes that are of given `type`.
  replacementsOfType(type: string): number {
    let count = 0;
    for (const replacement of this.replacements) {
      if (!(replacement instanceof HTMLElement)) {
        continue;
      }
      if (replacement[annotationType] === type) {
        count++;
      }
    }
    return count;
  }

  // Replaces the replacement of given `type` by a text node with same text
  // content.
  removeReplacementsOfType(type: string): void {
    for (let i = 0; i < this.replacements.length; i++) {
      const replacement = this.replacements[i];
      if (!(replacement instanceof HTMLElement)) {
        continue;
      }
      if (replacement[annotationType] === type) {
        const textNode =
            document.createTextNode(replacement.textContent ?? '') as
            TextWithSymbolIndex;
        // Check if current replacement is live.
        const id = replacement[replacementNodeDecorationId];
        const parentNode = replacement.parentNode;
        if (parentNode && id) {
          parentNode.replaceChild(textNode, replacement);
          textNode[replacementNodeDecorationId] = id;
          // The node has been replace. Remove the symbols so it is not
          // considered as a replacement node in observers anymore..
          delete replacement[replacementNodeDecorationId];
          delete replacement[originalNodeDecorationId];
        }
        this.replacements[i] = textNode;
      }
    }
  }

  // Replaces `replacementNode` inside `replacements` with `newReplacement`.
  // Meant to be used to merge `TextDecoration`. If this is live,
  // the `newReplacements` are made live too. This doesn't change
  // `originalTextNode` so on `restore` the original text will return.
  replaceReplacementNode(
      replacementNode: Node,
      newReplacements: ReplacementWithSymbolIndex[]): void {
    // Find `replacementNode` in `replacements`.
    const index = this.replacements.indexOf(replacementNode);
    if (index < 0) {
      return;
    }
    // Remove from array and replace with `newReplacements`.
    this.replacements.splice(index, 1, ...newReplacements);
    // If this decoration is live, apply the `newReplacements`.
    if (this.live) {
      const parentNode = replacementNode.parentNode;
      if (!parentNode) {
        return;
      }
      for (const replacement of newReplacements) {
        replacement[replacementNodeDecorationId] = this.id;
        parentNode.insertBefore(replacement, replacementNode);
      }
      parentNode.removeChild(replacementNode);
    }
  }

  // This decoration was corrupted, restore all live replacements as text nodes,
  // ignoring `originalTextNode` as to not overwrite the corruption that came
  // from 3p code and should have precedence. Afterward this decoration cannot
  // be used anymore.
  cleanupAfterCorruption(): void {
    for (const replacement of this.replacements) {
      if (replacement instanceof HTMLElement && replacement.parentNode) {
        replacement.parentNode.replaceChild(
            document.createTextNode(replacement.textContent ?? ''),
            replacement);
      }
      delete replacement[replacementNodeDecorationId];
    }
    delete this.originalTextNode[originalNodeDecorationId];
  }
}

export {
  ReplacementWithSymbolIndex,
  originalNodeDecorationId,
  replacementNodeDecorationId,
  annotationUniqueId,
  annotationType,
  annotationFullText,
  annotationExternalData,
  createChromeAnnotation,
  createSpace,
  isDecorationNode,
  TextDecoration,
}
