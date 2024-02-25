// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_decoration.ts.
 */

import {createChromeAnnotation, isDecorationNode, originalNodeDecorationId, replacementNodeDecorationId, TextDecoration} from '//ios/web/annotations/resources/text_decoration.js';
import {HTMLElementWithSymbolIndex, TextWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {expectEq, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextDecoration extends TestSuite {
  // Checks that applying a decoration works properly for the page html and the
  // Symbol tags.
  testTextDecorationReplacement() {
    const originalHTML = '<div id="d1">Hello</div>' +
        '<div id="d2">Small</div>' +
        '<div id="d3">World</div>';
    const decoratedHTML = '<div id="d1">Hello</div>' +
        '<div id="d2"><chrome_annotation>Small</chrome_annotation></div>' +
        '<div id="d3">World</div>';
    load(originalHTML);
    const body = document.body;
    const d2 = document.querySelector('#d2') as HTMLElementWithSymbolIndex;
    const originalTextNode = d2.childNodes[0] as TextWithSymbolIndex;
    const replacement =
        createChromeAnnotation(2, 'Small', 'SIZE', 'Small', 'external-key');

    expectEq(undefined, originalTextNode[originalNodeDecorationId]);
    expectEq(undefined, replacement[replacementNodeDecorationId]);
    const decoration = new TextDecoration(1, originalTextNode, [replacement]);
    expectEq(undefined, originalTextNode[originalNodeDecorationId]);
    expectEq(undefined, replacement[replacementNodeDecorationId]);
    expectEq(false, isDecorationNode(originalTextNode));
    expectEq(false, isDecorationNode(replacement));

    expectEq(originalHTML, body.innerHTML);
    decoration.apply();
    expectEq(true, decoration.live);
    expectEq(decoratedHTML, body.innerHTML);
    expectEq(1, originalTextNode[originalNodeDecorationId]);
    expectEq(1, replacement[replacementNodeDecorationId]);
    expectEq(true, isDecorationNode(originalTextNode));
    expectEq(true, isDecorationNode(replacement));

    // Check for no tagging on parent node.
    expectEq(undefined, d2[originalNodeDecorationId]);
    expectEq(undefined, d2[replacementNodeDecorationId]);

    decoration.restore();
    expectEq(false, decoration.live);
    expectEq(originalHTML, body.innerHTML);
    expectEq(undefined, originalTextNode[originalNodeDecorationId]);
    expectEq(undefined, replacement[replacementNodeDecorationId]);
    expectEq(false, isDecorationNode(originalTextNode));
    expectEq(false, isDecorationNode(replacement));
  }

  // Tests replacing a node with multiple nodes.
  testTextDecorationComplexReplacements() {
    const originalHTML = '<div id="d1">Hello</div>' +
        '<div id="d2">Small</div>' +
        '<div id="d3">World</div>';
    const decoratedHTML = '<div id="d1">Hello</div>' +
        '<div id="d2">S<chrome_annotation>mal</chrome_annotation>l</div>' +
        '<div id="d3">World</div>';
    load(originalHTML);
    const body = document.body;
    const d2 = document.querySelector('#d2')!;
    const originalTextNode = d2.childNodes[0] as TextWithSymbolIndex;
    const prefix = document.createTextNode('S');
    const replacement =
        createChromeAnnotation(2, 'mal', 'SIZE', 'mal', 'external-key');
    const postfix = document.createTextNode('l');

    const decoration =
        new TextDecoration(1, originalTextNode, [prefix, replacement, postfix]);
    decoration.apply();
    expectEq(true, decoration.live);
    expectEq(decoratedHTML, body.innerHTML);
    expectEq(1, originalTextNode[originalNodeDecorationId]);
    expectEq(1, replacement[replacementNodeDecorationId]);
    expectEq(true, isDecorationNode(originalTextNode));
    expectEq(true, isDecorationNode(replacement));

    decoration.restore();
    expectEq(false, decoration.live);
    expectEq(originalHTML, body.innerHTML);
    expectEq(undefined, originalTextNode[originalNodeDecorationId]);
    expectEq(undefined, replacement[replacementNodeDecorationId]);
    expectEq(false, isDecorationNode(originalTextNode));
    expectEq(false, isDecorationNode(replacement));
  }

  // Tests counting and removing decorations by type.
  testTextDecorationTypes() {
    const originalHTML = '<div id="d1">Hello World</div>';
    const decoratedHTML = '<div id="d1">H' +
        '<chrome_annotation>ell</chrome_annotation>' +
        'o W' +
        '<chrome_annotation>orld</chrome_annotation>' +
        '</div>';
    load(originalHTML);
    const body = document.body;
    const d1 = document.querySelector('#d1')!;
    const originalTextNode = d1.childNodes[0] as Text;
    const replacementTextNode1 = document.createTextNode('H');
    const replacement2 =
        createChromeAnnotation(2, 'ell', '@ELL', 'ell', 'external-key');
    const replacementTextNode3 = document.createTextNode('o W');
    const replacement4 =
        createChromeAnnotation(2, 'orld', '@ORLD', 'orld', 'external-key');

    const decoration = new TextDecoration(1, originalTextNode, [
      replacementTextNode1, replacement2, replacementTextNode3, replacement4
    ]);
    decoration.apply();
    expectEq(decoratedHTML, body.innerHTML);
    expectEq(1, decoration.replacementsOfType('@ELL'));
    expectEq(1, decoration.replacementsOfType('@ORLD'));

    const noOrldHTML = '<div id="d1">H' +
        '<chrome_annotation>ell</chrome_annotation>' +
        'o World' +
        '</div>';
    decoration.removeReplacementsOfType('@ORLD');
    expectEq(noOrldHTML, body.innerHTML);

    decoration.restore();
    expectEq(originalHTML, body.innerHTML);
  }

  // Tests merging replacements, before and after the decoration is live.
  testTextDecorationReplaceLive() {
    const originalHTML = '<div id="d1">Hello World</div>';
    const decoratedHTML = '<div id="d1">J' +
        '<chrome_annotation>ell</chrome_annotation>' +
        'o W' +
        '<chrome_annotation>orld</chrome_annotation>' +
        '</div>';
    load(originalHTML);
    const body = document.body;
    const d1 = document.querySelector('#d1')!;
    const originalTextNode = d1.childNodes[0] as Text;
    const replacementTextNode1 = document.createTextNode('H');
    const replacement2 =
        createChromeAnnotation(2, 'ell', '@ELL', 'ell', 'external-key');
    const replacementTextNode3 = document.createTextNode('o W');
    const replacement4 =
        createChromeAnnotation(2, 'orld', '@ORLD', 'orld', 'external-key');

    const decoration = new TextDecoration(1, originalTextNode, [
      replacementTextNode1, replacement2, replacementTextNode3, replacement4
    ]);

    // Before live.
    decoration.replaceReplacementNode(
        replacementTextNode1, [document.createTextNode('J')]);
    decoration.apply();
    expectEq(decoratedHTML, body.innerHTML);

    // After live.
    decoration.replaceReplacementNode(
        replacementTextNode3, [document.createTextNode('y M')]);

    const liveDecoratedHTML = '<div id="d1">J' +
        '<chrome_annotation>ell</chrome_annotation>' +
        'y M' +
        '<chrome_annotation>orld</chrome_annotation>' +
        '</div>';
    expectEq(liveDecoratedHTML, body.innerHTML);

    // The original test should come back on `restore`.
    decoration.restore();
    expectEq(originalHTML, body.innerHTML);
  }

  // Tests proper DOM cleanup when an annotation is reported corrupted.
  testTextDecorationCorrupted() {
    const originalHTML = '<div id="d1">Hello World</div>';
    load(originalHTML);
    const body = document.body;
    const d1 = document.querySelector('#d1')!;
    const originalTextNode = d1.childNodes[0] as Text;
    const replacementTextNode1 = document.createTextNode('H');
    const replacement2 =
        createChromeAnnotation(2, 'ell', '@ELL', 'ell', 'external-key');
    const replacementTextNode3 = document.createTextNode('o W');
    const replacement4 =
        createChromeAnnotation(2, 'orld', '@ORLD', 'orld', 'external-key');

    const decoration = new TextDecoration(1, originalTextNode, [
      replacementTextNode1, replacement2, replacementTextNode3, replacement4
    ]);
    decoration.apply();

    // Something happened and decoration is corrupted, calling
    // `cleanupAfterCorruption` should drop this decoration without restoring
    // `originalTextNode`. The web engine will merge neighbour text nodes.
    decoration.cleanupAfterCorruption();
    expectEq(originalHTML, body.innerHTML);
  }
}

export {TestTextDecoration}
