// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Based heavily on code from the Google iOS app.
 *
 * @fileoverview A find in page tool.  It scans the DOM for elements with the
 * text being search for, and wraps them with a span that highlights them.
 */

/**
 * Namespace for this file.  Depends on __gCrWeb having already been injected.
 */
__gCrWeb['findInPage'] = {};

/**
 * Index of the current highlighted choice.  -1 means none.
 * @type {number}
 */
__gCrWeb['findInPage']['index'] = -1;

/**
 * The list of found searches in span form.
 * @type {Array<Element>}
 */
__gCrWeb['findInPage']['spans'] = [];

/**
 * The list of frame documents.
 * TODO(justincohen): x-domain frames won't work.
 * @type {Array<Document>}
 */
__gCrWeb['findInPage'].frameDocs = [];

/**
 * Associate array to stash element styles while the element is highlighted.
 * @type {Object<Element,Object<string,string>>}
 */
__gCrWeb['findInPage'].savedElementStyles = {};

/**
 * The style DOM element that we add.
 * @type {Element}
 */
__gCrWeb['findInPage'].style = null;

/**
 * Width we expect the page to be.  For example (320/480) for iphone,
 * (1024/768) for ipad.
 * @type {number}
 */
__gCrWeb['findInPage'].pageWidth = 320;

/**
 * Height we expect the page to be.
 * @type {number}
 */
__gCrWeb['findInPage'].pageHeight = 480;

/**
 * Maximum number of visible elements to count
 * @type {number}
 */
__gCrWeb['findInPage'].maxVisibleElements = 100;

/**
 * A search is in progress.
 * @type {boolean}
 */
__gCrWeb['findInPage'].searchInProgress = false;

/**
 * Node names that are not going to be processed.
 * @type {Object}
 */
__gCrWeb['findInPage'].ignoreNodeNames = {
 'SCRIPT': 1,
 'STYLE': 1,
 'EMBED': 1,
 'OBJECT': 1
};

/**
 * Class name of CSS element.
 * @type {string}
 */
__gCrWeb['findInPage'].CSS_CLASS_NAME = 'find_in_page';

/**
 * ID of CSS style.
 * @type {string}
 */
__gCrWeb['findInPage'].CSS_STYLE_ID = '__gCrWeb.findInPageStyle';

/**
 * Result passed back to app to indicate no results for the query.
 * @type {string}
 */
__gCrWeb['findInPage'].NO_RESULTS = '[0,[0,0,0]]';

/**
 * Regex to escape regex special characters in a string.
 * @type {RegExp}
 */
__gCrWeb['findInPage'].REGEX_ESCAPER = /([.?*+^$[\]\\(){}|-])/g;

__gCrWeb['findInPage'].getCurrentSpan = function() {
  return __gCrWeb['findInPage']['spans'][__gCrWeb['findInPage']['index']];
};

/**
 * Creates the regex needed to find the text.
 * @param {string} findText Phrase to look for.
 * @param {boolean} opt_split True to split up the phrase.
 * @return {RegExp} regex needed to find the text.
 */
__gCrWeb['findInPage'].getRegex = function(findText, opt_split) {
  var regexString = '';
  if (opt_split) {
    var words = [];
    var split = findText.split(' ');
    for (var i = 0; i < split.length; i++) {
      words.push(__gCrWeb['findInPage'].escapeRegex(split[i]));
    }
    var joinedWords = words.join('|');
    regexString = '(' +
        // Match at least one word.
        '\\b(?:' + joinedWords + ')' +
        // Include zero or more additional words separated by whitespace.
        '(?:\\s*\\b(?:' + joinedWords + '))*' +
        ')';
  } else {
    regexString = '(' + __gCrWeb['findInPage'].escapeRegex(findText) + ')';
  }
  return new RegExp(regexString, 'ig');
};

/**
 * Get current timestamp.
 * @return {number} timestamp.
 */
__gCrWeb['findInPage'].time = function() {
  return (new Date).getTime();
};

/**
 * After |timeCheck| iterations, return true if |now| - |start| is greater than
 * |timeout|.
 * @return {boolean} Find in page needs to return.
 */
__gCrWeb['findInPage'].overTime = function() {
  return (__gCrWeb['findInPage'].time() - __gCrWeb['findInPage'].startTime >
          __gCrWeb['findInPage'].timeout);
};

/**
 * Looks for a phrase in the DOM.
 * @param {string} findText Phrase to look for like "ben franklin".
 * @param {boolean} opt_split True to split up the words and look for any
 *     of them.  False to require the full phrase to be there.
 *     Undefined will try the full phrase, and if nothing is found do the split.
 * @param {number} timeout Maximum time to run.
 * @return {string} How many results there are in the page in the form of
       [highlightedWordsCount, [index, pageLocationX, pageLocationY]].
 */
__gCrWeb['findInPage']['highlightWord'] =
    function(findText, opt_split, timeout) {
  if (__gCrWeb['findInPage']['spans'] &&
      __gCrWeb['findInPage']['spans'].length) {
    // Clean up a previous run.
    __gCrWeb['findInPage']['clearHighlight']();
  }
  if (!findText) {
    // No searching for emptyness.
    return __gCrWeb['findInPage'].NO_RESULTS;
  }

  // Store all DOM modifications to do them in a tight loop at once.
  __gCrWeb['findInPage'].replacements = [];

  // Node is what we are currently looking at.
  __gCrWeb['findInPage'].node = document.body;

  // Holds what nodes we have not processed yet.
  __gCrWeb['findInPage'].stack = [];

  // Push frames into stack too.
  for (var i = __gCrWeb['findInPage'].frameDocs.length - 1; i >= 0; i--) {
    var doc = __gCrWeb['findInPage'].frameDocs[i];
    __gCrWeb['findInPage'].stack.push(doc);
  }

  // Number of visible elements found.
  __gCrWeb['findInPage'].visibleFound = 0;

  // Index tracking variables so search can be broken up into multiple calls.
  __gCrWeb['findInPage'].visibleIndex = 0;
  __gCrWeb['findInPage'].replacementsIndex = 0;
  __gCrWeb['findInPage'].replacementNewNodesIndex = 0;

  __gCrWeb['findInPage'].regex =
      __gCrWeb['findInPage'].getRegex(findText, opt_split);

  __gCrWeb['findInPage'].searchInProgress = true;

  return __gCrWeb['findInPage']['pumpSearch'](timeout);
};

/**
 * Break up find in page DOM regex, DOM manipulation and visibility check
 * into sections that can be stopped and restarted later.  Because the js runs
 * in the main UI thread, anything over timeout will cause the UI to lock up.
 * @param {number} timeout Only run find in page until timeout.
 * @return {string} string in the form of "[bool, int]", where bool indicates
                    whether the text was found and int idicates text position.
 */
__gCrWeb['findInPage']['pumpSearch'] = function(timeout) {
  var opt_split = false;
  // TODO(justincohen): It would be better if this DCHECKed.
  if (__gCrWeb['findInPage'].searchInProgress == false)
    return __gCrWeb['findInPage'].NO_RESULTS;

  __gCrWeb['findInPage'].timeout = timeout;
  __gCrWeb['findInPage'].startTime = __gCrWeb['findInPage'].time();

  var regex = __gCrWeb['findInPage'].regex;
  // Go through every node in DFS fashion.
  while (__gCrWeb['findInPage'].node) {
    var node = __gCrWeb['findInPage'].node;
    var children = node.childNodes;
    if (children && children.length) {
      // add all (reasonable) children
      for (var i = children.length - 1; i >= 0; --i) {
        var child = children[i];
        if ((child.nodeType == 1 || child.nodeType == 3) &&
            !__gCrWeb['findInPage'].ignoreNodeNames[child.nodeName]) {
          __gCrWeb['findInPage'].stack.push(children[i]);
        }
      }
    }
    if (node.nodeType == 3 && node.parentNode) {
      var strIndex = 0;
      var nodes = [];
      var match;
      while (match = regex.exec(node.textContent)) {
        try {
          var matchText = match[0];

          // If there is content before this match, add it to a new text node.
          if (match.index > 0) {
            var nodeSubstr = node.textContent.substring(strIndex,
                                                        match.index);
            nodes.push(node.ownerDocument.createTextNode(nodeSubstr));
          }

          // Now create our matched element.
          var element = node.ownerDocument.createElement('chrome_find');
          element.setAttribute('class', __gCrWeb['findInPage'].CSS_CLASS_NAME);
          element.innerHTML = __gCrWeb['findInPage'].escapeHTML(matchText);
          nodes.push(element);

          strIndex = match.index + matchText.length;
        } catch (e) {
          // Do nothing.
        }
      }
      if (nodes.length) {
        // Add any text after our matches to a new text node.
        if (strIndex < node.textContent.length) {
          var substr = node.textContent.substring(strIndex,
                                                  node.textContent.length);
          nodes.push(node.ownerDocument.createTextNode(substr));
        }
        __gCrWeb['findInPage'].replacements.push(
            {oldNode: node, newNodes: nodes});
        regex.lastIndex = 0;
      }

    }

    if (__gCrWeb['findInPage'].overTime())
      return '[false]';

    if (__gCrWeb['findInPage'].stack.length > 0) {
      __gCrWeb['findInPage'].node = __gCrWeb['findInPage'].stack.pop();
    } else {
      __gCrWeb['findInPage'].node = null;
    }
  }

  // Insert each of the replacement nodes into the old node's parent, then
  // remove the old node.
  var replacements = __gCrWeb['findInPage'].replacements;

  // Last position in replacements array.
  var rIndex = __gCrWeb['findInPage'].replacementsIndex;
  var rMax = replacements.length;
  for (; rIndex < rMax; rIndex++) {
    var replacement = replacements[rIndex];
    var parent = replacement.oldNode.parentNode;
    if (parent == null)
      continue;
    var rNodesMax = replacement.newNodes.length;
    for (var rNodesIndex = __gCrWeb['findInPage'].replacementNewNodesIndex;
         rNodesIndex < rNodesMax; rNodesIndex++) {
      if (__gCrWeb['findInPage'].overTime()) {
        __gCrWeb['findInPage'].replacementsIndex = rIndex;
        __gCrWeb['findInPage'].replacementNewNodesIndex = rNodesIndex;
        return __gCrWeb.stringify([false]);
      }
      parent.insertBefore(replacement.newNodes[rNodesIndex],
                          replacement.oldNode);
    }
    parent.removeChild(replacement.oldNode);
    __gCrWeb['findInPage'].replacementNewNodesIndex = 0;
  }
  // Save last position in replacements array.
  __gCrWeb['findInPage'].replacementsIndex = rIndex;

  __gCrWeb['findInPage']['spans'] =
      __gCrWeb['findInPage'].getAllElementsByClassName(
          __gCrWeb['findInPage'].CSS_CLASS_NAME);

  // Count visible elements.
  var max = __gCrWeb['findInPage']['spans'].length;
  var maxVisible = __gCrWeb['findInPage'].maxVisibleElements;
  for (var index = __gCrWeb['findInPage'].visibleIndex; index < max; index++) {
    var elem = __gCrWeb['findInPage']['spans'][index];
    if (__gCrWeb['findInPage'].overTime()) {
      __gCrWeb['findInPage'].visibleIndex = index;
      return __gCrWeb.stringify([false]);
    }

    // Stop after |maxVisible| elements.
    if (__gCrWeb['findInPage'].visibleFound > maxVisible) {
      __gCrWeb['findInPage']['spans'][index].visibleIndex = maxVisible;
      continue;
    }

    if (__gCrWeb['findInPage'].isVisible(elem)) {
      __gCrWeb['findInPage'].visibleFound++;
      __gCrWeb['findInPage']['spans'][index].visibleIndex =
          __gCrWeb['findInPage'].visibleFound;
    }
  }

  __gCrWeb['findInPage'].searchInProgress = false;

  var pos;
  // Try again flow.
  // If opt_split is true, we are done since we won't do any better.
  // If opt_split is false, they were explicit about wanting the full thing
  // so do not try with a split.
  // If opt_split is undefined and we did not find an answer, go ahead and try
  // splitting the terms.
  if (__gCrWeb['findInPage']['spans'].length == 0 && opt_split === undefined) {
    // Try to be more aggressive:
    return __gCrWeb['findInPage']['highlightWord'](findText, true);
  } else {
    pos = __gCrWeb['findInPage']['goNext']();
    if (pos) {
      return '[' + __gCrWeb['findInPage'].visibleFound + ',' + pos + ']';
    } else if (opt_split === undefined) {
      // Nothing visible, go ahead and be more aggressive.
      return __gCrWeb['findInPage']['highlightWord'](findText, true);
    } else {
      return __gCrWeb['findInPage'].NO_RESULTS;
    }
  }
};

/**
 * Converts a node list to an array.
 * @param {NodeList} nodeList DOM node list.
 * @return {Array<Node>} array.
 */
__gCrWeb['findInPage'].toArray = function(nodeList) {
  var array = [];
  for (var i = 0; i < nodeList.length; i++)
    array[i] = nodeList[i];
  return array;
};

/**
 * Return all elements of class name, spread out over various frames.
 * @param {string} name of class.
 * @return {Array<Node>} array of elements matching class name.
 */
__gCrWeb['findInPage'].getAllElementsByClassName = function(name) {
  var nodeList = document.getElementsByClassName(name);
  var elements = __gCrWeb['findInPage'].toArray(nodeList);
  for (var i = __gCrWeb['findInPage'].frameDocs.length - 1; i >= 0; i--) {
    var doc = __gCrWeb['findInPage'].frameDocs[i];
    nodeList = doc.getElementsByClassName(name);
    elements = elements.concat(__gCrWeb['findInPage'].toArray(nodeList));
  }
  return elements;
};

/**
 * Removes all currently highlighted spans.
 * Note: It does not restore previous state, just removes the class name.
 */
__gCrWeb['findInPage']['clearHighlight'] = function() {
  if (__gCrWeb['findInPage']['index'] >= 0) {
    __gCrWeb['findInPage'].removeSelectHighlight(
        __gCrWeb['findInPage'].getCurrentSpan());
  }
  // Store all DOM modifications to do them in a tight loop.
  var modifications = [];
  var length = __gCrWeb['findInPage']['spans'].length;
  var prevParent = null;
  for (var i = length - 1; i >= 0; i--) {
    var elem = __gCrWeb['findInPage']['spans'][i];
    var parentNode = elem.parentNode;
    // Safari has an occasional |elem.innerText| bug that drops the trailing
    // space.  |elem.innerText| would be more correct in this situation, but
    // since we only allow text in this element, grabbing the HTML value should
    // not matter.
    var nodeText = elem.innerHTML;
    // If this element has the same parent as the previous, check if we should
    // add this node to the previous one.
    if (prevParent && prevParent.isSameNode(parentNode) &&
        elem.nextSibling.isSameNode(
            __gCrWeb['findInPage']['spans'][i + 1].previousSibling)) {
      var prevMod = modifications[modifications.length - 1];
      prevMod.nodesToRemove.push(elem);
      var elemText = elem.innerText;
      if (elem.previousSibling) {
        prevMod.nodesToRemove.push(elem.previousSibling);
        elemText = elem.previousSibling.textContent + elemText;
      }
      prevMod.replacement.textContent =
          elemText + prevMod.replacement.textContent;
    }
    else { // Element isn't attached to previous, so create a new modification.
      var nodesToRemove = [elem];
      if (elem.previousSibling && elem.previousSibling.nodeType == 3) {
        nodesToRemove.push(elem.previousSibling);
        nodeText = elem.previousSibling.textContent + nodeText;
      }
      if (elem.nextSibling && elem.nextSibling.nodeType == 3) {
        nodesToRemove.push(elem.nextSibling);
        nodeText = nodeText + elem.nextSibling.textContent;
      }
      var textNode = elem.ownerDocument.createTextNode(nodeText);
      modifications.push({nodesToRemove: nodesToRemove, replacement: textNode});
    }
    prevParent = parentNode;
  }
  var numMods = modifications.length;
  for (i = numMods - 1; i >= 0; i--) {
    var mod = modifications[i];
    for (var j = 0; j < mod.nodesToRemove.length; j++) {
      var existing = mod.nodesToRemove[j];
      if (j == 0) {
        existing.parentNode.replaceChild(mod.replacement, existing);
      } else {
        existing.parentNode.removeChild(existing);
      }
    }
  }

  __gCrWeb['findInPage']['spans'] = [];
  __gCrWeb['findInPage']['index'] = -1;
};

/**
 * Increments the index of the current highlighted span or, if the index is
 * already at the end, sets it to the index of the first span in the page.
 */
__gCrWeb['findInPage']['incrementIndex'] = function() {
  if (__gCrWeb['findInPage']['index'] >=
      __gCrWeb['findInPage']['spans'].length - 1) {
    __gCrWeb['findInPage']['index'] = 0;
  } else {
    __gCrWeb['findInPage']['index']++;
  }
};

/**
 * Switches to the next result, animating a little highlight in the process.
 * @return {string} JSON encoded array of coordinates to scroll to, or blank if
 *     nothing happened.
 */
__gCrWeb['findInPage']['goNext'] = function() {
  if (!__gCrWeb['findInPage']['spans'] ||
      __gCrWeb['findInPage']['spans'].length == 0) {
    return '';
  }
  if (__gCrWeb['findInPage']['index'] >= 0) {
    // Remove previous highlight.
    __gCrWeb['findInPage'].removeSelectHighlight(
        __gCrWeb['findInPage'].getCurrentSpan());
  }
  // Iterate through to the next index, but because they might not be visible,
  // keep trying until you find one that is.  Make sure we don't loop forever by
  // stopping on what we are currently highlighting.
  var oldIndex = __gCrWeb['findInPage']['index'];
  __gCrWeb['findInPage']['incrementIndex']();
  while (!__gCrWeb['findInPage'].isVisible(
              __gCrWeb['findInPage'].getCurrentSpan())) {
    if (oldIndex === __gCrWeb['findInPage']['index']) {
      // Checked all spans but didn't find anything else visible.
      return '';
    }
    __gCrWeb['findInPage']['incrementIndex']();
    if (0 === __gCrWeb['findInPage']['index'] && oldIndex < 0) {
      // Didn't find anything visible and haven't highlighted anything yet.
      return '';
    }
  }
  // Return scroll dimensions.
  return __gCrWeb['findInPage'].findScrollDimensions();
};

/**
 * Decrements the index of the current highlighted span or, if the index is
 * already at the beginning, sets it to the index of the last span in the page.
 */
__gCrWeb['findInPage']['decrementIndex'] = function() {
  if (__gCrWeb['findInPage']['index'] <= 0) {
    __gCrWeb['findInPage']['index'] =
        __gCrWeb['findInPage']['spans'].length - 1;
  } else {
    __gCrWeb['findInPage']['index']--;
  }
};

/**
 * Switches to the previous result, animating a little highlight in the process.
 * @return {string} JSON encoded array of coordinates to scroll to, or blank if
 *     nothing happened.
 */
__gCrWeb['findInPage']['goPrev'] = function() {
  if (!__gCrWeb['findInPage']['spans'] ||
      __gCrWeb['findInPage']['spans'].length == 0) {
    return '';
  }
  if (__gCrWeb['findInPage']['index'] >= 0) {
    // Remove previous highlight.
    __gCrWeb['findInPage'].removeSelectHighlight(
        __gCrWeb['findInPage'].getCurrentSpan());
  }
  // Iterate through to the next index, but because they might not be visible,
  // keep trying until you find one that is.  Make sure we don't loop forever by
  // stopping on what we are currently highlighting.
  var old = __gCrWeb['findInPage']['index'];
  __gCrWeb['findInPage']['decrementIndex']();
  while (!__gCrWeb['findInPage'].isVisible(
             __gCrWeb['findInPage'].getCurrentSpan())) {
    __gCrWeb['findInPage']['decrementIndex']();
    if (old == __gCrWeb['findInPage']['index']) {
      // Checked all spans but didn't find anything.
      return '';
    }
  }

  // Return scroll dimensions.
  return __gCrWeb['findInPage'].findScrollDimensions();
};

/**
 * Adds the special highlighting to the result at the index.
 * @param {number=} opt_index Index to replace __gCrWeb['findInPage']['index']
 *                  with.
 */
__gCrWeb['findInPage'].addHighlightToIndex = function(opt_index) {
  if (opt_index !== undefined) {
    __gCrWeb['findInPage']['index'] = opt_index;
  }
  __gCrWeb['findInPage'].addSelectHighlight(
      __gCrWeb['findInPage'].getCurrentSpan());
};

/**
 * Updates the elements style, while saving the old style in
 * __gCrWeb['findInPage'].savedElementStyles.
 * @param {Element} element Element to update.
 * @param {string} style Name of the style to update.
 * @param {string} value New style value.
 */
__gCrWeb['findInPage'].updateElementStyle = function(element, style, value) {
  if (!__gCrWeb['findInPage'].savedElementStyles[element]) {
    __gCrWeb['findInPage'].savedElementStyles[element] = {};
  }
  // We need to keep the original style setting for this element, so if we've
  // already saved a value for this style don't update it.
  if (!__gCrWeb['findInPage'].savedElementStyles[element][style]) {
    __gCrWeb['findInPage'].savedElementStyles[element][style] =
        element.style[style];
  }
  element.style[style] = value;
};

/**
 * Adds selected highlight style to the specified element.
 * @param {Element} element Element to highlight.
 */
__gCrWeb['findInPage'].addSelectHighlight = function(element) {
  element.className = (element.className || '') + ' findysel';
};

/**
 * Removes selected highlight style from the specified element.
 * @param {Element} element Element to remove highlighting from.
 */
__gCrWeb['findInPage'].removeSelectHighlight = function(element) {
  element.className = (element.className || '').replace(/\sfindysel/g, '');

  // Restore any styles we may have saved when adding the select highlighting.
  var savedStyles = __gCrWeb['findInPage'].savedElementStyles[element];
  if (savedStyles) {
    for (var style in savedStyles) {
      element.style[style] = savedStyles[style];
    }
    delete __gCrWeb['findInPage'].savedElementStyles[element];
  }
};

/**
 * Normalize coordinates according to the current document dimensions. Don't go
 * too far off the screen in either direction. Try to center if possible.
 * @param {Element} elem Element to find normalized coordinates for.
 * @return {Array<number>} Normalized coordinates.
 */
__gCrWeb['findInPage'].getNormalizedCoordinates = function(elem) {
  var fip = __gCrWeb['findInPage'];
  var pos = fip.findAbsolutePosition(elem);
  var maxX =
      Math.max(fip.getBodyWidth(), pos[0] + elem.offsetWidth);
  var maxY =
      Math.max(fip.getBodyHeight(), pos[1] + elem.offsetHeight);
  // Don't go too far off the screen in either direction.  Try to center if
  // possible.
  var xPos = Math.max(0,
      Math.min(maxX - window.innerWidth,
               pos[0] - (window.innerWidth / 2)));
  var yPos = Math.max(0,
      Math.min(maxY - window.innerHeight,
               pos[1] - (window.innerHeight / 2)));
  return [xPos, yPos];
};

/**
 * Scale coordinates according to the width of the screen, in case the screen
 * is zoomed out.
 * @param {Array<number>} coordinates Coordinates to scale.
 * @return {Array<number>} Scaled coordinates.
 */
__gCrWeb['findInPage'].scaleCoordinates = function(coordinates) {
  var scaleFactor = __gCrWeb['findInPage'].pageWidth / window.innerWidth;
  return [coordinates[0] * scaleFactor, coordinates[1] * scaleFactor];
};

/**
 * Finds the position of the result and scrolls to it.
 * @param {number=} opt_index Index to replace __gCrWeb['findInPage']['index']
 *                  with.
 * @return {string} JSON encoded array of the scroll coordinates "[x, y]".
 */
__gCrWeb['findInPage'].findScrollDimensions = function(opt_index) {
  if (opt_index !== undefined) {
    __gCrWeb['findInPage']['index'] = opt_index;
  }
  var elem = __gCrWeb['findInPage'].getCurrentSpan();
  if (!elem) {
    return '';
  }
  var normalized = __gCrWeb['findInPage'].getNormalizedCoordinates(elem);
  var xPos = normalized[0];
  var yPos = normalized[1];

  // Perform the scroll.
  //window.scrollTo(xPos, yPos);

  if (xPos < window.pageXOffset ||
      xPos >= (window.pageXOffset + window.innerWidth) ||
      yPos < window.pageYOffset ||
      yPos >= (window.pageYOffset + window.innerHeight)) {
    // If it's off the screen.  Wait a bit to start the highlight animation so
    // that scrolling can get there first.
    window.setTimeout(__gCrWeb['findInPage'].addHighlightToIndex, 250);
  } else {
    __gCrWeb['findInPage'].addHighlightToIndex();
  }
  var scaled = __gCrWeb['findInPage'].scaleCoordinates(normalized);
  var index = __gCrWeb['findInPage'].getCurrentSpan().visibleIndex;
  scaled.unshift(index);
  return __gCrWeb.stringify(scaled);
};

/**
 * Initialize the __gCrWeb['findInPage'] module.
 * @param {number} width Width of page.
 * @param {number} height Height of page.

 */
__gCrWeb['findInPage']['init'] = function(width, height) {
  if (__gCrWeb['findInPage'].hasInitialized) {
    return;
  }
  __gCrWeb['findInPage'].pageWidth = width;
  __gCrWeb['findInPage'].pageHeight = height;
  __gCrWeb['findInPage'].frameDocs = __gCrWeb['findInPage'].frameDocuments();
  __gCrWeb['findInPage'].enable();
  __gCrWeb['findInPage'].hasInitialized = true;
};

/**
 * When the GSA app detects a zoom change, we need to update our css.
 * @param {number} width Width of page.
 * @param {number} height Height of page.
 */
__gCrWeb['findInPage']['fixZoom'] = function(width, height) {
  __gCrWeb['findInPage'].pageWidth = width;
  __gCrWeb['findInPage'].pageHeight = height;
  if (__gCrWeb['findInPage'].style) {
    __gCrWeb['findInPage'].removeStyle();
    __gCrWeb['findInPage'].addStyle();
  }
};

/**
 * Enable the __gCrWeb['findInPage'] module.
 * Mainly just adds the style for the classes.
 */
__gCrWeb['findInPage'].enable = function() {
  if (__gCrWeb['findInPage'].style) {
    // Already enabled.
    return;
  }
  __gCrWeb['findInPage'].addStyle();
};

/**
 * Gets the scale ratio between the application window and the web document.
 * @return {number} Scale.
 */
__gCrWeb['findInPage'].getPageScale = function() {
  return (__gCrWeb['findInPage'].pageWidth /
      __gCrWeb['findInPage'].getBodyWidth());
};

/**
 * Maximum padding added to a highlighted item when selected.
 * @type {number}
 */
__gCrWeb['findInPage'].MAX_HIGHLIGHT_PADDING = 10;

/**
 * Adds the appropriate style element to the page.
 */
__gCrWeb['findInPage'].addStyle = function() {
  __gCrWeb['findInPage'].addDocumentStyle(document);
  for (var i = __gCrWeb['findInPage'].frameDocs.length - 1; i >= 0; i--) {
    var doc = __gCrWeb['findInPage'].frameDocs[i];
    __gCrWeb['findInPage'].addDocumentStyle(doc);
  }
};

__gCrWeb['findInPage'].addDocumentStyle = function(thisDocument) {
  var styleContent = [];
  function addCSSRule(name, style) {
    styleContent.push(name, '{', style, '}');
  };
  var scale = __gCrWeb['findInPage'].getPageScale();
  var zoom = (1.0 / scale);
  var left = ((1 - scale) / 2 * 100);
  addCSSRule('.' + __gCrWeb['findInPage'].CSS_CLASS_NAME,
             'background-color:#ffff00 !important;' +
             'padding:0px;margin:0px;' +
             'overflow:visible !important;');
  addCSSRule('.findysel',
             'background-color:#ff9632 !important;' +
             'padding:0px;margin:0px;' +
             'overflow:visible !important;');
  __gCrWeb['findInPage'].style = thisDocument.createElement('style');
  __gCrWeb['findInPage'].style.id = __gCrWeb['findInPage'].CSS_STYLE_ID;
  __gCrWeb['findInPage'].style.setAttribute('type', 'text/css');
  __gCrWeb['findInPage'].style.appendChild(
      thisDocument.createTextNode(styleContent.join('')));
  thisDocument.body.appendChild(__gCrWeb['findInPage'].style);
};

/**
 * Removes the style element from the page.
 */
__gCrWeb['findInPage'].removeStyle = function() {
  if (__gCrWeb['findInPage'].style) {
    __gCrWeb['findInPage'].removeDocumentStyle(document);
    for (var i = __gCrWeb['findInPage'].frameDocs.length - 1; i >= 0; i--) {
      var doc = __gCrWeb['findInPage'].frameDocs[i];
      __gCrWeb['findInPage'].removeDocumentStyle(doc);
    }
    __gCrWeb['findInPage'].style = null;
  }
};

__gCrWeb['findInPage'].removeDocumentStyle = function(thisDocument) {
  var style = thisDocument.getElementById(__gCrWeb['findInPage'].CSS_STYLE_ID);
  thisDocument.body.removeChild(style);
};

/**
 * Disables the __gCrWeb['findInPage'] module.
 * Basically just removes the style and class names.
 */
__gCrWeb['findInPage']['disable'] = function() {
  if (__gCrWeb['findInPage'].style) {
    __gCrWeb['findInPage'].removeStyle();
    window.setTimeout(__gCrWeb['findInPage']['clearHighlight'], 0);
  }
  __gCrWeb['findInPage'].hasInitialized = false;
};

/**
* Returns the width of the document.body.  Sometimes though the body lies to
* try to make the page not break rails, so attempt to find those as well.
* An example: wikipedia pages for the ipad.
* @return {number} Width of the document body.
*/
__gCrWeb['findInPage'].getBodyWidth = function() {
  var body = document.body;
  var documentElement = document.documentElement;
  return Math.max(body.scrollWidth, documentElement.scrollWidth,
                  body.offsetWidth, documentElement.offsetWidth,
                  body.clientWidth, documentElement.clientWidth);
};

/**
 * Returns the height of the document.body.  Sometimes though the body lies to
 * try to make the page not break rails, so attempt to find those as well.
 * An example: wikipedia pages for the ipad.
 * @return {number} Height of the document body.
 */
__gCrWeb['findInPage'].getBodyHeight = function() {
  var body = document.body;
  var documentElement = document.documentElement;
  return Math.max(body.scrollHeight, documentElement.scrollHeight,
                  body.offsetHeight, documentElement.offsetHeight,
                  body.clientHeight, documentElement.clientHeight);
};

/**
 * Helper function that determines if an element is visible.
 * @param {Element} elem Element to check.
 * @return {boolean} Whether elem is visible or not.
 */
__gCrWeb['findInPage'].isVisible = function(elem) {
  if (!elem) {
    return false;
  }
  var top = 0;
  var left = 0;
  var bottom = Infinity;
  var right = Infinity;

  var originalElement = elem;
  var nextOffsetParent = originalElement.offsetParent;
  var computedStyle =
      elem.ownerDocument.defaultView.getComputedStyle(elem, null);

  // We are currently handling all scrolling through the app, which means we can
  // only scroll the window, not any scrollable containers in the DOM itself. So
  // for now this function returns false if the element is scrolled outside the
  // viewable area of its ancestors.
  // TODO(justincohen): handle scrolling within the DOM.
  var pageHeight = __gCrWeb['findInPage'].getBodyHeight();
  var pageWidth = __gCrWeb['findInPage'].getBodyWidth();

  while (elem && elem.nodeName.toUpperCase() != 'BODY') {
    if (elem.style.display === 'none' ||
        elem.style.visibility === 'hidden' ||
        elem.style.opacity === 0 ||
        computedStyle.display === 'none' ||
        computedStyle.visibility === 'hidden' ||
        computedStyle.opacity === 0) {
      return false;
    }

    // For the original element and all ancestor offsetParents, trim down the
    // visible area of the original element.
    if (elem.isSameNode(originalElement) || elem.isSameNode(nextOffsetParent)) {
      var visible = elem.getBoundingClientRect();
      if (elem.style.overflow === 'hidden' &&
          (visible.width === 0 || visible.height === 0))
        return false;

      top = Math.max(top, visible.top + window.pageYOffset);
      bottom = Math.min(bottom, visible.bottom + window.pageYOffset);
      left = Math.max(left, visible.left + window.pageXOffset);
      right = Math.min(right, visible.right + window.pageXOffset);

      // The element is not within the original viewport.
      var notWithinViewport = top < 0 || left < 0;

      // The element is flowing off the boundary of the page. Note this is
      // not comparing to the size of the window, but the calculated offset
      // size of the document body. This can happen if the element is within
      // a scrollable container in the page.
      var offPage = right > pageWidth || bottom > pageHeight;
      if (notWithinViewport || offPage) {
        return false;
      }
      nextOffsetParent = elem.offsetParent;
    }

    elem = elem.parentNode;
    computedStyle = elem.ownerDocument.defaultView.getComputedStyle(elem, null);
  }
  return true;
};

/**
 * Helper function to find the absolute position of an element on the page.
 * @param {Element} elem Element to check.
 * @return {Array<number>} [x, y] positions.
 */
__gCrWeb['findInPage'].findAbsolutePosition = function(elem) {
  var boundingRect = elem.getBoundingClientRect();
  return [boundingRect.left + window.pageXOffset,
      boundingRect.top + window.pageYOffset];
};

/**
 * @param {string} text Text to escape.
 * @return {string} escaped text.
 */
__gCrWeb['findInPage'].escapeHTML = function(text) {
  var unusedDiv = document.createElement('div');
  unusedDiv.innerText = text;
  return unusedDiv.innerHTML;
};

/**
 * Escapes regexp special characters.
 * @param {string} text Text to escape.
 * @return {string} escaped text.
 */
__gCrWeb['findInPage'].escapeRegex = function(text) {
  return text.replace(__gCrWeb['findInPage'].REGEX_ESCAPER, '\\$1');
};

/**
 * Gather all iframes in the main window.
 * @return {Array<Document>} frames.
 */
__gCrWeb['findInPage'].frameDocuments = function() {
  var windowsToSearch = [window];
  var documents = [];
  while (windowsToSearch.length != 0) {
    var win = windowsToSearch.pop();
    for (var i = win.frames.length - 1; i >= 0; i--) {
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

window.addEventListener('pagehide', __gCrWeb['findInPage']['disable']);
