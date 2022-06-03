// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('__crWeb.textFragments');
goog.module.declareLegacyNamespace();

const utils = goog.require('googleChromeLabs.textFragmentPolyfill.textFragmentUtils');

/**
 * @fileoverview Interface used for Chrome/WebView to call into the
 * text-fragments-polyfill lib, which handles finding text fragments provided
 * by the navigation layer, highlighting them, and scrolling them into view.
 */

(function() {

  __gCrWeb['textFragments'] = {};

  var marks;

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
        // If length < 1, then nothing was found. If length > 1, then the
        // fragment in the URL is ambiguous (i.e., it could identify multiple
        // different places on the page) so we discard it as well.
        if (foundRanges.length === 1) {
          ++successCount;
          let newMarks = utils.markRange(foundRanges[0]);
          if (Array.isArray(newMarks)) {
            marks.push(...newMarks);
          }
        }
      }
    }

    if (scroll && marks.length > 0)
      utils.scrollElementIntoView(marks[0]);

    // Clean-up marks whenever the user taps somewhere on the page. Use capture
    // to make sure the event listener is executed immediately and cannot be
    // prevented by the event target (during bubble phase).
    document.addEventListener("click", handleClick, /*useCapture=*/true);

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
    }
})();
