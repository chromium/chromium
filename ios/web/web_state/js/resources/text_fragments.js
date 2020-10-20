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

  __gCrWeb.textFragments.getLinkToText = function() {
    const selection = window.getSelection();
    const selectedText = `"${selection.toString()}"`;
    const selectionRect = {
      x: 0,
      y: 0,
      width: 0,
      height: 0
    };

    if (selection.rangeCount) {
      const domRect = selection.getRangeAt(0).getClientRects()[0];
      selectionRect.x = domRect.x;
      selectionRect.y = domRect.y;
      selectionRect.width = domRect.width;
      selectionRect.height = domRect.height;
    }

    // TODO(crbug.com/1091918): Call into the JavaScript text-fragments-polyfill
    // library to generate the actual URL.
    const link = 'http://example.com/#:~:text=You%20may%20use%20this%20domain';

    return {
      link: link,
      selectedText: selectedText,
      selectionRect: selectionRect
    };
  }

  /**
   * Does the actual work for handleTextFragments.
   */
  const doHandleTextFragments = function(fragments, scroll) {
    const marks = [];
    let successCount = 0;

    for (const fragment of fragments) {
      // Process the fragments, then filter out any that evaluate to false.
      const newMarks = utils.processTextFragmentDirective(fragment)
          .filter((mark) => { return !!mark });

      if (Array.isArray(newMarks)) {
        if (newMarks.length > 0) {
          ++successCount;
        }

        marks.push(...newMarks);
      }
    }

    if (scroll && marks.length > 0)
      utils.scrollElementIntoView(marks[0]);

    for (const mark of marks) {
      mark.addEventListener("click", () => {
        utils.removeMarks(marks);
      });
    }

    __gCrWeb.message.invokeOnHost({
      command: 'textFragments.response',
      result: {
        successCount: successCount,
        fragmentsCount: fragments.length
      }
    });
  }
})();
