// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// iOS compatible implementation of
// https://github.com/chromium/dom-distiller/blob/master/
// heuristics/distillable/extract_features.js#L13.
// For performance reasons, we remove the visibility check, use
// getElementsByTagName(), and retrieve only <p> and <pre> elements once.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

interface Result {
  numElements: number;
  numAnchors: number;
  numForms: number;
  mozScore: number;
  mozScoreAllSqrt: number;
  mozScoreAllLinear: number;
  time: number;
}

/**
 * Helper method for calculateMozScore() to calculate the character length
 * in an element.
 * @param {HTMLElement} element An element to calculate text length.
 * @return {float} The character length of text in |element|.
 */
function getTextLengthForNode(element: HTMLElement): number {
  const unlikelyCandidates = new RegExp(
    'banner|combx|comment|community|disqus|extra|foot|header|menu|related|' +
      'remark|rss|share|shoutbox|sidebar|skyscraper|sponsor|ad-break|' +
      'agegate|pagination|pager|popup');
  const candidates = new RegExp('and|article|body|column|main|shadow');
  const matchString = element.className + ' ' + element.id;
  if (unlikelyCandidates.test(matchString) &&
      !candidates.test(matchString)) {
    return 0;
  }
  if (element.matches && element.matches('li p')) {
    return 0;
  }
  const textContentLength = element.textContent?.length || 0;
  // Caps the max character length to 1000 for each element.
  return Math.min(1000, textContentLength);
}

/**
 * Returns a list of element text lengths for the elements passed in.
 * @param {!HTMLCollection} pElements List of all <p> elements.
 * @param {!HTMLCollection} preElements List of all <pre> elements.
 * element.
 * @return {Array}
 */
function getPageTextContent(
  pElements: HTMLCollectionOf<HTMLElement>,
  preElements: HTMLCollectionOf<HTMLElement>,
): number[] {
  const textLengths: number[] = [];
  for (let i = 0; i < pElements.length; i++) {
    const element: HTMLElement | undefined = pElements[i];
    if (element === undefined) {
      continue;
    }
    textLengths.push(getTextLengthForNode(element));
  }
  for (let i = 0; i < preElements.length; i++) {
    const element: HTMLElement | undefined = preElements[i];
    if (element === undefined) {
      continue;
    }
    textLengths.push(getTextLengthForNode(element));
  }
  return textLengths;
}

/**
 * Calculates the readability score of the page based on the element text
 * length list retrieved from getPageTextContent().
 * @param {!Array} textList List of element text lengths.
 * @param {!float} power Exponent applied to scoring.
 * @param {!int} minimumThreshold Minimum word length in order to count
 * the text in the element.
 * @return {float}
 */
function calculateMozScore(
  textList: number[],
  power: number,
  minimumThreshold: number,
): number {
  let score = 0;
  for (let i = 0; i < textList.length; i++) {
      const textListNum : number | undefined = textList[i];
      if (textListNum === undefined) {
          continue;
      }
    if (textListNum < minimumThreshold) {
      continue;
    }
    score += Math.pow(textListNum - minimumThreshold, power);
  }
  return score;
}

// Retrieves various DOM features and sends them back to the native code.
function retrieveDOMFeatures(): void {
  // Measure execution time to ensure that it remains performant
  // (i.e. single digit milliseconds).
  const start = performance.now();
  const body = document.body;
  if (!body) {
    return;
  }
  const pElements = document.body.getElementsByTagName('p');
  const preElements = document.body.getElementsByTagName('pre');
  const elementTextLengthList = getPageTextContent(pElements, preElements);
  const result: Result = {
    'numElements': body.getElementsByTagName('*').length,
    'numAnchors': body.getElementsByTagName('a').length,
    'numForms': body.getElementsByTagName('form').length,
    'mozScore': Math.min(
      6 * Math.sqrt(1000 - 140),
      calculateMozScore(elementTextLengthList, 0.5, 140),
    ),
    'mozScoreAllSqrt': Math.min(
      6 * Math.sqrt(1000), calculateMozScore(elementTextLengthList, 0.5, 0),
    ),
    'mozScoreAllLinear': Math.min(
      6 * 1000, calculateMozScore(elementTextLengthList, 1, 0),
    ),
    'time': 0,
  };
  const end = performance.now();
  const total = end - start;
  result['time'] = total;
  sendWebKitMessage('ReaderModeMessageHandler', result);
}

const readerModeApi = new CrWebApi();
readerModeApi.addFunction('retrieveDOMFeatures', retrieveDOMFeatures);
gCrWeb.registerApi('readerMode', readerModeApi);
