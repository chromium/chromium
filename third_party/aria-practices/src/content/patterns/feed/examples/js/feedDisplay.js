/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 */

'use strict';

/**
 * @namespace aria
 */
var aria = aria || {};

aria.RestaurantData = [
  {
    name: "Tito's Tacos",
    rating: 5,
    type: 'Mexican, Tacos',
    street: '123 Blueberry Ln',
    citystate: 'San Dimas, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'Sakura Sushi',
    rating: 4,
    type: 'Japanese, Fish, Sushi',
    street: '123 Blueberry Ln',
    citystate: 'Pomona, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'Prime Steakhouse',
    rating: 4,
    type: 'Steakhouses',
    street: '123 Blueberry Ln',
    citystate: 'Claremont, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'The Pizza Factory',
    rating: 4,
    type: 'Pizza',
    street: '123 Blueberry Ln',
    citystate: 'Pomona, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: "Emperor's Mongolian",
    rating: 5,
    type: 'Mongolian, Barbecue, Buffets',
    street: '123 Blueberry Ln',
    citystate: 'La Verne, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'Backyard Grill',
    rating: 3,
    type: 'American, Burgers',
    street: '123 Blueberry Ln',
    citystate: 'San Dimas, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'Taste Kitchen',
    rating: 4,
    type: 'American',
    street: '123 Blueberry Ln',
    citystate: 'Claremont, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'Bon Appetit',
    rating: 4,
    type: 'French',
    street: '123 Blueberry Ln',
    citystate: 'La Verne, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: "Sally's Sandwiches",
    rating: 3,
    type: 'Sandwiches, American',
    street: '123 Blueberry Ln',
    citystate: 'San Dimas, CA',
    phone: '(111) 111-1111',
    image: '',
  },
  {
    name: 'The HotPot Spot',
    rating: 5,
    type: 'Hot Pot, Japanese',
    street: '123 Blueberry Ln',
    citystate: 'Pomona, CA',
    phone: '(111) 111-1111',
    image: '',
  },
];

aria.FeedDisplay = function (feed, fetchData) {
  this.feed = feed;
  this.feedNode = feed.getFeedNode();
  this.fetchData = fetchData;
  this.currentPage = 0;
  this.perPage = 10;
  this.feedSize = 0;
  this.feedItems = [];
  this.loading = false;
  this.loadingDelay = 200; // in milliseconds

  this.lastChecked = Date.now();
  this.loadData();
  this.setupEvents();
};

aria.FeedDisplay.prototype.setDelay = function (delay) {
  this.loadingDelay = delay;
};

aria.FeedDisplay.prototype.loadData = function () {
  var feedData = this.fetchData(this.currentPage, this.perPage);

  if (!this.feedNode || this.loading) {
    return;
  }

  this.feedNode.setAttribute('aria-busy', 'true');
  this.loading = true;

  var loadingItems = [];

  Array.prototype.forEach.call(
    feedData,
    function (itemData) {
      var newFeedItem = this.renderItemData(itemData);
      loadingItems.push(newFeedItem);
    },
    this
  );

  this.delayRender(
    loadingItems,
    function () {
      this.feedNode.removeAttribute('aria-busy');
      this.loading = false;
      this.checkLoadMore();
    }.bind(this)
  );
};

aria.FeedDisplay.prototype.delayRender = function (items, onRenderDone) {
  if (!items || !items.length) {
    onRenderDone();
    return;
  }

  var newFeedItem = items.shift();
  this.feedNode.appendChild(newFeedItem);
  this.feedItems.push(newFeedItem);
  this.feed.addItem(newFeedItem);

  this.feedItems.forEach(function (feedItem) {
    feedItem.setAttribute('aria-setsize', this.feedItems.length);
  }, this);

  setTimeout(
    function () {
      this.delayRender(items, onRenderDone);
    }.bind(this),
    this.loadingDelay
  );
};

aria.FeedDisplay.prototype.renderItemData = function (itemData) {
  var feedItem = document.createElement('div');

  this.feedSize++;

  feedItem.setAttribute('role', 'article');
  feedItem.className = 'restaurant-item';
  feedItem.setAttribute('aria-posinset', this.feedSize);
  feedItem.setAttribute('tabindex', '0');

  var itemDetails = document.createElement('div');
  itemDetails.className = 'restaurant-details';
  var itemContent = '';
  var describedbyIDs = [];
  var restaurantID = 'restaurant-name-' + this.feedSize;
  feedItem.setAttribute('aria-labelledby', restaurantID);

  if (itemData.image) {
    itemContent += '<div class="restaurant-image">' + itemData.image + '</div>';
  }

  itemContent +=
    '<div class="restaurant-name" ' +
    'id="' +
    restaurantID +
    '">' +
    itemData.name +
    '</div>';

  if (itemData.rating) {
    const starText = `${itemData.rating} ${
      itemData.rating === 1 ? 'star' : 'stars'
    }`;
    var ratingID = 'restaurant-rating-' + this.feedSize;
    itemContent +=
      '<div class="restaurant-rating" id="' +
      ratingID +
      '" data-rating-value="' +
      itemData.rating +
      '">' +
      '<svg aria-label="' +
      starText +
      '" xmlns="http://www.w3.org/2000/svg" viewBox="38 -5 100 48" width="90" height="25">' +
      '<defs>' +
      '<g id="star">' +
      '<polygon points="2.0,13.4 11.7,20.5 8.0,31.1 17.7,24.8 27.4,31.9 23.7,20.5 33.4,13.4 21.4,13.4 17.7,2.0 14.0,13.4" />' +
      '</g>' +
      '</defs>' +
      '<g class="star-1 star-2 star-3 star-4 star-5">' +
      '<use class="star" xlink:href="#star" x="0" y="0" />' +
      '</g>' +
      '<g class="star-2 star-3 star-4 star-5">' +
      '<use class="star" xlink:href="#star" x="35" y="0" />' +
      '</g>' +
      '<g class="star-3 star-4 star-5">' +
      '<use class="star" xlink:href="#star" x="70" y="0" />' +
      '</g>' +
      '<g class="star-4 star-5">' +
      '<use class="star" xlink:href="#star" x="105" y="0" />' +
      '</g>' +
      '<g class="star-5">' +
      '<use class="star" xlink:href="#star" x="140" y="0" />' +
      '</g>' +
      '</svg>' +
      '</div>';
    describedbyIDs.push(ratingID);
  }

  if (itemData.type) {
    var typeID = 'restaurant-type-' + this.feedSize;
    itemContent +=
      '<div class="restaurant-type" id="' +
      typeID +
      '">' +
      itemData.type +
      '</div>';
    describedbyIDs.push(typeID);
  }

  itemDetails.innerHTML = itemContent;
  feedItem.appendChild(itemDetails);

  var locationBlock = document.createElement('div');
  var locationID = 'restaurant-location-' + this.feedSize;
  locationBlock.setAttribute('id', locationID);
  locationBlock.className = 'location-block';
  var locationContent = '';
  describedbyIDs.push(locationID);

  if (itemData.street) {
    locationContent +=
      '<div class="restaurant-street">' + itemData.street + '</div>';
  }

  if (itemData.citystate) {
    locationContent +=
      '<div class="restaurant-citystate">' + itemData.citystate + '</div>';
  }

  if (itemData.phone) {
    locationContent +=
      '<div class="restaurant-phone">' + itemData.phone + '</div>';
  }

  locationBlock.innerHTML = locationContent;
  feedItem.appendChild(locationBlock);

  feedItem.setAttribute('aria-describedby', describedbyIDs.join(' '));

  var actions = document.createElement('div');
  actions.className = 'restaurant-actions';
  actions.innerHTML =
    '<button type="button" class="bookmark-button">Bookmark</button>';
  feedItem.appendChild(actions);

  return feedItem;
};

aria.FeedDisplay.prototype.setupEvents = function () {
  window.addEventListener('scroll', this.handleScroll.bind(this));
};

aria.FeedDisplay.prototype.handleScroll = function () {
  var now = Date.now();

  if (this.lastChecked + 100 - now < 0) {
    this.checkLoadMore();
    this.lastChecked = now;
  }
};

aria.FeedDisplay.prototype.checkLoadMore = function () {
  if (!this.feedItems || !this.feedItems.length) {
    return;
  }

  var lastFeedItem = this.feedItems[this.feedItems.length - 1];
  var scrollTop =
    window.pageYOffset ||
    document.documentElement.scrollTop ||
    document.body.scrollTop ||
    0;
  var scrollBottom = scrollTop + window.innerHeight;

  if (scrollBottom >= lastFeedItem.offsetTop - 300) {
    this.loadData();
  }
};
