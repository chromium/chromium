// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Finds the paragraph closest to the middle of the viewport and returns
// information to identify it.
function findScrollAnchor(): void {
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
    const pText = closestParagraph.innerText;
    // The hashing method must be consistent with `scrollToParagraphByHash` in
    // `components/dom_distiller/core/javascript/dom_distiller_viewer.js`.
    const hashCode = (s: string) =>
        s.split('').reduce((a, b) => ((a << 5) - a + b.charCodeAt(0)) | 0, 0);
    const progress = (viewportMiddle - rect.top) / rect.height;
    sendWebKitMessage('ReaderModeScrollAnchorMessageHandler', {
      'hash': hashCode(pText),
      'charCount': pText.length,
      'progress': progress,
      'isScrolledAtTop': window.scrollY === 0,
    });
  }
}

const readerModeScrollAnchorApi = new CrWebApi();
readerModeScrollAnchorApi.addFunction('findScrollAnchor', findScrollAnchor);
gCrWeb.registerApi('readerModeScrollAnchor', readerModeScrollAnchorApi);
