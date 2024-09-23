// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface used to monitor and extract visible text on the page
 * and pass it on to the annotations manager.
 */

import {TextAnnotationList, TextViewportAnnotation} from '//ios/web/annotations/resources/text_annotation_list.js';
import {TextClick} from '//ios/web/annotations/resources/text_click.js';
import {annotationExternalData, annotationFullText} from '//ios/web/annotations/resources/text_decoration.js';
import {TextDecorator} from '//ios/web/annotations/resources/text_decorator.js';
import {TextDOMObserver} from '//ios/web/annotations/resources/text_dom_observer.js';
import {getMetaContentByHttpEquiv, hasNoIntentDetection, HTMLElementWithSymbolIndex, NodeWithSymbolIndex, noFormatDetectionTypes, rectFromElement} from '//ios/web/annotations/resources/text_dom_utils.js';
import {TextChunk, TextExtractor} from '//ios/web/annotations/resources/text_extractor.js';
import {TextIntersectionObserver} from '//ios/web/annotations/resources/text_intersection_observer.js';
import {TextStyler} from '//ios/web/annotations/resources/text_styler.js';
import {IdleTaskTracker} from '//ios/web/annotations/resources/text_tasks.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Used to trigger text extraction and decoration when system is idle.
let idleTaskTracker: IdleTaskTracker|null;
let intersectionObserver: TextIntersectionObserver|null;
let mutationObserver: TextDOMObserver|null;
let styler: TextStyler|null;
let decorator: TextDecorator|null;
let extractor: TextExtractor|null;
let click: TextClick|null;

// Mark: Private API

let uniqueId = 0;
const chunksInFlight = new Map<number, TextChunk>();

// Consumer of text `chunk` that, memorizes them and sends them to the browser
// side for intent detection.
function textChunkConsumer(chunk: TextChunk): void {
  chunksInFlight.set(++uniqueId, chunk);
  if (chunksInFlight.size === 1) {
    idleTaskTracker?.startActivityListeners();
  }
  let disabledTypes = noFormatDetectionTypes();
  sendWebKitMessage('annotations', {
    command: 'annotations.extractedText',
    text: chunk.text,
    seqId: uniqueId,
    metadata: {
      htmlLang: document.documentElement.lang,
      httpContentLanguage: getMetaContentByHttpEquiv('content-language'),
      wkNoTelephone: disabledTypes.has('telephone'),
      wkNoEmail: disabledTypes.has('email'),
      wkNoAddress: disabledTypes.has('address'),
      wkNoDate: disabledTypes.has('date'),
      wkNoUnit: disabledTypes.has('unit'),
    },
  });
}

// Creates a task to decorate the given `annotations` against the TextChunk with
// `seqId`. The text chunk is then deleted.
function decorateChunk(
    annotations: TextViewportAnnotation[], seqId: number): void {
  // Find and remove the TextChunk.
  const chunk = chunksInFlight.get(seqId);
  chunksInFlight.delete(seqId);

  idleTaskTracker?.schedule(() => {
    const run = new TextAnnotationList(
        annotations, chunk?.visibleStart, chunk?.visibleEnd);
    const count = annotations.length;
    if (decorator && chunk) {
      decorator.decorateChunk(chunk, run);
    } else {
      run.fail();
    }
    sendWebKitMessage('annotations', {
      command: 'annotations.decoratingComplete',
      annotations: count,
      successes: run.successes,
      failures: run.failures,
      cancelled: run.cancelled,
    });
  });

  if (chunksInFlight.size === 0) {
    idleTaskTracker?.stopActivityListeners();
  }
}

// Consumer of taps on annotations. Forwards to the browser side.
function tapConsumer(
    annotation: HTMLElementWithSymbolIndex, cancel: boolean): void {
  decorator?.highlightAnnotation(annotation);
  sendWebKitMessage('annotations', {
    command: 'annotations.onClick',
    cancel: cancel,
    data: annotation[annotationExternalData],
    rect: rectFromElement(annotation),
    text: annotation[annotationFullText],
  });
}

function decorationNodeRemovedConsumer(node: NodeWithSymbolIndex): void {
  decorator?.decorationNodeUnexpectedlyRemoved(node);
  // Clean cache of corrupted annotation on the browser side.
  sendWebKitMessage('annotations', {
    command: 'annotations.decoratingComplete',
    annotations: 0,
    successes: 0,
    failures: 1,
    cancelled: [node[annotationExternalData]],
  });
}

// Mark: Public API

// Starts the annotation observer.
function start(): void {
  // Check for already started or for a page request to not detect intent.
  if (hasNoIntentDetection() || intersectionObserver) {
    return;
  }
  const root = document.documentElement;
  idleTaskTracker = new IdleTaskTracker();
  click = new TextClick(root, tapConsumer, () => decorator?.decorations);
  extractor = new TextExtractor(textChunkConsumer);
  styler = new TextStyler();
  decorator = new TextDecorator(styler);
  intersectionObserver =
      new TextIntersectionObserver(root, extractor, idleTaskTracker);
  mutationObserver = new TextDOMObserver(
      root, intersectionObserver, decorationNodeRemovedConsumer);
  intersectionObserver.start();
  mutationObserver.start();
  click.start();
}

// Stops the annotation observer.
function stop(): void {
  click?.stop();
  idleTaskTracker?.shutdown();
  mutationObserver?.stop();
  intersectionObserver?.stop();
  decorator?.removeAllDecorations();
  styler = null;
  decorator = null;
  intersectionObserver = null;
  mutationObserver = null;
  extractor = null;
  idleTaskTracker = null;
  click = null;
}

// Triggers `decorator` to apply `annotations` on the cached text chunk for
// the given `seqId`.
function decorateAnnotations(
    annotations: TextViewportAnnotation[], seqId: number): void {
  decorateChunk(annotations, seqId);
}

function removeDecorations(): void {
  decorator?.removeAllDecorations();
}

function removeDecorationsWithType(type: string): void {
  decorator?.removeDecorationsOfType(type);
}

function removeHighlight(): void {
  decorator?.removeHighlight();
}

gCrWeb.annotations = {
  start,
  stop,
  decorateAnnotations,
  removeDecorations,
  removeDecorationsWithType,
  removeHighlight,
};
