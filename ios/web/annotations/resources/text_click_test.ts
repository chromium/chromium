// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_click.ts.
 */

import {AnnotationsTapConsumer, TextClick} from '//ios/web/annotations/resources/text_click.js';
import {expectEq, FakeTaskTimer, load, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextClick extends TestSuite {
  // Mark:  AnnotationsTapConsumer

  tappedAnnotation?: HTMLElement;
  tappedCancel?: boolean;

  tapConsumer: AnnotationsTapConsumer =
      (annotation: HTMLElement, cancel: boolean): void => {
        this.tappedAnnotation = annotation;
        this.tappedCancel = cancel;
      };

  // Mark:  tests

  override setUp() {
    this.tappedAnnotation = undefined;
    this.tappedCancel = undefined;
  }

  // Tests that `tapConsumer` is called (and `tappedCancel` is true) when no DOM
  // mutation occurs during the bubbling of a click event.
  testTextClickNoMutations() {
    const decoratedHTML = '<div id="outer">' +
        '<chrome_annotation>Hello</chrome_annotation>' +
        '</div>';
    load(decoratedHTML);

    const annotation = document.querySelector('chrome_annotation')!;
    const timer = new FakeTaskTimer();
    const clicker = new TextClick(
        document.documentElement, this.tapConsumer, () => undefined, timer,
        /* mutationCheckDelay */ 50, annotation);
    clicker.start();

    const event = new Event('click', {bubbles: true, cancelable: true});
    annotation.dispatchEvent(event);

    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after click:');
    timer.moveAhead(/* ms= */ 10, /* times= */ 4);  // -> 40ms total
    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after 40ms:');
    timer.moveAhead(/* ms= */ 10, /* times= */ 2);  // -> 60ms total
    expectEq(annotation, this.tappedAnnotation, 'tappedAnnotation after 60ms:');
    expectEq(false, this.tappedCancel, 'tappedCancel after 60ms:');

    clicker.stop();
  }

  // Tests that a click event stopped with `stopImmediatePropagation` doesn't
  // trigger the `tapConsumer`.
  testTextClickStopped() {
    const decoratedHTML = '<div id="outer">' +
        '<chrome_annotation>Hello</chrome_annotation>' +
        '</div>';
    load(decoratedHTML);

    const outer = document.querySelector('#outer')!;
    outer.addEventListener('click', (event: Event) => {
      event.stopImmediatePropagation();
    });
    const annotation = document.querySelector('chrome_annotation')!;
    const timer = new FakeTaskTimer();
    const clicker = new TextClick(
        document.documentElement, this.tapConsumer, () => undefined, timer,
        /* mutationCheckDelay */ 50, annotation);
    clicker.start();

    const event = new Event('click', {bubbles: true, cancelable: true});
    annotation.dispatchEvent(event);

    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after click:');
    timer.moveAhead(/* ms= */ 10, /* times= */ 10);  // -> 100ms total
    // Should not reach calling the AnnotationsTapConsumer.
    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after 100ms:');
    expectEq(undefined, this.tappedCancel, 'tappedCancel after 100ms:');

    clicker.stop();
  }

  // Tests that a click event stopped with `preventDefault` does trigger the
  // `tapConsumer` but with `tappedCancel` set to true.
  testTextClickPrevented() {
    const decoratedHTML = '<div id="outer">' +
        '<chrome_annotation>Hello</chrome_annotation>' +
        '</div>';
    load(decoratedHTML);

    const outer = document.querySelector('#outer')!;
    outer.addEventListener('click', (event: Event) => {
      event.preventDefault();
    });
    const annotation = document.querySelector('chrome_annotation')!;
    const timer = new FakeTaskTimer();
    const clicker = new TextClick(
        document.documentElement, this.tapConsumer, () => undefined, timer,
        /* mutationCheckDelay */ 50, annotation);
    clicker.start();

    const event = new Event('click', {bubbles: true, cancelable: true});
    annotation.dispatchEvent(event);

    // Without delay, this event should be cancelled:
    expectEq(
        annotation, this.tappedAnnotation, 'tappedAnnotation after 100ms:');
    expectEq(true, this.tappedCancel, 'tappedCancel after 100ms:');

    clicker.stop();
  }

  // Tests that a click event stopped due to DOM mutation does trigger the
  // `tapConsumer` but with `tappedCancel` set to true.
  testTextClickWithMutationInsideTree() {
    const decoratedHTML = '<div id="outer">' +
        '<div id="mutate">I will mutate!' +
        '<chrome_annotation>Hello</chrome_annotation></div>' +
        '</div>';
    load(decoratedHTML);

    const annotation = document.querySelector('chrome_annotation')!;
    const timer = new FakeTaskTimer();
    const clicker = new TextClick(
        document.documentElement, this.tapConsumer, () => undefined, timer,
        /* mutationCheckDelay */ 50, annotation);
    clicker.start();

    const event = new Event('click', {bubbles: true, cancelable: true});
    annotation.dispatchEvent(event);

    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after click:');
    timer.moveAhead(/* ms= */ 10, /* times= */ 2);  // -> 20ms total
    document.querySelector('#mutate')!.appendChild(
        document.createTextNode('Mutated!'));
    clicker.updateForTesting();
    timer.moveAhead(/* ms= */ 10, /* times= */ 4);  // -> 60ms total
    expectEq(annotation, this.tappedAnnotation, 'tappedAnnotation after 60ms:');
    expectEq(true, this.tappedCancel, 'tappedCancel after 60ms:');

    clicker.stop();
  }

  // Tests that a click event is not stopped by a DOM mutation outside of the
  // tree of the annotation.
  testTextClickWithMutationOutsideTree() {
    const decoratedHTML = '<div id="outer">' +
        '<div id="mutate">I will mutate!</div>' +
        '<chrome_annotation>Hello</chrome_annotation>' +
        '</div>';
    load(decoratedHTML);

    const annotation = document.querySelector('chrome_annotation')!;
    const timer = new FakeTaskTimer();
    const clicker = new TextClick(
        document.documentElement, this.tapConsumer, () => undefined, timer,
        /* mutationCheckDelay */ 50, annotation);
    clicker.start();

    const event = new Event('click', {bubbles: true, cancelable: true});
    annotation.dispatchEvent(event);

    expectEq(undefined, this.tappedAnnotation, 'tappedAnnotation after click:');
    timer.moveAhead(/* ms= */ 10, /* times= */ 2);  // -> 20ms total
    document.querySelector('#mutate')!.appendChild(
        document.createTextNode('Mutated!'));
    clicker.updateForTesting();
    timer.moveAhead(/* ms= */ 10, /* times= */ 4);  // -> 60ms total
    expectEq(annotation, this.tappedAnnotation, 'tappedAnnotation after 60ms:');
    expectEq(false, this.tappedCancel, 'tappedCancel after 60ms:');

    clicker.stop();
  }
}

export {TestTextClick}
