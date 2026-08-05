/*
 *   File:   carousel-prev-next.js
 *
 *   Desc:   Carousel widget with Previous and Next Buttons that implements ARIA Authoring Practices
 *
 */

'use strict';

var CarouselPreviousNext = function (node, options) {
  // merge passed options with defaults
  options = Object.assign(
    { moreaccessible: false, paused: false, norotate: false },
    options || {}
  );

  // a prefers-reduced-motion user setting must always override autoplay
  var hasReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)');
  if (hasReducedMotion.matches) {
    options.paused = true;
  }

  /* DOM properties */
  this.domNode = node;

  this.carouselItemNodes = node.querySelectorAll('.carousel-item');

  this.containerNode = node.querySelector('.carousel-items');
  this.liveRegionNode = node.querySelector('.carousel-items');
  this.pausePlayButtonNode = null;
  this.previousButtonNode = null;
  this.nextButtonNode = null;

  this.playLabel = 'Start automatic slide show';
  this.pauseLabel = 'Stop automatic slide show';

  /* State properties */
  this.hasUserActivatedPlay = false; // set when the user activates the play/pause button
  this.isAutoRotationDisabled = options.norotate; // This property for disabling auto rotation
  this.isPlayingEnabled = !options.paused; // This property is also set in updatePlaying method
  this.timeInterval = 5000; // length of slide rotation in ms
  this.currentIndex = 0; // index of current slide
  this.slideTimeout = null; // save reference to setTimeout

  // Pause Button

  var elem = document.querySelector('.carousel .controls button.rotation');
  if (elem) {
    this.pausePlayButtonNode = elem;
    this.pausePlayButtonNode.addEventListener(
      'click',
      this.handlePausePlayButtonClick.bind(this)
    );
  }

  // Previous Button

  elem = document.querySelector('.carousel .controls button.previous');
  if (elem) {
    this.previousButtonNode = elem;
    this.previousButtonNode.addEventListener(
      'click',
      this.handlePreviousButtonClick.bind(this)
    );
    this.previousButtonNode.addEventListener(
      'focus',
      this.handleFocusIn.bind(this)
    );
    this.previousButtonNode.addEventListener(
      'blur',
      this.handleFocusOut.bind(this)
    );
  }

  // Next Button

  elem = document.querySelector('.carousel .controls button.next');
  if (elem) {
    this.nextButtonNode = elem;
    this.nextButtonNode.addEventListener(
      'click',
      this.handleNextButtonClick.bind(this)
    );
    this.nextButtonNode.addEventListener(
      'focus',
      this.handleFocusIn.bind(this)
    );
    this.nextButtonNode.addEventListener(
      'blur',
      this.handleFocusOut.bind(this)
    );
  }

  // Carousel item events

  for (var i = 0; i < this.carouselItemNodes.length; i++) {
    var carouselItemNode = this.carouselItemNodes[i];

    // support stopping rotation when any element receives focus in the tabpanel
    carouselItemNode.addEventListener('focusin', this.handleFocusIn.bind(this));
    carouselItemNode.addEventListener(
      'focusout',
      this.handleFocusOut.bind(this)
    );

    var imageLinkNode = carouselItemNode.querySelector('.carousel-image a');

    if (imageLinkNode) {
      imageLinkNode.addEventListener(
        'focus',
        this.handleImageLinkFocus.bind(this)
      );
      imageLinkNode.addEventListener(
        'blur',
        this.handleImageLinkBlur.bind(this)
      );
    }
  }

  // Handle hover events
  this.domNode.addEventListener('mouseover', this.handleMouseOver.bind(this));
  this.domNode.addEventListener('mouseout', this.handleMouseOut.bind(this));

  // initialize behavior based on options

  this.enableOrDisableAutoRotation(options.norotate);
  this.updatePlaying(!options.paused && !options.norotate);
  this.setAccessibleStyling(options.moreaccessible);
  this.rotateSlides();
};

/* Public function to disable/enable rotation and if false, hide pause/play button*/
CarouselPreviousNext.prototype.enableOrDisableAutoRotation = function (
  disable
) {
  this.isAutoRotationDisabled = disable;
  this.pausePlayButtonNode.hidden = disable;
};

/* Public function to update controls/caption styling */
CarouselPreviousNext.prototype.setAccessibleStyling = function (accessible) {
  if (accessible) {
    this.domNode.classList.add('carousel-moreaccessible');
  } else {
    this.domNode.classList.remove('carousel-moreaccessible');
  }
};

CarouselPreviousNext.prototype.showCarouselItem = function (index) {
  this.currentIndex = index;

  for (var i = 0; i < this.carouselItemNodes.length; i++) {
    var carouselItemNode = this.carouselItemNodes[i];
    if (index === i) {
      carouselItemNode.classList.add('active');
    } else {
      carouselItemNode.classList.remove('active');
    }
  }
};

CarouselPreviousNext.prototype.previousCarouselItem = function () {
  var nextIndex = this.currentIndex - 1;
  if (nextIndex < 0) {
    nextIndex = this.carouselItemNodes.length - 1;
  }
  this.showCarouselItem(nextIndex);
};

CarouselPreviousNext.prototype.nextCarouselItem = function () {
  var nextIndex = this.currentIndex + 1;
  if (nextIndex >= this.carouselItemNodes.length) {
    nextIndex = 0;
  }
  this.showCarouselItem(nextIndex);
};

CarouselPreviousNext.prototype.rotateSlides = function () {
  if (!this.isAutoRotationDisabled) {
    if (
      (!this.hasFocus && !this.hasHover && this.isPlayingEnabled) ||
      this.hasUserActivatedPlay
    ) {
      this.nextCarouselItem();
    }
  }

  this.slideTimeout = setTimeout(
    this.rotateSlides.bind(this),
    this.timeInterval
  );
};

CarouselPreviousNext.prototype.updatePlaying = function (play) {
  this.isPlayingEnabled = play;

  if (play) {
    this.pausePlayButtonNode.setAttribute('aria-label', this.pauseLabel);
    this.pausePlayButtonNode.classList.remove('play');
    this.pausePlayButtonNode.classList.add('pause');
    this.liveRegionNode.setAttribute('aria-live', 'off');
  } else {
    this.pausePlayButtonNode.setAttribute('aria-label', this.playLabel);
    this.pausePlayButtonNode.classList.remove('pause');
    this.pausePlayButtonNode.classList.add('play');
    this.liveRegionNode.setAttribute('aria-live', 'polite');
  }
};

/* Event Handlers */

CarouselPreviousNext.prototype.handleImageLinkFocus = function () {
  this.liveRegionNode.classList.add('focus');
};

CarouselPreviousNext.prototype.handleImageLinkBlur = function () {
  this.liveRegionNode.classList.remove('focus');
};

CarouselPreviousNext.prototype.handleMouseOver = function (event) {
  if (!this.pausePlayButtonNode.contains(event.target)) {
    this.hasHover = true;
  }
};

CarouselPreviousNext.prototype.handleMouseOut = function () {
  this.hasHover = false;
};

/* EVENT HANDLERS */

CarouselPreviousNext.prototype.handlePausePlayButtonClick = function () {
  this.hasUserActivatedPlay = !this.isPlayingEnabled;
  this.updatePlaying(!this.isPlayingEnabled);
};

CarouselPreviousNext.prototype.handlePreviousButtonClick = function () {
  this.previousCarouselItem();
};

CarouselPreviousNext.prototype.handleNextButtonClick = function () {
  this.nextCarouselItem();
};

/* Event Handlers for carousel items*/

CarouselPreviousNext.prototype.handleFocusIn = function () {
  this.liveRegionNode.setAttribute('aria-live', 'polite');
  this.hasFocus = true;
};

CarouselPreviousNext.prototype.handleFocusOut = function () {
  if (this.isPlayingEnabled) {
    this.liveRegionNode.setAttribute('aria-live', 'off');
  }
  this.hasFocus = false;
};

/* Initialize Carousel and options */

window.addEventListener(
  'load',
  function () {
    var carouselEls = document.querySelectorAll('.carousel');
    var carousels = [];

    // set example behavior based on
    // default setting of the checkboxes and the parameters in the URL
    // update checkboxes based on any corresponding URL parameters
    var checkboxes = document.querySelectorAll(
      '.carousel-options input[type=checkbox]'
    );
    var urlParams = new URLSearchParams(location.search);
    var carouselOptions = {};

    // initialize example features based on
    // default setting of the checkboxes and the parameters in the URL
    // update checkboxes based on any corresponding URL parameters
    checkboxes.forEach(function (checkbox) {
      var checked = checkbox.checked;

      if (urlParams.has(checkbox.value)) {
        var urlParam = urlParams.get(checkbox.value);
        if (typeof urlParam === 'string') {
          checked = urlParam === 'true';
          checkbox.checked = checked;
        }
      }

      carouselOptions[checkbox.value] = checkbox.checked;
    });

    carouselEls.forEach(function (node) {
      carousels.push(new CarouselPreviousNext(node, carouselOptions));
    });

    // add change event to checkboxes
    checkboxes.forEach(function (checkbox) {
      var updateEvent;
      switch (checkbox.value) {
        case 'moreaccessible':
          updateEvent = 'setAccessibleStyling';
          break;
        case 'norotate':
          updateEvent = 'enableOrDisableAutoRotation';
          break;
      }

      // update the carousel behavior and URL when a checkbox state changes
      checkbox.addEventListener('change', function (event) {
        urlParams.set(event.target.value, event.target.checked + '');
        window.history.replaceState(
          null,
          '',
          window.location.pathname + '?' + urlParams
        );

        if (updateEvent) {
          carousels.forEach(function (carousel) {
            carousel[updateEvent](event.target.checked);
          });
        }
      });
    });
  },
  false
);
