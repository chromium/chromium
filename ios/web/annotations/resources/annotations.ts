// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface used to extract the page text, up to a certain limit,
 * and pass it on to the annotations manager.
 */

import {MS_DELAY_BEFORE_TRIGGER, NO_DECORATION_NODE_NAMES, NON_TEXT_NODE_NAMES} from '//ios/web/annotations/resources/annotations_constants.js';
import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Mark: Private properties

interface Annotation {
  /**  Character index to start of annotation. */
  start: number;
  /** Character index to end of annotation (first character after text) */
  end: number;
  /** The annotation text used to ensure the text in the page hasn't changed. */
  text: string;
  /** Annotation type. */
  type: string;
  /** A passed in string that will be sent back to obj in tap handler. */
  data: string;
}

/**
 * `Replacement` represent a single child node replacement inside a childless
 * text Node. `index` is the source annotation index. `left` and `right` is the
 * range inside `text` for a child Node.
 * All new nodes (HTMLElement) have `data` added to their `dataset`.
 */
class Replacement {
  constructor(
      public index: number, public left: number, public right: number,
      public text: string, public type: string, public annotationText: string,
      public data: string) {}
}

/**
 * A `Decoration` is the `original` Node and the list of `Node`s that make up
 * a similar replacement.
 */
class Decoration {
  constructor(public original: Node, public replacements: Node[]) {}
}

/**
 * Section (like find in page) is used to be able to find text even if
 * there are DOM changes between extraction and decoration. Using WeakRef
 * around nodes also avoids holding on to deleted nodes.
 */
class Section {
  constructor(public node: WeakRef<Node>, public index: number) {}
}

/**
 * Monitors DOM mutations between instance construction until a call to
 * `stopObserving`.
 */
class MutationsDuringClickTracker {
  hasMutations = false;
  mutationObserver: MutationObserver;
  mutationExtendId = 0;

  // Constructs a new instance given an `initialEvent` and starts listening for
  // changes to the DOM.
  constructor(private readonly initialEvent: Event) {
    this.mutationObserver =
        new MutationObserver((mutationList: MutationRecord[]) => {
          for (let mutation of mutationList) {
            if (mutation.target.contains(this.initialEvent.target as Node)) {
              this.hasMutations = true;
              this.stopObserving();
              break;
            }
          }
        });
    this.mutationObserver.observe(
        document, {attributes: false, childList: true, subtree: true});
  }

  // Returns true if event doesn't matches the event passed at construction,
  // or it was prevented or if any DOM mutations occurred.
  hasPreventativeActivity(event: Event): boolean {
    return event !== this.initialEvent || event.defaultPrevented ||
        this.hasMutations;
  }

  // Extends DOM observation by triggering `then` after de delay. This can be
  // called multiple times if needed.
  extendObservation(then: Function): void {
    if (this.mutationExtendId) {
      clearTimeout(this.mutationExtendId);
    }
    this.mutationExtendId = setTimeout(then, MS_DELAY_BEFORE_TRIGGER);
  }

  stopObserving(): void {
    if (this.mutationExtendId) {
      clearTimeout(this.mutationExtendId);
    }
    this.mutationExtendId = 0;
    this.mutationObserver?.disconnect();
  }
}

/**
 * Searches page elements for "nointentdetection" meta tag. Returns true if
 * "nointentdetection" meta tag is defined.
 */
function hasNoIntentDetection() {
  const metas = document.getElementsByTagName('meta');
  for (let i = 0; i < metas.length; i++) {
    if (metas[i]!.getAttribute('name') === 'chrome' &&
        metas[i]!.getAttribute('content') === 'nointentdetection') {
      return true;
    }
  }

  return false;
}

// Returns all types in meta tags 'format-detection', where the type is
// assigned 'no'.
function noFormatDetectionTypes(): Set<string> {
  const metas = document.getElementsByTagName('meta');
  let types = new Set<string>();
  for (const meta of metas) {
    if (meta.getAttribute('name') !== 'format-detection')
      continue;
    let content = meta.getAttribute('content');
    if (!content)
      continue;
    let matches = content.toLowerCase().matchAll(/([a-z]+)\s*=\s*([a-z]+)/g);
    if (!matches)
      continue;
    for (let match of matches) {
      if (match && match[2] === 'no' && match[1]) {
        types.add(match[1]);
      }
    }
  }
  return types;
}

/**
 * Searches page elements for "notranslate" meta tag. Returns true if
 * "notranslate" meta tag is defined.
 */
function hasNoTranslate(): boolean {
  const metas = document.getElementsByTagName('meta');
  for (let i = 0; i < metas.length; i++) {
    if (metas[i]!.getAttribute('name') === 'google' &&
        metas[i]!.getAttribute('content') === 'notranslate') {
      return true;
    }
  }
  return false;
}

/**
 * Gets the content of a meta tag by httpEquiv for `httpEquiv`. The function is
 * case insensitive.
 */
function getMetaContentByHttpEquiv(httpEquiv: string) {
  const metaTags = document.getElementsByTagName('meta');
  for (let metaTag of metaTags) {
    if (metaTag.httpEquiv.toLowerCase() === httpEquiv) {
      return metaTag.content;
    }
  }
  return '';
}

const highlightTextColor = '#000';
const highlightBackgroundColor = 'rgba(20,111,225,0.25)';
const decorationStyles = 'border-bottom-width: 1px; ' +
    'border-bottom-style: dotted; ' +
    'background-color: transparent';
const decorationStylesForPhoneAndEmail = 'border-bottom-width: 1px; ' +
    'border-bottom-style: solid; ' +
    'background-color: transparent';
const decorationDefaultColor = 'blue';

/**
 * Callback for processing a node during DOM traversal
 * @param node - Node or Element containing text or newline.
 * @param index - index into the stream or characters so far.
 * @param text - the text for this node.
 */
type EnumNodesFunction = (node: Node, index: number, text: string) => boolean;

let decorations: Decoration[] = [];

let sections: Section[];

// Mark: Public API functions called from native code.

/**
 * Extracts first `maxChars` text characters from the page. Once done it
 * send a 'annotations.extractedText' command with the 'text'.
 * @param maxChars - maximum number of characters to parse out.
 * @param seqId - id of extracted text to pass back.
 */
function extractText(maxChars: number, seqId: number): void {
  // If page is reloaded, remove decorations because the external cache
  // will need to be rebuilt with new data.
  if (decorations.length) {
    removeDecorations();
  }
  let disabledTypes = noFormatDetectionTypes();
  sendWebKitMessage('annotations', {
    command: 'annotations.extractedText',
    text: getPageText(maxChars),
    seqId: seqId,
    // When changing metadata please update i/w/p/a/annotations_text_observer.h
    metadata: {
      hasNoIntentDetection: hasNoIntentDetection(),
      hasNoTranslate: hasNoTranslate(),
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

/**
 * Decorate the page with the given `annotations`. Annotations will be sorted
 * and one will be dropped when two overlaps.
 * @param annotations - list of annotations.
 */
function decorateAnnotations(annotations: Annotation[]): void {
  // Avoid redoing without going through `removeDecorations` first.
  if (decorations.length || !annotations.length)
    return;

  let failures = 0;
  decorations = [];

  // Check CHROME_ANNOTATION on capturing and bubbling event.
  document.addEventListener('click', handleTopTap.bind(document));
  document.addEventListener('click', handleTopTap.bind(document), true);

  annotations = removeOverlappingAnnotations(annotations);

  // Reparse page finding annotations and styling them.
  let annotationIndex = 0;
  enumerateSectionsNodes((node, index, text) => {
    if (!node.parentNode || text === '\n')
      return true;

    // Skip annotation with end before index. This would happen if some nodes
    // are deleted between text fetching and decorating.
    while (annotationIndex < annotations.length) {
      const annotation = annotations[annotationIndex];
      if (!annotation || annotation.end > index) {
        break;
      }
      failures++;
      annotationIndex++;
    }

    const length = text.length;
    let replacements: Replacement[] = [];
    while (annotationIndex < annotations.length) {
      const annotation = annotations[annotationIndex];
      if (!annotation) {
        break;
      }
      const start = annotation.start;
      const end = annotation.end;
      if (index < end && index + length > start) {
        // Inside node
        const left = Math.max(0, start - index);
        const right = Math.min(length, end - index);
        const nodeText = text.substring(left, right);
        const annotationLeft = Math.max(0, index - start);
        const annotationRight = Math.min(end - start, index + length - start);
        const annotationText =
            annotation.text.substring(annotationLeft, annotationRight);
        // Text has changed, forget the rest of this annotation.
        if (nodeText != annotationText) {
          failures++;
          annotationIndex++;
          continue;
        }
        replacements.push(new Replacement(
            annotationIndex, left, right, nodeText, annotation.type,
            annotation.text, annotation.data));
        // If annotation is completed, move to next annotation and retry on
        // this node to fit more annotations if needed.
        if (end <= index + length) {
          annotationIndex++;
          continue;
        }
      }
      break;
    }

    // If the hit on a link (or other interactive tags), do not stylize. The
    // check doesn't happen before the annotation loop above, to keep the
    // running cursor's (annotationIndex) integrity. It also doesn't happen
    // at text extraction, to allow these tag's text to participate in a bigger
    // intent detection.
    let currentParentNode: Node|null = node.parentNode;
    while (currentParentNode) {
      if (currentParentNode instanceof HTMLElement &&
          NO_DECORATION_NODE_NAMES.has(currentParentNode.tagName)) {
        replacements = [];
        failures++;
        break;
      }
      currentParentNode = currentParentNode.parentNode;
    }

    replaceNode(node, replacements, text);

    return annotationIndex < annotations.length;
  });

  // Any remaining annotations left untouched are failures.
  failures += annotations.length - annotationIndex;

  sendWebKitMessage('annotations', {
    command: 'annotations.decoratingComplete',
    successes: annotations.length - failures,
    failures: failures,
    annotations: annotations.length,
    cancelled: []
  });
}

/**
 * Remove current decorations.
 */
function removeDecorations(): void {
  for (let decoration of decorations) {
    const replacements = decoration.replacements;
    const parentNode = replacements[0]!.parentNode;
    if (!parentNode)
      return;
    parentNode.insertBefore(decoration.original, replacements[0]!);
    for (let replacement of replacements) {
      parentNode.removeChild(replacement);
    }
  }
  decorations = [];
}

/**
 * Remove current decorations of a given type.
 * @param type - the type of annotations to remove.
 */
function removeDecorationsWithType(type: string): void {
  var remainingDecorations: Decoration[] = [];
  for (let decoration of decorations) {
    const replacements = decoration.replacements;
    const parentNode = replacements[0]!.parentNode;
    if (!parentNode)
      return;

    var hasReplacementOfType = false;
    var hasReplacementOfAnotherType = false;
    for (let replacement of replacements) {
      if (!(replacement instanceof HTMLElement)) {
        continue;
      }
      var element = replacement as HTMLElement;
      var replacementType = element.getAttribute('data-type');
      if (replacementType === type) {
        hasReplacementOfType = true;
      } else {
        hasReplacementOfAnotherType = true;
      }
    }
    if (!hasReplacementOfType) {
      // This decoration is of another type, leave it as it is.
      remainingDecorations.push(decoration);
      continue;
    }

    if (!hasReplacementOfAnotherType) {
      // Restore previous node
      parentNode.insertBefore(decoration.original, replacements[0]!);
      for (let replacement of replacements) {
        parentNode.removeChild(replacement);
      }
      continue;
    }

    // The decoration is of mixed type. Just replace the <chrome_annotation>
    // of `type` by a text node with same text content.
    let newReplacements: Node[] = [];
    for (let replacement of replacements) {
      if (!(replacement instanceof HTMLElement)) {
        newReplacements.push(replacement);
        continue;
      }
      var element = replacement as HTMLElement;
      var replacementType = element.getAttribute('data-type');
      if (replacementType !== type) {
        newReplacements.push(replacement);
        continue;
      }
      let text = document.createTextNode(element.textContent ?? '');
      parentNode.replaceChild(text, element);
      newReplacements.push(text);
    }
    decoration.replacements = newReplacements;
    remainingDecorations.push(decoration);
  }
  decorations = remainingDecorations;
}

/**
 * Removes any highlight on all annotations.
 */
function removeHighlight(): void {
  for (let decoration of decorations) {
    for (let replacement of decoration.replacements) {
      if (!(replacement instanceof HTMLElement)) {
        continue;
      }
      replacement.style.color = '';
      replacement.style.background = '';
    }
  }
}

// Mark: Private helper functions

/**
 * Traverse the DOM tree starting at `root` and call process on each text
 * node.
 * @param root - root node where to start traversal.
 * @param process - callback for each text node.
 * @param includeShadowDOM - when true, shadow DOM is also traversed.
 * @param filterInvisibles - when true, filters out invisible nodes.
 */
function enumerateTextNodes(
    root: Node, process: EnumNodesFunction,
    includeShadowDOM: boolean = true,
    filterInvisibles: boolean = true): void {
  const nodes: Node[] = [root];
  let index = 0;
  let isPreviousSpace = true;

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
      // Reject editable nodes.
      if (node instanceof Element && node.getAttribute('contenteditable')) {
        continue;
      }
      if (node.nodeName === 'BR') {
        if (isPreviousSpace)
          continue;
        if (!process(node, index, '\n'))
          break;
        isPreviousSpace = true;
        index += 1;
        continue;
      }
      const style = window.getComputedStyle(node as Element);
      // Only proceed if the element is visible or if invisibles are to be kept.
      if (filterInvisibles &&
          (style.display === 'none' || style.visibility === 'hidden')) {
        continue;
      }
      // No need to add a line break before `body` as it is the first element.
      if (node.nodeName.toUpperCase() !== 'BODY' &&
          style.display !== 'inline' && !isPreviousSpace) {
        if (!process(node, index, '\n'))
          break;
        isPreviousSpace = true;
        index += 1;
      }

      if (includeShadowDOM) {
        const element = node as Element;
        if (element.shadowRoot && element.shadowRoot != node) {
          nodes.push(element.shadowRoot);
          continue;
        }
      }
    }

    if (node.hasChildNodes()) {
      for (let childIdx = node.childNodes.length - 1; childIdx >= 0;
           childIdx--) {
        nodes.push(node.childNodes[childIdx]!);
      }
    } else if (node.nodeType === Node.TEXT_NODE && node.textContent) {
      const isSpace = node.textContent.trim() === '';
      if (isSpace && isPreviousSpace)
        continue;
      if (!process(node, index, node.textContent))
        break;
      isPreviousSpace = isSpace;
      index += node.textContent.length;
    }
  }
}

/**
 * Alternative to `enumerateTextNodes` using sections.
 */
function enumerateSectionsNodes(process: EnumNodesFunction): void {
  for (let section of sections) {
    const node: Node|undefined = section.node.deref();
    if (!node)
      continue;

    const text: string|null =
        node.nodeType === Node.ELEMENT_NODE ? '\n' : node.textContent;
    if (text && !process(node, section.index, text))
      break;
  }
}

/**
 * Returns first `maxChars` text characters from the page. If intents are
 * disabled, return an empty string.
 * @param maxChars - maximum number of characters to parse out.
 */
function getPageText(maxChars: number): string {
  const parts: string[] = [];
  sections = [];
  enumerateTextNodes(document.body, function(node, index, text) {
    sections.push(new Section(new WeakRef<Node>(node), index));
    if (index + text.length > maxChars) {
      parts.push(text.substring(0, maxChars - index));
    } else {
      parts.push(text);
    }
    return index + text.length < maxChars;
  });
  return ''.concat(...parts);
}

let mutationDuringClickObserver: MutationsDuringClickTracker|null;

// Stops observing DOM mutations.
function cancelObserver(): void {
  mutationDuringClickObserver?.stopObserving();
  mutationDuringClickObserver = null;
}

// Monitors taps at the top, document level. This checks if it is tap
// triggered by an annotation and if no DOM mutation have happened while the
// event is bubbling up. If it's the case, the annotation callback is called.
function handleTopTap(event: Event): void {
  const annotation = event.target;
  if (annotation instanceof HTMLElement &&
      annotation.tagName === 'CHROME_ANNOTATION') {
    if (event.eventPhase === Event.CAPTURING_PHASE) {
      // Initiates a `mutationDuringClickObserver` that will be checked at
      // bubble up phase where it will be decided if the click should be
      // cancelled.
      cancelObserver();
      mutationDuringClickObserver = new MutationsDuringClickTracker(event);
    } else if (mutationDuringClickObserver) {
      // At BUBBLING_PHASE.
      if (!mutationDuringClickObserver.hasPreventativeActivity(event)) {
        mutationDuringClickObserver.extendObservation(() => {
          if (mutationDuringClickObserver) {
            highlightAnnotation(annotation);
            onClickAnnotation(
                annotation, mutationDuringClickObserver.hasMutations);
          }
        });
      } else {
        onClickAnnotation(annotation, mutationDuringClickObserver.hasMutations);
      }
    }
  } else {
    cancelObserver();
  }
}

// Sends click to browser side and cancel observer.
function onClickAnnotation(annotation: HTMLElement, cancel: boolean): void {
  sendWebKitMessage('annotations', {
    command: 'annotations.onClick',
    cancel: cancel,
    data: annotation.dataset['data'],
    rect: rectFromElement(annotation),
    text: annotation.dataset['annotation'],
  });
  cancelObserver();
}

/**
 * Highlights all elements related to the annotation of which `annotation` is an
 * element of.
 */
function highlightAnnotation(annotation: HTMLElement) {
  // Using webkit edit selection kills a second tapping on the element and also
  // causes a merge with the edit menu in some circumstance.
  // Using custom highlight instead.
  for (let decoration of decorations) {
    for (let replacement of decoration.replacements) {
      if (!(replacement instanceof HTMLElement)) {
        continue;
      }
      if (replacement.tagName === 'CHROME_ANNOTATION' &&
          replacement.dataset['index'] === annotation.dataset['index']) {
        replacement.style.color = highlightTextColor;
        replacement.style.backgroundColor = highlightBackgroundColor;
      }
    }
  }
}

/**
 * Sorts and removes olverlappings annotations.
 * @param annotations - input annotations, cleaned in-place.
 */
function removeOverlappingAnnotations(annotations: Annotation[]): Annotation[] {
  // Sort the annotations, in place.
  annotations.sort((a, b) => {
    return a.start - b.start;
  });

  // Remove overlaps (lower indexed annotation has priority).
  let previous: Annotation|undefined = undefined;
  return annotations.filter((annotation) => {
    if (previous && previous.start < annotation.end &&
        previous.end > annotation.start) {
      return false;
    }
    previous = annotation;
    return true;
  });
}

/**
 * Removes `node` from the DOM and replaces it with elements described by
 * `replacements` for the given `text`.
 */
function replaceNode(
    node: Node, replacements: Replacement[], text: string): void {
  const parentNode: Node|null = node.parentNode;

  if (replacements.length <= 0 || !parentNode) {
    return;
  }

  let textColor: string = decorationDefaultColor;
  if (parentNode instanceof Element) {
    textColor = window.getComputedStyle(parentNode).color || textColor;
  }

  let cursor = 0;
  const parts: Node[] = [];
  for (let replacement of replacements) {
    if (replacement.left > cursor) {
      parts.push(
          document.createTextNode(text.substring(cursor, replacement.left)));
    }
    const element = document.createElement('chrome_annotation');
    element.setAttribute('data-index', '' + replacement.index);
    element.setAttribute('data-data', replacement.data);
    element.setAttribute('data-annotation', replacement.annotationText);
    element.setAttribute('data-type', replacement.type);
    element.setAttribute('role', 'link');
    // Use textContent not innerText, since setting innerText will cause
    // the text to be parsed and '\n' to be upgraded to <br>.
    element.textContent = replacement.text;

    if (replacement.type == 'PHONE_NUMBER' || replacement.type == 'EMAIL') {
      element.style.cssText = decorationStylesForPhoneAndEmail;
    } else {
      element.style.cssText = decorationStyles;
    }

    element.style.borderBottomColor = textColor;
    parts.push(element);
    cursor = replacement.right;
  }
  if (cursor < text.length) {
    parts.push(document.createTextNode(text.substring(cursor, text.length)));
  }

  for (let part of parts) {
    parentNode.insertBefore(part, node);
  }
  parentNode.removeChild(node);

  // Keep track to be able to undo in `removeDecorations`.
  decorations.push(new Decoration(node, parts));
}

function rectFromElement(element: Element) {
  const domRect = element.getClientRects()[0];
  if (!domRect) {
    return {};
  }
  return {
    x: domRect.x,
    y: domRect.y,
    width: domRect.width,
    height: domRect.height
  };
}

gCrWeb.annotations = {
  extractText,
  decorateAnnotations,
  removeDecorations,
  removeDecorationsWithType,
  removeHighlight,
};
