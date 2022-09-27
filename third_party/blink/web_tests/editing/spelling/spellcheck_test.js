// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// This file provides
// |spellcheck_test(sample, tester, expectedText, opt_title)| asynchronous test
// to W3C test harness for easier writing of spellchecker test cases.
//
// |sample| is
// - Either an HTML fragment text which is inserted as |innerHTML|, in which
//   case It should have at least one focus boundary point marker "|" and at
//   most one anchor boundary point marker "^" indicating the initial selection.
//   TODO(editing-dev): Make initial selection work with TEXTAREA and INPUT.
// - Or a |Sample| object created by some previous test.
//
// |tester| is either name with parameter of execCommand or function taking
// up to two parameters: |document|, and |testRunner|. The |testRunner| is for
// the frame in which the test is run, and allows the |tester| to inject test
// behaviour into the frame, such as execCommand().
//
// |expectedText| is an HTML fragment indicating the expected result, where text
// with spelling marker is surrounded by '#', and text with grammar marker is
// surrounded by '~'.
//
// |opt_args| is an optional object with the following optional fields:
// - title: the title of the test case.
// - callback: a callback function to be run after the test passes, which takes
//   one parameter -- the |Sample| at the end of the test
// It is allowed to pass a string as |arg_args| to indicate the title only.
//
// See spellcheck_test.html for sample usage.

(function() {
const Sample = window.Sample;

// TODO(editing-dev): Once we can import JavaScript file from scripts, we should
// import "external/wpt/html/resources/common.js", since |HTML5_VOID_ELEMENTS|
// is defined in there.
/**
 * @const @type {!Set<string>}
 * only void (without end tag) HTML5 elements
 */
const HTML5_VOID_ELEMENTS = new Set([
  'area', 'base', 'br', 'col', 'command', 'embed', 'hr', 'img', 'input',
  'keygen', 'link', 'meta', 'param', 'source','track', 'wbr' ]);

// TODO(editing-dev): Reduce code duplication with assert_selection's Serializer
// once we can import and export Javascript modules.

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
 * @return {boolean}
 */
function isHTMLInputElement(node) {
  return node.nodeName === 'INPUT';
}

/**
 * @param {!Node} node
 * @return {boolean}
 */
function isHTMLTextAreaElement(node) {
  return node.nodeName === 'TEXTAREA';
}

/**
 * @param {?Range} range
 * @param {!Node} node
 * @param {number} offset
 */
function isAtRangeEnd(range, node, offset) {
  return range && node === range.endContainer && offset === range.endOffset;
}

class MarkerSerializer {
  /**
   * @public
   * @param {!Object} markerTypes
   */
  constructor(markerTypes) {
    /** @type {!Array<string>} */
    this.strings_ = [];
    /** @type {!Object} */
    this.markerTypes_ = markerTypes;
    /** @type {!Object} */
    this.activeMarkerRanges_ = {};
    for (let type in markerTypes)
      this.activeMarkerRanges_[type] = null;
  }

  /**
   * @private
   * @param {string} string
   */
  emit(string) { this.strings_.push(string); }

  /**
   * @private
   * @param {!CharacterData} node
   * @param {number} offset
   * @return {number} The next offset at which a current active marker ends or
   *                  a new marker starts. Returns node.data.length if there is
   *                  no more markers.
   */
  advancedTo(node, offset) {
    var nextCheckPoint = node.data.length;
    for (let type in this.markerTypes_) {
      // Handle the ending of the current active marker.
      if (isAtRangeEnd(this.activeMarkerRanges_[type], node, offset))
        this.emit(this.markerTypes_[type]);

      // Recompute the current active marker and the next check point
      this.activeMarkerRanges_[type] = null;
      /** @type {number} */
      const markerCount = internals.markerCountForNode(node, type);
      for (let i = 0; i < markerCount; ++i) {
        const marker = internals.markerRangeForNode(node, type, i);
        assert_equals(
            marker.startContainer, node,
            'Internal error: marker range not starting in the annotated node.');
        assert_equals(
            marker.endContainer, node,
            'Internal error: marker range not ending in the annotated node.');
        assert_greater_than(marker.endOffset, marker.startOffset,
                            'Internal error: marker range is collapsed.');
        if (marker.startOffset <= offset && offset < marker.endOffset) {
          this.activeMarkerRanges_[type] = marker;
          nextCheckPoint = Math.min(nextCheckPoint, marker.endOffset);
          // Handle the starting of the current active marker.
          if (offset === marker.startOffset)
            this.emit(this.markerTypes_[type]);
        } else if (marker.startOffset > offset) {
          nextCheckPoint = Math.min(nextCheckPoint, marker.startOffset);
        }
      }
    }
    return nextCheckPoint;
  }

  /**
   * @private
   * @param {!CharacterData} node
   */
  handleCharacterData(node) {
    /** @type {string} */
    const text = node.nodeValue;
    /** @type {number} */
    const length = text.length;
    for (let offset = 0; offset < length;) {
      const nextCheckPoint = this.advancedTo(node, offset);
      this.emit(text.substring(offset, nextCheckPoint));
      offset = nextCheckPoint;
    }
    this.advancedTo(node, length);
  }

  /**
   * @private
   * @param {!HTMLElement} element
   */
  handleInnerEditorOf(element) {
    /** @type {!HTMLDivElement} */
    const innerEditor = internals.innerEditorElement(element);
    assert_equals(innerEditor.tagName, 'DIV',
                  'Internal error: inner editor DIV not found.');
    innerEditor.childNodes.forEach(child => {
      assert_true(isCharacterData(child),
                  'Internal error: inner editor having child node that is ' +
                  'not CharacterData.');
      this.handleCharacterData(child);
    });
  }

  /**
   * @private
   * @param {!HTMLInputElement} element
   */
  handleInputNode(element) {
    this.emit(' value="');
    this.handleInnerEditorOf(element);
    this.emit('"');
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
          // HTMLInputElement's value attribute need special handling.
          if (isHTMLInputElement(element)) {
            if (attr.name === 'value')
              return;
          }
          if (attr.value === '')
            return this.emit(` ${attr.name}`);
          const value = attr.value.replace(/&/g, '&amp;')
                            .replace(/\u0022/g, '&quot;')
                            .replace(/\u0027/g, '&apos;');
          this.emit(` ${attr.name}="${value}"`);
        });
    if (isHTMLInputElement(element) && element.value)
      this.handleInputNode(element);
    this.emit('>');
    if (HTML5_VOID_ELEMENTS.has(tagName))
      return;
    this.serializeChildren(element);
    this.emit(`</${tagName}>`);
  }

  /**
   * @public
   * @param {!HTMLDocument} document
   */
  serialize(document) {
    if (document.body)
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
    // For TEXTAREA, handle its inner editor instead of its children.
    if (isHTMLTextAreaElement(element) && element.value) {
      this.handleInnerEditorOf(element);
      return;
    }

    element.childNodes.forEach(child => this.serializeInternal(child));
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
 * @param {!Document} document
 * @return {boolean}
 */
function hasPendingSpellCheckRequest(document) {
  return internals.lastSpellCheckRequestSequence(document) !==
      internals.lastSpellCheckProcessedSequence(document);
}

/** @type {string} */
const kTitle = 'title';
/** @type {string} */
const kCallback = 'callback';
/** @type {string} */
const kIsSpellcheckTest = 'isSpellcheckTest';
/** @type {string} */
const kNeedsFullCheck = 'needsFullCheck';

// Spellchecker gets triggered not only by text and selection change, but also
// by focus change. For example, misspelling markers in <INPUT> disappear when
// the window loses focus, even though the selection does not change.
// Therefore, we disallow spellcheck tests from running simultaneously to
// prevent interference among them. If we call spellcheck_test while another
// test is running, the new test will be added into testQueue waiting for the
// completion of the previous test.

/** @type {boolean} */
var spellcheckTestRunning = false;
/** @type {!Array<!Object>} */
const testQueue = [];

/** @type {?Function} */
let verificationForCurrentTest = null;

/**
 * @param {!Test} testObject
 * @param {!Sample|string} input
 * @param {function(!Document)|string} tester
 * @param {string} expectedText
 */
function invokeSpellcheckTest(testObject, input, tester, expectedText) {
  spellcheckTestRunning = true;

  testObject.step(() => {
    // TODO(xiaochengh): Merge the following part with |assert_selection|.
    /** @type {!Sample} */
    const sample = typeof(input) === 'string' ? new Sample(input) : input;
    testObject.sample = sample;

    sample.setMockSpellCheckerEnabled(true);
    sample.setSpellCheckResolvedCallback(() => {
      if (verificationForCurrentTest)
         verificationForCurrentTest();
    });

    if (typeof(tester) === 'function') {
      tester.call(window, sample.document, sample.window.testRunner);
    } else if (typeof(tester) === 'string') {
      const strings = tester.split(/ (.+)/);
      sample.document.execCommand(strings[0], false, strings[1]);
    } else {
      assert_unreached(`Invalid tester: ${tester}`);
    }

    assert_not_equals(
        window.testRunner, undefined,
        'window.testRunner is required for automated spellcheck tests.');
    assert_not_equals(
        window.internals, undefined,
        'window.internals is required for automated spellcheck tests.');

    assert_equals(
        verificationForCurrentTest, null,
        'Internal error: previous test not verified yet');

    verificationForCurrentTest = () => {
      if (hasPendingSpellCheckRequest(sample.document))
        return;

      testObject.step(() => {
        /** @type {!MarkerSerializer} */
        const serializer = new MarkerSerializer({
          spelling: '#',
          grammar: '~'});

        assert_equals(serializer.serialize(sample.document), expectedText);
        testObject.done();
      });
    };
    if (internals.idleTimeSpellCheckerState(sample.document) === 'HotModeRequested')
      internals.runIdleTimeSpellChecker(sample.document);
    if (testObject.properties[kNeedsFullCheck]) {
      while (internals.idleTimeSpellCheckerState(sample.document) !== 'Inactive')
        internals.runIdleTimeSpellChecker(sample.document);
    }

    // For a test that does not create new spell check request, a synchronous
    // verification finishes everything.
    verificationForCurrentTest();
  });
}

add_result_callback(testObj => {
    if (!testObj.properties[kIsSpellcheckTest])
      return;

    verificationForCurrentTest = null;

    /** @type {boolean} */
    var shouldRemoveSample = false;
    if (testObj.status === testObj.PASS) {
      if (testObj.properties[kCallback])
        testObj.properties[kCallback](testObj.sample);
      else
        shouldRemoveSample = true;
    } else {
      if (window.testRunner)
        shouldRemoveSample = true;
    }

    if (shouldRemoveSample)
      testObj.sample.remove();
    else
      testObj.sample.keep();

    // This is the earliest timing when a new spellcheck_test can be started.
    spellcheckTestRunning = false;

    /** @type {Object} */
    const next = testQueue.shift();
    if (next === undefined)
      return;
    invokeSpellcheckTest(next.testObject, next.input,
                         next.tester, next.expectedText);
});

/**
 * @param {Object=} passedArgs
 * @return {!Object}
 */
function getTestArguments(passedArgs) {
  const args = {};
  args[kIsSpellcheckTest] = true;
  [kTitle, kCallback, kNeedsFullCheck].forEach(key => args[key] = undefined);
  if (!passedArgs)
    return args;

  if (typeof(passedArgs) === 'string') {
    args[kTitle] = passedArgs;
    return args;
  }

  [kTitle, kCallback, kNeedsFullCheck].forEach(
      key => args[key] = passedArgs[key]);
  return args;
}

/**
 * @param {!Sample|string} input
 * @param {function(!Document)|string} tester
 * @param {string} expectedText
 * @param {Object=} opt_args
 */
function spellcheckTest(input, tester, expectedText, opt_args) {
  /** @type {!Object} */
  const args = getTestArguments(opt_args);
  /** @type {!Test}  */
  const testObject = async_test(args[kTitle], args);

  if (spellcheckTestRunning) {
    testQueue.push({
        testObject: testObject, input: input,
        tester: tester, expectedText: expectedText});
    return;
  }

  invokeSpellcheckTest(testObject, input, tester, expectedText);
}

// Export symbols
window.spellcheck_test = spellcheckTest;
})();
