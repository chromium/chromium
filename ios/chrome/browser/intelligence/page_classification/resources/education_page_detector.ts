// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Extracts readable word count and heading density for
 * Education vertical evaluation.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

interface EducationDomFeatures {
  word_count: number;
  heading_count: number;
}

const IGNORED_TAGS = new Set([
  'SCRIPT',
  'STYLE',
  'NAV',
  'HEADER',
  'FOOTER',
  'NOSCRIPT',
  'SVG',
  'ASIDE',
  'FORM',
  'BUTTON',
  'INPUT',
  'TEXTAREA',
  'SELECT',
  'OPTION',
]);

const IGNORED_SELECTOR =
    'script, style, nav, header, footer, noscript, svg, aside, form, button, ' +
    'input, textarea, select, option, [aria-hidden="true"]';

/**
 * Checks if an element should be ignored and pruned from traversal.
 */
function shouldIgnoreElement(element: Element): boolean {
  return IGNORED_TAGS.has(element.tagName) ||
      element.getAttribute('aria-hidden') === 'true';
}

/**
 * Extracts readable body text word count and heading count from the DOM.
 */
function extractDOMFeatures(): EducationDomFeatures {
  try {
    const body = document.body;
    if (!body) {
      return {word_count: 0, heading_count: 0};
    }

    // Count valid headings (h1 - h4) that are not inside ignored containers.
    let headingCount = 0;
    const headings = body.querySelectorAll('h1, h2, h3, h4');
    for (const h of headings) {
      if (h.textContent && h.textContent.trim().length > 0 &&
          !h.closest(IGNORED_SELECTOR)) {
        headingCount++;
      }
    }

    // Walk visible text nodes using TreeWalker. Subtrees of ignored elements
    // are skipped in O(1) by returning FILTER_REJECT on element nodes.
    let totalWords = 0;
    const walker = document.createTreeWalker(
        body,
        NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
        {
          acceptNode: (node: Node) => {
            if (node.nodeType === Node.ELEMENT_NODE) {
              if (shouldIgnoreElement(node as Element)) {
                return NodeFilter.FILTER_REJECT;
              }
              return NodeFilter.FILTER_SKIP;
            }
            return NodeFilter.FILTER_ACCEPT;
          },
        },
    );

    let node = walker.nextNode();
    while (node) {
      const text = node.nodeValue;
      if (text) {
        const matches = text.trim().match(/\S+/g);
        if (matches) {
          totalWords += matches.length;
        }
      }
      node = walker.nextNode();
    }

    return {
      word_count: totalWords,
      heading_count: headingCount,
    };
  } catch (err) {
    console.error('[PageClassification:JS] extractDOMFeatures error:', err);
    return {word_count: 0, heading_count: 0};
  }
}

const educationDetectorApi = new CrWebApi('education_page_detector');
educationDetectorApi.addFunction('extractDOMFeatures', extractDOMFeatures);
if (!gCrWeb.hasRegisteredApi('education_page_detector')) {
  gCrWeb.registerApi(educationDetectorApi);
}
