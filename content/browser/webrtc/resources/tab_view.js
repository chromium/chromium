// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A TabView provides the ability to create tabs and switch between tabs. It's
 * responsible for creating the DOM and managing the visibility of each tab.
 * The first added tab is active by default and the others hidden.
 */
var TabView = (function() {
  'use strict';

  /**
   * @constructor
   * @param {Element} root The root DOM element containing the tabs.
   */
  function TabView(root) {
    this.root_ = root;
    this.ACTIVE_TAB_HEAD_CLASS_ = 'active-tab-head';
    this.ACTIVE_TAB_BODY_CLASS_ = 'active-tab-body';
    this.TAB_HEAD_CLASS_ = 'tab-head';
    this.TAB_BODY_CLASS_ = 'tab-body';

    /**
     * A mapping for an id to the tab elements.
     * @type {!Object<!TabDom>}
     * @private
     */
    this.tabElements_ = {};

    this.headBar_ = null;
    this.activeTabId_ = null;
    this.initializeHeadBar_();
  }

  // Creates a simple object containing the tab head and body elements.
  function TabDom(h, b) {
    this.head = h;
    this.body = b;
  }

  TabView.prototype = {
    /**
     * Adds a tab with the specified id and title.
     * @param {string} id
     * @param {string} title
     * @return {!Element} The tab body element.
     */
    addTab: function(id, title) {
      if (this.tabElements_[id]) {
        throw 'Tab already exists: ' + id;
      }

      var head = document.createElement('span');
      head.className = this.TAB_HEAD_CLASS_;
      head.textContent = title;
      head.title = title;
      this.headBar_.appendChild(head);
      head.addEventListener('click', this.switchTab_.bind(this, id));

      var body = document.createElement('div');
      body.className = this.TAB_BODY_CLASS_;
      body.id = id;
      this.root_.appendChild(body);

      this.tabElements_[id] = new TabDom(head, body);

      if (!this.activeTabId_) {
        this.switchTab_(id);
      }
      return this.tabElements_[id].body;
    },

    /** Removes the tab. @param {string} id */
    removeTab: function(id) {
      if (!this.tabElements_[id]) {
        return;
      }
      this.tabElements_[id].head.parentNode.removeChild(
          this.tabElements_[id].head);
      this.tabElements_[id].body.parentNode.removeChild(
          this.tabElements_[id].body);

      delete this.tabElements_[id];
      if (this.activeTabId_ === id) {
        this.switchTab_(Object.keys(this.tabElements_)[0]);
      }
    },

    /**
     * Switches the specified tab into view.
     *
     * @param {string} activeId The id the of the tab that should be switched to
     *     active state.
     * @private
     */
    switchTab_: function(activeId) {
      if (this.activeTabId_ && this.tabElements_[this.activeTabId_]) {
        this.tabElements_[this.activeTabId_].body.classList.remove(
            this.ACTIVE_TAB_BODY_CLASS_);
        this.tabElements_[this.activeTabId_].head.classList.remove(
            this.ACTIVE_TAB_HEAD_CLASS_);
      }
      this.activeTabId_ = activeId;
      if (this.tabElements_[activeId]) {
        this.tabElements_[activeId].body.classList.add(
            this.ACTIVE_TAB_BODY_CLASS_);
        this.tabElements_[activeId].head.classList.add(
            this.ACTIVE_TAB_HEAD_CLASS_);
      }
    },

    /** Initializes the bar containing the tab heads. */
    initializeHeadBar_: function() {
      this.headBar_ = document.createElement('div');
      this.root_.appendChild(this.headBar_);
      this.headBar_.style.textAlign = 'center';
    },
  };
  return TabView;
})();
