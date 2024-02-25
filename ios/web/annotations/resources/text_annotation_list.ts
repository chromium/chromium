// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility classes to handle iterating through an array of
 * `TextViewportAnnotation`.
 */

// Matches an annotation returned by the browser side.
interface TextViewportAnnotation {
  //  Character index to start of annotation.
  start: number;
  // Character index to end of annotation (first character after text).
  end: number;
  // The annotation text used to ensure the text in the page hasn't changed.
  text: string;
  // Annotation type.
  type: string;
  // A passed in string that will be sent back to obj in tap handler.
  data: string;
}

// Holds data needed to move through an array of `TextViewportAnnotation`. The
// array is sorted and overlaps are removed.
class TextAnnotationList {
  // Unique id  number for current annotation, updated on `next`. Must be
  // different for each annotation in the page, not just a single instance of
  // `TextAnnotationList`.
  static currentAnnotationId: number = 1;

  // All failed annotations.
  cancelled: TextViewportAnnotation[] = [];

  // Index of current annotation.
  private annotationIndex = 0;

  // Current total of `successes` (annotations ending with `next`) or `failures`
  // (annotations ending with `skip` or clipped out).
  successes = 0;
  get failures(): number {
    return this.cancelled.length;
  }

  // Takes the list of `annotations` and an optional clip window (`clipStart`,
  // `clipEnd` text indices) that potentially reduces the annotations in the
  // list.
  constructor(
      private annotations: TextViewportAnnotation[], clipStart?: number,
      clipEnd?: number) {
    this.annotations = this.cleanUpAnnotations(annotations, clipStart, clipEnd);
  }

  // Returns `true` if the `annotationIndex` indicates all `annotations`
  // have been handled.
  get done(): boolean {
    return this.annotationIndex >= this.annotations.length;
  }

  // Returns current annotation or `null` if none.
  get currentAnnotation(): TextViewportAnnotation|null {
    return this.annotations[this.annotationIndex] || null;
  }

  // Returns this current `currentAnnotation` generated unique id.
  get annotationId(): number {
    return TextAnnotationList.currentAnnotationId;
  }

  // Moves to next annotation and updates `annotationId`. Increases `successes`.
  next(): void {
    this.successes++;
    this.annotationIndex++;
    TextAnnotationList.currentAnnotationId++;
  }

  // Fails the remaining annotations. See `skip`.
  fail(): void {
    this.skip(this.annotations.length - this.annotationIndex);
  }

  // Skips `count` annotations. Every annotation skipped is added as one more
  // failure. Computes a new annotationId.
  skip(count = 1): void {
    for (; count > 0; count--) {
      const annotation = this.annotations[this.annotationIndex];
      if (annotation) {
        this.cancelled.push(annotation);
      }
      this.annotationIndex++;
    }
    TextAnnotationList.currentAnnotationId++;
  }

  // Skips annotations that end before given text `index` (into the
  // corresponding text chunk) and updates failures.
  skipBeforeIndex(index: number): void {
    const annotationIndex = this.annotationIndex;
    let count = 0;
    while (annotationIndex + count < this.annotations.length) {
      const annotation = this.annotations[annotationIndex + count];
      if (!annotation || annotation.end > index) {
        break;
      }
      count++;
    }
    if (count) {
      this.skip(count);
    }
  }

  // Sorts and removes overlapping and/or clipped `annotations`. Every removed
  // annotation is counted as a failure.
  private cleanUpAnnotations(
      annotations: TextViewportAnnotation[], clipStart?: number,
      clipEnd?: number): TextViewportAnnotation[] {
    // Sort the annotations, in place.
    annotations.sort((a, b) => a.start - b.start);

    // Remove overlaps (lower indexed annotation has priority) and annotation
    // fully before or after clip area [`clipStart`, `clipEnd`).
    let previous: TextViewportAnnotation|null = null;
    return annotations.filter((annotation) => {
      const overlaps = previous && previous.start < annotation.end &&
          previous.end > annotation.start;
      const clips = (clipStart && annotation.end <= clipStart) ||
          (clipEnd && annotation.start >= clipEnd);
      if (overlaps || clips) {
        this.cancelled.push(annotation);
        return false;
      }
      previous = annotation;
      return true;
    });
  }
}

export {
  TextViewportAnnotation,
  TextAnnotationList,
}
