// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Keeps track of all the existing PlayerInfo and
 * audio stream objects and is the entry-point for messages from the backend.
 *
 * The events captured by Manager (add, remove, update) are relayed
 * to the clientRenderer which it can choose to use to modify the UI.
 */
var Manager = (function() {
  'use strict';

  function createChild(parent, tag) {
    const node = document.createElement(tag);
    parent.appendChild(node);
    node.createTextNode = function(text) {
      const textNode = document.createTextNode(text);
      node.appendChild(textNode);
    };
    return node;
  }

  function CreateNotice() {
    const newContents = document.createElement('div');
    createChild(newContents, 'h1')
        .createTextNode('Media Internals Is being moved to devtools!');
    createChild(newContents, 'h3').createTextNode('Here\'s how to use it:');

    const directions = createChild(newContents, 'ol');

    const devtoolsExperiments = createChild(directions, 'li');
    devtoolsExperiments.createTextNode(
        'Ensure Devtools Experiments is enabled.  ');

    const experimentsLink = createChild(devtoolsExperiments, 'a');
    experimentsLink.href = 'chrome://flags/#enable-devtools-experiments';
    experimentsLink.createTextNode('Enable Flag Here');

    createChild(directions, 'li')
        .createTextNode('In devtools (F11) press F1 (open settings).');
    createChild(directions, 'li')
        .createTextNode(
            'Select the "Experiments" tab on the side,' +
            ' and check the "Media Element Inspection" box');
    createChild(directions, 'li')
        .createTextNode(
            'Restart devtools, and find "Media" under the "More Tools" menu.');
    return newContents;
  }

  function Manager(clientRenderer) {
    this.clientRenderer_ = clientRenderer;
    // Remove contents and add message.
    this.media_tab_ = $('players');
    this.media_tab_.innerHTML = '';
    this.media_tab_.appendChild(CreateNotice());
  }

  Manager.prototype = {};

  return Manager;
}());
