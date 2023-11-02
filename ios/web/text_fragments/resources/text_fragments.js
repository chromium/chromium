// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as utils from '//third_party/text-fragments-polyfill/src/src/text-fragment-utils.js';

/**
 * @fileoverview Interface used for Chrome/WebView to call into the
 * text-fragments-polyfill lib, which handles finding text fragments provided
 * by the navigation layer, highlighting them, and scrolling them into view.
 */

(function() {

  __gCrWeb['textFragments'] = {};

  let marks;
  let cachedFragments;

  /**
   * Attempts to identify and highlight the given text fragments and
   * optionally, scroll them into view, and apply default colors.
   * @param {object[]} fragments - Text fragments to process
   * @param {bool} scroll - scroll into view
   * @param {string} backgroundColor - default text fragments background
   *    color in hexadecimal value enabled by IOSSharedHighlightingColorChange
   *    feature flag
   * @param {string} foregroundColor - default text fragments foreground
   *    color in hexadecimal value enabled by IOSSharedHighlightingColorChange
   *    feature flag.
   */
  __gCrWeb.textFragments.handleTextFragments =
      function(fragments, scroll, backgroundColor, foregroundColor) {
    // If |marks| already exists, it's because we've already highlighted
    // fragments on this page. This might happen if the user got here by
    // navigating back. Stop now to avoid creating nested <mark> elements.
    if (marks?.length)
      return;

    const markDefaultStyle = backgroundColor && foregroundColor ? {
      backgroundColor: `#${backgroundColor}`,
      color: `#${foregroundColor}`
    } : null;

    if (document.readyState === "complete" ||
        document.readyState === "interactive") {
      doHandleTextFragments(fragments, scroll, markDefaultStyle);
    } else {
      document.addEventListener('DOMContentLoaded', () => {
        doHandleTextFragments(fragments, scroll, markDefaultStyle);
      });
    }
  };

  __gCrWeb.textFragments.removeHighlights = function(new_url) {
    utils.removeMarks(marks);
    marks = null;
    document.removeEventListener("click", handleClick,
                                 /*useCapture=*/true);
    if (new_url) {
      try {
        history.replaceState(
            history.state,  // Don't overwrite any existing state object
            "",  // Title param is required but unused
            new_url);
      } catch (err) {
        // history.replaceState throws an exception if the origin of the new URL
        // is different from the current one. This shouldn't happen, but if it
        // does, we don't want the exception to bubble up and cause
        // side-effects.
      }
    }
  };

  /**
   * Does the actual work for handleTextFragments.
   */
  const doHandleTextFragments = function(fragments, scroll, markStyle) {
    marks = [];
    let successCount = 0;

    if (markStyle) utils.setDefaultTextFragmentsStyle(markStyle);

    for (const fragment of fragments) {
      // Process the fragments, then filter out any that evaluate to false.
      const foundRanges = utils.processTextFragmentDirective(fragment)
          .filter((mark) => { return !!mark });

      if (Array.isArray(foundRanges)) {
        // If length < 1, then nothing was found. If length > 1, the spec says
        // to take the first instance.
        if (foundRanges.length >= 1) {
          ++successCount;
          let newMarks = utils.markRange(foundRanges[0]);
          if (Array.isArray(newMarks)) {
            marks.push(...newMarks);
          }
        }
      }
    }

    if (scroll && marks.length > 0) {
      cachedFragments = fragments;
      utils.scrollElementIntoView(marks[0]);
    }

    // Send events back to the browser when the user taps a mark, and when the
    // user taps the page anywhere. We have to send both because one is consumed
    // when kIOSSharedHighlightingV2 is enabled, and the other when it's
    // disabled, and this JS file doesn't know about flag states.

    // Use capture to make sure the event listener is executed immediately and
    // cannot be prevented by the event target (during bubble phase).
    document.addEventListener("click", handleClick, /*useCapture=*/true);
    for (let mark of marks) {
      mark.addEventListener("click", handleClickWithSender.bind(mark), true);
    }

    __gCrWeb.common.sendWebKitMessage('textFragments', {
      command: 'textFragments.processingComplete',
      result: {
        successCount: successCount,
        fragmentsCount: fragments.length
      }
    });
  };

  const handleClick = function () {
      __gCrWeb.common.sendWebKitMessage('textFragments', {
        command: 'textFragments.onClick'
      });
    };

  const handleClickWithSender = function(event) {
    const mark = event.currentTarget;

    // Traverse upwards from the mark element to see if it's a child of an <a>.
    // If so, discard the event to prevent showing a menu while navigation is
    // in progress.
    let node = mark.parentNode;
    while (node != null) {
      if (node.tagName == 'A') {
        return;
      }
      node = node.parentNode;
    }

    __gCrWeb.common.sendWebKitMessage('textFragments', {
      command: 'textFragments.onClickWithSender',
      rect: rectFromElement(mark),
      text: `"${mark.innerText}"`,
      fragments: cachedFragments
    });
  };

  const rectFromElement = function(elt) {
    const domRect = elt.getClientRects()[0];
    return {
      x: domRect.x,
      y: domRect.y,
      width: domRect.width,
      height: domRect.height
    };
  };
})();
