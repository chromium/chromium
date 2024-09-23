// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface used to extract visible text on the page, add extra
 * at the ends, and pass it on to the a consumer.
 */

import {nextLeaf, previousLeaf, TextWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {TextNodeVisitor} from '//ios/web/annotations/resources/text_intersection_observer.js';

// TODO(crbug.com/40936184): investigate concatening of nodes and RTL languages.

// Character added to the extracted text that intent detection should not cross.
const SECTION_BREAK = ' ‡ ';

// Minimum number of characters to add at ends of sections.
const EXTRA_CHARACTERS_AT_END = 128;

const KNOWN_INLINE_ELEMENTS: Set<string> = new Set([
  'A', 'ABBR', 'B', 'CITE', 'CODE', 'I', 'DFN', 'EM', 'MARK', 'SMALL', 'SPAN',
  'STRONG', 'SUB', 'SUP', 'VAR'
]);

// A section is a `textNode` and an index. The index is the position when this
// node's text is in the full extracted text. Note that some text, like breaks
// and spaces, are not in `TextSection`s. Neither are text nodes with no text or
// with only spaces and newlines.
class TextSection {
  private sourceTextNode: WeakRef<TextWithSymbolIndex>;

  constructor(textNode: TextWithSymbolIndex, public index: number) {
    this.sourceTextNode = new WeakRef<TextWithSymbolIndex>(textNode);
  }

  get textNode(): TextWithSymbolIndex|null {
    return this.sourceTextNode.deref() || null;
  }
}

// Consumer of `TextChunk` callback.
type TextChunkConsumer = {
  (chunk: TextChunk): void;
};

// A piece of extracted text and the sections needed to locate back the nodes
// from which the text, at a given index, comes from.
class TextChunk {
  text: string = '';
  sections: TextSection[] = [];

  // `firstNodeOffset` is the offset to the first character in the first
  // `TextSection`. The text before the offset is not included in `text`.
  // The offset will be subtracted to index of the first node when calling
  // the section enumerator. `visibleStart` and `visibleEnd` define the range
  // in which any annotation having at least one character in will be decorated.
  constructor(
      public firstNodeOffset: number, public visibleStart: number,
      public visibleEnd: number) {}

  // Adds a list of `sections` at the end of the current list. Adds the given
  // `text` at the end of the current text.
  add(sections: TextSection[], text: string): void {
    // The new section needs to be offsetted based on how much text is here
    // already. Note that `firstNodeOffset` is indepedant of this.
    const offset = this.text.length;
    for (const section of sections) {
      section.index += offset;
    }
    this.text += text;
    this.sections.push(...sections);
  }
}

// A `TextNodeVisitor` that assembles the text. It adds breaks where needed and
// concatenates prefix and suffix text (of at most `extraCharactersAtEnd`) at
// each end.
class TextExtractor implements TextNodeVisitor {
  constructor(
      private consumer: TextChunkConsumer,
      private extraCharactersAtEnd = EXTRA_CHARACTERS_AT_END,
      private sectionBreak = SECTION_BREAK) {}

  private parts: string[] = [];
  private sections: TextSection[] = [];

  // `true` when a text break has been added. A text break is meant to replace
  // non visible or invalid nodes to avoid creating false context by combining
  // text before and after the break.
  private broken = true;
  // Current index, it is equivalent to `''.concat(...this.parts).length`.
  private index = 0;

  // `true` when a space has already been added between text.
  spaced = true;

  // Mark: TextNodeVisitor

  begin(): void {
    this.parts = [];
    this.sections = [];
    this.broken = true;
    this.spaced = true;
    this.index = 0;
  }

  visibleTextNode(textNode: Text): void {
    if (textNode.textContent!.trim()) {
      this.parts.push(textNode.textContent!);
      this.sections.push(new TextSection(textNode, this.index));
      this.index += textNode.textContent!.length;
      this.broken = false;
      this.spaced = false;
    } else {
      this.addSpaceIfNeeded();
    }
  }

  enterVisibleNode(node: Node): void {
    if (node instanceof Element && !KNOWN_INLINE_ELEMENTS.has(node.nodeName)) {
      this.addSpaceIfNeeded();
    }
  }

  leaveVisibleNode(node: Node): void {
    if (node instanceof Element && !KNOWN_INLINE_ELEMENTS.has(node.nodeName)) {
      this.addSpaceIfNeeded();
    }
  }

  invisibleNode(node: Node): void {
    if (node.nodeType === Node.COMMENT_NODE) {
      // Completely ignore comments.
    } else if (
        node.nodeType === Node.TEXT_NODE &&
        (!node.textContent || !node.textContent.trim())) {
      // Skip empty text nodes. They are not real breaks.
      this.addSpaceIfNeeded();
    } else if (!this.broken) {
      // Text section break, no section registered.
      this.parts.push(this.sectionBreak);
      this.index += this.sectionBreak.length;
      this.broken = true;
      this.spaced = false;
    }
  }

  end(): void {
    // If there's no new text, cancel extraction. It doesn't make sense
    // to send prefix and suffix characters and send those two ends.
    if (this.sections.length === 0) {
      return;
    }

    // To catch an address on multiple line scrolling in, the extra 'window'
    // (rootMargin) isn't enough, it just pushes the problem below or above the
    // viewport. This solves the issue by always adding extra text before
    // and after, regardless of that text's visibility and not removing it
    // from the DOM observer or intersection observer.
    const firstNode: Node = this.sections[0]!.textNode!;
    const [firstNodeOffset, prefixText, prefixSections] =
        this.extractPrefix(firstNode);
    const lastNode: Node = this.sections[this.sections.length - 1]!.textNode!;
    const [postfixText, postfixSections] =
        this.extractPostfix(lastNode, this.spaced);
    const text = ''.concat(...this.parts);
    const chunk = new TextChunk(
        firstNodeOffset, prefixText.length, prefixText.length + text.length);
    chunk.add(prefixSections, prefixText);
    chunk.add(this.sections, text);
    chunk.add(postfixSections, postfixText);
    this.consumer(chunk);
  }

  // Mark: Private API

  // Adds a single space between parts if there was none.
  private addSpaceIfNeeded() {
    if (!this.spaced) {
      // Spacer, no section registered.
      this.parts.push(' ');
      this.index++;
      this.spaced = true;
    }
  }

  // Extracts up to `extraCharactersAtEnd` before `beforeNode`. Returns
  // an array of `TextSection`, its combined text and the offset to
  // the first character in the first section. In case nothing can be found,
  // empty text and sections are returned.
  private extractPrefix(beforeNode: Node): [number, string, TextSection[]] {
    let sections: TextSection[] = [];
    let parts: string[] = [' '];
    // Leave space for a space and start `index` at 1 from the end.
    let index = this.extraCharactersAtEnd - 1;
    // Keep track of latest offset and since the traversal if backward, the
    // last value will be for the first node.
    let offset = 0;
    let spaced = true;
    let node: Node|null = previousLeaf(beforeNode, /* breakAtInvalid= */ true);
    while (node && index > 0) {
      if (node.nodeType === Node.TEXT_NODE && node.textContent &&
          node.textContent.trim()) {
        const textLength = node.textContent.length;
        const minLength = Math.min(index, textLength);
        offset = textLength - minLength;
        parts.push(node.textContent.substring(offset));
        sections.push(new TextSection(node as Text, index - minLength));
        index -= minLength;
        spaced = false;
      } else if (!spaced) {
        parts.push(' ');
        index--;
        spaced = true;
      }
      node = previousLeaf(node, /* breakAtInvalid= */ true);
    }
    if (sections.length > 0) {
      sections = sections.reverse();
      parts = parts.reverse();
      const text = ''.concat(...parts);
      // index will be > 0 if there wasn't enough text, so adjust the
      // sections to match the `text`.
      if (index > 0) {
        for (const section of sections) {
          section.index -= index;
        }
      }
      return [offset, text, sections];
    }
    return [0, '', []];
  }

  // Extracts up to `extraCharactersAtEnd` after `afterNode`. Returns
  // an array of `TextSection` and its combined text. In case nothng can
  // be found, empty text and sections are returned.
  private extractPostfix(afterNode: Node, alreadySpaced: boolean):
      [string, TextSection[]] {
    const sections: TextSection[] = [];
    const parts: string[] = [];
    let index = 0;
    let spaced = alreadySpaced;
    if (!alreadySpaced) {
      parts.push(' ');
      index++;
      spaced = true;
    }
    const maxChars = alreadySpaced ? this.extraCharactersAtEnd :
                                     this.extraCharactersAtEnd - 1;
    let node: Node|null = nextLeaf(afterNode, /* breakAtInvalid= */ true);
    while (node && index < maxChars) {
      if (node.nodeType === Node.TEXT_NODE && node.textContent &&
          node.textContent.trim()) {
        const textLength = node.textContent.length;
        const minLength = Math.min(maxChars - index, textLength);
        parts.push(node.textContent.substring(0, minLength));
        sections.push(new TextSection(node as Text, index));
        index += minLength;
        spaced = false;
      } else if (!spaced) {
        parts.push(' ');
        index++;
        spaced = true;
      }
      node = nextLeaf(node, /* breakAtInvalid= */ true);
    }
    if (sections.length > 0) {
      const text = ''.concat(...parts);
      return [text, sections];
    }
    return ['', []];
  }
}

export {
  TextSection,
  TextChunk,
  TextChunkConsumer,
  TextExtractor,
}
