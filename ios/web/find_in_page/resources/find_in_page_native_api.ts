// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {
  CSS_CLASS_NAME,
  CSS_CLASS_NAME_SELECT,
  CSS_STYLE_ID,
  IGNORE_NODE_NAMES,
  MAX_VISIBLE_ELEMENTS,
  TIMEOUT
} from '//ios/web/find_in_page/resources/find_in_page_constants.js';

import {Match, PartialMatch, Replacement, Section, Timer} from
    '//ios/web/find_in_page/resources/find_in_page.js';
import {createRegex, escapeHTML} from
    '//ios/web/find_in_page/resources/find_in_page_utils.js';

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
// clang-format on

/**
 * Based on code from the Google iOS app.
 *
 * @fileoverview A find in page tool. It can:
 *   1. Search for given string in the DOM, and highlight them in yellow color;
 *   2. Allow users to navigate through all match results, and highlight the
 * selected one in orange color;
 */

// Mark: Private properties

/**
 * A string made by concatenating textContent.toLowerCase() of all TEXT nodes
 * within current web page.
 * @type {string}
 */
let allText_ = '';

/**
 * All the sections_ in |allText_|.
 * @type {Array<Section>}
 */
let sections_: Section[] = [];

/**
 * The index of the Section where the last PartialMatch is found.
 */
let sectionsIndex_ = 0;

/**
 * Index of the currently selected match relative to all visible matches
 * on the frame. -1 if there is no currently selected match.
 * @type {number}
 */
let selectedVisibleMatchIndex_ = -1;

/**
 * Index of the currently selected match relative to
 * __gCrWeb.findInPage.matches. -1 if there is no currently selected match.
 * @type {number}
 */
let selectedMatchIndex_ = -1;

/**
 * The ID for the next Match found in |allText_|. This ID is used for
 * identifying PartialMatches of the Match, so that when
 * |processPartialMatchesInCurrentSection| is called, the <chrome_find> Nodes
 * created for each PartialMatch can be recorded in the corresponding Match.
 */
let matchId_ = 0;

/**
 * A temporary array used for storing all PartialMatches inside current Section.
 * @type {Array<PartialMatch>}
 */
let partialMatches_: PartialMatch[] = [];

/**
 * The replacements of current FindInPage action.
 * @type {Array<Replacement>}
 */
let replacements_: Replacement[] = [];

/**
 * The index of the Replacement from which the highlight process continue when
 * pumpSearch is called.
 * @type {Number}
 */
let replacementsIndex_ = 0;

/**
 * The total number of visible matches found.
 * @type {Number}
 */
let visibleMatchCount_ = 0;

/**
 * The last index from which the match counting process left off.
 * This is necessary since the counting might be interrupted by pumping.
 */
let visibleMatchesCountIndexIterator_ = 0;

/**
 * The style DOM element that we add.
 * @type {Element}
 */
let styleElement_: Element|null;

/**
 * A search is in progress.
 * @type {boolean}
 */
let searchInProgress_ = false;

/**

 * Whether or not search state variables are in a clean empty state.
 * @type {boolean}
 */
let searchStateIsClean_ = true;

// Mark: Private helper functions

/**
 * Do binary search in |sections_|[sectionsIndex_, ...) to find the first
 * Section S which has S.end > |index|.
 * @param {number} index The search target. This should be a valid index of
 *     |allText_|.
 * @return {number} The index of the result in |sections_|.
 */
function findFirstSectionEndsAfter_(index: number): number {
  let left = sectionsIndex_;
  let right = sections_.length;
  while (left < right) {
    let mid = Math.floor((left + right) / 2);
    const section = sections_[mid];
    if (section && section.end <= index) {
      left = mid + 1;
    } else {
      right = mid;
    }
  }
  return left;
}

/**
 * Process all PartialMatches inside current Section. For current Section's
 * node.textContent, all texts that are match results will be wrapped in
 * <chrome_find>, and other texts will be put inside plain TEXT Nodes. All
 * created Nodes will be stored in the Replacement of current Section, and all
 * <chrome_find> Nodes will also be recorded in their belonging Matches.
 * |partialMatches_| will be cleared when processing ends.
 * @return {undefined}
 */
function processPartialMatchesInCurrentSection(): void {
  if (partialMatches_.length == 0)
    return;
  let section = sections_[sectionsIndex_];
  if (!section) {
    return;
  }
  let oldNode = section.node;
  if (!oldNode.ownerDocument || !oldNode.textContent) {
    return;
  }
  let newNodes: Node[] = [];
  let previousEnd = section.begin;
  for (const partialMatch of partialMatches_) {
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
    newNode.innerHTML = escapeHTML(oldNode.textContent.substring(
        partialMatch.begin - section.begin, partialMatch.end - section.begin));
    newNodes.push(newNode);
    previousEnd = partialMatch.end;

    // Record the <chrome_find> Node in corresponding Match.
    gCrWeb.findInPage.matches[partialMatch.matchId].nodes.push(newNode);
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
 * @return {Match} The currently selected Match. Returns undefined if no
 * currently selected match.
 */
function getCurrentSelectedMatch_(): Match|undefined {
  if (selectedMatchIndex_ < 0) {
    return undefined;
  }
  return gCrWeb.findInPage.matches[selectedMatchIndex_];
};

/**
 * Counts the total number of visible matches.
 * @param {Timer} used to pause the counting if overall search
 * has taken too long.
 * @return {Number} of visible matches.
 */
function countVisibleMatches_(timer: Timer|null): number {
  let max = gCrWeb.findInPage.matches.length;
  let maxVisible = MAX_VISIBLE_ELEMENTS;
  var currentlyVisibleMatchCount = 0;
  for (let index = visibleMatchesCountIndexIterator_; index < max; index++) {
    if (timer && timer.overtime()) {
      visibleMatchesCountIndexIterator_ = index;
      return TIMEOUT;
    }

    // Stop after |maxVisible| elements.
    if (currentlyVisibleMatchCount > maxVisible) {
      continue;
    }

    let match = gCrWeb.findInPage.matches[index];
    if (match && match.visible()) {
      currentlyVisibleMatchCount++;
    }
  }
  visibleMatchCount_ = currentlyVisibleMatchCount;
  visibleMatchesCountIndexIterator_ = 0;
  return currentlyVisibleMatchCount;
};

/**
 * Removes highlights of previous search and reset all global vars.
 * @return {undefined}
 */
function cleanUp_(): void {
  for (const replacement of replacements_) {
    replacement.undoSwap();
  }

  allText_ = '';
  sections_ = [];
  sectionsIndex_ = 0;

  gCrWeb.findInPage.matches = [];
  selectedMatchIndex_ = -1;
  selectedVisibleMatchIndex_ = -1;
  matchId_ = 0;
  partialMatches_ = [];

  replacements_ = [];
  replacementsIndex_ = 0;

  searchStateIsClean_ = true;
};

/**
 * Scrolls to the position of the currently selected match.
 */
function scrollToCurrentlySelectedMatch_(): void {
  let match = getCurrentSelectedMatch_();
  if (!match) {
    return;
  }

  const nodes = match.nodes;
  if (!nodes || nodes.length == 0) {
    return;
  }

  const firstNode = nodes[0];
  if (!firstNode) {
    return;
  }

  firstNode.scrollIntoView({block: 'center', inline: 'center'});
};

/**
 * Enable find in page by adding the appropriate style element to the page.
 */
function enable_(): void {
  if (styleElement_) {
    // Already enabled.
    return;
  }

  let styleContent: string[] = [];
  function addCSSRule(name: string, style: string) {
    styleContent.push(name, '{', style, '}');
  };
  addCSSRule(
      '.' + CSS_CLASS_NAME,
      'background-color:#ffff00 !important;' +
          'padding:0px;margin:0px;' +
          'overflow:visible !important;');
  addCSSRule(
      '.' + CSS_CLASS_NAME_SELECT,
      'background-color:#ff9632 !important;' +
          'padding:0px;margin:0px;' +
          'overflow:visible !important;');
  styleElement_ = document.createElement('style');
  styleElement_.id = CSS_STYLE_ID;
  styleElement_.setAttribute('type', 'text/css');
  styleElement_.appendChild(document.createTextNode(styleContent.join('')));
  document.body.appendChild(styleElement_);
};

/**
 * Removes the style element from the page.
 */
function removeStyle_(): void {
  if (styleElement_) {
    let style = document.getElementById(CSS_STYLE_ID);
    if (style) {
      document.body.removeChild(style);
    }
    styleElement_ = null;
  }
};

// Mark: Public API functions called from native code.

/**
 * Looks for a phrase in the DOM.
 * @param {string} string Phrase to look for like "ben franklin".
 * @param {number} timeout Maximum time to run.
 * @return {number} that represents the total matches found.
 */
function findString(string: string, timeout: number): number {
  // Enable findInPage module if hasn't been done yet.
  if (!gCrWeb.findInPage.hasInitialized) {
    enable_();
    gCrWeb.findInPage.hasInitialized = true;
  }

  if (!searchStateIsClean_) {
    // Clean up a previous run.
    cleanUp_();
  }
  if (!string) {
    // No searching for emptyness.
    return 0;
  }

  // Holds what nodes we have not processed yet.
  gCrWeb.findInPage.stack = [document.body];

  // Number of visible matches found.
  visibleMatchCount_ = 0;

  // Index tracking variables so search can be broken up into multiple calls.
  visibleMatchesCountIndexIterator_ = 0;

  gCrWeb.findInPage.regex = createRegex(string);

  searchInProgress_ = true;

  return pumpSearch(timeout);
};

/**
 * Do following steps:
 *   1. Do a DFS in the page, concatenate all TEXT Nodes' content into
 *      |allText_|, and create |sections_| to record which part of |allText_|
 *      belongs to which Node;
 *   2. Do regex match in |allText_| to find all matches, create |replacements_|
 *      for highlighting all results and |matches_| for
 *      highlighting selected result;
 *   3. Execute |replacements_| to highlight all results;
 *   4. Check the visibility of each Match;
 *
 * If |timeout| has been reached, the function will return TIMEOUT, and the
 * caller need to call this function again to continue searching. This prevents
 * the Js thread from blocking the WebView's UI.
 *
 * @param {number} timeout Only run find in page until timeout.
 * @return {number} that represents the total matches found.
 */
function pumpSearch(timeout: number): number {
  // TODO(crbug.com/41420794): It would be better if this DCHECKed.
  if (searchInProgress_ == false) {
    return 0;
  }

  searchStateIsClean_ = false;

  let timer = new Timer(timeout);

  // Go through every node in DFS fashion.
  while (gCrWeb.findInPage.stack.length) {
    let node = gCrWeb.findInPage.stack.pop();
    let children = node.childNodes;
    if (children && children.length) {
      // add all (reasonable) children
      for (let i = children.length - 1; i >= 0; --i) {
        let child = children[i];
        if ((child.nodeType == 1 || child.nodeType == 3) &&
            !IGNORE_NODE_NAMES.has(child.nodeName)) {
          gCrWeb.findInPage.stack.push(children[i]);
        }
      }
    }

    // Build up |allText_| and |sections_|.
    if (node.nodeType == 3 && node.parentNode) {
      sections_.push(new Section(
          allText_.length, allText_.length + node.textContent.length, node));
      allText_ += node.textContent.toLowerCase();
    }

    if (timer.overtime()) {
      return TIMEOUT;
    }
  }

  // Do regex match in |allText_|, create |matches| and |replacements|. The
  // regex is set on __gCrWeb, so its state is kept between continuous calls on
  // pumpSearch.
  let regex = gCrWeb.findInPage.regex;
  if (regex) {
    for (let res; res = regex.exec(allText_);) {
      // The range of current Match in |allText_| is [begin, end).
      let begin = res.index;
      let end = begin + res[0].length;
      gCrWeb.findInPage.matches.push(new Match());

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
        if (!section) {
          break;
        }
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

      if (timer.overtime()) {
        return TIMEOUT;
      }
    }
    // Process remaining PartialMatches.
    processPartialMatchesInCurrentSection();
    gCrWeb.findInPage.regex = undefined;
  }

  // Execute replacements to highlight search results.
  for (let i = replacementsIndex_; i < replacements_.length; ++i) {
    if (timer.overtime()) {
      replacementsIndex_ = i;
      return TIMEOUT;
    }
    const replacement = replacements_[i];
    if (replacement) {
      replacement.doSwap();
    }
  }

  let visibleMatchCount = countVisibleMatches_(timer);

  searchInProgress_ = false;

  return visibleMatchCount;
};

/**
 * Selects the |index|-th visible matchand scrolls to that match. The total
 * visible matches count is also recalculated.
 * If there is no longer an |index|-th visible match, then the last visible
 * match will be selected if |index| is less than the currently selected
 * match or the first visible match will be selected if |index| is greater.
 * If there are currently no visible matches, sets
 * |selectedVisibleMatchIndex_| to -1.
 * No-op if invalid |index| is passed.
 * @param {Number} index of visible match to highlight.
 * @return {Dictionary} of currently visible matches and currently selected
 * match index.
 */
function selectAndScrollToVisibleMatch(index: number):
    {matches: number, index: number, contextString?: string} {
  if (index >= visibleMatchCount_ || index < 0) {
    // Do nothing if invalid index is passed or if there are no matches.
    return {matches: visibleMatchCount_, index: selectedMatchIndex_};
  }

  // Remove previous highlight.
  let match = getCurrentSelectedMatch_();
  if (match) {
    match.removeSelectHighlight();
  }

  // Recalculate total visible matches in case it has changed.
  let visibleMatchCount = countVisibleMatches_(null);

  if (visibleMatchCount == 0) {
    selectedMatchIndex_ = -1;
    selectedVisibleMatchIndex_ = -1;
    return {matches: visibleMatchCount, index: -1};
  }

  if (index >= visibleMatchCount) {
    // There are no longer that many visible matches.
    // Select the last match if moving to previous match.
    // Select the first currently visible match if moving to next match.
    index = index > selectedVisibleMatchIndex_ ? 0 : visibleMatchCount - 1;
  }

  let total_match_index = 0;
  var visible_match_count = index;
  // Select the |index|-th visible match.
  while (total_match_index < gCrWeb.findInPage.matches.length) {
    if (gCrWeb.findInPage.matches[total_match_index].visible()) {
      visible_match_count--;
      if (visible_match_count < 0) {
        break;
      }
    }
    total_match_index++;
  }

  selectedMatchIndex_ = total_match_index;
  selectedVisibleMatchIndex_ = index;

  match = getCurrentSelectedMatch_();
  if (!match) {
    return {matches: visibleMatchCount, index: -1};
  }
  match.addSelectHighlight();
  scrollToCurrentlySelectedMatch_();

  // Get string consisting of the text contents of the match nodes and the
  // nodes before and after them, if applicable.
  // This will be read out as an accessibility notification for the match.
  // The siblings's textContent are added into the node array, because the
  // nextSibling and previousSibling properties to the match nodes sometimes
  // are text nodes, not HTML nodes. This results in '[object Text]' string
  // being added to the array instead of the object.

  let contextString = '';
  const firstNode = match.nodes[0];
  if (firstNode && firstNode.previousSibling) {
    contextString += firstNode.previousSibling.textContent;
  }
  contextString += match.nodes
                       .map(function(node) {
                         if (node.textContent) {
                           return node.textContent;
                         } else {
                           return node;
                         }
                       })
                       .join('');

  const lastNode = match.nodes[match.nodes.length - 1];
  if (lastNode && lastNode.nextSibling) {
    contextString += lastNode.nextSibling.textContent;
  }

  return {
    matches: visibleMatchCount,
    index: index,
    contextString: contextString
  };
};

/**
 * Disables the __gCrWeb.findInPage module and removes any matches and the style
 * and class names.
 */
function stop(): void {
  if (styleElement_) {
    removeStyle_();
    cleanUp_();
  }
  gCrWeb.findInPage.hasInitialized = false;
};

// Mark: Public API

/**
 * The list of all the matches in current page.
 * @type {Array<Match>}
 */
let matches: Match[] = [];

gCrWeb.findInPage = {
  findString,
  matches,
  pumpSearch,
  selectAndScrollToVisibleMatch,
  stop
};
