/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

/* global aria */

'use strict';

/**
 * ARIA Feed Example
 *
 * @function onload
 * @description Initialize the feed once the page has loaded
 */
window.addEventListener('load', function () {
  var feedNode = document.getElementById('restaurant-feed');
  var delaySelect = window.parent.document.getElementById('delay-time-select');
  var termsOfUse = window.parent.document.getElementById('terms-of-use');
  var restaurantFeed = new aria.Feed(feedNode, termsOfUse);

  var restaurantFeedDisplay = new aria.FeedDisplay(restaurantFeed, function () {
    return aria.RestaurantData;
  });

  restaurantFeedDisplay.setDelay(delaySelect.value);
  delaySelect.addEventListener('change', function () {
    restaurantFeedDisplay.setDelay(delaySelect.value);
  });
});
