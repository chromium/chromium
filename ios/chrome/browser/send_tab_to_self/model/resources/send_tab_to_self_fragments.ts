// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {generateFragment, GenerateFragmentStatus, isValidRangeForFragmentGeneration} from '//third_party/text-fragments-polyfill/src/src/fragment-generation-utils.js';
import * as utils from '//third_party/text-fragments-polyfill/src/src/text-fragment-utils.js';

/**
 * @fileoverview Interface for Send Tab To Self to use the
 * text-fragments-polyfill library.
 */

/**
 * Checks if the given element is a valid candidate for text fragment
 * generation.
 */
function isValidElement(element: Element): boolean {
  const tagName = element.tagName ? element.tagName.toUpperCase() : '';
  const isRootElement = tagName === 'BODY' || tagName === 'HTML';
  const hasTooManyChildren = element.childNodes.length >= 50;
  return !isRootElement && !hasTooManyChildren;
}

/** Attempts to generate a text fragment for the viewport center. */
function getLinkToTextForViewportCenter() {
  // Use visualViewport if available to target the visible area (e.g. when
  // pinch-zoomed). Add offset because caretRangeFromPoint expects layout
  // viewport coordinates.
  const viewport = window.visualViewport;
  const centerX = viewport
      ? (viewport.width / 2 + viewport.offsetLeft)
      : window.innerWidth / 2;
  const centerY = viewport
      ? (viewport.height / 2 + viewport.offsetTop)
      : window.innerHeight / 2;

  const range = (document as Document & {
                  caretRangeFromPoint(x: number, y: number): Range,
                }).caretRangeFromPoint(centerX, centerY);

  if (!range) {
    return {status: GenerateFragmentStatus.INVALID_SELECTION};
  }

  // The text-fragments-polyfill expects a non-empty selection range to work.
  // When caretRangeFromPoint returns a collapsed range (i.e., a single point),
  // we must expand it to select the entire surrounding node so the polyfill has
  // text to analyze.
  if (!isValidRangeForFragmentGeneration(range)) {
    const container = range.startContainer;

    if (container.nodeType === Node.TEXT_NODE) {
      // If the point landed directly on text, try to select just that node.
      range.selectNodeContents(container);
    }

    if (!isValidRangeForFragmentGeneration(range)) {
      if (container.nodeType === Node.TEXT_NODE && container.parentNode) {
        // If the text node alone is still not enough (e.g. it's empty or only
        // punctuation), try selecting its parent element.
        const parent = container.parentNode as Element;
        if (parent.nodeType === Node.ELEMENT_NODE && isValidElement(parent)) {
          range.selectNodeContents(parent);
        }
      } else if (container.nodeType === Node.ELEMENT_NODE) {
        // If the point landed on an empty element space, we try to select it.
        // However, we explicitly avoid selecting giant root-level containers
        // (like BODY or HTML) or elements with excessive children (> 50),
        // because expanding a massive range can cause the polyfill to freeze
        // or timeout the page.
        const elem = container as Element;
        if (isValidElement(elem)) {
          range.selectNodeContents(elem);
        }
      }
    }
  }

  // Mock Selection object as required by text-fragments-polyfill.
  const selectionMock = {
    getRangeAt: (_index: number) => range,
    toString: () => range.toString(),
    rangeCount: 1,
  };

  const response = generateFragment(selectionMock as unknown as Selection);
  return {
    status: response.status,
    fragment: response.fragment,
  };
}

/**
 * Inserts a temporary span at the start of the matched range to scroll to it,
 * without modifying the text content with visible highlights.
 */
function scrollRangeIntoView(range: Range) {
  try {
    const span = document.createElement('span');
    span.id = 'stts-scroll-target';
    range.insertNode(span);

    // Find scrollable parent and scroll it into view, starting from the span.
    span.scrollIntoView({
      behavior: 'auto',
      block: 'center',
      inline: 'nearest',
    });

    // Delay removal to allow native scroll to complete in WKWebView.
    window.setTimeout(() => {
      span.remove();
    }, 1000);
  } catch (e: any) {
    // Ignore errors during scroll; we don't want to crash page scripts.
  }
}

/**
 * Scrolls the page to the target text fragment.
 */
async function scrollToTextFragment(fragment: string) {
  try {
    const parsedFragmentDirectives =
        utils.parseFragmentDirectives({text: [fragment]});
    const parsed = parsedFragmentDirectives['text']?.[0];
    if (!parsed) {
      return;
    }

    // Modern sites often render content asynchronously after PageLoaded.
    // Retry searching for the text fragment for up to 1 second to give the
    // page time to populate its DOM.
    const maxAttempts = 10;
    for (let attempts = 0; attempts < maxAttempts; attempts++) {
      const ranges = utils.processTextFragmentDirective(parsed);
      if (ranges && ranges.length > 0) {
        scrollRangeIntoView(ranges[0]);
        return;
      }
      await new Promise(resolve => window.setTimeout(resolve, 100));
    }
  } catch (e: any) {
    // Ignore errors during parse or processing.
  }
}

const sttsApi = new CrWebApi('stts');

sttsApi.addFunction('getLinkToText', getLinkToTextForViewportCenter);
sttsApi.addFunction('scrollToTextFragment', scrollToTextFragment);
gCrWeb.registerApi(sttsApi);

