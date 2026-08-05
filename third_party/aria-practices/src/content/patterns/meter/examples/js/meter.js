'use strict';
var Meter = function (element) {
  this.rootEl = element;
  this.fillEl = element.querySelector('.fill');

  // set up min, max, and current value
  var min = element.getAttribute('aria-valuemin');
  var max = element.getAttribute('aria-valuemax');
  var value = element.getAttribute('aria-valuenow');
  this._update(parseFloat(min), parseFloat(max), parseFloat(value));
};

/* Private methods */

// returns a number representing a percentage between 0 - 100
Meter.prototype._calculatePercentFill = function (min, max, value) {
  if (
    typeof min !== 'number' ||
    typeof max !== 'number' ||
    typeof value !== 'number'
  ) {
    return 0;
  }

  return (100 * (value - min)) / (max - min);
};

// returns an hsl color string between red and green
Meter.prototype._getColorValue = function (percent) {
  // red is 0deg, green is 120deg in hsl
  // if percent is 100, hue should be red, and if percent is 0, hue should be green
  var hue = (percent / 100) * (0 - 120) + 120;

  return 'hsl(' + hue + ', 100%, 40%)';
};

// no return value; updates the meter element
Meter.prototype._update = function (min, max, value) {
  // update fill width
  if (min !== this.min || max !== this.max || value !== this.value) {
    var percentFill = this._calculatePercentFill(min, max, value);
    this.fillEl.style.width = percentFill + '%';
    this.fillEl.style.color = this._getColorValue(percentFill);
  }

  // update aria attributes
  if (min !== this.min) {
    this.min = min;
    this.rootEl.setAttribute('aria-valuemin', min + '');
  }

  if (max !== this.max) {
    this.max = max;
    this.rootEl.setAttribute('aria-valuemax', max + '');
  }

  if (value !== this.value) {
    this.value = value;
    this.rootEl.setAttribute('aria-valuenow', value + '');
  }
};

/* Public methods */

// no return value; modifies the meter element based on a new value
Meter.prototype.setValue = function (value) {
  if (typeof value !== 'number') {
    value = parseFloat(value);
  }

  if (!isNaN(value)) {
    this._update(this.min, this.max, value);
  }
};

/* Code for example page */

window.addEventListener('load', function () {
  // init meters
  var meterEls = document.querySelectorAll('[role=meter]');
  var meters = [];
  Array.prototype.slice.call(meterEls).forEach(function (meterEl) {
    meters.push(new Meter(meterEl));
  });

  // randomly update meter values

  // returns an id for setInterval
  function playMeters() {
    return window.setInterval(function () {
      meters.forEach(function (meter) {
        meter.setValue(Math.random() * 100);
      });
    }, 5000);
  }

  // start meters
  var updateInterval = playMeters();

  // play/pause meter updates
  var playButton = document.querySelector('.play-meters');
  playButton.addEventListener('click', function () {
    var isPaused = playButton.classList.contains('paused');

    if (isPaused) {
      updateInterval = playMeters();
      playButton.classList.remove('paused');
      playButton.innerHTML = 'Pause Updates';
    } else {
      clearInterval(updateInterval);
      playButton.classList.add('paused');
      playButton.innerHTML = 'Start Updates';
    }
  });
});
