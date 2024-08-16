// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles multiple decorations on the web page.
 */

import {TextAnnotationList} from '//ios/web/annotations/resources/text_annotation_list.js';
import {annotationUniqueId, createChromeAnnotation, createSpace, originalNodeDecorationId, replacementNodeDecorationId, TextDecoration} from '//ios/web/annotations/resources/text_decoration.js';
import {annotationCanBeCrossElement, HTMLElementWithSymbolIndex, NodeWithSymbolIndex, TextWithSymbolIndex} from '//ios/web/annotations/resources/text_dom_utils.js';
import {TextChunk} from '//ios/web/annotations/resources/text_extractor.js';
import {TextStyler} from '//ios/web/annotations/resources/text_styler.js';

// `AnnotationsReplacement` represent an interval replacement inside a text
// Node. `id` is a unique annotation id number. `left` and `right` is the range
// inside `text`. All new nodes (HTMLElement) have `data` added as their
// `annotationExternalData`.
class AnnotationsReplacement {
  constructor(
      public id: number, public left: number, public right: number,
      public text: string, public type: string, public fullText: string,
      public data: string, public needsPrecedingSpace: boolean) {}
}

// Highlighting styles.
const HIGHLIGHT_TEXT_COLOR = '#000';
const HIGHLIGHT_BACKGROUND_COLOR = 'rgba(20,111,225,0.25)';

// One important thing to remember is: A textNode can include a part of, the
// whole of or many annotations. Reversely, an annotation can cover a part of,
// the whole of or many text nodes.

// Five cases are possible regarding the text node to be decorated:
// 1. It's an original node in another decoration:
//    Replace by a sub enumeration of non decorated intervals (i.e.
//    text nodes) computed from the other decoration's replacement nodes, and
//    apply case 2. This would happen if chunks in flight cross.
// 2. It's a replacement node from another decoration (happens when grabbing
//    extra text and an annotation staggles a border of the extraction window):
//    Merge the new decoration into first one (who has the "real" original)
//    so a cleanup doesn't affect one of the two decoration.
// 3. An untouched node:
//    Create a decoration.
// 4. An untouched node deleted from DOM, but not gc collected.
//    If node.getRootNode({composed: true}) === node, the node is detached
//    so it should be skipped and not decorated.
// 5. An untouched node, deleted and gc collected.
//    Will be ignored and not decorated.

class TextDecorator {
  constructor(private styler: TextStyler) {}

  // The key is a unique `number` tagged on the original node and replacememnt
  // nodes of the elements in an `AnnotationDecoration`.
  decorations = new Map<number, TextDecoration>();

  // Unique id used in `decorations` map.
  uniqueId = 0;

  // A mutation observer callback that observed the original nodes.
  // If an original node is updated, the decoration should be restored.
  private mutationCallback =
      (mutationList: MutationRecord[]) => {
        for (let mutation of mutationList) {
          let target = mutation.target as TextWithSymbolIndex;
          if (!target[originalNodeDecorationId]) {
            continue;
          }
          let replacementId = target[originalNodeDecorationId];
          const liveDecoration = this.decorations.get(replacementId);
          liveDecoration?.restore();
        }
      }

  // A mutation observer callback that observed the original nodes.
  // If an original node is updated, the decoration should be restored.
  private mutationObserver = new MutationObserver(this.mutationCallback);

  // Decorates a single `textNode` at given `index` in full chunk text.
  private decorateNode(
      run: TextAnnotationList, textNode: TextWithSymbolIndex,
      index: number): void {
    // Skip annotation with end before index. This would happen if some nodes
    // are deleted between text fetching and decorating.
    run.skipBeforeIndex(index);

    // The text of this textNode and its length.
    const text = textNode.textContent!;
    const length = text.length;

    // `replacements` will hold all the intervals inside `text` that are
    // annotations, according to what's in the given run list.
    const replacements: AnnotationsReplacement[] = [];
    // This loop can 'eat' none or any number of annotations.
    while (!run.done) {
      const annotation = run.currentAnnotation;
      if (!annotation) {
        break;
      }
      const start = annotation.start;
      const end = annotation.end;
      if (index < end && index + length > start) {
        if (end > index + length &&
            !(annotationCanBeCrossElement(annotation.type))) {
          run.skip();
          continue;
        }
        // Position and substring inside the textNode. A textNode can include
        // a part of, the whole of or many annotations.
        const left = Math.max(0, start - index);
        const right = Math.min(length, end - index);
        const nodeText = text.substring(left, right);
        // Position and substring inside the annotation that should match the
        // the substring inside the textNode. An annotation can cover a part of,
        // the whole of or many text nodes.
        const annotationLeft = Math.max(0, index - start);
        const annotationRight = Math.min(end - start, index + length - start);
        const expectedText =
            annotation.text.substring(annotationLeft, annotationRight);
        // If the text has changed, forget the rest of this annotation.
        if (nodeText !== expectedText) {
          run.skip();
          continue;
        }
        // If there is a space before the annotation, and if it is not carried
        // in the annotation text then the space disappears.
        const needsPrecedingSpace = left > 0 && text[left - 1] == ' ';
        replacements.push(new AnnotationsReplacement(
            run.annotationId, left, right, nodeText, annotation.type,
            annotation.text, annotation.data, needsPrecedingSpace));
        // If annotation is completed, move to next annotation and retry on
        // this node to fit more annotations if needed.
        if (end <= index + length) {
          run.next();
          continue;
        }
      }
      break;
    }

    const decoration = this.decorateSingleNode(textNode, replacements, text);
    if (decoration) {
      // Check if this node is already a replacement node.
      const replacementId = textNode[replacementNodeDecorationId];
      if (replacementId) {
        // Case 2 (and indirectly 1) - Merge into existing decoration.
        const liveDecoration = this.decorations.get(replacementId);
        liveDecoration?.replaceReplacementNode(
            textNode, decoration.replacements);
      } else {
        this.decorations.set(decoration.id, decoration);
        // Observe the original text Node. If the value change, the annotation
        // should be reverted.
        this.mutationObserver.observe(textNode, {characterData: true});
        decoration.apply();
      }
    }
  }

  // Creates an `TextDecoration` for the given `textNode`. Replacement are
  // intervals inside the node's `text` where a (part of a) CHROME_ANNOTATION
  // should be. This function will create the annotations elements and the text
  // nodes needed to cover the whole of the original `textNode`.
  private decorateSingleNode(
      textNode: Text, replacements: AnnotationsReplacement[],
      text: string): TextDecoration|null {
    const parentNode: Node|null = textNode.parentNode;

    if (replacements.length <= 0 || !parentNode) {
      return null;
    }

    let cursor = 0;
    const parts: Node[] = [];
    for (const replacement of replacements) {
      if (replacement.left > cursor) {
        if (replacement.needsPrecedingSpace) {
          parts.push(document.createTextNode(
              text.substring(cursor, replacement.left - 1)));
          const space = createSpace(replacement.id, replacement.type);
          parts.push(space);
          this.styler.space(space);
        } else {
          parts.push(document.createTextNode(
              text.substring(cursor, replacement.left)));
        }
      }
      const element = createChromeAnnotation(
          replacement.id, replacement.text, replacement.type,
          replacement.fullText, replacement.data);
      this.styler.style(parentNode, element, replacement.type);
      parts.push(element);
      cursor = replacement.right;
    }
    if (cursor < text.length) {
      parts.push(document.createTextNode(text.substring(cursor, text.length)));
    }

    return new TextDecoration(++this.uniqueId, textNode, parts);
  }

  // Applies `annotations` on given `chunk` nodes.
  decorateChunk(chunk: TextChunk, run: TextAnnotationList): void {
    if (!run.currentAnnotation) {
      return;
    }

    let offset = chunk.firstNodeOffset;
    for (const section of chunk.sections) {
      const textNode = section.textNode;
      if (!textNode || !textNode.textContent) {
        offset = 0;
        continue;
      }
      const existingId = textNode[originalNodeDecorationId];
      if (existingId) {
        // Case 1.
        // Get other decoration.
        const decoration = this.decorations.get(existingId);
        if (decoration) {
          let innerIndex = 0;
          // Iterate on replacements that are text nodes. Compute an innerIndex.
          for (const replacement of decoration.replacements) {
            if (!replacement.textContent) {
              continue;
            }
            if (replacement.nodeType == Node.TEXT_NODE) {
              this.decorateNode(
                  run, replacement as Text,
                  section.index + innerIndex - offset);
            }
            innerIndex += replacement.textContent.length;
          }
        }
      } else if (textNode.parentNode && textNode.getRootNode({
                   composed: true
                 }) !== textNode) {
        // Case 2 and 3.
        this.decorateNode(run, textNode, section.index - offset);
      } else {
        // Case 4 and 5, skip dead nodes.
      }

      offset = 0;
      if (run.done) {
        break;
      }
    }
  }

  // Tells decorator that the given `node` (a replacement node) was removed from
  // the DOM by an external actor.
  decorationNodeUnexpectedlyRemoved(node: NodeWithSymbolIndex): void {
    // Only node tagged with `replacementNodeDecorationId` should end up here,
    // since the original nodes are already under the decorator's control.
    // As soon as part of a decoration is removed by an external actor,
    // this decoration loses integrity. The best course is to cleanup and delete
    // it from here. This can sadly leave some dead annotations cached on the
    // browser side until a page refresh.
    const decorationId = node[replacementNodeDecorationId];
    const decoration = this.decorations.get(decorationId);
    if (decoration) {
      decoration.cleanupAfterCorruption();
      this.decorations.delete(decorationId);
    }
  }

  // Removes all decorations.
  removeAllDecorations(): void {
    this.decorations.forEach((decoration) => {
      decoration.restore();
    });
    this.mutationObserver.disconnect();
    this.decorations.clear();
  }

  // Removes all decorations of given `type`.
  removeDecorationsOfType(type: string): void {
    this.decorations.forEach((decoration, key) => {
      const replacements = decoration.replacements;
      const parentNode = replacements[0]!.parentNode;
      if (!parentNode) {
        return;
      }

      const count = decoration.replacementsOfType(type);
      if (count === 0) {
        // This decoration is of another type, leave it as it is.
        return;
      }
      if (count === decoration.replacements.length) {
        // Fully of given type.
        decoration.restore();
        this.decorations.delete(key);
        return;
      }

      // The decoration is of mixed type. Just replace CHROME_ANNOTATIONs
      // of `type` by a text node with same text content.
      decoration.removeReplacementsOfType(type);
    });
  }

  // Highlights all elements related to the annotation of which `annotation` is
  // part of. Using webkit edit selection kills a second tapping on the
  // element and also causes a merge with the edit menu in some circumstance.
  // Using custom highlight instead.
  highlightAnnotation(annotation: HTMLElementWithSymbolIndex): void {
    const annotationId = annotation[annotationUniqueId];
    this.decorations.forEach((decoration) => {
      for (const replacement of decoration.replacements) {
        if (replacement instanceof HTMLElement &&
            replacement[annotationUniqueId] === annotationId) {
          replacement.style.color = HIGHLIGHT_TEXT_COLOR;
          replacement.style.background = HIGHLIGHT_BACKGROUND_COLOR;
        }
      }
    });
  }

  // Removes any highlight on all annotations.
  removeHighlight(): void {
    this.decorations.forEach((decoration) => {
      for (const replacement of decoration.replacements) {
        if (replacement instanceof HTMLElement) {
          replacement.style.color = '';
          replacement.style.background = '';
        }
      }
    });
  }
}

export {
  TextDecorator,
}
