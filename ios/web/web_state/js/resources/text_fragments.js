// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('__crWeb.textFragments');
goog.module.declareLegacyNamespace();

const utils = goog.require(
    'googleChromeLabs.textFragmentPolyfill.textFragmentUtils');

/**
 * @fileoverview Interface used for Chrome/WebView to call into the
 * text-fragments-polyfill lib, which handles finding text fragments provided
 * by the navigation layer, highlighting them, and scrolling them into view.
 */

(function() {

  __gCrWeb['textFragments'] = {};

  /**
   * Attempts to identify and highlight the given text fragments and,
   * optionally, scroll them into view.
   */
  __gCrWeb.textFragments.handleTextFragments = function(fragments, scroll) {
    if (document.readyState === "complete" ||
        document.readyState === "interactive") {
      doHandleTextFragments(fragments, scroll);
    } else {
      document.addEventListener("DOMContentLoaded", () => {
        doHandleTextFragments(fragments, scroll);
      });
    }
  }

  /**
   * Does the actual work for handleTextFragments.
   */
  const doHandleTextFragments = function(fragments, scroll) {
    const marks = [];
    let successCount = 0;

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
    document.addEventListener("click", function removeMarksFunction() {
      utils.removeMarks(marks);
      document.removeEventListener("click", removeMarksFunction,
                                   /*useCapture=*/true);
    }, /*useCapture=*/true);

    __gCrWeb.message.invokeOnHost({
      command: 'textFragments.response',
      result: {
        successCount: successCount,
        fragmentsCount: fragments.length
      }
    });
  }
})();
