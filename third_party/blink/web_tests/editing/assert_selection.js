// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file provides |assert_selection(sample, tester, expectedText, options)|
// assertion to W3C test harness to write editing test cases easier.
//
// |sample| is an HTML fragment text which is inserted as |innerHTML|. It should
// have at least one focus boundary point marker "|" and at most one anchor
// boundary point marker "^".
//
// |tester| is either name with parameter of execCommand or function taking
// one parameter |Selection|.
//
// |expectedText| is an HTML fragment text containing at most one focus marker
// and anchor marker. If resulting selection is none, you don't need to have
// anchor and focus markers.
//
// |options| is a string as description, undefined, or a dictionary containing:
//  description: A description
//  dumpAs: 'domtree' or 'flattree'. Default is 'domtree'.
//  removeSampleIfSucceeded: A boolean. Default is true.
//  dumpFromRoot: A boolean. Default is false.
//
// Example:
//  test(() => {
//    assert_selection(
//      '|foo',
//      (selection) => selection.modify('extent', 'forward, 'character'),
//      '<a href="http://bar">^f|oo</a>'
//  });
//
//  test(() => {
//    assert_selection(
//      'x^y|z',
//      'bold',  // execCommand name as a test
//      'x<b>y</b>z',
//      'Insert B tag');
//  });
//
//  test(() => {
//    assert_selection(
//      'x^y|z',
//      'createLink http://foo',  // execCommand name and parameter
//      'x<a href="http://foo/">y</a></b>z',
//      'Insert B tag');
//  });
//
//

// TODO(yosin): Please use "clang-format -style=Chromium -i" for formatting
// this file.

(function() {
/** @enum{string} */
const DumpAs = {
  DOM_TREE: 'domtree',
  FLAT_TREE: 'flattree',
};

// border-size of IFRAME which hosts sample HTML. This value comes from
// "core/html/resources/html.css".
const kIFrameBorderSize = 2;

/** @const @type {string} */
const kTextArea = 'TEXTAREA';

class Traversal {
  /**
   * @param {!Node} node
   * @return {Node}
   */
  firstChildOf(node) { throw new Error('You should implement firstChildOf'); }

  /**
   * @param {!Node} node
   * @return {!Generator<Node>}
   */
  *childNodesOf(node) {
    for (let child = this.firstChildOf(node); child !== null;
         child = this.nextSiblingOf(child)) {
      yield child;
    }
  }

  /**
   * @param {!Window} window
   * @return !SampleSelection
   */
  fromDOMSelection(window) {
    throw new Error('You should implement fromDOMSelection');
  }

  /**
   * @param {!Node} node
   * @return {Node}
   */
  nextSiblingOf(node) { throw new Error('You should implement nextSiblingOf'); }
}

class DOMTreeTraversal extends Traversal {
  /**
   * @override
   * @param {!Node} node
   * @return {Node}
   */
  firstChildOf(node) { return node.firstChild; }

  /**
   * @param {!Window} window
   * @return !SampleSelection
   */
  fromDOMSelection(window) {
    return SampleSelection.fromDOMSelection(window.getSelection());
  }

  /**
   * @param {!Node} node
   * @return {Node}
   */
  nextSiblingOf(node) { return node.nextSibling; }
};

class FlatTreeTraversal extends Traversal {
  /**
   * @override
   * @param {!Node} node
   * @return {Node}
   */
  firstChildOf(node) { return internals.firstChildInFlatTree(node); }

  /**
   * @param {!Window} window
   * @return !SampleSelection
   */
  fromDOMSelection(window) {
    return SampleSelection.fromDOMSelection(
        internals.getSelectionInFlatTree(window));
  }

  /**
   * @param {!Node} node
   * @return {Node}
   */
  nextSiblingOf(node) { return internals.nextSiblingInFlatTree(node); }
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isCharacterData(node) {
  return node.nodeType === Node.TEXT_NODE ||
      node.nodeType === Node.COMMENT_NODE;
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isElement(node) {
  return node.nodeType === Node.ELEMENT_NODE;
}

/**
 * @param {!Node} node
 * @param {number} offset
 */
function checkValidNodeAndOffset(node, offset) {
  if (!node)
    throw new Error('Node parameter should not be a null.');
  if (offset < 0)
    throw new Error(`Assumes ${offset} >= 0`);
  if (isElement(node)) {
    if (offset > node.childNodes.length)
      throw new Error(`Bad offset ${offset} for ${node}`);
    return;
  }
  if (isCharacterData(node)) {
    if (offset > node.nodeValue.length)
      throw new Error(`Bad offset ${offset} for ${node}`);
    return;
  }
  throw new Error(`Invalid node: ${node}`);
}

class SampleSelection {
  /** @public */
  constructor() {
    /** @type {?Node} */
    this.anchorNode_ = null;
    /** @type {number} */
    this.anchorOffset_ = 0;
    /** @type {?Node} */
    this.focusNode_ = null;
    /** @type {number} */
    this.focusOffset_ = 0;
    /** @type {HTMLElement} */
    this.shadowHost_ = null;
  }

  /**
   * @public
   * @param {!Node} node
   * @param {number} offset
   */
  collapse(node, offset) {
    checkValidNodeAndOffset(node, offset);
    this.anchorNode_ = this.focusNode_ = node;
    this.anchorOffset_ = this.focusOffset_ = offset;
  }

  /**
   * @public
   * @param {!Node} node
   * @param {number} offset
   */
  extend(node, offset) {
    checkValidNodeAndOffset(node, offset);
    this.focusNode_ = node;
    this.focusOffset_ = offset;
  }

  /** @public @return {?Node} */
  get anchorNode() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.anchorNode_;
  }
  /** @public @return {number} */
  get anchorOffset() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.anchorOffset_;
  }
  /** @public @return {?Node} */
  get focusNode() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.focusNode_;
  }
  /** @public @return {number} */
  get focusOffset() {
    console.assert(!this.isNone, 'Selection should not be a none.');
    return this.focusOffset_;
  }

  /** @public @return {HTMLElement} */
  get shadowHost() {
    return this.shadowHost_;
  }

  /**
   * @public
   * @return {boolean}
   */
  get isCollapsed() {
    return this.anchorNode === this.focusNode &&
        this.anchorOffset === this.focusOffset;
  }

  /**
   * @public
   * @return {boolean}
   */
  get isNone() { return this.anchorNode_ === null; }

  /**
   * @public
   * @param {!Selection} domSelection
   * @return {!SampleSelection}
   */
  static fromDOMSelection(domSelection) {
    /** type {!SampleSelection} */
    const selection = new SampleSelection();
    selection.anchorNode_ = domSelection.anchorNode;
    selection.anchorOffset_ = domSelection.anchorOffset;
    selection.focusNode_ = domSelection.focusNode;
    selection.focusOffset_ = domSelection.focusOffset;

    if (selection.anchorNode_ === null)
      return selection;

    const document = selection.anchorNode_.ownerDocument;
    selection.shadowHost_ = (() => {
        if (!document.activeElement)
          return null;
        if (document.activeElement.nodeName !== kTextArea)
          return null;
        const selectedNode =
            selection.anchorNode.childNodes[selection.anchorOffset];
        if (document.activeElement !== selectedNode)
          return null;
        return selectedNode;
    })();
    return selection;
  }

  /** @override */
  toString() {
    if (this.isNone)
      return 'SampleSelection()';
    if (this.isCollapsed)
      return `SampleSelection(${this.focusNode_}@${this.focusOffset_})`;
    return `SampleSelection(anchor: ${this.anchorNode_}@${this.anchorOffset_}` +
        `focus: ${this.focusNode_}@${this.focusOffset_}`;
  }
}

// Extracts selection from marker "^" as anchor and "|" as focus from
// DOM tree and removes them.
class Parser {
  /** @private */
  constructor() {
    /** @type {?Node} */
    this.anchorNode_ = null;
    /** @type {number} */
    this.anchorOffset_ = 0;
    /** @type {?Node} */
    this.focusNode_ = null;
    /** @type {number} */
    this.focusOffset_ = 0;
  }

  /**
   * @public
   * @return {!SampleSelection}
   */
  get selection() {
    const selection = new SampleSelection();
    if (!this.anchorNode_ && !this.focusNode_)
      return selection;
    if (this.anchorNode_ && this.focusNode_) {
      selection.collapse(this.anchorNode_, this.anchorOffset_);
      selection.extend(this.focusNode_, this.focusOffset_);
      return selection;
    }
    if (this.focusNode_) {
      selection.collapse(this.focusNode_, this.focusOffset_);
      return selection;
    }
    throw new Error('There is no focus marker');
  }

  /**
   * @private
   * @param {!CharacterData} node
   * @param {number} nodeIndex
   */
  handleCharacterData(node, nodeIndex) {
    /** @type {string} */
    const text = node.nodeValue;
    /** @type {number} */
    const anchorOffset = text.indexOf('^');
    /** @type {number} */
    const focusOffset = text.indexOf('|');
    /** @type {!Node} */
    const parentNode = node.parentNode;
    node.nodeValue = text.replace('^', '').replace('|', '');
    if (node.nodeValue.length == 0) {
      if (anchorOffset >= 0)
        this.rememberSelectionAnchor(parentNode, nodeIndex);
      if (focusOffset >= 0)
        this.rememberSelectionFocus(parentNode, nodeIndex);
      node.remove();
      return;
    }
    if (anchorOffset >= 0 && focusOffset >= 0) {
      if (anchorOffset > focusOffset) {
        this.rememberSelectionAnchor(node, anchorOffset - 1);
        this.rememberSelectionFocus(node, focusOffset);
        return;
      }
      this.rememberSelectionAnchor(node, anchorOffset);
      this.rememberSelectionFocus(node, focusOffset - 1);
      return;
    }
    if (anchorOffset >= 0) {
      this.rememberSelectionAnchor(node, anchorOffset);
      return;
    }
    if (focusOffset < 0)
      return;
    this.rememberSelectionFocus(node, focusOffset);
  }

  /**
   * @private
   * @param {!Element} element
   */
  handleElementNode(element) {
    /** @type {number} */
    let childIndex = 0;
    for (const child of Array.from(element.childNodes)) {
      this.parseInternal(child, childIndex);
      if (!child.parentNode)
        continue;
      ++childIndex;
    }
  }

  /**
   * @private
   * @param {!Node} node
   * @return {!SampleSelection}
   */
  parse(node) {
    this.parseInternal(node, 0);
    return this.selection;
  }

  /**
   * @private
   * @param {!Node} node
   * @param {number} nodeIndex
   */
  parseInternal(node, nodeIndex) {
    if (isElement(node))
      return this.handleElementNode(node);
    if (isCharacterData(node))
      return this.handleCharacterData(node, nodeIndex);
    throw new Error(`Unexpected node ${node}`);
  }

  /**
   * @private
   * @param {!Node} node
   * @param {number} offset
   */
  rememberSelectionAnchor(node, offset) {
    checkValidNodeAndOffset(node, offset);
    console.assert(
        this.anchorNode_ === null, 'Anchor marker should be one.',
        this.anchorNode_, this.anchorOffset_);
    this.anchorNode_ = node;
    this.anchorOffset_ = offset;
  }

  /**
   * @private
   * @param {!Node} node
   * @param {number} offset
   */
  rememberSelectionFocus(node, offset) {
    checkValidNodeAndOffset(node, offset);
    console.assert(
        this.focusNode_ === null, 'Focus marker should be one.',
        this.focusNode_, this.focusOffset_);
    this.focusNode_ = node;
    this.focusOffset_ = offset;
  }

  /**
   * @public
   * @param {!Node} node
   * @return {!SampleSelection}
   */
  static parse(node) { return (new Parser()).parse(node); }
}

// TODO(yosin): Once we can import JavaScript file from scripts, we should
// import "external/wpt/html/resources/common.js", since |HTML5_VOID_ELEMENTS|
// is defined in there.
/**
 * @const @type {!Set<string>}
 * only void (without end tag) HTML5 elements
 */
const HTML5_VOID_ELEMENTS = new Set([
  'area', 'base', 'br', 'col', 'command', 'embed', 'hr', 'img', 'input',
  'keygen', 'link', 'meta', 'param', 'source','track', 'wbr' ]);

class Serializer {
  /**
   * @public
   * @param {!SampleSelection} selection
   * @param {!Traversal} traversal
   */
  constructor(selection, traversal) {
    /** @type {!SampleSelection} */
    this.selection_ = selection;
    /** @type {!Array<string>} */
    this.strings_ = [];
    /** @type {!Traversal} */
    this.traversal_ = traversal;
  }

  /**
   * @private
   * @param {string} string
   */
  emit(string) { this.strings_.push(string); }

  /**
   * @private
   * @param {!HTMLElement} parentNode
   * @param {number} childIndex
   */
  handleSelection(parentNode, childIndex) {
    if (this.selection_.isNone)
      return;
    if (this.selection_.shadowHost)
      return;
    if (parentNode === this.selection_.focusNode &&
        childIndex === this.selection_.focusOffset) {
      this.emit('|');
      return;
    }
    if (parentNode === this.selection_.anchorNode &&
        childIndex === this.selection_.anchorOffset) {
      this.emit('^');
    }
  }

  /**
   * @private
   * @param {!CharacterData} node
   */
  handleCharacterData(node) {
    /** @type {string} */
    const text = node.nodeValue;
    if (this.selection_.isNone)
      return this.emit(text);
    /** @type {number} */
    const anchorOffset = this.selection_.anchorOffset;
    /** @type {number} */
    const focusOffset = this.selection_.focusOffset;
    if (node === this.selection_.focusNode &&
        node === this.selection_.anchorNode) {
      if (anchorOffset === focusOffset) {
        this.emit(text.substr(0, focusOffset));
        this.emit('|');
        this.emit(text.substr(focusOffset));
        return;
      }
      if (anchorOffset < focusOffset) {
        this.emit(text.substr(0, anchorOffset));
        this.emit('^');
        this.emit(text.substr(anchorOffset, focusOffset - anchorOffset));
        this.emit('|');
        this.emit(text.substr(focusOffset));
        return;
      }
      this.emit(text.substr(0, focusOffset));
      this.emit('|');
      this.emit(text.substr(focusOffset, anchorOffset - focusOffset));
      this.emit('^');
      this.emit(text.substr(anchorOffset));
      return;
    }
    if (node === this.selection_.anchorNode) {
      this.emit(text.substr(0, anchorOffset));
      this.emit('^');
      this.emit(text.substr(anchorOffset));
      return;
    }
    if (node === this.selection_.focusNode) {
      this.emit(text.substr(0, focusOffset));
      this.emit('|');
      this.emit(text.substr(focusOffset));
      return;
    }
    this.emit(text);
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  handleElementNode(element) {
    /** @type {string} */
    const tagName = element.tagName.toLowerCase();
    this.emit(`<${tagName}`);
    Array.from(element.attributes)
        .sort((attr1, attr2) => attr1.name.localeCompare(attr2.name))
        .forEach(attr => {
          if (attr.value === '')
            return this.emit(` ${attr.name}`);
          const value = attr.value.replace(/&/g, '&amp;')
                            .replace(/\u0022/g, '&quot;')
                            .replace(/\u0027/g, '&apos;');
          this.emit(` ${attr.name}="${value}"`);
        });
    this.emit('>');
    if (element.nodeName === kTextArea)
      return this.handleTextArea(element);
    if (this.traversal_.firstChildOf(element) === null &&
        HTML5_VOID_ELEMENTS.has(tagName)) {
      return;
    }
    this.serializeChildren(element);
    this.emit(`</${tagName}>`);
  }

  /**
   * @private
   * @param {!HTMLTextAreaElement} textArea
   */
  handleTextArea(textArea) {
    /** @type {string} */
    const value = textArea.value;
    if (this.selection_.shadowHost !== textArea) {
      this.emit(value);
    } else {
      /** @type {number} */
      const start = textArea.selectionStart;
      /** @type {number} */
      const end = textArea.selectionEnd;
      /** @type {boolean} */
      const isBackward = start < end &&
                         textArea.selectionDirection === 'backward';
      const startMarker = isBackward ? '|' : '^';
      const endMarker = isBackward ? '^' : '|';
      this.emit(value.substr(0, start));
      if (start < end) {
        this.emit(startMarker);
        this.emit(value.substr(start, end - start));
      }
      this.emit(endMarker);
      this.emit(value.substr(end));
    }
    this.emit('</textarea>');
  }

  /**
   * @public
   * @param {!HTMLDocument} document
   * @param {boolean} dumpFromRoot
   */
  serialize(document, dumpFromRoot) {
    if (document.body && !dumpFromRoot)
        this.serializeChildren(document.body);
    else
        this.serializeInternal(document.documentElement);
    return this.strings_.join('');
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  serializeChildren(element) {
    if (this.traversal_.firstChildOf(element) === null) {
      this.handleSelection(element, 0);
      return;
    }

    /** @type {number} */
    let childIndex = 0;
    for (let child of this.traversal_.childNodesOf(element)) {
      this.handleSelection(element, childIndex);
      this.serializeInternal(child, childIndex);
      ++childIndex;
    }
    this.handleSelection(element, childIndex);
  }

  /**
   * @private
   * @param {!Node} node
   */
  serializeInternal(node) {
    if (isElement(node))
      return this.handleElementNode(node);
    if (isCharacterData(node))
      return this.handleCharacterData(node);
    throw new Error(`Unexpected node ${node}`);
  }
}

/**
 * @param {!HTMLElement} element
 * @return {number}
 */
function computeLeft(element) {
  let left = kIFrameBorderSize + element.ownerDocument.offsetLeft;
  for (let runner = element; runner; runner = runner.offsetParent)
    left += runner.offsetLeft;
  return left;
}

/**
 * @param {!HTMLElement} element
 * @return {number}
 */
function computeRight(element) {
  return this.computeLeft(element) + element.offsetWidth;
}

/**
 * @param {!HTMLElement} element
 * @return {number}
 */
function computeTop(element) {
  let top = kIFrameBorderSize + element.ownerDocument.offsetTop;
  for (let runner = element; runner; runner = runner.offsetParent)
    top += runner.offsetTop;
  return top;
}

/**
 * @param {!HTMLElement} element
 * @return {number}
 */
function computeBottom(element) {
  return this.computeTop(element) + element.offsetHeight;
}

/**
 * @this {!Selection}
 * @param {string} html
 * @param {string=} opt_text
 */
function setClipboardData(html, opt_text) {
  assert_not_equals(window.internals, undefined,
    'This test requests clipboard access from JavaScript.');
  function computeTextData() {
    if (opt_text !== undefined)
      return opt_text;
    const element = document.createElement('div');
    element.innerHTML = html;
    return element.textContent;
  }
  function copyHandler(event) {
    const clipboardData = event.clipboardData;
    clipboardData.setData('text/plain', computeTextData());
    clipboardData.setData('text/html', html);
    event.preventDefault();
  }
  document.addEventListener('copy', copyHandler);
  document.execCommand('copy');
  document.removeEventListener('copy', copyHandler);
}

class Sample {
  /**
   * @public
   * @param {string} sampleText
   */
  constructor(sampleText) {
    /** @const @type {!HTMLIFrameElement} */
    this.iframe_ = Sample.getOrCreatePlayground();
    /** @const @type {!HTMLDocument} */
    this.document_ = this.iframe_.contentDocument;

    // Set focus to sample IFRAME to make |eventSender| and
    // |testRunner.execCommand()| to work on sample rather than main frame.
    this.iframe_.focus();
    /** @const @type {!Selection} */
    this.selection_ = this.iframe_.contentWindow.getSelection();
    this.selection_.document = this.document_;
    this.selection_.document.offsetLeft = this.iframe_.offsetLeft;
    this.selection_.document.offsetTop = this.iframe_.offsetTop;
    this.selection_.setClipboardData = setClipboardData;
    this.selection_.computeLeft = computeLeft;
    this.selection_.computeRight = computeRight;
    this.selection_.computeTop = computeTop;
    this.selection_.computeBottom = computeBottom;
    this.selection_.window = this.iframe_.contentWindow;
    this.load(sampleText);
  }

  /** @return {!HTMLDocument} */
  get document() { return this.document_; }

  /** @return {!Selection} */
  get selection() { return this.selection_; }

  /** @return {string} */
  static get playgroundId() { return 'playground'; }

  /**
   * @public
   * Marks this sample not to be reused.
   */
  keep() {
    this.iframe_.removeAttribute('id');
  }

  /**
   * @private
   * @param {string} sampleText
   */
  load(sampleText) {
    const anchorMarker = sampleText.indexOf('^');
    const focusMarker = sampleText.indexOf('|');
    if (focusMarker < 0 && anchorMarker >= 0) {
      throw new Error(`You should specify caret position in "${sampleText}".`);
    }
    if (focusMarker != sampleText.lastIndexOf('|')) {
      throw new Error(
          `You should have at least one focus marker "|" in "${sampleText}".`);
    }
    if (anchorMarker != sampleText.lastIndexOf('^')) {
      throw new Error(
          `You should have at most one anchor marker "^" in "${sampleText}".`);
    }
    if (anchorMarker >= 0 && focusMarker >= 0 &&
        (anchorMarker + 1 === focusMarker || anchorMarker - 1 === focusMarker)) {
      throw new Error(
          `You should have focus marker and should not have anchor marker if and only if selection is a caret in "${sampleText}".`);
    }
    this.document_.body.innerHTML = sampleText;
    /** @type {!SampleSelection} */
    const selection = Parser.parse(this.document_.body);
    if (selection.isNone)
      return;
    if (this.loadSelectionInTextArea(selection))
      return;
    this.selection_.collapse(selection.anchorNode, selection.anchorOffset);
    if (this.selection_.rangeCount > 0)
      this.selection_.extend(selection.focusNode, selection.focusOffset);
  }

  /**
   * @private
   * @param {!SampleSelection} selection
   * @return {boolean} Returns true if selection is in TEXTAREA.
   */
  loadSelectionInTextArea(selection) {
    /** @type {Node} */
    const enclosingNode = selection.anchorNode.parentNode;
    if (selection.focusNode.parentNode !== enclosingNode)
      return false;
    if (enclosingNode.nodeName !== kTextArea)
      return false;
    if (selection.anchorNode !== selection.focusNode)
      throw new Error('Selection in TEXTAREA should be in same Text node.');
    enclosingNode.focus();
    if (selection.anchorOffset < selection.focusOffset) {
      enclosingNode.setSelectionRange(selection.anchorOffset,
                                      selection.focusOffset);
      return true;
    }
    enclosingNode.setSelectionRange(selection.focusOffset,
                                    selection.anchorOffset,
                                    'backward');
    return true;
  }

  /**
   * @public
   */
  remove() { this.iframe_.remove(); }

  /**
   * @public
   */
  reset() {
    if (window.internals && internals.isOverwriteModeEnabled(this.document_))
      internals.toggleOverwriteModeEnabled(this.document_);
    this.document_.documentElement.innerHTML = '<head></head><body></body>';
    this.selection.removeAllRanges();
    this.iframe_.style.display = 'none';
  }

  /** @return {HTMLIFrameElement} */
  static getOrCreatePlayground() {
    const present = document.getElementById(Sample.playgroundId);
    if (present) {
      present.style.display = 'block';
      return present;
    }
    const iframe = document.createElement('iframe');
    iframe.setAttribute('id', Sample.playgroundId);
    if (!document.body)
        document.body = document.createElement("body");
    document.body.appendChild(iframe);
    return iframe;
  }

  /**
   * @public
   * @param {!Traversal} traversal
   * @param {boolean} dumpFromRoot
   * @return {string}
   */
  serialize(traversal, dumpFromRoot) {
    /** @type {!SampleSelection} */
    const selection = traversal.fromDOMSelection(this.document_.defaultView);
    /** @type {!Serializer} */
    const serializer = new Serializer(selection, traversal);
    return serializer.serialize(this.document_, dumpFromRoot);
  }
}

function assembleDescription() {
  function getStack() {
    let stack;
    try {
      throw new Error('get line number');
    } catch (error) {
      stack = error.stack.split('\n').slice(1);
    }
    return stack
  }

  const RE_IN_ASSERT_SELECTION = new RegExp('assert_selection\\.js');
  for (const line of getStack()) {
    const match = RE_IN_ASSERT_SELECTION.exec(line);
    if (!match) {
      const RE_LAYOUTTESTS = new RegExp('(?<=LayoutTests/|web_tests/).*');
      return RE_LAYOUTTESTS.exec(line);
    }
  }
  return '';
}

/**
 * @param {string} expectedText
 */
function checkExpectedText(expectedText) {
  /** @type {number} */
  const anchorOffset = expectedText.indexOf('^');
  /** @type {number} */
  const focusOffset = expectedText.indexOf('|');
  if (anchorOffset != expectedText.lastIndexOf('^')) {
      throw new Error(
        `You should have at most one anchor marker "^" in "${expectedText}".`);
  }
  if (focusOffset != expectedText.lastIndexOf('|')) {
      throw new Error(
        `You should have at most one focus marker "|" in "${expectedText}".`);
  }
  if (anchorOffset >= 0 && focusOffset < 0) {
    throw new Error(
        `You should have a focus marker "|" in "${expectedText}".`);
  }
  if (anchorOffset >= 0 && focusOffset >= 0 &&
     (anchorOffset + 1 === focusOffset || anchorOffset - 1 === focusOffset)) {
    throw new Error(
        `You should have focus marker and should not have anchor marker if and only if selection is a caret in "${expectedText}".`);
  }
}

/**
 * @param {string} str1
 * @param {string} str2
 * @return {string}
 */
function commonPrefixOf(str1, str2) {
  for (let index = 0; index < str1.length; ++index) {
    if (str1[index] !== str2[index])
      return str1.substr(0, index);
  }
  return str1;
}

/**
 * @param {string} passedInputText
 * @param {function(!Selection)|string} tester
 * @param {string} expectedText
 * @param {Object=} opt_options
 * @return {!Sample}
 */
function assertSelectionAndReturnSample(
    passedInputText, tester, passedExpectedText, opt_options = {}) {
  const kDescription = 'description';
  const kDumpAs = 'dumpAs';
  const kRemoveSampleIfSucceeded = 'removeSampleIfSucceeded';
  const kDumpFromRoot = 'dumpFromRoot';
  /** @type {!Object} */
  const options = typeof(opt_options) === 'string'
      ? {description: opt_options} : opt_options;
  /** @type {string} */
  const description = kDescription in options
      ? options[kDescription] : assembleDescription();
  /** @type {boolean} */
  const removeSampleIfSucceeded = kRemoveSampleIfSucceeded in options
      ? !!options[kRemoveSampleIfSucceeded] : true;
  /** @type {DumpAs} */
  const dumpAs = options[kDumpAs] || DumpAs.DOM_TREE;
  /** @type {boolean} */
  const dumpFromRoot = options[kDumpFromRoot] || false;

  const inputText = (() => {
    if (typeof(passedInputText) === 'string')
      return passedInputText;
    if (Array.isArray(passedInputText))
      return passedInputText.join("");
    throw new Error('InputText must be a string or an array of strings.');
  })();

  const expectedText = (() => {
    if (typeof(passedExpectedText) === 'string')
      return passedExpectedText;
    if (Array.isArray(passedExpectedText))
      return passedExpectedText.join("");
    throw new Error('ExpectedText must be a string or an array of strings.');
  })();

  checkExpectedText(expectedText);
  const sample = new Sample(inputText);
  if (typeof(tester) === 'function') {
    tester.call(window, sample.selection);
  } else if (typeof(tester) === 'string') {
    const strings = tester.split(/ (.+)/);
    sample.document.execCommand(strings[0], false, strings[1]);
  } else {
    throw new Error(`Invalid tester: ${tester}`);
  }

  /** @type {!Traversal} */
  const traversal = (() => {
    switch (dumpAs) {
      case DumpAs.DOM_TREE:
        return new DOMTreeTraversal();
      case DumpAs.FLAT_TREE:
        if (!window.internals)
          throw new Error('This test requires window.internals.');
        return new FlatTreeTraversal();
      default:
        throw `${kDumpAs} must be one of ` +
              `{${Object.values(DumpAs).join(', ')}}` +
              ` instead of '${dumpAs}'`;
    }
  })();

  /** @type {string} */
  const actualText = sample.serialize(traversal, dumpFromRoot);
  // We keep sample HTML when assertion is false for ease of debugging test
  // case.
  if (actualText === expectedText) {
    if (removeSampleIfSucceeded)
        sample.reset();
    else
        sample.keep();
    return sample;
  }
  sample.keep();
  throw new Error(`${description}\n` +
    `\t expected ${expectedText},\n` +
    `\t but got  ${actualText},\n` +
    `\t sameupto ${commonPrefixOf(expectedText, actualText)}`);
}

/** Like `assertSelectionAndReturnSample` but without return value.
 *
 * @param {string} passedInputText
 * @param {function(!Selection)|string} tester
 * @param {string} expectedText
 * @param {Object=} opt_options
 */
function assertSelection(
  passedInputText, tester, passedExpectedText, opt_options) {
    assertSelectionAndReturnSample(passedInputText, tester, passedExpectedText, opt_options);
}

/**
 * @param {string} inputText
 * @param {function(!Selection)|string} tester
 * @param {string} expectedText
 * @param {Object=} opt_options
 * @param {string=} opt_description
 */
function selectionTest(inputText, tester, expectedText, opt_options,
                       opt_description) {
  const description = typeof(opt_options) === 'string' ? opt_options
                                                       : opt_description;
  const options = typeof(opt_options) === 'string' ? undefined : opt_options;
  test(() => assertSelection(inputText, tester, expectedText, options),
       description);
}

// Export symbols
window.Sample = Sample;
window.assert_selection = assertSelection;
window.assert_selection_and_return_sample = assertSelectionAndReturnSample;
window.selection_test = selectionTest;
window.DOMTreeTraversal = DOMTreeTraversal;
})();
