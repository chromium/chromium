// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface used to extract the page text, up to a certain limit,
 * and pass it on to the annotations manager.
 */

__gCrWeb['annotations'] = {};

// Local debug stub.
// TODO(crbug.com/1350973): remove on full launch.
function log(value) {
  __gCrWeb.common.sendWebKitMessage('annotations', {
    command: 'annotations.log',
    text: __gCrWeb.stringify(value),
  });
}

// Used by the `enumerateTextNodes` function below.
const NON_TEXT_NODE_NAMES = new Set(
    ['SCRIPT', 'NOSCRIPT', 'STYLE', 'EMBED', 'OBJECT', 'TEXTAREA', 'IFRAME']);

/**
 * Extracts first `maxChars` text characters from the page. Once done it
 * send a 'annotations.extractedText' command with the 'text'.
 * @param {double} maxChars - maximum number of characters to parse out
 */
__gCrWeb.annotations.extractText = function(maxChars) {
  __gCrWeb.common.sendWebKitMessage('annotations', {
    command: 'annotations.extractedText',
    text: getPageText(maxChars),
  });
};

/**
 * Array of decorations objects, the original Node and the replacements nodes.
 * Each object contains the following Key/Values:
 *  {Node} original - the original Node that was replaced.
 *  {Node[]} replacements - the Nodes the replaced the original.
 */
let decorations;

/**
 * Decorate the page with the given `annotations`. Each annotation has the
 * following key/values:
 *  {int} start - character index to start of annotation.
 *  {int} end - character index to end of annotation.
 *  {string} text - the annotation text, used to ensure the text in the page
 *    hasn't changed.
 *  {string} style - style to apply to the <chrome_annotation> element(s)
 *    enclosing the annotation.
 *    TODO(crbug.com/1350973): refactor style as keys in a local style dict.
 * Annotations will be sorted and one will be dropped when two overlaps.
 * @param {object[]} annotations - list of annotations
 * TODO(crbug.com/1350973): refactor in smaller pieces.
 */
__gCrWeb.annotations.decorateAnnotations = function(annotations) {
  // Avoid redoing without going through `removeDecorations` first.
  if (decorations?.length || !annotations?.length)
    return;

  let failures = 0;
  decorations = [];

  // Sort the annotations.
  annotations.sort((a, b) => {
    return a.start - b.start;
  });

  // Remove overlaps (lower indexed annotation has priority).
  let previous = null;
  annotations.filter((element) => {
    if (previous && previous.start < element.end &&
        previous.end > element.start) {
      return false;
    }
    previous = element;
    return true;
  });

  // Reparse page finding annotations and styling them.
  let annotationIndex = 0;
  enumerateSectionsNodes(document.body, function(node, index, text) {
    if (!node.parentNode)
      return true;

    // Skip annotation with end before index. This would happen if some nodes
    // are deleted between text fetching and decorating.
    while (annotationIndex < annotations.length &&
           annotations[annotationIndex].end <= index) {
      log({
        reason: 'skipping',
        annotationText: annotations[annotationIndex].text,
      });
      failures++;
      annotationIndex++;
    }

    let length = text.length;
    let replacements = [];
    while (annotationIndex < annotations.length) {
      let annotation = annotations[annotationIndex];
      let start = annotation.start, end = annotation.end;
      if (index < end && index + length > start) {
        // Inside node
        let left = Math.max(0, start - index),
            right = Math.min(length, end - index);
        let nodeText = text.substring(left, right);
        let annotationLeft = Math.max(0, index - start),
            annotationRight = Math.min(end - start, index + length - start);
        let annotationText =
            annotation.text.substring(annotationLeft, annotationRight);
        // Text has changed, forget the rest of this annotation.
        if (nodeText != annotationText) {
          log({
            reason: 'mismatch',
            nodeText: nodeText,
            annotationText: annotationText,
          });
          failures++;
          annotationIndex++;
          continue;
        }
        replacements.push({
          left: left,
          right: right,
          text: nodeText,
          style: annotation.style,
          data: annotation.data,
        });
        // If annotation is completed, move to next annotation and retry on
        // this node to fit more annotations if needed.
        if (end <= index + length) {
          annotationIndex++;
          continue;
        }
      }
      break;
    }

    // If the hit on a link, do not stylize. The check doesn't happen before
    // the annotation loop above, to keep the running cursor's (annotationIndex)
    // integrity.
    for (let i = node.parentNode; i != null; i = i.parentNode) {
      if (i.tagName == 'A') {
        replacements = [];
        break;
      }
    }

    // Process replacement list.
    if (replacements.length > 0) {
      let parentNode = node.parentNode;
      let cursor = 0;
      let parts = [];
      for (let replacement of replacements) {
        if (replacement.left > cursor) {
          parts.push(document.createTextNode(
              text.substring(cursor, replacement.left)));
        }
        let element = document.createElement('chrome_annotation');
        element.setAttribute('data-data', replacement.data);
        element.innerText = replacement.text;
        element.style.cssText = replacement.style;
        element.addEventListener('click', handleClick.bind(element), true);
        parts.push(element);
        cursor = replacement.right;
      }
      if (cursor < length) {
        parts.push(document.createTextNode(text.substring(cursor, length)));
      }

      for (let part of parts) {
        parentNode.insertBefore(part, node);
      }
      parentNode.removeChild(node);

      // Keep track to be able to undo in `removeDecorations`.
      decorations.push({original: node, replacements: parts});
    }

    return annotationIndex < annotations.length;
  });

  // Any remaining annotations left untouched are failures.
  failures += annotations.length - annotationIndex;

  __gCrWeb.common.sendWebKitMessage('annotations', {
    command: 'annotations.decoratingComplete',
    successes: annotations.length - failures,
    annotations: annotations.length
  });
};

/**
 * Remove current decorations.
 */
__gCrWeb.annotations.removeDecorations = function() {
  for (let decoration of decorations) {
    let replacements = decoration.replacements;
    let parentNode = replacements[0].parentNode;
    if (!parentNode)
      return;
    parentNode.insertBefore(decoration.original, replacements[0]);
    for (let replacement of replacements) {
      parentNode.removeChild(replacement);
    }
  }
  decorations = [];
};

/**
 * Traverse the DOM tree starting at `root` and call process on each text
 * node.
 * @param {Node} root - root node where to start traversal.
 * @param {function} process - callback for each text node. Takes 3
 *   parameters:
 *    {Node} node - Node or Element containing text or newline.
 *    {int} index - index into the stream or characters so far.
 *    {string} text - the text for this node.
 * @param {bool} includeShadowDOM - when true, shadow DOM is also traversed.
 * TODO(crbug.com/1350973): check with language detection if we can turn on
 * includeShadowDOM for all.
 */
const enumerateTextNodes = function(root, process, includeShadowDOM = true) {
  let nodes = [root];
  let index = 0;

  while (nodes.length > 0) {
    let node = nodes.pop();
    if (!node) {
      break;
    }

    // Formatting and filtering.
    if (node.nodeType === Node.ELEMENT_NODE) {
      // Reject non-text nodes such as scripts.
      if (NON_TEXT_NODE_NAMES.has(node.nodeName)) {
        continue;
      }
      if (node.nodeName === 'BR') {
        if (!process(node, index, '\n'))
          break;
        index += 1;
        continue;
      }
      const style = window.getComputedStyle(node);
      // Only proceed if the element is visible.
      if (style.display === 'none' || style.visibility === 'hidden') {
        continue;
      }
      // No need to add a line break before `body` as it is the first element.
      if (node.nodeName.toUpperCase() !== 'BODY' &&
          style.display !== 'inline') {
        if (!process(node, index, '\n'))
          break;
        index += 1;
      }
    }

    if (includeShadowDOM && node.shadowRoot && node.shadowRoot != node) {
      nodes.push(node.shadowRoot);
    } else if (node.hasChildNodes()) {
      for (let childIdx = node.childNodes.length - 1; childIdx >= 0;
           childIdx--) {
        nodes.push(node.childNodes[childIdx]);
      }
    } else if (node.nodeType === Node.TEXT_NODE && node.textContent) {
      if (!process(node, index, node.textContent))
        break;
      index += node.textContent.length;
    }
  }
};

/**
 * Sections (like find in page) are used to be able to find text even if
 * there are DOM changes between extraction and decoration. Using WeakRef
 * around nodes also avoids holding on to deleted nodes.
 * TODO(crbug.com/1350973): WeakRef starts in 14.5, remove checks once 14 is
 *   deprecated. This also means that < 14.5 sectionsNodes is never releasing
 *   nodes, even if they are released from the DOM.
 */
let sectionNodes;
let sectionIndex;

/**
 * Alternative to `enumerateTextNodes` using sections.
 */
const enumerateSectionsNodes =
    function(root, process) {
  for (let i = 0; i < sectionNodes.length; i++) {
    let node = WeakRef ? sectionNodes[i].deref() : sectionNodes[i];
    if (!node)
      continue;

    let index = sectionIndex[i];
    let text = node.nodeType === Node.ELEMENT_NODE ? '\n' : node.textContent;
    if (!process(node, index, text))
      break;
  }
}

/**
 * Returns first `maxChars` text characters from the page.
 * @param {double} maxChars - maximum number of characters to parse out
 */
const getPageText = function(maxChars) {
  let parts = [];
  sectionNodes = [];
  sectionIndex = [];
  enumerateTextNodes(document.body, function(node, index, text) {
    sectionNodes.push(WeakRef ? new WeakRef(node) : node);
    sectionIndex.push(index);
    if (index + text.length > maxChars) {
      parts.push(text.substring(0, maxChars - index));
    } else {
      parts.push(text);
    }
    return index + text.length < maxChars;
  });
  return ''.concat(...parts);
};

const handleClick = function(event) {
  // TODO(crbug.com/1350973): select text, it might involve previous and
  //  next nodes. Can we do this by using querySelector and unique annotation
  //  ids?
  const annotation = event.currentTarget;
  __gCrWeb.common.sendWebKitMessage('annotations', {
    command: 'annotations.onClick',
    data: annotation.dataset.data,
    rect: rectFromElement(annotation),
    text: `"${annotation.innerText}"`,
  });
};

const rectFromElement = function(elt) {
  const domRect = elt.getClientRects()[0];
  return {
    x: domRect.x,
    y: domRect.y,
    width: domRect.width,
    height: domRect.height
  };
};
