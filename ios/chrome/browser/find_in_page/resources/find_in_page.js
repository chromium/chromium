// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Based on code from the Google iOS app.
 *
 * @fileoverview A find in page tool. It can:
 *   1. Search for given string in the DOM, and highlight them in yellow color;
 *   2. Allow users to navigate through all match results, and highlight the
 * selected one in orange color;
 */

(function() {
/**
   * Namespace for this file.
   * This overrides the ios/web find in page implementation to ensure there are
   * no unintended collisions.
   */
__gCrWeb.findInPage = {};

/**
 * A string made by concatenating textContent.toLowerCase() of all TEXT nodes
 * within current web page.
 * @type {string}
 */
let allText_ = '';

/**
 * A Section contains the info of one TEXT node in the |allText_|. The node's
 * textContent is [begin, end) of |allText_|.
 */
class Section {
  /**
   * @param {number} begin Beginning index of |node|.textContent in |allText_|.
   * @param {number} end Ending index of |node|.textContent in |allText_|.
   * @param {Node} node The TEXT Node of this section.
   */
  constructor(begin, end, node) {
    this.begin = begin;
    this.end = end;
    this.node = node;
  }
}

/**
 * All the sections_ in |allText_|.
 * @type {Array<Section>}
 */
let sections_ = [];

/**
 * The index of the Section where the last PartialMatch is found.
 */
let sectionsIndex_ = 0;

/**
 * Do binary search in |sections_|[sectionsIndex_, ...) to find the first
 * Section S which has S.end > |index|.
 * @param {number} index The search target. This should be a valid index of
 *     |allText_|.
 * @return {number} The index of the result in |sections_|.
 */
function findFirstSectionEndsAfter_(index) {
  let left = sectionsIndex_;
  let right = sections_.length;
  while (left < right) {
    let mid = Math.floor((left + right) / 2);
    if (sections_[mid].end <= index) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  return left;
}

/**
 * A Match represents a match result in the document. |this.nodes| stores all
 * the <chrome_find> Nodes created for highlighting the matched text. If it
 * contains only one Node, it means the match is found within one HTML TEXT
 * Node, otherwise the match involves multiple HTML TEXT Nodes.
 */
class Match {
  constructor() {
    this.nodes = [];
  }

  /**
   * Returns if all <chrome_find> Nodes of this match are visible.
   * @return {Boolean} If the Match is visible.
   */
  visible() {
    for (let i = 0; i < this.nodes.length; ++i) {
      if (!isElementVisible_(this.nodes[i]))
        return false;
    }
    return true;
  }

  /**
   * Adds orange color highlight for "selected match result", over the yellow
   * color highlight for "normal match result".
   * @return {undefined}
   */
  addSelectHighlight() {
    for (let i = 0; i < this.nodes.length; ++i) {
      this.nodes[i].className = (this.nodes[i].className || '') + ' findysel';
    }
  }

  /**
   * Clears the orange color highlight.
   * @return {undefined}
   */
  removeSelectHighlight() {
    for (let i = 0; i < this.nodes.length; ++i) {
      this.nodes[i].className =
          (this.nodes[i].className || '').replace(/\sfindysel/g, '');
    }
  }
}

/**
 * The list of all the matches in current page.
 * @type {Array<Match>}
 */
__gCrWeb.findInPage.matches = [];

/**
 * Index of the current highlighted choice.  -1 means none.
 * @type {number}
 */
__gCrWeb.findInPage.selectedMatchIndex = -1;

/**
 * The ID for the next Match found in |allText_|. This ID is used for
 * identifying PartialMatches of the Match, so that when
 * |processPartialMatchesInCurrentSection| is called, the <chrome_find> Nodes
 * created for each PartialMatch can be recorded in the corresponding Match.
 */
let matchId_ = 0;

/**
 * A part of a Match, within a Section. A Match may cover multiple sections_ in
 * |allText_|, so it must be split into multiple PartialMatches and then
 * dispatched into the Sections they belong. The range of a PartialMatch in
 * |allText_| is [begin, end). Exactly one <chrome_find> will be created for
 * each PartialMatch.
 */
class PartialMatch {
  /**
   * @param {number} matchId ID of the Match to which this PartialMatch belongs.
   * @param {number} begin Beginning index of partial match text in |allText_|.
   * @param {number} end Ending index of partial match text in |allText_|.
   */
  constructor(matchId, begin, end) {
    this.matchId = matchId;
    this.begin = begin;
    this.end = end;
  }
}

/**
 * A temporary array used for storing all PartialMatches inside current Section.
 * @type {Array<PartialMatch>}
 */
let partialMatches_ = [];

/**
 * A Replacement represents a DOM operation that swaps |oldNode| with |newNodes|
 * under the parent of |oldNode| to highlight the match result inside |oldNode|.
 * |newNodes| may contain plain TEXT Nodes for unhighlighted parts and
 * <chrome_find> nodes for highlighted parts. This operation will be executed
 * reversely when clearing current highlights for next FindInPage action.
 */
class Replacement {
  /**
   * @param {Node} oldNode The HTML Node containing search result.
   * @param {Array<Node>} newNodes New HTML Nodes created for substitution of
   *     |oldNode|.
   */
  constructor(oldNode, newNodes) {
    this.oldNode = oldNode;
    this.newNodes = newNodes;
  }

  /**
   * Executes the replacement to highlight search result.
   * @return {undefined}
   */
  doSwap() {
    let parentNode = this.oldNode.parentNode;
    if (!parentNode)
      return;
    for (var i = 0; i < this.newNodes.length; ++i) {
      parentNode.insertBefore(this.newNodes[i], this.oldNode);
    }
    parentNode.removeChild(this.oldNode);
  }

  /**
   * Executes the replacement reversely to clear the highlight.
   * @return {undefined}
   */
  undoSwap() {
    let parentNode = this.newNodes[0].parentNode;
    if (!parentNode)
      return;
    parentNode.insertBefore(this.oldNode, this.newNodes[0]);
    for (var i = 0; i < this.newNodes.length; ++i) {
      parentNode.removeChild(this.newNodes[i]);
    }
  }
}

/**
 * The replacements of current FindInPage action.
 * @type {Array<Replacement>}
 */
let replacements_ = [];

/**
 * The index of the Replacement from which the highlight process continue when
 * pumpSearch is called.
 * @type {Number}
 */
let replacementsIndex_ = 0;

/**
 * Process all PartialMatches inside current Section. For current Section's
 * node.textContent, all texts that are match results will be wrapped in
 * <chrome_find>, and other texts will be put inside plain TEXT Nodes. All
 * created Nodes will be stored in the Replacement of current Section, and all
 * <chrome_find> Nodes will also be recorded in their belonging Matches.
 * |partialMatches_| will be cleared when processing ends.
 * @return {undefined}
 */
function processPartialMatchesInCurrentSection() {
  if (partialMatches_.length == 0)
    return;
  let section = sections_[sectionsIndex_];
  let oldNode = section.node;
  let newNodes = [];
  let previousEnd = section.begin;
  for (let i = 0; i < partialMatches_.length; ++i) {
    let partialMatch = partialMatches_[i];
    // Create the TEXT node for leading non-matching string piece. Notice that
    // substr must be taken from TEXT Node.textContent instead of |allText_|
    // since it's in lower case.
    if (partialMatch.begin > previousEnd) {
      newNodes.push(
          oldNode.ownerDocument.createTextNode(oldNode.textContent.substring(
              previousEnd - section.begin,
              partialMatch.begin - section.begin)));
    }
    // Create the <chrome_find> Node for matching text.
    let newNode = oldNode.ownerDocument.createElement('chrome_find');
    newNode.setAttribute('class', CSS_CLASS_NAME);
    newNode.innerHTML = escapeHTML_(oldNode.textContent.substring(
        partialMatch.begin - section.begin, partialMatch.end - section.begin));
    newNodes.push(newNode);
    previousEnd = partialMatch.end;

    // Record the <chrome_find> Node in corresponding Match.
    __gCrWeb.findInPage.matches[partialMatch.matchId].nodes.push(newNode);
  }
  // Create the TEXT node for trailing non-matching string piece.
  if (previousEnd != section.end) {
    newNodes.push(
        oldNode.ownerDocument.createTextNode(oldNode.textContent.substring(
            previousEnd - section.begin, section.end - section.begin)));
  }

  // Create the Replacement of current Section.
  replacements_.push(new Replacement(oldNode, newNodes));

  partialMatches_ = [];
}

/**
 * The list of frame documents.
 * TODO(crbug.com/895529): x-domain frames won't work.
 * @type {Array<Document>}
 */
let frameDocs_ = [];

/**
 * The style DOM element that we add.
 * @type {Element}
 */
let styleElement_ = null;

/**
 * Width we expect the page to be.  For example (320/480) for iphone,
 * (1024/768) for ipad.
 * @type {number}
 */
let pageWidth_ = 320;

/**
 * Height we expect the page to be.
 * @type {number}
 */
let pageHeight_ = 480;

/**
 * Maximum number of visible elements to count
 * @type {number}
 */
const MAX_VISIBLE_ELEMENTS = 100;

/**
 * A search is in progress.
 * @type {boolean}
 */
let searchInProgress_ = false;

/**
 * Node names that are not going to be processed.
 * @type {Object}
 */
const IGNORE_NODE_NAMES =
    new Set(['SCRIPT', 'STYLE', 'EMBED', 'OBJECT', 'SELECT', 'TEXTAREA']);

/**
 * Class name of CSS element.
 * @type {string}
 */
const CSS_CLASS_NAME = 'find_in_page';

/**
 * ID of CSS style.
 * @type {string}
 */
const CSS_STYLE_ID = '__gCrWeb.findInPageStyle';

/**
 * Result passed back to app to indicate no results for the query.
 * @type {string}
 */
const NO_RESULTS = '[0,[0,0,0]]';

/**
 * Result passed back to app to indicate pumpSearch has reached timeout.
 * @type {string}
 */
const TIMEOUT = '[false]';

/**
 * Regex to escape regex special characters in a string.
 * @type {RegExp}
 */
const REGEX_ESCAPER = /([.?*+^$[\]\\(){}|-])/g;

/**
 * @return {Match} The currently selected Match.
 */
function getCurrentSelectedMatch_() {
  return __gCrWeb.findInPage.matches[__gCrWeb.findInPage.selectedMatchIndex];
};

/**
 * Creates the regex needed to find the text.
 * @param {string} findText Phrase to look for.
 * @return {RegExp} regex needed to find the text.
 */
function getRegex_(findText) {
  let regexString = '(' + escapeRegex_(findText) + ')';
  return new RegExp(regexString, 'ig');
};

/**
 * A timer that checks timeout for long tasks.
 */
class Timer {
  /**
   * @param {Number} timeoutMs Timeout in milliseconds.
   */
  constructor(timeoutMs) {
    this.beginTime = Date.now();
    this.timeoutMs = timeoutMs;
  }

  /**
   * @return {Boolean} Whether this timer has been reached.
   */
  overtime() {
    return Date.now() - this.beginTime > this.timeoutMs;
  }
}

/**
 * Looks for a phrase in the DOM.
 * @param {string} findText Phrase to look for like "ben franklin".
 * @param {number} timeout Maximum time to run.
 * @return {string} How many results there are in the page in the form of
       [highlightedWordsCount, [index, pageLocationX, pageLocationY]].
 */
__gCrWeb.findInPage.highlightWord = function(findText, timeout) {
  if (__gCrWeb.findInPage.matches && __gCrWeb.findInPage.matches.length) {
    // Clean up a previous run.
    cleanUp_();
  }
  if (!findText) {
    // No searching for emptyness.
    return NO_RESULTS;
  }

  // Holds what nodes we have not processed yet.
  __gCrWeb.findInPage.stack = [];

  // Push frames into stack too.
  for (let i = frameDocs_.length - 1; i >= 0; i--) {
    let doc = frameDocs_[i];
    __gCrWeb.findInPage.stack.push(doc);
  }
  __gCrWeb.findInPage.stack.push(document.body);

  // Number of visible elements found.
  __gCrWeb.findInPage.visibleFound = 0;

  // Index tracking variables so search can be broken up into multiple calls.
  __gCrWeb.findInPage.visibleIndex = 0;

  __gCrWeb.findInPage.regex = getRegex_(findText);

  searchInProgress_ = true;

  return __gCrWeb.findInPage.pumpSearch(timeout);
};

/**
 * Do following steps:
 *   1. Do a DFS in the page, concatenate all TEXT Nodes' content into
 *      |allText_|, and create |sections_| to record which part of |allText_|
 *      belongs to which Node;
 *   2. Do regex match in |allText_| to find all matches, create |replacements_|
 *      for highlighting all results and |__gCrWeb.findInPage.matches| for
 *      highlighting selected result;
 *   3. Execute |replacements_| to highlight all results;
 *   4. Check the visibility of each Match;
 *   5. Call __gCrWeb.findInPage.goNext.
 *
 * If |timeout| has been reached, the function will return TIMEOUT, and the
 * caller need to call this function again to continue searching. This prevents
 * the Js thread from blocking the WebView's UI.
 *
 * @param {number} timeout Only run find in page until timeout.
 * @return {string} string in the form of "[bool, int]", where bool indicates
                    whether the text was found and int idicates text position.
 */
__gCrWeb.findInPage.pumpSearch = function(timeout) {
  // TODO(crbug.com/895531): It would be better if this DCHECKed.
  if (searchInProgress_ == false)
    return NO_RESULTS;

  let timer = new Timer(timeout);

  // Go through every node in DFS fashion.
  while (__gCrWeb.findInPage.stack.length) {
    let node = __gCrWeb.findInPage.stack.pop();
    let children = node.childNodes;
    if (children && children.length) {
      // add all (reasonable) children
      for (let i = children.length - 1; i >= 0; --i) {
        let child = children[i];
        if ((child.nodeType == 1 || child.nodeType == 3) &&
            !IGNORE_NODE_NAMES.has(child.nodeName)) {
          __gCrWeb.findInPage.stack.push(children[i]);
        }
      }
    }

    // Build up |allText_| and |sections_|.
    if (node.nodeType == 3 && node.parentNode) {
      sections_.push(new Section(
          allText_.length, allText_.length + node.textContent.length, node));
      allText_ += node.textContent.toLowerCase();
    }

    if (timer.overtime())
      return TIMEOUT;
  }

  // Do regex match in |allText_|, create |matches| and |replacements|. The
  // regex is set on __gCrWeb, so its state is kept between continuous calls on
  // pumpSearch.
  let regex = __gCrWeb.findInPage.regex;
  if (regex) {
    for (let res; res = regex.exec(allText_);) {
      // The range of current Match in |allText_| is [begin, end).
      let begin = res.index;
      let end = begin + res[0].length;
      __gCrWeb.findInPage.matches.push(new Match());

      // Find the Section where current Match starts.
      let oldSectionIndex = sectionsIndex_;
      let newSectionIndex = findFirstSectionEndsAfter_(begin);
      // If current Match starts at a new Section, process current Section and
      // move to the new Section.
      if (newSectionIndex > oldSectionIndex) {
        processPartialMatchesInCurrentSection();
        sectionsIndex_ = newSectionIndex;
      }

      // Create all PartialMatches of current Match.
      while (true) {
        let section = sections_[sectionsIndex_];
        partialMatches_.push(new PartialMatch(
            matchId_, Math.max(section.begin, begin),
            Math.min(section.end, end)));
        // If current Match.end exceeds current Section.end, process current
        // Section and move to next Section.
        if (section.end < end) {
          processPartialMatchesInCurrentSection();
          ++sectionsIndex_;
        } else {
          // Current Match ends in current Section.
          break;
        }
      }
      ++matchId_;

      if (timer.overtime())
        return TIMEOUT;
    }
    // Process remaining PartialMatches.
    processPartialMatchesInCurrentSection();
    __gCrWeb.findInPage.regex = undefined;
  }

  // Execute replacements to highlight search results.
  for (let i = replacementsIndex_; i < replacements_.length; ++i) {
    if (timer.overtime()) {
      replacementsIndex_ = i;
      return TIMEOUT;
    }
    replacements_[i].doSwap();
  }

  // Count visible elements.
  let max = __gCrWeb.findInPage.matches.length;
  let maxVisible = MAX_VISIBLE_ELEMENTS;
  for (let index = __gCrWeb.findInPage.visibleIndex; index < max; index++) {
    let match = __gCrWeb.findInPage.matches[index];
    if (timer.overtime()) {
      __gCrWeb.findInPage.visibleIndex = index;
      return TIMEOUT;
    }

    // Stop after |maxVisible| elements.
    if (__gCrWeb.findInPage.visibleFound > maxVisible) {
      match.visibleIndex = maxVisible;
      continue;
    }

    if (match.visible()) {
      __gCrWeb.findInPage.visibleFound++;
      match.visibleIndex = __gCrWeb.findInPage.visibleFound;
    }
  }

  searchInProgress_ = false;

  let pos = __gCrWeb.findInPage.goNext();
  if (pos) {
    return '[' + __gCrWeb.findInPage.visibleFound + ',' + pos + ']';
  } else {
    return NO_RESULTS;
  }
};

/**
 * Removes highlights of previous search and reset all global vars.
 * @return {undefined}
 */
function cleanUp_() {
  for (let i = 0; i < replacements_.length; ++i) {
    replacements_[i].undoSwap();
  }

  allText_ = '';
  sections_ = [];
  sectionsIndex_ = 0;

  __gCrWeb.findInPage.matches = [];
  __gCrWeb.findInPage.selectedMatchIndex = -1;
  matchId_ = 0;
  partialMatches_ = [];

  replacements_ = [];
  replacementsIndex_ = 0;
};

/**
 * Increments the index of the current selected Match or, if the index is
 * already at the end, sets it to the index of the first Match in the page.
 */
__gCrWeb.findInPage.incrementIndex = function() {
  if (__gCrWeb.findInPage.selectedMatchIndex >=
      __gCrWeb.findInPage.matches.length - 1) {
    __gCrWeb.findInPage.selectedMatchIndex = 0;
  } else {
    __gCrWeb.findInPage.selectedMatchIndex++;
  }
};

/**
 * Switches to the next result, animating a little highlight in the process.
 * @return {string} JSON encoded array of coordinates to scroll to, or blank if
 *     nothing happened.
 */
__gCrWeb.findInPage.goNext = function() {
  if (!__gCrWeb.findInPage.matches || __gCrWeb.findInPage.matches.length == 0) {
    return '';
  }
  if (__gCrWeb.findInPage.selectedMatchIndex >= 0) {
    // Remove previous highlight.
    getCurrentSelectedMatch_().removeSelectHighlight();
  }
  // Iterate through to the next index, but because they might not be visible,
  // keep trying until you find one that is.  Make sure we don't loop forever by
  // stopping on what we are currently highlighting.
  let oldIndex = __gCrWeb.findInPage.selectedMatchIndex;
  __gCrWeb.findInPage.incrementIndex();
  while (!getCurrentSelectedMatch_().visible()) {
    if (oldIndex === __gCrWeb.findInPage.selectedMatchIndex) {
      // Checked all matches but didn't find anything else visible.
      return '';
    }
    __gCrWeb.findInPage.incrementIndex();
    if (0 === __gCrWeb.findInPage.selectedMatchIndex && oldIndex < 0) {
      // Didn't find anything visible and haven't highlighted anything yet.
      return '';
    }
  }
  // Return scroll dimensions.
  return findScrollDimensions_();
};

/**
 * Decrements the index of the current selected Match or, if the index is
 * already at the beginning, sets it to the index of the last Match in the page.
 */
__gCrWeb.findInPage.decrementIndex = function() {
  if (__gCrWeb.findInPage.selectedMatchIndex <= 0) {
    __gCrWeb.findInPage.selectedMatchIndex =
        __gCrWeb.findInPage.matches.length - 1;
  } else {
    __gCrWeb.findInPage.selectedMatchIndex--;
  }
};

/**
 * Switches to the previous result, animating a little highlight in the process.
 * @return {string} JSON encoded array of coordinates to scroll to, or blank if
 *     nothing happened.
 */
__gCrWeb.findInPage.goPrev = function() {
  if (!__gCrWeb.findInPage.matches || __gCrWeb.findInPage.matches.length == 0) {
    return '';
  }
  if (__gCrWeb.findInPage.selectedMatchIndex >= 0) {
    // Remove previous highlight.
    getCurrentSelectedMatch_().removeSelectHighlight();
  }
  // Iterate through to the next index, but because they might not be visible,
  // keep trying until you find one that is.  Make sure we don't loop forever by
  // stopping on what we are currently highlighting.
  let old = __gCrWeb.findInPage.selectedMatchIndex;
  __gCrWeb.findInPage.decrementIndex();
  while (!getCurrentSelectedMatch_().visible()) {
    __gCrWeb.findInPage.decrementIndex();
    if (old == __gCrWeb.findInPage.selectedMatchIndex) {
      // Checked all matches but didn't find anything.
      return '';
    }
  }

  // Return scroll dimensions.
  return findScrollDimensions_();
};

/**
 * Normalize coordinates according to the current document dimensions. Don't go
 * too far off the screen in either direction. Try to center if possible.
 * @param {Element} elem Element to find normalized coordinates for.
 * @return {Array<number>} Normalized coordinates.
 */
function getNormalizedCoordinates_(elem) {
  let pos = findAbsolutePosition_(elem);
  let maxX = Math.max(getBodyWidth_(), pos[0] + elem.offsetWidth);
  let maxY = Math.max(getBodyHeight_(), pos[1] + elem.offsetHeight);
  // Don't go too far off the screen in either direction.  Try to center if
  // possible.
  let xPos = Math.max(
      0, Math.min(maxX - window.innerWidth, pos[0] - (window.innerWidth / 2)));
  let yPos = Math.max(
      0,
      Math.min(maxY - window.innerHeight, pos[1] - (window.innerHeight / 2)));
  return [xPos, yPos];
};

/**
 * Scale coordinates according to the width of the screen, in case the screen
 * is zoomed out.
 * @param {Array<number>} coordinates Coordinates to scale.
 * @return {Array<number>} Scaled coordinates.
 */
function scaleCoordinates_(coordinates) {
  let scaleFactor = pageWidth_ / window.innerWidth;
  return [coordinates[0] * scaleFactor, coordinates[1] * scaleFactor];
};

/**
 * Finds the position of the result.
 * @return {string} JSON encoded array of the scroll coordinates "[x, y]".
 */
function findScrollDimensions_() {
  let match = getCurrentSelectedMatch_();
  if (!match) {
    return '';
  }
  let normalized = getNormalizedCoordinates_(match.nodes[0]);
  let xPos = normalized[0];
  let yPos = normalized[1];

  match.addSelectHighlight();
  let scaled = scaleCoordinates_(normalized);
  let index = match.visibleIndex;
  scaled.unshift(index);
  return __gCrWeb.stringify(scaled);
};

/**
 * Initialize the __gCrWeb.findInPage module.
 * @param {number} width Width of page.
 * @param {number} height Height of page.

 */
__gCrWeb.findInPage.init = function(width, height) {
  if (__gCrWeb.findInPage.hasInitialized) {
    return;
  }
  pageWidth_ = width;
  pageHeight_ = height;
  frameDocs_ = getFrameDocuments_();
  enable_();
  __gCrWeb.findInPage.hasInitialized = true;
};

/**
 * Enable the __gCrWeb.findInPage module.
 * Mainly just adds the style for the classes.
 */
function enable_() {
  if (styleElement_) {
    // Already enabled.
    return;
  }
  addStyle_();
};

/**
 * Gets the scale ratio between the application window and the web document.
 * @return {number} Scale.
 */
function getPageScale_() {
  return (pageWidth_ / getBodyWidth_());
};

/**
 * Adds the appropriate style element to the page.
 */
function addStyle_() {
  addDocumentStyle_(document);
  for (let i = frameDocs_.length - 1; i >= 0; i--) {
    let doc = frameDocs_[i];
    addDocumentStyle_(doc);
  }
};

function addDocumentStyle_(thisDocument) {
  let styleContent = [];
  function addCSSRule(name, style) {
    styleContent.push(name, '{', style, '}');
  };
  let scale = getPageScale_();
  let zoom = (1.0 / scale);
  let left = ((1 - scale) / 2 * 100);
  addCSSRule(
      '.' + CSS_CLASS_NAME,
      'background-color:#ffff00 !important;' +
          'padding:0px;margin:0px;' +
          'overflow:visible !important;');
  addCSSRule(
      '.findysel',
      'background-color:#ff9632 !important;' +
          'padding:0px;margin:0px;' +
          'overflow:visible !important;');
  styleElement_ = thisDocument.createElement('style');
  styleElement_.id = CSS_STYLE_ID;
  styleElement_.setAttribute('type', 'text/css');
  styleElement_.appendChild(thisDocument.createTextNode(styleContent.join('')));
  thisDocument.body.appendChild(styleElement_);
};

/**
 * Removes the style element from the page.
 */
function removeStyle_() {
  if (styleElement_) {
    removeDocumentStyle_(document);
    for (let i = frameDocs_.length - 1; i >= 0; i--) {
      let doc = frameDocs_[i];
      removeDocumentStyle_(doc);
    }
    styleElement_ = null;
  }
};

function removeDocumentStyle_(thisDocument) {
  let style = thisDocument.getElementById(CSS_STYLE_ID);
  thisDocument.body.removeChild(style);
};

/**
 * Disables the __gCrWeb.findInPage module.
 * Basically just removes the style and class names.
 */
__gCrWeb.findInPage.disable = function() {
  if (styleElement_) {
    removeStyle_();
    window.setTimeout(cleanUp_, 0);
  }
  __gCrWeb.findInPage.hasInitialized = false;
};

/**
 * Returns the width of the document.body.  Sometimes though the body lies to
 * try to make the page not break rails, so attempt to find those as well.
 * An example: wikipedia pages for the ipad.
 * @return {number} Width of the document body.
 */
function getBodyWidth_() {
  let body = document.body;
  let documentElement = document.documentElement;
  return Math.max(
      body.scrollWidth, documentElement.scrollWidth, body.offsetWidth,
      documentElement.offsetWidth, body.clientWidth,
      documentElement.clientWidth);
};

/**
 * Returns the height of the document.body.  Sometimes though the body lies to
 * try to make the page not break rails, so attempt to find those as well.
 * An example: wikipedia pages for the ipad.
 * @return {number} Height of the document body.
 */
function getBodyHeight_() {
  let body = document.body;
  let documentElement = document.documentElement;
  return Math.max(
      body.scrollHeight, documentElement.scrollHeight, body.offsetHeight,
      documentElement.offsetHeight, body.clientHeight,
      documentElement.clientHeight);
};

/**
 * Helper function that determines if an element is visible.
 * @param {Element} elem Element to check.
 * @return {boolean} Whether elem is visible or not.
 */
function isElementVisible_(elem) {
  if (!elem) {
    return false;
  }
  let top = 0;
  let left = 0;
  let bottom = Infinity;
  let right = Infinity;

  let originalElement = elem;
  let nextOffsetParent = originalElement.offsetParent;

  // We are currently handling all scrolling through the app, which means we can
  // only scroll the window, not any scrollable containers in the DOM itself. So
  // for now this function returns false if the element is scrolled outside the
  // viewable area of its ancestors.
  // TODO(crbug.com/915357): handle scrolling within the DOM.
  let bodyHeight = getBodyHeight_();
  let bodyWidth = getBodyWidth_();

  while (elem && elem.nodeName.toUpperCase() != 'BODY') {
    let computedStyle =
        elem.ownerDocument.defaultView.getComputedStyle(elem, null);

    if (elem.style.display === 'none' || elem.style.visibility === 'hidden' ||
        elem.style.opacity === 0 || computedStyle.display === 'none' ||
        computedStyle.visibility === 'hidden' || computedStyle.opacity === 0) {
      return false;
    }

    // For the original element and all ancestor offsetParents, trim down the
    // visible area of the original element.
    if (elem.isSameNode(originalElement) || elem.isSameNode(nextOffsetParent)) {
      let visible = elem.getBoundingClientRect();
      if (elem.style.overflow === 'hidden' &&
          (visible.width === 0 || visible.height === 0))
        return false;

      top = Math.max(top, visible.top + window.pageYOffset);
      bottom = Math.min(bottom, visible.bottom + window.pageYOffset);
      left = Math.max(left, visible.left + window.pageXOffset);
      right = Math.min(right, visible.right + window.pageXOffset);

      // The element is not within the original viewport.
      let notWithinViewport = top < 0 || left < 0;

      // The element is flowing off the boundary of the page. Note this is
      // not comparing to the size of the window, but the calculated offset
      // size of the document body. This can happen if the element is within
      // a scrollable container in the page.
      let offPage = right > bodyWidth || bottom > bodyHeight;
      if (notWithinViewport || offPage) {
        return false;
      }
      nextOffsetParent = elem.offsetParent;
    }

    elem = elem.parentNode;
  }
  return true;
};

/**
 * Helper function to find the absolute position of an element on the page.
 * @param {Element} elem Element to check.
 * @return {Array<number>} [x, y] positions.
 */
function findAbsolutePosition_(elem) {
  let boundingRect = elem.getBoundingClientRect();
  return [
    boundingRect.left + window.pageXOffset,
    boundingRect.top + window.pageYOffset
  ];
};

/**
 * @param {string} text Text to escape.
 * @return {string} escaped text.
 */
function escapeHTML_(text) {
  let unusedDiv = document.createElement('div');
  unusedDiv.innerText = text;
  return unusedDiv.innerHTML;
};

/**
 * Escapes regexp special characters.
 * @param {string} text Text to escape.
 * @return {string} escaped text.
 */
function escapeRegex_(text) {
  return text.replace(REGEX_ESCAPER, '\\$1');
};

/**
 * Gather all iframes in the main window.
 * @return {Array<Document>} frames.
 */
function getFrameDocuments_() {
  let windowsToSearch = [window];
  let documents = [];
  while (windowsToSearch.length != 0) {
    let win = windowsToSearch.pop();
    for (let i = win.frames.length - 1; i >= 0; i--) {
      // The following try/catch catches a webkit error when searching a page
      // with iframes. See crbug.com/702566 for details.
      // To verify that this is still necessary:
      // 1. Remove this try/catch.
      // 2. Go to a page with iframes.
      // 3. Search for anything.
      // 4. Check if the webkit debugger spits out SecurityError (DOM Exception)
      // and the search fails. If it doesn't, feel free to remove this.
      try {
        if (win.frames[i].document) {
          documents.push(win.frames[i].document);
          windowsToSearch.push(win.frames[i]);
        }
      } catch (e) {
        // Do nothing.
      }
    }
  }
  return documents;
};

window.addEventListener('pagehide', __gCrWeb.findInPage.disable);
})();
