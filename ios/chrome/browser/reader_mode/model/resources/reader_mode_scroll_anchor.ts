// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

interface ScrollAnchor {
  paragraph: HTMLElement;
  progress: number;
  isScrolledAtTop: boolean;
}

let savedAnchor: ScrollAnchor|null = null;

// Records the paragraph closest to the middle of the viewport and the scroll
// progress within it before the DOM is modified (e.g. by reverting
// translation).
function prepareScrollAnchor(): void {
  savedAnchor = null;
  const viewportMiddle = window.innerHeight / 2;
  const paragraphs = document.querySelectorAll('p');
  let closestParagraph: HTMLElement|null = null;
  let minDistance = Infinity;
  for (let i = 0; i < paragraphs.length; i++) {
    const p = paragraphs[i]!;
    const rect = p.getBoundingClientRect();
    if (rect.height > 0) {
      const pMiddle = rect.top + rect.height / 2;
      const distance = Math.abs(pMiddle - viewportMiddle);
      if (distance < minDistance) {
        minDistance = distance;
        closestParagraph = p;
      }
    }
  }
  if (closestParagraph) {
    const rect = closestParagraph.getBoundingClientRect();
    savedAnchor = {
      paragraph: closestParagraph,
      progress: (viewportMiddle - rect.top) / rect.height,
      isScrolledAtTop: window.scrollY === 0,
    };
  }
}

// Finds the paragraph closest to the middle of the viewport (using the saved
// anchor if `prepareScrollAnchor` was called) and returns information to
// identify it.
function findScrollAnchor(): void {
  if (!savedAnchor) {
    prepareScrollAnchor();
  }
  if (savedAnchor) {
    const pText = savedAnchor.paragraph.innerText;
    // The hashing method must be consistent with `scrollToParagraphByHash` in
    // `components/web_distiller/core/javascript/web_distiller_viewer_main.js`.
    const hashCode = (s: string) =>
        s.split('').reduce((a, b) => ((a << 5) - a + b.charCodeAt(0)) | 0, 0);
    sendWebKitMessage('ReaderModeScrollAnchorMessageHandler', {
      'hash': hashCode(pText),
      'charCount': pText.length,
      'progress': savedAnchor.progress,
      'isScrolledAtTop': savedAnchor.isScrolledAtTop,
    });
    savedAnchor = null;
  }
}

const readerModeScrollAnchorApi = new CrWebApi('readerModeScrollAnchor');
readerModeScrollAnchorApi.addFunction(
    'prepareScrollAnchor', prepareScrollAnchor);
readerModeScrollAnchorApi.addFunction('findScrollAnchor', findScrollAnchor);
gCrWeb.registerApi(readerModeScrollAnchorApi);
