// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The implementation here to calculate the mozScore, mozScoreAllSqrt,
// and mozScoreAllLinear is a copy of what is found in
// https://github.com/chromium/dom-distiller/blob/master/
// heuristics/distillable/extract_features.js#L13
// The visibility check is removed for its poor performance, and
// getElementsByTagName() is used for its faster runtime.
// In addition, the logic is restructured to only need
// to retrieve the <p> and <pre> elements once.

goog.provide('__crWeb.readingListDOM');

(function() {
/**
 * Determines if the page has a particular tag indicating article content.
 * @return {boolean}
 */
function _hasOGArticle() {
  const elems = document.head.querySelectorAll(
      'meta[property="og:type"],meta[name="og:type"]');
  for (const elem of elems) {
    if (elem.content && elem.content.toUpperCase() === 'ARTICLE') {
      return true;
    }
  }
  return false;
}

/**
 * Helper method for _baselineMozScore() to calculate the character length in an
 * element.
 * @param {Element} element An element to calculate the contained text length.
 * @return {float} The character length of text in |element|.
 */
function _getTextLengthForNode(element) {
  let unlikelyCandidates = new RegExp(
      'banner|combx|comment|community|disqus|extra|foot|header|menu|related|' +
      'remark|rss|share|shoutbox|sidebar|skyscraper|sponsor|ad-break|agegate|' +
      'pagination|pager|popup');
  let possibleCandiates = new RegExp('and|article|body|column|main|shadow');
  let matchString = element.className + ' ' + element.id;
  if (unlikelyCandidates.test(matchString) &&
      !possibleCandiates.test(matchString)) {
    return 0;
  }

  if (element.matches && element.matches('li p')) {
    return 0;
  }

  var textContentLength = element.textContent.length;
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
function _getPageTextContent(pElements, preElements) {
  var text_lengths = [];
  for (var i = 0; i < pElements.length; i++) {
    var element = pElements[i];
    text_lengths.push(_getTextLengthForNode(element));
  }
  for (var i = 0; i < preElements.length; i++) {
    var element = preElements[i];
    text_lengths.push(_getTextLengthForNode(element));
  }
  return text_lengths;
}

/**
 * Calculates the readability score of the page based on the element text length
 * list retrieved from _getPageTextContent().
 * @param {!Array} textList List of element text lengths.
 * @param {!float} power Exponent applied to scoring.
 * @param {!int} cut Minimum word length in order to count the text in the
 * element.
 * @return {float}
 */
function _calculateMozScore(textList, power, cut) {
  score = 0;
  for (var i = 0; i < textList.length; i++) {
    if (textList[i] < cut) {
      continue;
    }
    score += Math.pow(textList[i] - cut, power);
  }
  return score;
}

// Retrieves various DOM features and sends them back to the native code.
function _retrieveFeatures() {
  // Measure execution time to ensure that it remains performant
  // (i.e. single digit milliseconds).
  const start = performance.now();

  const body = document.body;
  var p_elements = document.body.getElementsByTagName('p');
  var pre_elements = document.body.getElementsByTagName('pre');
  if (!body) {
    return;
  }

  // Calculate word count in p tags.
  var wordCount = 0;
  for (var i = 0; i < p_elements.length; i++) {
    var matches = p_elements[i].innerText.match(/[\u00ff-\uffff]|\S+/g);
    if (matches) {
      wordCount += matches.length;
    }
  }

  var elementTextLengthList = _getPageTextContent(p_elements, pre_elements);

  const result = {
    'opengraph': _hasOGArticle(),
    'url': document.location.href,
    'numElements': body.getElementsByTagName('*').length,
    'numAnchors': body.getElementsByTagName('a').length,
    'numForms': body.getElementsByTagName('form').length,
    'wordCount': wordCount,
    'mozScore': Math.min(
        6 * Math.sqrt(1000 - 140),
        _calculateMozScore(elementTextLengthList, 0.5, 140)),
    'mozScoreAllSqrt': Math.min(
        6 * Math.sqrt(1000), _calculateMozScore(elementTextLengthList, 0.5, 0)),
    'mozScoreAllLinear':
        Math.min(6 * 1000, _calculateMozScore(elementTextLengthList, 1, 0)),
  }

  const end = performance.now();
  const total = end - start;
  result['time'] = total;
  __gCrWeb.common.sendWebKitMessage('ReadingListDOMMessageHandler', result);
}


// Delay execution for 1 second in case content is added after the DOM is
// created. Delaying can also help prevent performance issues as the page may
// be busy right at document end time.
setTimeout(function() {
  _retrieveFeatures();
}, 1000);
}());
