// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as utils from '//third_party/text-fragments-polyfill/src/src/text-fragment-utils.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Interface used for Chrome/WebView to call into the
 * text-fragments-polyfill lib, which handles finding text fragments provided
 * by the navigation layer, highlighting them, and scrolling them into view.
 */

declare interface TextFragment {
  textStart: string,
  textEnd?: string,
  prefix?: string,
  suffix?: string
}

declare interface MarkStyle {
  backgroundColor: string,
  color: string
}

// Stores an array of <mark> html elements.
let marks: Element[] | null;
let cachedFragments: TextFragment[];

/**
* Attempts to identify and highlight the given text fragments and
* optionally, scroll them into view, and apply default colors.
*/
function handleTextFragments(fragments:TextFragment[], scroll: boolean,
          backgroundColor: string, foregroundColor: string): void {
  // If `marks` already exists, it's because we've already highlighted
  // fragments on this page. This might happen if the user got here by
  // navigating back. Stop now to avoid creating nested <mark> elements.
  if (marks?.length)
    return;

  let markDefaultStyle: MarkStyle | null = null;

  if (backgroundColor && foregroundColor) {
    markDefaultStyle =
      {backgroundColor: `#${backgroundColor}`,
      color: `#${foregroundColor}`};
  }

  if (document.readyState === "complete" ||
      document.readyState === "interactive") {
    doHandleTextFragments(fragments, scroll, markDefaultStyle);
    return;
  }

  document.addEventListener('DOMContentLoaded', () => {
    doHandleTextFragments(fragments, scroll, markDefaultStyle);
  });
};

function removeHighlights(new_url: string): void {
    if (marks) {
        utils.removeMarks(marks);
        marks = null;
    }
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
function doHandleTextFragments(fragments: TextFragment[],
      scroll: boolean, markStyle: MarkStyle | null): void {
  marks = [];
  let successCount = 0;

  if (markStyle) utils.setDefaultTextFragmentsStyle(markStyle);

  for (const fragment of fragments) {
    // Process the fragments, then filter out any that evaluate to false.
    const foundRanges: Range[] = utils.processTextFragmentDirective(fragment)
        .filter((mark) => { return !!mark });

    if (Array.isArray(foundRanges)) {
      // If length < 1, then nothing was found. If length > 1, the spec says
      // to take the first instance.
      if (foundRanges.length >= 1) {
        ++successCount;
        let newMarks: Element[] = utils.markRange(foundRanges[0] as Range);
        if (Array.isArray(newMarks)) {
          marks.push(...newMarks);
        }
      }
    }
  }

  if (scroll && marks.length > 0) {
    cachedFragments = fragments;
    utils.scrollElementIntoView(marks[0] as Element);
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

  sendWebKitMessage('textFragments', {
    command: 'textFragments.processingComplete',
    result: {
      successCount: successCount,
      fragmentsCount: fragments.length
    }
  });
};

function handleClick(): void {
  sendWebKitMessage('textFragments', {
    command: 'textFragments.onClick'
  });
};

function handleClickWithSender(event: Event): void {
  if (!(event.currentTarget instanceof HTMLElement)) {
    return;
  }

  const mark = event.currentTarget as HTMLElement;

  // Traverse upwards from the mark element to see if it's a child of an <a>.
  // If so, discard the event to prevent showing a menu while navigation is
  // in progress.
  let node = mark.parentNode;
  while (node != null) {
    if (node instanceof HTMLElement && node.tagName == 'A') {
      return;
    }
    node = node.parentNode;
  }

  sendWebKitMessage('textFragments', {
    command: 'textFragments.onClickWithSender',
    rect: rectFromElement(mark),
    text: `"${mark.innerText}"`,
    fragments: cachedFragments
  });
};

function rectFromElement(elt: Element) {
  const domRect = elt.getClientRects()[0];
  return {
    x: domRect?.x,
    y: domRect?.y,
    width: domRect?.width,
    height: domRect?.height
  };
};

gCrWeb.textFragments = { handleTextFragments, removeHighlights }
