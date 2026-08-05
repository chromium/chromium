/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

'use strict';

/**
 * @namespace aria
 */
var aria = aria || {};

// anchorItem item to focus on with control + END
aria.Feed = function (feedNode, anchorItem) {
  this.feedNode = feedNode;
  this.anchorItem = anchorItem;
  this.feedItems = [];
  this.setupEvents();
};

aria.Feed.prototype.getFeedNode = function () {
  return this.feedNode;
};

aria.Feed.prototype.addItem = function (item) {
  this.feedItems.push(item);
};

aria.Feed.prototype.setupEvents = function () {
  this.feedNode.addEventListener('keydown', this.mapKeyShortcut.bind(this));
};

aria.Feed.prototype.focusItem = function (item) {
  if (!item || !item.focus) {
    return;
  }

  item.focus();
};

aria.Feed.prototype.mapKeyShortcut = function (event) {
  var key = event.which || event.keyCode;
  var focusedArticle = aria.Utils.matches(event.target, '[role="article"]')
    ? event.target
    : aria.Utils.getAncestorBySelector(event.target, '[role="article"]');

  if (!focusedArticle) {
    return;
  }

  var focusedIndex = focusedArticle.getAttribute('aria-posinset');

  switch (key) {
    case aria.KeyCode.PAGE_UP:
      event.preventDefault();

      // Move up focus
      if (focusedIndex > 1) {
        // Need to increment by 2 because focusIndex is 1-indexed
        this.focusItem(this.feedItems[focusedIndex - 2]);
      }
      break;
    case aria.KeyCode.PAGE_DOWN:
      event.preventDefault();
      // Move down focus
      if (this.feedItems.length >= focusedIndex) {
        // Do not need to increment focusIndex because it is 1-indexed
        this.focusItem(this.feedItems[focusedIndex]);
      }
      break;
    case aria.KeyCode.HOME:
      if (event.ctrlKey && this.feedItems.length > 0) {
        event.preventDefault();
        this.focusItem(this.feedItems[0]);
      }
      break;
    case aria.KeyCode.END:
      if (event.ctrlKey) {
        event.preventDefault();
        this.anchorItem.focus();
      }
      break;
  }
};
