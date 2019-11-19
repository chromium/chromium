'use strict';
/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/**
 * @enum {number}
 */
var WeekDay = {
  Sunday: 0,
  Monday: 1,
  Tuesday: 2,
  Wednesday: 3,
  Thursday: 4,
  Friday: 5,
  Saturday: 6
};

/**
 * @type {Object}
 */
var global = {
  picker: null,
  params: {
    locale: 'en-US',
    weekStartDay: WeekDay.Sunday,
    dayLabels: ['S', 'M', 'T', 'W', 'T', 'F', 'S'],
    ampmLabels: ['AM', 'PM'],
    shortMonthLabels: [
      'Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sept', 'Oct',
      'Nov', 'Dec'
    ],
    isLocaleRTL: false,
    isFormControlsRefreshEnabled: false,
    mode: 'date',
    isAMPMFirst: false,
    hasAMPM: false,
    hasSecond: false,
    hasMillisecond: false,
    weekLabel: 'Week',
    anchorRectInScreen: new Rectangle(0, 0, 0, 0),
    currentValue: null
  }
};

// ----------------------------------------------------------------
// Utility functions

/**
 * @return {!boolean}
 */
function hasInaccuratePointingDevice() {
  return matchMedia('(any-pointer: coarse)').matches;
}

/**
 * @return {!string} lowercase locale name. e.g. "en-us"
 */
function getLocale() {
  return (global.params.locale || 'en-us').toLowerCase();
}

/**
 * @return {!string} lowercase language code. e.g. "en"
 */
function getLanguage() {
  var locale = getLocale();
  var result = locale.match(/^([a-z]+)/);
  if (!result)
    return 'en';
  return result[1];
}

/**
 * @param {!number} number
 * @return {!string}
 */
function localizeNumber(number) {
  return window.pagePopupController.localizeNumberString(number);
}

/**
 * @type {Intl.DateTimeFormat}
 */
let japaneseEraFormatter = null;

/**
 * @param {!number} year
 * @param {!number} month
 * @return {!string}
 */
function formatJapaneseImperialEra(year, month) {
  // Eras prior to Meiji are not helpful.
  if (year <= 1867 || year == 1868 && month <= 9)
    return '';
  if (!japaneseEraFormatter) {
    japaneseEraFormatter = new Intl.DateTimeFormat(
        'ja-JP-u-ca-japanese', {era: 'long', year: 'numeric'});
  }
  // Produce the era for day 16 because it's almost the midpoint of a month.
  // 275760-09-13 is the last valid date in ECMAScript. We apply day 7 in that
  // case because it's the midpoint between 09-01 and 09-13.
  let sampleDay = year == 275760 && month == 8 ? 7 : 16;
  let yearPart = japaneseEraFormatter.format(new Date(year, month, sampleDay));

  // We don't show an imperial era if it is greater than 99 because of space
  // limitation.
  if (yearPart.length > 5)
    return '';

  // Replace 1-nen with Gan-nen.
  if (yearPart.length == 4 && yearPart[2] == '1')
    yearPart = yearPart.substring(0, 2) + '\u5143\u5e74';

  return '(' + yearPart + ')';
}

function createUTCDate(year, month, date) {
  var newDate = new Date(0);
  newDate.setUTCFullYear(year);
  newDate.setUTCMonth(month);
  newDate.setUTCDate(date);
  return newDate;
}

/**
 * @param {string} dateString
 * @return {?Day|Week|Month}
 */
function parseDateString(dateString) {
  var month = Month.parse(dateString);
  if (month)
    return month;
  var week = Week.parse(dateString);
  if (week)
    return week;
  return Day.parse(dateString);
}

/**
 * @const
 * @type {number}
 */
var DaysPerWeek = 7;

/**
 * @const
 * @type {number}
 */
var MonthsPerYear = 12;

/**
 * @const
 * @type {number}
 */
var MillisecondsPerDay = 24 * 60 * 60 * 1000;

/**
 * @const
 * @type {number}
 */
var MillisecondsPerWeek = DaysPerWeek * MillisecondsPerDay;

/**
 * @constructor
 */
function DateType() {
}

/**
 * @constructor
 * @extends DateType
 * @param {!number} year
 * @param {!number} month
 * @param {!number} date
 */
function Day(year, month, date) {
  var dateObject = createUTCDate(year, month, date);
  if (isNaN(dateObject.valueOf()))
    throw 'Invalid date';
  /**
   * @type {number}
   * @const
   */
  this.year = dateObject.getUTCFullYear();
  /**
   * @type {number}
   * @const
   */
  this.month = dateObject.getUTCMonth();
  /**
   * @type {number}
   * @const
   */
  this.date = dateObject.getUTCDate();
};

Day.prototype = Object.create(DateType.prototype);

Day.ISOStringRegExp = /^(\d+)-(\d+)-(\d+)/;

/**
 * @param {!string} str
 * @return {?Day}
 */
Day.parse = function(str) {
  var match = Day.ISOStringRegExp.exec(str);
  if (!match)
    return null;
  var year = parseInt(match[1], 10);
  var month = parseInt(match[2], 10) - 1;
  var date = parseInt(match[3], 10);
  return new Day(year, month, date);
};

/**
 * @param {!number} value
 * @return {!Day}
 */
Day.createFromValue = function(millisecondsSinceEpoch) {
  return Day.createFromDate(new Date(millisecondsSinceEpoch))
};

/**
 * @param {!Date} date
 * @return {!Day}
 */
Day.createFromDate = function(date) {
  if (isNaN(date.valueOf()))
    throw 'Invalid date';
  return new Day(date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate());
};

/**
 * @param {!Day} day
 * @return {!Day}
 */
Day.createFromDay = function(day) {
  return day;
};

/**
 * @return {!Day}
 */
Day.createFromToday = function() {
  var now = new Date();
  return new Day(now.getFullYear(), now.getMonth(), now.getDate());
};

/**
 * @param {!DateType} other
 * @return {!boolean}
 */
Day.prototype.equals = function(other) {
  return other instanceof Day && this.year === other.year &&
      this.month === other.month && this.date === other.date;
};

/**
 * @param {!number=} offset
 * @return {!Day}
 */
Day.prototype.previous = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Day(this.year, this.month, this.date - offset);
};

/**
 * @param {!number=} offset
 * @return {!Day}
 */
Day.prototype.next = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Day(this.year, this.month, this.date + offset);
};

/**
 * @return {!Date}
 */
Day.prototype.startDate = function() {
  return createUTCDate(this.year, this.month, this.date);
};

/**
 * @return {!Date}
 */
Day.prototype.endDate = function() {
  return createUTCDate(this.year, this.month, this.date + 1);
};

/**
 * @return {!Day}
 */
Day.prototype.firstDay = function() {
  return this;
};

/**
 * @return {!Day}
 */
Day.prototype.middleDay = function() {
  return this;
};

/**
 * @return {!Day}
 */
Day.prototype.lastDay = function() {
  return this;
};

/**
 * @return {!number}
 */
Day.prototype.valueOf = function() {
  return createUTCDate(this.year, this.month, this.date).getTime();
};

/**
 * @return {!WeekDay}
 */
Day.prototype.weekDay = function() {
  return createUTCDate(this.year, this.month, this.date).getUTCDay();
};

/**
 * @return {!string}
 */
Day.prototype.toString = function() {
  var yearString = String(this.year);
  if (yearString.length < 4)
    yearString = ('000' + yearString).substr(-4, 4);
  return yearString + '-' + ('0' + (this.month + 1)).substr(-2, 2) + '-' +
      ('0' + this.date).substr(-2, 2);
};

/**
 * @return {!string}
 */
Day.prototype.format = function() {
  if (!Day.formatter) {
    Day.formatter = new Intl.DateTimeFormat(getLocale(), {
      weekday: 'long',
      year: 'numeric',
      month: 'long',
      day: 'numeric',
      timeZone: 'UTC'
    });
  }
  return Day.formatter.format(this.startDate());
};

// See platform/text/date_components.h.
Day.Minimum = Day.createFromValue(-62135596800000.0);
Day.Maximum = Day.createFromValue(8640000000000000.0);

// See core/html/forms/date_input_type.cc.
Day.DefaultStep = 86400000;
Day.DefaultStepBase = 0;

/**
 * @constructor
 * @extends DateType
 * @param {!number} year
 * @param {!number} week
 */
function Week(year, week) {
  /**
   * @type {number}
   * @const
   */
  this.year = year;
  /**
   * @type {number}
   * @const
   */
  this.week = week;
  // Number of years per year is either 52 or 53.
  if (this.week < 1 ||
      (this.week > 52 && this.week > Week.numberOfWeeksInYear(this.year))) {
    var normalizedWeek = Week.createFromDay(this.firstDay());
    this.year = normalizedWeek.year;
    this.week = normalizedWeek.week;
  }
}

Week.ISOStringRegExp = /^(\d+)-[wW](\d+)$/;

// See platform/text/date_components.h.
Week.Minimum = new Week(1, 1);
Week.Maximum = new Week(275760, 37);

// See core/html/forms/week_input_type.cc.
Week.DefaultStep = 604800000;
Week.DefaultStepBase = -259200000;

Week.EpochWeekDay = createUTCDate(1970, 0, 0).getUTCDay();

/**
 * @param {!string} str
 * @return {?Week}
 */
Week.parse = function(str) {
  var match = Week.ISOStringRegExp.exec(str);
  if (!match)
    return null;
  var year = parseInt(match[1], 10);
  var week = parseInt(match[2], 10);
  return new Week(year, week);
};

/**
 * @param {!number} millisecondsSinceEpoch
 * @return {!Week}
 */
Week.createFromValue = function(millisecondsSinceEpoch) {
  return Week.createFromDate(new Date(millisecondsSinceEpoch))
};

/**
 * @param {!Date} date
 * @return {!Week}
 */
Week.createFromDate = function(date) {
  if (isNaN(date.valueOf()))
    throw 'Invalid date';
  var year = date.getUTCFullYear();
  if (year <= Week.Maximum.year &&
      Week.weekOneStartDateForYear(year + 1).getTime() <= date.getTime())
    year++;
  else if (
      year > 1 && Week.weekOneStartDateForYear(year).getTime() > date.getTime())
    year--;
  var week = 1 +
      Week._numberOfWeeksSinceDate(Week.weekOneStartDateForYear(year), date);
  return new Week(year, week);
};

/**
 * @param {!Day} day
 * @return {!Week}
 */
Week.createFromDay = function(day) {
  var year = day.year;
  if (year <= Week.Maximum.year && Week.weekOneStartDayForYear(year + 1) <= day)
    year++;
  else if (year > 1 && Week.weekOneStartDayForYear(year) > day)
    year--;
  var week = Math.floor(
      1 +
      (day.valueOf() - Week.weekOneStartDayForYear(year).valueOf()) /
          MillisecondsPerWeek);
  return new Week(year, week);
};

/**
 * @return {!Week}
 */
Week.createFromToday = function() {
  var now = new Date();
  return Week.createFromDate(
      createUTCDate(now.getFullYear(), now.getMonth(), now.getDate()));
};

/**
 * @param {!number} year
 * @return {!Date}
 */
Week.weekOneStartDateForYear = function(year) {
  if (year < 1)
    return createUTCDate(1, 0, 1);
  // The week containing January 4th is week one.
  var yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
  return createUTCDate(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
};

/**
 * @param {!number} year
 * @return {!Day}
 */
Week.weekOneStartDayForYear = function(year) {
  if (year < 1)
    return Day.Minimum;
  // The week containing January 4th is week one.
  var yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
  return new Day(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
};

/**
 * @param {!number} year
 * @return {!number}
 */
Week.numberOfWeeksInYear = function(year) {
  if (year < 1 || year > Week.Maximum.year)
    return 0;
  else if (year === Week.Maximum.year)
    return Week.Maximum.week;
  return Week._numberOfWeeksSinceDate(
      Week.weekOneStartDateForYear(year),
      Week.weekOneStartDateForYear(year + 1));
};

/**
 * @param {!Date} baseDate
 * @param {!Date} date
 * @return {!number}
 */
Week._numberOfWeeksSinceDate = function(baseDate, date) {
  return Math.floor(
      (date.getTime() - baseDate.getTime()) / MillisecondsPerWeek);
};

/**
 * @param {!DateType} other
 * @return {!boolean}
 */
Week.prototype.equals = function(other) {
  return other instanceof Week && this.year === other.year &&
      this.week === other.week;
};

/**
 * @param {!number=} offset
 * @return {!Week}
 */
Week.prototype.previous = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Week(this.year, this.week - offset);
};

/**
 * @param {!number=} offset
 * @return {!Week}
 */
Week.prototype.next = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Week(this.year, this.week + offset);
};

/**
 * @return {!Date}
 */
Week.prototype.startDate = function() {
  var weekStartDate = Week.weekOneStartDateForYear(this.year);
  weekStartDate.setUTCDate(weekStartDate.getUTCDate() + (this.week - 1) * 7);
  return weekStartDate;
};

/**
 * @return {!Date}
 */
Week.prototype.endDate = function() {
  if (this.equals(Week.Maximum))
    return Day.Maximum.startDate();
  return this.next().startDate();
};

/**
 * @return {!Day}
 */
Week.prototype.firstDay = function() {
  var weekOneStartDay = Week.weekOneStartDayForYear(this.year);
  return weekOneStartDay.next((this.week - 1) * DaysPerWeek);
};

/**
 * @return {!Day}
 */
Week.prototype.middleDay = function() {
  return this.firstDay().next(3);
};

/**
 * @return {!Day}
 */
Week.prototype.lastDay = function() {
  if (this.equals(Week.Maximum))
    return Day.Maximum;
  return this.next().firstDay().previous();
};

/**
 * @return {!number}
 */
Week.prototype.valueOf = function() {
  return this.firstDay().valueOf() - createUTCDate(1970, 0, 1).getTime();
};

/**
 * @return {!string}
 */
Week.prototype.toString = function() {
  var yearString = String(this.year);
  if (yearString.length < 4)
    yearString = ('000' + yearString).substr(-4, 4);
  return yearString + '-W' + ('0' + this.week).substr(-2, 2);
};

/**
 * @constructor
 * @extends DateType
 * @param {!number} year
 * @param {!number} month
 */
function Month(year, month) {
  /**
   * @type {number}
   * @const
   */
  this.year = year + Math.floor(month / MonthsPerYear);
  /**
   * @type {number}
   * @const
   */
  this.month = month % MonthsPerYear < 0 ?
      month % MonthsPerYear + MonthsPerYear :
      month % MonthsPerYear;
};

Month.ISOStringRegExp = /^(\d+)-(\d+)$/;

// See platform/text/date_components.h.
Month.Minimum = new Month(1, 0);
Month.Maximum = new Month(275760, 8);

// See core/html/forms/month_input_type.cc.
Month.DefaultStep = 1;
Month.DefaultStepBase = 0;

/**
 * @param {!string} str
 * @return {?Month}
 */
Month.parse = function(str) {
  var match = Month.ISOStringRegExp.exec(str);
  if (!match)
    return null;
  var year = parseInt(match[1], 10);
  var month = parseInt(match[2], 10) - 1;
  return new Month(year, month);
};

/**
 * @param {!number} value
 * @return {!Month}
 */
Month.createFromValue = function(monthsSinceEpoch) {
  return new Month(1970, monthsSinceEpoch)
};

/**
 * @param {!Date} date
 * @return {!Month}
 */
Month.createFromDate = function(date) {
  if (isNaN(date.valueOf()))
    throw 'Invalid date';
  return new Month(date.getUTCFullYear(), date.getUTCMonth());
};

/**
 * @param {!Day} day
 * @return {!Month}
 */
Month.createFromDay = function(day) {
  return new Month(day.year, day.month);
};

/**
 * @return {!Month}
 */
Month.createFromToday = function() {
  var now = new Date();
  return new Month(now.getFullYear(), now.getMonth());
};

/**
 * @return {!boolean}
 */
Month.prototype.containsDay = function(day) {
  return this.year === day.year && this.month === day.month;
};

/**
 * @param {!Month} other
 * @return {!boolean}
 */
Month.prototype.equals = function(other) {
  return other instanceof Month && this.year === other.year &&
      this.month === other.month;
};

/**
 * @param {!number=} offset
 * @return {!Month}
 */
Month.prototype.previous = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Month(this.year, this.month - offset);
};

/**
 * @param {!number=} offset
 * @return {!Month}
 */
Month.prototype.next = function(offset) {
  if (typeof offset === 'undefined')
    offset = 1;
  return new Month(this.year, this.month + offset);
};

/**
 * @return {!Date}
 */
Month.prototype.startDate = function() {
  return createUTCDate(this.year, this.month, 1);
};

/**
 * @return {!Date}
 */
Month.prototype.endDate = function() {
  if (this.equals(Month.Maximum))
    return Day.Maximum.startDate();
  return this.next().startDate();
};

/**
 * @return {!Day}
 */
Month.prototype.firstDay = function() {
  return new Day(this.year, this.month, 1);
};

/**
 * @return {!Day}
 */
Month.prototype.middleDay = function() {
  return new Day(this.year, this.month, this.month === 2 ? 14 : 15);
};

/**
 * @return {!Day}
 */
Month.prototype.lastDay = function() {
  if (this.equals(Month.Maximum))
    return Day.Maximum;
  return this.next().firstDay().previous();
};

/**
 * @return {!number}
 */
Month.prototype.valueOf = function() {
  return (this.year - 1970) * MonthsPerYear + this.month;
};

/**
 * @return {!string}
 */
Month.prototype.toString = function() {
  var yearString = String(this.year);
  if (yearString.length < 4)
    yearString = ('000' + yearString).substr(-4, 4);
  return yearString + '-' + ('0' + (this.month + 1)).substr(-2, 2);
};

/**
 * @return {!string}
 */
Month.prototype.toLocaleString = function() {
  if (global.params.locale === 'ja')
    return '' + this.year + '\u5e74' +
        formatJapaneseImperialEra(this.year, this.month) + ' ' +
        (this.month + 1) + '\u6708';
  return window.pagePopupController.formatMonth(this.year, this.month);
};

/**
 * @return {!string}
 */
Month.prototype.toShortLocaleString = function() {
  return window.pagePopupController.formatShortMonth(this.year, this.month);
};

// ----------------------------------------------------------------
// Initialization

/**
 * @param {Event} event
 */
function handleMessage(event) {
  if (global.argumentsReceived)
    return;
  global.argumentsReceived = true;
  initialize(JSON.parse(event.data));
}

/**
 * @param {!Object} params
 */
function setGlobalParams(params) {
  var name;
  for (name in global.params) {
    if (typeof params[name] === 'undefined')
      console.warn('Missing argument: ' + name);
  }
  for (name in params) {
    global.params[name] = params[name];
  }
};

/**
 * @param {!Object} args
 */
function initialize(args) {
  setGlobalParams(args);
  if (global.params.suggestionValues && global.params.suggestionValues.length)
    openSuggestionPicker();
  else
    openCalendarPicker();
}

function closePicker() {
  if (global.picker)
    global.picker.cleanup();
  var main = $('main');
  main.innerHTML = '';
  main.className = '';
};

function openSuggestionPicker() {
  closePicker();
  if (global.params.isFormControlsRefreshEnabled) {
    document.body.classList.add('controls-refresh');
  }
  global.picker = new SuggestionPicker($('main'), global.params);
};

function openCalendarPicker() {
  closePicker();
  if (global.params.isFormControlsRefreshEnabled) {
    if (global.params.mode == 'month') {
      return initializeMonthPicker(global.params);
    } else if (global.params.mode == 'time') {
      return initializeTimePicker(global.params);
    } else if (global.params.mode == 'datetime-local') {
      return initializeDateTimeLocalPicker(global.params);
    }
  }

  global.picker = new CalendarPicker(global.params.mode, global.params);
  global.picker.attachTo($('main'));
};

// Parameter t should be a number between 0 and 1.
var AnimationTimingFunction = {
  Linear: function(t) {
    return t;
  },
  EaseInOut: function(t) {
    t *= 2;
    if (t < 1)
      return Math.pow(t, 3) / 2;
    t -= 2;
    return Math.pow(t, 3) / 2 + 1;
  }
};

/**
 * @constructor
 * @extends EventEmitter
 */
function AnimationManager() {
  EventEmitter.call(this);

  this._isRunning = false;
  this._runningAnimatorCount = 0;
  this._runningAnimators = {};
  this._animationFrameCallbackBound = this._animationFrameCallback.bind(this);
}

AnimationManager.prototype = Object.create(EventEmitter.prototype);

AnimationManager.EventTypeAnimationFrameWillFinish = 'animationFrameWillFinish';

AnimationManager.prototype._startAnimation = function() {
  if (this._isRunning)
    return;
  this._isRunning = true;
  window.requestAnimationFrame(this._animationFrameCallbackBound);
};

AnimationManager.prototype._stopAnimation = function() {
  if (!this._isRunning)
    return;
  this._isRunning = false;
};

/**
 * @param {!Animator} animator
 */
AnimationManager.prototype.add = function(animator) {
  if (this._runningAnimators[animator.id])
    return;
  this._runningAnimators[animator.id] = animator;
  this._runningAnimatorCount++;
  if (this._needsTimer())
    this._startAnimation();
};

/**
 * @param {!Animator} animator
 */
AnimationManager.prototype.remove = function(animator) {
  if (!this._runningAnimators[animator.id])
    return;
  delete this._runningAnimators[animator.id];
  this._runningAnimatorCount--;
  if (!this._needsTimer())
    this._stopAnimation();
};

AnimationManager.prototype._animationFrameCallback = function(now) {
  if (this._runningAnimatorCount > 0) {
    for (var id in this._runningAnimators) {
      this._runningAnimators[id].onAnimationFrame(now);
    }
  }
  this.dispatchEvent(AnimationManager.EventTypeAnimationFrameWillFinish);
  if (this._isRunning)
    window.requestAnimationFrame(this._animationFrameCallbackBound);
};

/**
 * @return {!boolean}
 */
AnimationManager.prototype._needsTimer = function() {
  return this._runningAnimatorCount > 0 ||
      this.hasListener(AnimationManager.EventTypeAnimationFrameWillFinish);
};

/**
 * @param {!string} type
 * @param {!Function} callback
 * @override
 */
AnimationManager.prototype.on = function(type, callback) {
  EventEmitter.prototype.on.call(this, type, callback);
  if (this._needsTimer())
    this._startAnimation();
};

/**
 * @param {!string} type
 * @param {!Function} callback
 * @override
 */
AnimationManager.prototype.removeListener = function(type, callback) {
  EventEmitter.prototype.removeListener.call(this, type, callback);
  if (!this._needsTimer())
    this._stopAnimation();
};

AnimationManager.shared = new AnimationManager();

/**
 * @constructor
 * @extends EventEmitter
 */
function Animator() {
  EventEmitter.call(this);

  /**
   * @type {!number}
   * @const
   */
  this.id = Animator._lastId++;
  /**
   * @type {!number}
   */
  this.duration = 100;
  /**
   * @type {?function}
   */
  this.step = null;
  /**
   * @type {!boolean}
   * @protected
   */
  this._isRunning = false;
  /**
   * @type {!number}
   */
  this.currentValue = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._lastStepTime = 0;
}

Animator.prototype = Object.create(EventEmitter.prototype);

Animator._lastId = 0;

Animator.EventTypeDidAnimationStop = 'didAnimationStop';

/**
 * @return {!boolean}
 */
Animator.prototype.isRunning = function() {
  return this._isRunning;
};

Animator.prototype.start = function() {
  this._lastStepTime = performance.now();
  this._isRunning = true;
  AnimationManager.shared.add(this);
};

Animator.prototype.stop = function() {
  if (!this._isRunning)
    return;
  this._isRunning = false;
  AnimationManager.shared.remove(this);
  this.dispatchEvent(Animator.EventTypeDidAnimationStop, this);
};

/**
 * @param {!number} now
 */
Animator.prototype.onAnimationFrame = function(now) {
  this._lastStepTime = now;
  this.step(this);
};

/**
 * @constructor
 * @extends Animator
 */
function TransitionAnimator() {
  Animator.call(this);
  /**
   * @type {!number}
   * @protected
   */
  this._from = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._to = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._delta = 0;
  /**
   * @type {!number}
   */
  this.progress = 0.0;
  /**
   * @type {!function}
   */
  this.timingFunction = AnimationTimingFunction.Linear;
}

TransitionAnimator.prototype = Object.create(Animator.prototype);

/**
 * @param {!number} value
 */
TransitionAnimator.prototype.setFrom = function(value) {
  this._from = value;
  this._delta = this._to - this._from;
};

TransitionAnimator.prototype.start = function() {
  console.assert(isFinite(this.duration));
  this.progress = 0.0;
  this.currentValue = this._from;
  Animator.prototype.start.call(this);
};

/**
 * @param {!number} value
 */
TransitionAnimator.prototype.setTo = function(value) {
  this._to = value;
  this._delta = this._to - this._from;
};

/**
 * @param {!number} now
 */
TransitionAnimator.prototype.onAnimationFrame = function(now) {
  this.progress += (now - this._lastStepTime) / this.duration;
  this.progress = Math.min(1.0, this.progress);
  this._lastStepTime = now;
  this.currentValue =
      this.timingFunction(this.progress) * this._delta + this._from;
  this.step(this);
  if (this.progress === 1.0) {
    this.stop();
    return;
  }
};

/**
 * @constructor
 * @extends Animator
 * @param {!number} initialVelocity
 * @param {!number} initialValue
 */
function FlingGestureAnimator(initialVelocity, initialValue) {
  Animator.call(this);
  /**
   * @type {!number}
   */
  this.initialVelocity = initialVelocity;
  /**
   * @type {!number}
   */
  this.initialValue = initialValue;
  /**
   * @type {!number}
   * @protected
   */
  this._elapsedTime = 0;
  var startVelocity = Math.abs(this.initialVelocity);
  if (startVelocity > this._velocityAtTime(0))
    startVelocity = this._velocityAtTime(0);
  if (startVelocity < 0)
    startVelocity = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._timeOffset = this._timeAtVelocity(startVelocity);
  /**
   * @type {!number}
   * @protected
   */
  this._positionOffset = this._valueAtTime(this._timeOffset);
  /**
   * @type {!number}
   */
  this.duration = this._timeAtVelocity(0);
}

FlingGestureAnimator.prototype = Object.create(Animator.prototype);

// Velocity is subject to exponential decay. These parameters are coefficients
// that determine the curve.
FlingGestureAnimator._P0 = -5707.62;
FlingGestureAnimator._P1 = 0.172;
FlingGestureAnimator._P2 = 0.0037;

/**
 * @param {!number} t
 */
FlingGestureAnimator.prototype._valueAtTime = function(t) {
  return FlingGestureAnimator._P0 * Math.exp(-FlingGestureAnimator._P2 * t) -
      FlingGestureAnimator._P1 * t - FlingGestureAnimator._P0;
};

/**
 * @param {!number} t
 */
FlingGestureAnimator.prototype._velocityAtTime = function(t) {
  return -FlingGestureAnimator._P0 * FlingGestureAnimator._P2 *
      Math.exp(-FlingGestureAnimator._P2 * t) -
      FlingGestureAnimator._P1;
};

/**
 * @param {!number} v
 */
FlingGestureAnimator.prototype._timeAtVelocity = function(v) {
  return -Math.log(
             (v + FlingGestureAnimator._P1) /
             (-FlingGestureAnimator._P0 * FlingGestureAnimator._P2)) /
      FlingGestureAnimator._P2;
};

FlingGestureAnimator.prototype.start = function() {
  this._lastStepTime = performance.now();
  Animator.prototype.start.call(this);
};

/**
 * @param {!number} now
 */
FlingGestureAnimator.prototype.onAnimationFrame = function(now) {
  this._elapsedTime += now - this._lastStepTime;
  this._lastStepTime = now;
  if (this._elapsedTime + this._timeOffset >= this.duration) {
    this.stop();
    return;
  }
  var position = this._valueAtTime(this._elapsedTime + this._timeOffset) -
      this._positionOffset;
  if (this.initialVelocity < 0)
    position = -position;
  this.currentValue = position + this.initialValue;
  this.step(this);
};

/**
 * @constructor
 * @extends EventEmitter
 * @param {?Element} element
 * View adds itself as a property on the element so we can access it from Event.target.
 */
function View(element) {
  EventEmitter.call(this);
  /**
   * @type {Element}
   * @const
   */
  this.element = element || createElement('div');
  this.element.$view = this;
  this.bindCallbackMethods();
}

View.prototype = Object.create(EventEmitter.prototype);

/**
 * @param {!Element} ancestorElement
 * @return {?Object}
 */
View.prototype.offsetRelativeTo = function(ancestorElement) {
  var x = 0;
  var y = 0;
  var element = this.element;
  while (element) {
    x += element.offsetLeft || 0;
    y += element.offsetTop || 0;
    element = element.offsetParent;
    if (element === ancestorElement)
      return {x: x, y: y};
  }
  return null;
};

/**
 * @param {!View|Node} parent
 * @param {?View|Node=} before
 */
View.prototype.attachTo = function(parent, before) {
  if (parent instanceof View)
    return this.attachTo(parent.element, before);
  if (typeof before === 'undefined')
    before = null;
  if (before instanceof View)
    before = before.element;
  parent.insertBefore(this.element, before);
};

View.prototype.bindCallbackMethods = function() {
  for (var methodName in this) {
    if (!/^on[A-Z]/.test(methodName))
      continue;
    if (this.hasOwnProperty(methodName))
      continue;
    var method = this[methodName];
    if (!(method instanceof Function))
      continue;
    this[methodName] = method.bind(this);
  }
};

/**
 * @constructor
 * @extends View
 */
function ScrollView() {
  View.call(this, createElement('div', ScrollView.ClassNameScrollView));
  /**
   * @type {Element}
   * @const
   */
  this.contentElement =
      createElement('div', ScrollView.ClassNameScrollViewContent);
  this.element.appendChild(this.contentElement);
  /**
   * @type {number}
   */
  this.minimumContentOffset = -Infinity;
  /**
   * @type {number}
   */
  this.maximumContentOffset = Infinity;
  /**
   * @type {number}
   * @protected
   */
  this._contentOffset = 0;
  /**
   * @type {number}
   * @protected
   */
  this._width = 0;
  /**
   * @type {number}
   * @protected
   */
  this._height = 0;
  /**
   * @type {Animator}
   * @protected
   */
  this._scrollAnimator = null;
  /**
   * @type {?Object}
   */
  this.delegate = null;
  /**
   * @type {!number}
   */
  this._lastTouchPosition = 0;
  /**
   * @type {!number}
   */
  this._lastTouchVelocity = 0;
  /**
   * @type {!number}
   */
  this._lastTouchTimeStamp = 0;

  this.element.addEventListener('mousewheel', this.onMouseWheel, false);
  this.element.addEventListener('touchstart', this.onTouchStart, false);

  /**
   * The content offset is partitioned so the it can go beyond the CSS limit
   * of 33554433px.
   * @type {number}
   * @protected
   */
  this._partitionNumber = 0;
}

ScrollView.prototype = Object.create(View.prototype);

ScrollView.PartitionHeight = 100000;
ScrollView.ClassNameScrollView = 'scroll-view';
ScrollView.ClassNameScrollViewContent = 'scroll-view-content';

/**
 * @param {!Event} event
 */
ScrollView.prototype.onTouchStart = function(event) {
  var touch = event.touches[0];
  this._lastTouchPosition = touch.clientY;
  this._lastTouchVelocity = 0;
  this._lastTouchTimeStamp = event.timeStamp;
  if (this._scrollAnimator)
    this._scrollAnimator.stop();
  window.addEventListener('touchmove', this.onWindowTouchMove, false);
  window.addEventListener('touchend', this.onWindowTouchEnd, false);
};

/**
 * @param {!Event} event
 */
ScrollView.prototype.onWindowTouchMove = function(event) {
  var touch = event.touches[0];
  var deltaTime = event.timeStamp - this._lastTouchTimeStamp;
  var deltaY = this._lastTouchPosition - touch.clientY;
  this.scrollBy(deltaY, false);
  this._lastTouchVelocity = deltaY / deltaTime;
  this._lastTouchPosition = touch.clientY;
  this._lastTouchTimeStamp = event.timeStamp;
  event.stopPropagation();
  event.preventDefault();
};

/**
 * @param {!Event} event
 */
ScrollView.prototype.onWindowTouchEnd = function(event) {
  if (Math.abs(this._lastTouchVelocity) > 0.01) {
    this._scrollAnimator =
        new FlingGestureAnimator(this._lastTouchVelocity, this._contentOffset);
    this._scrollAnimator.step = this.onFlingGestureAnimatorStep;
    this._scrollAnimator.start();
  }
  window.removeEventListener('touchmove', this.onWindowTouchMove, false);
  window.removeEventListener('touchend', this.onWindowTouchEnd, false);
};

/**
 * @param {!Animator} animator
 */
ScrollView.prototype.onFlingGestureAnimatorStep = function(animator) {
  this.scrollTo(animator.currentValue, false);
};

/**
 * @return {!Animator}
 */
ScrollView.prototype.scrollAnimator = function() {
  return this._scrollAnimator;
};

/**
 * @param {!number} width
 */
ScrollView.prototype.setWidth = function(width) {
  console.assert(isFinite(width));
  if (this._width === width)
    return;
  this._width = width;
  this.element.style.width = this._width + 'px';
};

/**
 * @return {!number}
 */
ScrollView.prototype.width = function() {
  return this._width;
};

/**
 * @param {!number} height
 */
ScrollView.prototype.setHeight = function(height) {
  console.assert(isFinite(height));
  if (this._height === height)
    return;
  this._height = height;
  this.element.style.height = height + 'px';
  if (this.delegate)
    this.delegate.scrollViewDidChangeHeight(this);
};

/**
 * @return {!number}
 */
ScrollView.prototype.height = function() {
  return this._height;
};

/**
 * @param {!Animator} animator
 */
ScrollView.prototype.onScrollAnimatorStep = function(animator) {
  this.setContentOffset(animator.currentValue);
};

/**
 * @param {!number} offset
 * @param {?boolean} animate
 */
ScrollView.prototype.scrollTo = function(offset, animate) {
  console.assert(isFinite(offset));
  if (!animate) {
    this.setContentOffset(offset);
    return;
  }
  if (this._scrollAnimator)
    this._scrollAnimator.stop();
  this._scrollAnimator = new TransitionAnimator();
  this._scrollAnimator.step = this.onScrollAnimatorStep;
  this._scrollAnimator.setFrom(this._contentOffset);
  this._scrollAnimator.setTo(offset);
  this._scrollAnimator.duration = 300;
  this._scrollAnimator.start();
};

/**
 * @param {!number} offset
 * @param {?boolean} animate
 */
ScrollView.prototype.scrollBy = function(offset, animate) {
  this.scrollTo(this._contentOffset + offset, animate);
};

/**
 * @return {!number}
 */
ScrollView.prototype.contentOffset = function() {
  return this._contentOffset;
};

/**
 * @param {?Event} event
 */
ScrollView.prototype.onMouseWheel = function(event) {
  this.setContentOffset(this._contentOffset - event.wheelDelta / 30);
  event.stopPropagation();
  event.preventDefault();
};


/**
 * @param {!number} value
 */
ScrollView.prototype.setContentOffset = function(value) {
  console.assert(isFinite(value));
  value = Math.min(
      this.maximumContentOffset - this._height,
      Math.max(this.minimumContentOffset, Math.floor(value)));
  if (this._contentOffset === value)
    return;
  this._contentOffset = value;
  this._updateScrollContent();
  if (this.delegate)
    this.delegate.scrollViewDidChangeContentOffset(this);
};

ScrollView.prototype._updateScrollContent = function() {
  var newPartitionNumber =
      Math.floor(this._contentOffset / ScrollView.PartitionHeight);
  var partitionChanged = this._partitionNumber !== newPartitionNumber;
  this._partitionNumber = newPartitionNumber;
  this.contentElement.style.webkitTransform = 'translate(0, ' +
      (-this.contentPositionForContentOffset(this._contentOffset)) + 'px)';
  if (this.delegate && partitionChanged)
    this.delegate.scrollViewDidChangePartition(this);
};

/**
 * @param {!View|Node} parent
 * @param {?View|Node=} before
 * @override
 */
ScrollView.prototype.attachTo = function(parent, before) {
  View.prototype.attachTo.call(this, parent, before);
  this._updateScrollContent();
};

/**
 * @param {!number} offset
 */
ScrollView.prototype.contentPositionForContentOffset = function(offset) {
  return offset - this._partitionNumber * ScrollView.PartitionHeight;
};

/**
 * @constructor
 * @extends View
 */
function ListCell() {
  View.call(this, createElement('div', ListCell.ClassNameListCell));

  /**
   * @type {!number}
   */
  this.row = NaN;
  /**
   * @type {!number}
   */
  this._width = 0;
  /**
   * @type {!number}
   */
  this._position = 0;
}

ListCell.prototype = Object.create(View.prototype);

ListCell.DefaultRecycleBinLimit = 64;
ListCell.ClassNameListCell = 'list-cell';
ListCell.ClassNameHidden = 'hidden';

/**
 * @return {!Array} An array to keep thrown away cells.
 */
ListCell.prototype._recycleBin = function() {
  console.assert(
      false,
      'NOT REACHED: ListCell.prototype._recycleBin needs to be overridden.');
  return [];
};

ListCell.prototype.throwAway = function() {
  this.hide();
  var limit = typeof this.constructor.RecycleBinLimit === 'undefined' ?
      ListCell.DefaultRecycleBinLimit :
      this.constructor.RecycleBinLimit;
  var recycleBin = this._recycleBin();
  if (recycleBin.length < limit)
    recycleBin.push(this);
};

ListCell.prototype.show = function() {
  this.element.classList.remove(ListCell.ClassNameHidden);
};

ListCell.prototype.hide = function() {
  this.element.classList.add(ListCell.ClassNameHidden);
};

/**
 * @return {!number} Width in pixels.
 */
ListCell.prototype.width = function() {
  return this._width;
};

/**
 * @param {!number} width Width in pixels.
 */
ListCell.prototype.setWidth = function(width) {
  if (this._width === width)
    return;
  this._width = width;
  this.element.style.width = this._width + 'px';
};

/**
 * @return {!number} Position in pixels.
 */
ListCell.prototype.position = function() {
  return this._position;
};

/**
 * @param {!number} y Position in pixels.
 */
ListCell.prototype.setPosition = function(y) {
  if (this._position === y)
    return;
  this._position = y;
  this.element.style.webkitTransform = 'translate(0, ' + this._position + 'px)';
};

/**
 * @param {!boolean} selected
 */
ListCell.prototype.setSelected = function(selected) {
  if (this._selected === selected)
    return;
  this._selected = selected;
  if (this._selected)
    this.element.classList.add('selected');
  else
    this.element.classList.remove('selected');
};

/**
 * @constructor
 * @extends View
 */
function ListView() {
  View.call(this, createElement('div', ListView.ClassNameListView));
  this.element.tabIndex = 0;
  this.element.setAttribute('role', 'grid');

  /**
   * @type {!number}
   * @private
   */
  this._width = 0;
  /**
   * @type {!Object}
   * @private
   */
  this._cells = {};

  /**
   * @type {!number}
   */
  this.selectedRow = ListView.NoSelection;

  /**
   * @type {!ScrollView}
   */
  this.scrollView = new ScrollView();
  this.scrollView.delegate = this;
  this.scrollView.minimumContentOffset = 0;
  this.scrollView.setWidth(0);
  this.scrollView.setHeight(0);
  this.scrollView.attachTo(this);

  this.element.addEventListener('click', this.onClick, false);

  /**
   * @type {!boolean}
   * @private
   */
  this._needsUpdateCells = false;
}

ListView.prototype = Object.create(View.prototype);

ListView.NoSelection = -1;
ListView.ClassNameListView = 'list-view';

ListView.prototype.onAnimationFrameWillFinish = function() {
  if (this._needsUpdateCells)
    this.updateCells();
};

/**
 * @param {!boolean} needsUpdateCells
 */
ListView.prototype.setNeedsUpdateCells = function(needsUpdateCells) {
  if (this._needsUpdateCells === needsUpdateCells)
    return;
  this._needsUpdateCells = needsUpdateCells;
  if (this._needsUpdateCells)
    AnimationManager.shared.on(
        AnimationManager.EventTypeAnimationFrameWillFinish,
        this.onAnimationFrameWillFinish);
  else
    AnimationManager.shared.removeListener(
        AnimationManager.EventTypeAnimationFrameWillFinish,
        this.onAnimationFrameWillFinish);
};

/**
 * @param {!number} row
 * @return {?ListCell}
 */
ListView.prototype.cellAtRow = function(row) {
  return this._cells[row];
};

/**
 * @param {!number} offset Scroll offset in pixels.
 * @return {!number}
 */
ListView.prototype.rowAtScrollOffset = function(offset) {
  console.assert(
      false,
      'NOT REACHED: ListView.prototype.rowAtScrollOffset needs to be overridden.');
  return 0;
};

/**
 * @param {!number} row
 * @return {!number} Scroll offset in pixels.
 */
ListView.prototype.scrollOffsetForRow = function(row) {
  console.assert(
      false,
      'NOT REACHED: ListView.prototype.scrollOffsetForRow needs to be overridden.');
  return 0;
};

/**
 * @param {!number} row
 * @return {!ListCell}
 */
ListView.prototype.addCellIfNecessary = function(row) {
  var cell = this._cells[row];
  if (cell)
    return cell;
  cell = this.prepareNewCell(row);
  cell.attachTo(this.scrollView.contentElement);
  cell.setWidth(this._width);
  cell.setPosition(this.scrollView.contentPositionForContentOffset(
      this.scrollOffsetForRow(row)));
  this._cells[row] = cell;
  return cell;
};

/**
 * @param {!number} row
 * @return {!ListCell}
 */
ListView.prototype.prepareNewCell = function(row) {
  console.assert(
      false,
      'NOT REACHED: ListView.prototype.prepareNewCell should be overridden.');
  return new ListCell();
};

/**
 * @param {!ListCell} cell
 */
ListView.prototype.throwAwayCell = function(cell) {
  delete this._cells[cell.row];
  cell.throwAway();
};

/**
 * @return {!number}
 */
ListView.prototype.firstVisibleRow = function() {
  return this.rowAtScrollOffset(this.scrollView.contentOffset());
};

/**
 * @return {!number}
 */
ListView.prototype.lastVisibleRow = function() {
  return this.rowAtScrollOffset(
      this.scrollView.contentOffset() + this.scrollView.height() - 1);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangeContentOffset = function(scrollView) {
  this.setNeedsUpdateCells(true);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangeHeight = function(scrollView) {
  this.setNeedsUpdateCells(true);
};

/**
 * @param {!ScrollView} scrollView
 */
ListView.prototype.scrollViewDidChangePartition = function(scrollView) {
  this.setNeedsUpdateCells(true);
};

ListView.prototype.updateCells = function() {
  var firstVisibleRow = this.firstVisibleRow();
  var lastVisibleRow = this.lastVisibleRow();
  console.assert(firstVisibleRow <= lastVisibleRow);
  for (var c in this._cells) {
    var cell = this._cells[c];
    if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
      this.throwAwayCell(cell);
  }
  for (var i = firstVisibleRow; i <= lastVisibleRow; ++i) {
    var cell = this._cells[i];
    if (cell)
      cell.setPosition(this.scrollView.contentPositionForContentOffset(
          this.scrollOffsetForRow(cell.row)));
    else
      this.addCellIfNecessary(i);
  }
  this.setNeedsUpdateCells(false);
};

/**
 * @return {!number} Width in pixels.
 */
ListView.prototype.width = function() {
  return this._width;
};

/**
 * @param {!number} width Width in pixels.
 */
ListView.prototype.setWidth = function(width) {
  if (this._width === width)
    return;
  this._width = width;
  this.scrollView.setWidth(this._width);
  for (var c in this._cells) {
    this._cells[c].setWidth(this._width);
  }
  this.element.style.width = this._width + 'px';
  this.setNeedsUpdateCells(true);
};

/**
 * @return {!number} Height in pixels.
 */
ListView.prototype.height = function() {
  return this.scrollView.height();
};

/**
 * @param {!number} height Height in pixels.
 */
ListView.prototype.setHeight = function(height) {
  this.scrollView.setHeight(height);
};

/**
 * @param {?Event} event
 */
ListView.prototype.onClick = function(event) {
  var clickedCellElement =
      enclosingNodeOrSelfWithClass(event.target, ListCell.ClassNameListCell);
  if (!clickedCellElement)
    return;
  var clickedCell = clickedCellElement.$view;
  if (clickedCell.row !== this.selectedRow)
    this.select(clickedCell.row);
};

/**
 * @param {!number} row
 */
ListView.prototype.select = function(row) {
  if (this.selectedRow === row)
    return;
  this.deselect();
  if (row === ListView.NoSelection)
    return;
  this.selectedRow = row;
  var selectedCell = this._cells[this.selectedRow];
  if (selectedCell)
    selectedCell.setSelected(true);
};

ListView.prototype.deselect = function() {
  if (this.selectedRow === ListView.NoSelection)
    return;
  var selectedCell = this._cells[this.selectedRow];
  if (selectedCell)
    selectedCell.setSelected(false);
  this.selectedRow = ListView.NoSelection;
};

/**
 * @param {!number} row
 * @param {!boolean} animate
 */
ListView.prototype.scrollToRow = function(row, animate) {
  this.scrollView.scrollTo(this.scrollOffsetForRow(row), animate);
};

/**
 * @constructor
 * @extends View
 * @param {!ScrollView} scrollView
 */
function ScrubbyScrollBar(scrollView) {
  View.call(
      this, createElement('div', ScrubbyScrollBar.ClassNameScrubbyScrollBar));

  /**
   * @type {!Element}
   * @const
   */
  this.thumb =
      createElement('div', ScrubbyScrollBar.ClassNameScrubbyScrollThumb);
  this.element.appendChild(this.thumb);

  /**
   * @type {!ScrollView}
   * @const
   */
  this.scrollView = scrollView;

  /**
   * @type {!number}
   * @protected
   */
  this._height = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._thumbHeight = 0;
  /**
   * @type {!number}
   * @protected
   */
  this._thumbPosition = 0;

  this.setHeight(0);
  this.setThumbHeight(ScrubbyScrollBar.ThumbHeight);

  /**
   * @type {?Animator}
   * @protected
   */
  this._thumbStyleTopAnimator = null;

  /**
   * @type {?number}
   * @protected
   */
  this._timer = null;

  this.element.addEventListener('mousedown', this.onMouseDown, false);
  this.element.addEventListener('touchstart', this.onTouchStart, false);
}

ScrubbyScrollBar.prototype = Object.create(View.prototype);

ScrubbyScrollBar.ScrollInterval = 16;
ScrubbyScrollBar.ThumbMargin = 2;
ScrubbyScrollBar.ThumbHeight = 30;
ScrubbyScrollBar.ClassNameScrubbyScrollBar = 'scrubby-scroll-bar';
ScrubbyScrollBar.ClassNameScrubbyScrollThumb = 'scrubby-scroll-thumb';

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onTouchStart = function(event) {
  var touch = event.touches[0];
  this._setThumbPositionFromEventPosition(touch.clientY);
  if (this._thumbStyleTopAnimator)
    this._thumbStyleTopAnimator.stop();
  this._timer =
      setInterval(this.onScrollTimer, ScrubbyScrollBar.ScrollInterval);
  window.addEventListener('touchmove', this.onWindowTouchMove, false);
  window.addEventListener('touchend', this.onWindowTouchEnd, false);
  event.stopPropagation();
  event.preventDefault();
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowTouchMove = function(event) {
  var touch = event.touches[0];
  this._setThumbPositionFromEventPosition(touch.clientY);
  event.stopPropagation();
  event.preventDefault();
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowTouchEnd = function(event) {
  this._thumbStyleTopAnimator = new TransitionAnimator();
  this._thumbStyleTopAnimator.step = this.onThumbStyleTopAnimationStep;
  this._thumbStyleTopAnimator.setFrom(this.thumb.offsetTop);
  this._thumbStyleTopAnimator.setTo((this._height - this._thumbHeight) / 2);
  this._thumbStyleTopAnimator.timingFunction =
      AnimationTimingFunction.EaseInOut;
  this._thumbStyleTopAnimator.duration = 100;
  this._thumbStyleTopAnimator.start();

  window.removeEventListener('touchmove', this.onWindowTouchMove, false);
  window.removeEventListener('touchend', this.onWindowTouchEnd, false);
  clearInterval(this._timer);
};

/**
 * @return {!number} Height of the view in pixels.
 */
ScrubbyScrollBar.prototype.height = function() {
  return this._height;
};

/**
 * @param {!number} height Height of the view in pixels.
 */
ScrubbyScrollBar.prototype.setHeight = function(height) {
  if (this._height === height)
    return;
  this._height = height;
  this.element.style.height = this._height + 'px';
  this.thumb.style.top = ((this._height - this._thumbHeight) / 2) + 'px';
  this._thumbPosition = 0;
};

/**
 * @param {!number} height Height of the scroll bar thumb in pixels.
 */
ScrubbyScrollBar.prototype.setThumbHeight = function(height) {
  if (this._thumbHeight === height)
    return;
  this._thumbHeight = height;
  this.thumb.style.height = this._thumbHeight + 'px';
  this.thumb.style.top = ((this._height - this._thumbHeight) / 2) + 'px';
  this._thumbPosition = 0;
};

/**
 * @param {number} position
 */
ScrubbyScrollBar.prototype._setThumbPositionFromEventPosition = function(
    position) {
  var thumbMin = ScrubbyScrollBar.ThumbMargin;
  var thumbMax =
      this._height - this._thumbHeight - ScrubbyScrollBar.ThumbMargin * 2;
  var y = position - this.element.getBoundingClientRect().top -
      this.element.clientTop + this.element.scrollTop;
  var thumbPosition = y - this._thumbHeight / 2;
  thumbPosition = Math.max(thumbPosition, thumbMin);
  thumbPosition = Math.min(thumbPosition, thumbMax);
  this.thumb.style.top = thumbPosition + 'px';
  this._thumbPosition =
      1.0 - (thumbPosition - thumbMin) / (thumbMax - thumbMin) * 2;
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onMouseDown = function(event) {
  this._setThumbPositionFromEventPosition(event.clientY);

  window.addEventListener('mousemove', this.onWindowMouseMove, false);
  window.addEventListener('mouseup', this.onWindowMouseUp, false);
  if (this._thumbStyleTopAnimator)
    this._thumbStyleTopAnimator.stop();
  this._timer =
      setInterval(this.onScrollTimer, ScrubbyScrollBar.ScrollInterval);
  event.stopPropagation();
  event.preventDefault();
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowMouseMove = function(event) {
  this._setThumbPositionFromEventPosition(event.clientY);
};

/**
 * @param {?Event} event
 */
ScrubbyScrollBar.prototype.onWindowMouseUp = function(event) {
  this._thumbStyleTopAnimator = new TransitionAnimator();
  this._thumbStyleTopAnimator.step = this.onThumbStyleTopAnimationStep;
  this._thumbStyleTopAnimator.setFrom(this.thumb.offsetTop);
  this._thumbStyleTopAnimator.setTo((this._height - this._thumbHeight) / 2);
  this._thumbStyleTopAnimator.timingFunction =
      AnimationTimingFunction.EaseInOut;
  this._thumbStyleTopAnimator.duration = 100;
  this._thumbStyleTopAnimator.start();

  window.removeEventListener('mousemove', this.onWindowMouseMove, false);
  window.removeEventListener('mouseup', this.onWindowMouseUp, false);
  clearInterval(this._timer);
};

/**
 * @param {!Animator} animator
 */
ScrubbyScrollBar.prototype.onThumbStyleTopAnimationStep = function(animator) {
  this.thumb.style.top = animator.currentValue + 'px';
};

ScrubbyScrollBar.prototype.onScrollTimer = function() {
  var scrollAmount = Math.pow(this._thumbPosition, 2) * 10;
  if (this._thumbPosition > 0)
    scrollAmount = -scrollAmount;
  this.scrollView.scrollBy(scrollAmount, false);
};

/**
 * @constructor
 * @extends ListCell
 * @param {!Array} shortMonthLabels
 */
function YearListCell(shortMonthLabels) {
  ListCell.call(this);
  this.element.classList.add(YearListCell.ClassNameYearListCell);
  this.element.style.height = YearListCell.GetHeight() + 'px';

  /**
   * @type {!Element}
   * @const
   */
  this.label = createElement('div', YearListCell.ClassNameLabel, '----');
  this.element.appendChild(this.label);
  this.label.style.height =
      (YearListCell.GetHeight() - YearListCell.BorderBottomWidth) + 'px';
  this.label.style.lineHeight =
      (YearListCell.GetHeight() - YearListCell.BorderBottomWidth) + 'px';

  /**
   * @type {!Array} Array of the 12 month button elements.
   * @const
   */
  this.monthButtons = [];
  var monthChooserElement =
      createElement('div', YearListCell.ClassNameMonthChooser);
  for (var r = 0; r < YearListCell.ButtonRows; ++r) {
    var buttonsRow =
        createElement('div', YearListCell.ClassNameMonthButtonsRow);
    buttonsRow.setAttribute('role', 'row');
    for (var c = 0; c < YearListCell.ButtonColumns; ++c) {
      var month = c + r * YearListCell.ButtonColumns;
      var button = createElement(
          'div', YearListCell.ClassNameMonthButton, shortMonthLabels[month]);
      button.setAttribute('role', 'gridcell');
      button.dataset.month = month;
      buttonsRow.appendChild(button);
      this.monthButtons.push(button);
    }
    monthChooserElement.appendChild(buttonsRow);
  }
  this.element.appendChild(monthChooserElement);

  /**
   * @type {!boolean}
   * @private
   */
  this._selected = false;
  /**
   * @type {!number}
   * @private
   */
  this._height = 0;
}

YearListCell.prototype = Object.create(ListCell.prototype);

YearListCell._Height = hasInaccuratePointingDevice() ? 31 : 25;
YearListCell._HeightRefresh = 25;
YearListCell.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return YearListCell._HeightRefresh;
  }
  return YearListCell._Height;
};
YearListCell.BorderBottomWidth = 1;
YearListCell.ButtonRows = 3;
YearListCell.ButtonColumns = 4;
YearListCell._SelectedHeight = hasInaccuratePointingDevice() ? 127 : 121;
YearListCell._SelectedHeightRefresh = 128;
YearListCell.GetSelectedHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return YearListCell._SelectedHeightRefresh;
  }
  return YearListCell._SelectedHeight;
};
YearListCell.ClassNameYearListCell = 'year-list-cell';
YearListCell.ClassNameLabel = 'label';
YearListCell.ClassNameMonthChooser = 'month-chooser';
YearListCell.ClassNameMonthButtonsRow = 'month-buttons-row';
YearListCell.ClassNameMonthButton = 'month-button';
YearListCell.ClassNameHighlighted = 'highlighted';
YearListCell.ClassNameSelected = 'selected';
YearListCell.ClassNameToday = 'today';

YearListCell._recycleBin = [];

/**
 * @return {!Array}
 * @override
 */
YearListCell.prototype._recycleBin = function() {
  return YearListCell._recycleBin;
};

/**
 * @param {!number} row
 */
YearListCell.prototype.reset = function(row) {
  this.row = row;
  this.label.textContent = row + 1;
  for (var i = 0; i < this.monthButtons.length; ++i) {
    this.monthButtons[i].classList.remove(YearListCell.ClassNameHighlighted);
    this.monthButtons[i].classList.remove(YearListCell.ClassNameSelected);
    this.monthButtons[i].classList.remove(YearListCell.ClassNameToday);
  }
  this.show();
};

/**
 * @return {!number} The height in pixels.
 */
YearListCell.prototype.height = function() {
  return this._height;
};

/**
 * @param {!number} height Height in pixels.
 */
YearListCell.prototype.setHeight = function(height) {
  if (this._height === height)
    return;
  this._height = height;
  this.element.style.height = this._height + 'px';
};

/**
 * @constructor
 * @extends ListView
 * @param {!Month} minimumMonth
 * @param {!Month} maximumMonth
 */
function YearListView(minimumMonth, maximumMonth) {
  ListView.call(this);
  this.element.classList.add('year-list-view');

  /**
   * @type {?Month}
   */
  this.highlightedMonth = null;
  /**
   * @type {?Month}
   */
  this._selectedMonth = null;
  /**
   * @type {!Month}
   * @const
   * @protected
   */
  this._minimumMonth = minimumMonth;
  /**
   * @type {!Month}
   * @const
   * @protected
   */
  this._maximumMonth = maximumMonth;

  this.scrollView.minimumContentOffset =
      (this._minimumMonth.year - 1) * YearListCell.GetHeight();
  this.scrollView.maximumContentOffset =
      (this._maximumMonth.year - 1) * YearListCell.GetHeight() +
      YearListCell.GetSelectedHeight();

  /**
   * @type {!Object}
   * @const
   * @protected
   */
  this._runningAnimators = {};
  /**
   * @type {!Array}
   * @const
   * @protected
   */
  this._animatingRows = [];
  /**
   * @type {!boolean}
   * @protected
   */
  this._ignoreMouseOutUntillNextMouseOver = false;

  /**
   * @type {!ScrubbyScrollBar}
   * @const
   */
  this.scrubbyScrollBar = new ScrubbyScrollBar(this.scrollView);
  this.scrubbyScrollBar.attachTo(this);

  this.element.addEventListener('mouseover', this.onMouseOver, false);
  this.element.addEventListener('mouseout', this.onMouseOut, false);
  this.element.addEventListener('keydown', this.onKeyDown, false);
  this.element.addEventListener('touchstart', this.onTouchStart, false);
}

YearListView.prototype = Object.create(ListView.prototype);

YearListView._Height = YearListCell._SelectedHeight - 1;
YearListView._VisibleYearsRefresh = 4;
YearListView._HeightRefresh = YearListCell._SelectedHeightRefresh - 1 +
    YearListView._VisibleYearsRefresh * YearListCell._HeightRefresh;
YearListView.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return YearListView._HeightRefresh;
  }
  return YearListView._Height;
};
YearListView.EventTypeYearListViewDidHide = 'yearListViewDidHide';
YearListView.EventTypeYearListViewDidSelectMonth = 'yearListViewDidSelectMonth';

/**
 * @param {?Event} event
 */
YearListView.prototype.onTouchStart = function(event) {
  var touch = event.touches[0];
  var monthButtonElement = enclosingNodeOrSelfWithClass(
      touch.target, YearListCell.ClassNameMonthButton);
  if (!monthButtonElement)
    return;
  var cellElement = enclosingNodeOrSelfWithClass(
      monthButtonElement, YearListCell.ClassNameYearListCell);
  var cell = cellElement.$view;
  this.highlightMonth(
      new Month(cell.row + 1, parseInt(monthButtonElement.dataset.month, 10)));
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onMouseOver = function(event) {
  var monthButtonElement = enclosingNodeOrSelfWithClass(
      event.target, YearListCell.ClassNameMonthButton);
  if (!monthButtonElement)
    return;
  var cellElement = enclosingNodeOrSelfWithClass(
      monthButtonElement, YearListCell.ClassNameYearListCell);
  var cell = cellElement.$view;
  this.highlightMonth(
      new Month(cell.row + 1, parseInt(monthButtonElement.dataset.month, 10)));
  this._ignoreMouseOutUntillNextMouseOver = false;
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onMouseOut = function(event) {
  if (this._ignoreMouseOutUntillNextMouseOver)
    return;
  var monthButtonElement = enclosingNodeOrSelfWithClass(
      event.target, YearListCell.ClassNameMonthButton);
  if (!monthButtonElement) {
    this.dehighlightMonth();
  }
};

/**
 * @param {!number} width Width in pixels.
 * @override
 */
YearListView.prototype.setWidth = function(width) {
  ListView.prototype.setWidth.call(
      this, width - this.scrubbyScrollBar.element.offsetWidth);
  this.element.style.width = width + 'px';
};

/**
 * @param {!number} height Height in pixels.
 * @override
 */
YearListView.prototype.setHeight = function(height) {
  ListView.prototype.setHeight.call(this, height);
  this.scrubbyScrollBar.setHeight(height);
};

/**
 * @enum {number}
 */
YearListView.RowAnimationDirection = {
  Opening: 0,
  Closing: 1
};

/**
 * @param {!number} row
 * @param {!YearListView.RowAnimationDirection} direction
 */
YearListView.prototype._animateRow = function(row, direction) {
  var fromValue = direction === YearListView.RowAnimationDirection.Closing ?
      YearListCell.GetSelectedHeight() :
      YearListCell.GetHeight();
  var oldAnimator = this._runningAnimators[row];
  if (oldAnimator) {
    oldAnimator.stop();
    fromValue = oldAnimator.currentValue;
  }
  var cell = this.cellAtRow(row);
  var animator = new TransitionAnimator();
  animator.step = this.onCellHeightAnimatorStep;
  animator.setFrom(fromValue);
  animator.setTo(
      direction === YearListView.RowAnimationDirection.Opening ?
          YearListCell.GetSelectedHeight() :
          YearListCell.GetHeight());
  animator.timingFunction = AnimationTimingFunction.EaseInOut;
  animator.duration = 300;
  animator.row = row;
  animator.on(
      Animator.EventTypeDidAnimationStop, this.onCellHeightAnimatorDidStop);
  this._runningAnimators[row] = animator;
  this._animatingRows.push(row);
  this._animatingRows.sort();
  animator.start();
};

/**
 * @param {?Animator} animator
 */
YearListView.prototype.onCellHeightAnimatorDidStop = function(animator) {
  delete this._runningAnimators[animator.row];
  var index = this._animatingRows.indexOf(animator.row);
  this._animatingRows.splice(index, 1);
};

/**
 * @param {!Animator} animator
 */
YearListView.prototype.onCellHeightAnimatorStep = function(animator) {
  var cell = this.cellAtRow(animator.row);
  if (cell)
    cell.setHeight(animator.currentValue);
  this.updateCells();
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onClick = function(event) {
  var oldSelectedRow = this.selectedRow;
  ListView.prototype.onClick.call(this, event);
  var year = this.selectedRow + 1;
  if (this.selectedRow !== oldSelectedRow) {
    // Always start with first month when changing the year.
    const month = new Month(year, 0);
    this.highlightMonth(month);
    this.dispatchEvent(
        YearListView.EventTypeYearListViewDidSelectMonth, this, month);
    this.scrollView.scrollTo(this.selectedRow * YearListCell.GetHeight(), true);
  } else {
    var monthButton = enclosingNodeOrSelfWithClass(
        event.target, YearListCell.ClassNameMonthButton);
    if (!monthButton || monthButton.getAttribute('aria-disabled') == 'true')
      return;
    var month = parseInt(monthButton.dataset.month, 10);
    this.dispatchEvent(
        YearListView.EventTypeYearListViewDidSelectMonth, this,
        new Month(year, month));
    this.hide();
  }
};

/**
 * @param {!number} scrollOffset
 * @return {!number}
 * @override
 */
YearListView.prototype.rowAtScrollOffset = function(scrollOffset) {
  var remainingOffset = scrollOffset;
  var lastAnimatingRow = 0;
  var rowsWithIrregularHeight = this._animatingRows.slice();
  if (this.selectedRow > -1 && !this._runningAnimators[this.selectedRow]) {
    rowsWithIrregularHeight.push(this.selectedRow);
    rowsWithIrregularHeight.sort();
  }
  for (var i = 0; i < rowsWithIrregularHeight.length; ++i) {
    var row = rowsWithIrregularHeight[i];
    var animator = this._runningAnimators[row];
    var rowHeight =
        animator ? animator.currentValue : YearListCell.GetSelectedHeight();
    if (remainingOffset <=
        (row - lastAnimatingRow) * YearListCell.GetHeight()) {
      return lastAnimatingRow +
          Math.floor(remainingOffset / YearListCell.GetHeight());
    }
    remainingOffset -= (row - lastAnimatingRow) * YearListCell.GetHeight();
    if (remainingOffset <= (rowHeight - YearListCell.GetHeight()))
      return row;
    remainingOffset -= rowHeight - YearListCell.GetHeight();
    lastAnimatingRow = row;
  }
  return lastAnimatingRow +
      Math.floor(remainingOffset / YearListCell.GetHeight());
};

/**
 * @param {!number} row
 * @return {!number}
 * @override
 */
YearListView.prototype.scrollOffsetForRow = function(row) {
  var scrollOffset = row * YearListCell.GetHeight();
  for (var i = 0; i < this._animatingRows.length; ++i) {
    var animatingRow = this._animatingRows[i];
    if (animatingRow >= row)
      break;
    var animator = this._runningAnimators[animatingRow];
    scrollOffset += animator.currentValue - YearListCell.GetHeight();
  }
  if (this.selectedRow > -1 && this.selectedRow < row &&
      !this._runningAnimators[this.selectedRow]) {
    scrollOffset += YearListCell.GetSelectedHeight() - YearListCell.GetHeight();
  }
  return scrollOffset;
};

/**
 * @param {!number} row
 * @return {!YearListCell}
 * @override
 */
YearListView.prototype.prepareNewCell = function(row) {
  var cell = YearListCell._recycleBin.pop() ||
      new YearListCell(global.params.shortMonthLabels);
  cell.reset(row);
  cell.setSelected(this.selectedRow === row);
  for (var i = 0; i < cell.monthButtons.length; ++i) {
    var month = new Month(row + 1, i);
    cell.monthButtons[i].id = month.toString();
    cell.monthButtons[i].setAttribute(
        'aria-disabled',
        this._minimumMonth > month || this._maximumMonth < month ? 'true' :
                                                                   'false');
    cell.monthButtons[i].setAttribute('aria-label', month.toLocaleString());
  }
  if (this.highlightedMonth && row === this.highlightedMonth.year - 1) {
    var monthButton = cell.monthButtons[this.highlightedMonth.month];
    monthButton.classList.add(YearListCell.ClassNameHighlighted);
    // aria-activedescendant assumes both elements have layoutObjects, and
    // |monthButton| might have no layoutObject yet.
    var element = this.element;
    setTimeout(function() {
      element.setAttribute('aria-activedescendant', monthButton.id);
    }, 0);
  }
  if (this._selectedMonth && (this._selectedMonth.year - 1) === row) {
    var monthButton = cell.monthButtons[this._selectedMonth.month];
    monthButton.classList.add(YearListCell.ClassNameSelected);
  }
  const todayMonth = Month.createFromToday();
  if ((todayMonth.year - 1) === row) {
    var monthButton = cell.monthButtons[todayMonth.month];
    monthButton.classList.add(YearListCell.ClassNameToday);
  }

  var animator = this._runningAnimators[row];
  if (animator)
    cell.setHeight(animator.currentValue);
  else if (row === this.selectedRow)
    cell.setHeight(YearListCell.GetSelectedHeight());
  else
    cell.setHeight(YearListCell.GetHeight());
  return cell;
};

/**
 * @override
 */
YearListView.prototype.updateCells = function() {
  var firstVisibleRow = this.firstVisibleRow();
  var lastVisibleRow = this.lastVisibleRow();
  console.assert(firstVisibleRow <= lastVisibleRow);
  for (var c in this._cells) {
    var cell = this._cells[c];
    if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
      this.throwAwayCell(cell);
  }
  for (var i = firstVisibleRow; i <= lastVisibleRow; ++i) {
    var cell = this._cells[i];
    if (cell)
      cell.setPosition(this.scrollView.contentPositionForContentOffset(
          this.scrollOffsetForRow(cell.row)));
    else
      this.addCellIfNecessary(i);
  }
  this.setNeedsUpdateCells(false);
};

/**
 * @override
 */
YearListView.prototype.deselect = function() {
  if (this.selectedRow === ListView.NoSelection)
    return;
  var selectedCell = this._cells[this.selectedRow];
  if (selectedCell)
    selectedCell.setSelected(false);
  this._animateRow(
      this.selectedRow, YearListView.RowAnimationDirection.Closing);
  this.selectedRow = ListView.NoSelection;
  this.setNeedsUpdateCells(true);
};

YearListView.prototype.deselectWithoutAnimating = function() {
  if (this.selectedRow === ListView.NoSelection)
    return;
  var selectedCell = this._cells[this.selectedRow];
  if (selectedCell) {
    selectedCell.setSelected(false);
    selectedCell.setHeight(YearListCell.GetHeight());
  }
  this.selectedRow = ListView.NoSelection;
  this.setNeedsUpdateCells(true);
};

/**
 * @param {!number} row
 * @override
 */
YearListView.prototype.select = function(row) {
  if (this.selectedRow === row)
    return;
  this.deselect();
  if (row === ListView.NoSelection)
    return;
  this.selectedRow = row;
  if (this.selectedRow !== ListView.NoSelection) {
    var selectedCell = this._cells[this.selectedRow];
    this._animateRow(
        this.selectedRow, YearListView.RowAnimationDirection.Opening);
    if (selectedCell)
      selectedCell.setSelected(true);
    var month = this.highlightedMonth ? this.highlightedMonth.month : 0;
    this.highlightMonth(new Month(this.selectedRow + 1, month));
  }
  this.setNeedsUpdateCells(true);
};

/**
 * @param {!number} row
 */
YearListView.prototype.selectWithoutAnimating = function(row) {
  if (this.selectedRow === row)
    return;
  this.deselectWithoutAnimating();
  if (row === ListView.NoSelection)
    return;
  this.selectedRow = row;
  if (this.selectedRow !== ListView.NoSelection) {
    var selectedCell = this._cells[this.selectedRow];
    if (selectedCell) {
      selectedCell.setSelected(true);
      selectedCell.setHeight(YearListCell.GetSelectedHeight());
    }
    var month = this.highlightedMonth ? this.highlightedMonth.month : 0;
    this.highlightMonth(new Month(this.selectedRow + 1, month));
  }
  this.setNeedsUpdateCells(true);
};

/**
 * @param {!Month} month
 * @return {?HTMLDivElement}
 */
YearListView.prototype.buttonForMonth = function(month) {
  if (!month)
    return null;
  var row = month.year - 1;
  var cell = this.cellAtRow(row);
  if (!cell)
    return null;
  return cell.monthButtons[month.month];
};

YearListView.prototype.dehighlightMonth = function() {
  if (!this.highlightedMonth)
    return;
  var monthButton = this.buttonForMonth(this.highlightedMonth);
  if (monthButton) {
    monthButton.classList.remove(YearListCell.ClassNameHighlighted);
  }
  this.highlightedMonth = null;
  this.element.removeAttribute('aria-activedescendant');
};

/**
 * @param {!Month} month
 */
YearListView.prototype.highlightMonth = function(month) {
  if (this.highlightedMonth && this.highlightedMonth.equals(month))
    return;
  this.dehighlightMonth();
  this.highlightedMonth = month;
  if (!this.highlightedMonth)
    return;
  var monthButton = this.buttonForMonth(this.highlightedMonth);
  if (monthButton) {
    monthButton.classList.add(YearListCell.ClassNameHighlighted);
    this.element.setAttribute('aria-activedescendant', monthButton.id);
  }
};

YearListView.prototype.setSelectedMonth = function(month) {
  this._selectedMonth = month;
};

YearListView.prototype.showSelectedMonth = function() {
  var monthButton = this.buttonForMonth(this._selectedMonth);
  if (monthButton) {
    monthButton.classList.add(YearListCell.ClassNameSelected);
  }
};

/**
 * @param {!Month} month
 */
YearListView.prototype.show = function(month) {
  this._ignoreMouseOutUntillNextMouseOver = true;

  this.scrollToRow(month.year - 1, false);
  this.selectWithoutAnimating(month.year - 1);
  this.highlightMonth(month);
  this.showSelectedMonth();
};

YearListView.prototype.hide = function() {
  this.dispatchEvent(YearListView.EventTypeYearListViewDidHide, this);
};

/**
 * @param {!Month} month
 */
YearListView.prototype._moveHighlightTo = function(month) {
  this.highlightMonth(month);
  this.select(this.highlightedMonth.year - 1);

  this.dispatchEvent(
      YearListView.EventTypeYearListViewDidSelectMonth, this, month);
  this.scrollView.scrollTo(this.selectedRow * YearListCell.GetHeight(), true);
  return true;
};

/**
 * @param {?Event} event
 */
YearListView.prototype.onKeyDown = function(event) {
  var key = event.key;
  var eventHandled = false;
  if (key == 't')
    eventHandled = this._moveHighlightTo(Month.createFromToday());
  else if (this.highlightedMonth) {
    if (global.params.isLocaleRTL ? key == 'ArrowRight' : key == 'ArrowLeft')
      eventHandled = this._moveHighlightTo(this.highlightedMonth.previous());
    else if (key == 'ArrowUp')
      eventHandled = this._moveHighlightTo(
          this.highlightedMonth.previous(YearListCell.ButtonColumns));
    else if (
        global.params.isLocaleRTL ? key == 'ArrowLeft' : key == 'ArrowRight')
      eventHandled = this._moveHighlightTo(this.highlightedMonth.next());
    else if (key == 'ArrowDown')
      eventHandled = this._moveHighlightTo(
          this.highlightedMonth.next(YearListCell.ButtonColumns));
    else if (key == 'PageUp')
      eventHandled =
          this._moveHighlightTo(this.highlightedMonth.previous(MonthsPerYear));
    else if (key == 'PageDown')
      eventHandled =
          this._moveHighlightTo(this.highlightedMonth.next(MonthsPerYear));
    else if (key == 'Enter') {
      this.dispatchEvent(
          YearListView.EventTypeYearListViewDidSelectMonth, this,
          this.highlightedMonth);
      this.hide();
      eventHandled = true;
    }
  } else if (key == 'ArrowUp') {
    this.scrollView.scrollBy(-YearListCell.GetHeight(), true);
    eventHandled = true;
  } else if (key == 'ArrowDown') {
    this.scrollView.scrollBy(YearListCell.GetHeight(), true);
    eventHandled = true;
  } else if (key == 'PageUp') {
    this.scrollView.scrollBy(-this.scrollView.height(), true);
    eventHandled = true;
  } else if (key == 'PageDown') {
    this.scrollView.scrollBy(this.scrollView.height(), true);
    eventHandled = true;
  }

  if (eventHandled) {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * @constructor
 * @extends View
 * @param {!Month} minimumMonth
 * @param {!Month} maximumMonth
 */
function MonthPopupView(minimumMonth, maximumMonth) {
  View.call(this, createElement('div', MonthPopupView.ClassNameMonthPopupView));

  /**
   * @type {!YearListView}
   * @const
   */
  this.yearListView = new YearListView(minimumMonth, maximumMonth);
  this.yearListView.attachTo(this);

  /**
   * @type {!boolean}
   */
  this.isVisible = false;

  this.element.addEventListener('click', this.onClick, false);
}

MonthPopupView.prototype = Object.create(View.prototype);

MonthPopupView.ClassNameMonthPopupView = 'month-popup-view';

MonthPopupView.prototype.show = function(initialMonth, calendarTableRect) {
  this.isVisible = true;
  document.body.appendChild(this.element);
  this.yearListView.setWidth(calendarTableRect.width - 2);
  this.yearListView.setHeight(YearListView.GetHeight());
  if (global.params.isLocaleRTL)
    this.yearListView.element.style.right = calendarTableRect.x + 'px';
  else
    this.yearListView.element.style.left = calendarTableRect.x + 'px';
  this.yearListView.element.style.top = calendarTableRect.y + 'px';
  this.yearListView.show(initialMonth);
  this.yearListView.element.focus();
};

MonthPopupView.prototype.hide = function() {
  if (!this.isVisible)
    return;
  this.isVisible = false;
  this.element.parentNode.removeChild(this.element);
  this.yearListView.hide();
};

/**
 * @param {?Event} event
 */
MonthPopupView.prototype.onClick = function(event) {
  if (event.target !== this.element)
    return;
  this.hide();
};

/**
 * @constructor
 * @extends View
 * @param {!number} maxWidth Maximum width in pixels.
 */
function MonthPopupButton(maxWidth) {
  View.call(
      this,
      createElement('button', MonthPopupButton.ClassNameMonthPopupButton));
  this.element.setAttribute('aria-label', global.params.axShowMonthSelector);

  /**
   * @type {!Element}
   * @const
   */
  this.labelElement = createElement(
      'span', MonthPopupButton.ClassNameMonthPopupButtonLabel, '-----');
  this.element.appendChild(this.labelElement);

  /**
   * @type {!Element}
   * @const
   */
  this.disclosureTriangleIcon =
      createElement('span', MonthPopupButton.ClassNameDisclosureTriangle);
  this.disclosureTriangleIcon.innerHTML =
      '<svg width=\'7\' height=\'5\'><polygon points=\'0,1 7,1 3.5,5\' style=\'fill:#000000;\' /></svg>';
  this.element.appendChild(this.disclosureTriangleIcon);

  /**
   * @type {!boolean}
   * @protected
   */
  this._useShortMonth = this._shouldUseShortMonth(maxWidth);
  this.element.style.maxWidth = maxWidth + 'px';

  this.element.addEventListener('click', this.onClick, false);
}

MonthPopupButton.prototype = Object.create(View.prototype);

MonthPopupButton.ClassNameMonthPopupButton = 'month-popup-button';
MonthPopupButton.ClassNameMonthPopupButtonLabel = 'month-popup-button-label';
MonthPopupButton.ClassNameDisclosureTriangle = 'disclosure-triangle';
MonthPopupButton.EventTypeButtonClick = 'buttonClick';

/**
 * @param {!number} maxWidth Maximum available width in pixels.
 * @return {!boolean}
 */
MonthPopupButton.prototype._shouldUseShortMonth = function(maxWidth) {
  document.body.appendChild(this.element);
  var month = Month.Maximum;
  for (var i = 0; i < MonthsPerYear; ++i) {
    this.labelElement.textContent = month.toLocaleString();
    if (this.element.offsetWidth > maxWidth)
      return true;
    month = month.previous();
  }
  document.body.removeChild(this.element);
  return false;
};

/**
 * @param {!Month} month
 */
MonthPopupButton.prototype.setCurrentMonth = function(month) {
  this.labelElement.textContent = this._useShortMonth ?
      month.toShortLocaleString() :
      month.toLocaleString();
};

/**
 * @param {?Event} event
 */
MonthPopupButton.prototype.onClick = function(event) {
  this.dispatchEvent(MonthPopupButton.EventTypeButtonClick, this);
};

/**
 * @constructor
 * @extends View
 */
function CalendarNavigationButton() {
  View.call(
      this,
      createElement(
          'button',
          CalendarNavigationButton.ClassNameCalendarNavigationButton));
  /**
   * @type {number} Threshold for starting repeating clicks in milliseconds.
   */
  this.repeatingClicksStartingThreshold =
      CalendarNavigationButton.DefaultRepeatingClicksStartingThreshold;
  /**
   * @type {number} Interval between reapeating clicks in milliseconds.
   */
  this.reapeatingClicksInterval =
      CalendarNavigationButton.DefaultRepeatingClicksInterval;
  /**
   * @type {?number} The ID for the timeout that triggers the repeating clicks.
   */
  this._timer = null;
  this.element.addEventListener('click', this.onClick, false);
  this.element.addEventListener('mousedown', this.onMouseDown, false);
  this.element.addEventListener('touchstart', this.onTouchStart, false);
};

CalendarNavigationButton.prototype = Object.create(View.prototype);

CalendarNavigationButton.DefaultRepeatingClicksStartingThreshold = 600;
CalendarNavigationButton.DefaultRepeatingClicksInterval = 300;
CalendarNavigationButton.LeftMargin = 4;
CalendarNavigationButton.Width = 24;
CalendarNavigationButton.ClassNameCalendarNavigationButton =
    'calendar-navigation-button';
CalendarNavigationButton.EventTypeButtonClick = 'buttonClick';
CalendarNavigationButton.EventTypeRepeatingButtonClick = 'repeatingButtonClick';

/**
 * @param {!boolean} disabled
 */
CalendarNavigationButton.prototype.setDisabled = function(disabled) {
  this.element.disabled = disabled;
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onClick = function(event) {
  this.dispatchEvent(CalendarNavigationButton.EventTypeButtonClick, this);
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onTouchStart = function(event) {
  if (this._timer !== null)
    return;
  this._timer =
      setTimeout(this.onRepeatingClick, this.repeatingClicksStartingThreshold);
  window.addEventListener('touchend', this.onWindowTouchEnd, false);
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onWindowTouchEnd = function(event) {
  if (this._timer === null)
    return;
  clearTimeout(this._timer);
  this._timer = null;
  window.removeEventListener('touchend', this.onWindowMouseUp, false);
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onMouseDown = function(event) {
  if (this._timer !== null)
    return;
  this._timer =
      setTimeout(this.onRepeatingClick, this.repeatingClicksStartingThreshold);
  window.addEventListener('mouseup', this.onWindowMouseUp, false);
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onWindowMouseUp = function(event) {
  if (this._timer === null)
    return;
  clearTimeout(this._timer);
  this._timer = null;
  window.removeEventListener('mouseup', this.onWindowMouseUp, false);
};

/**
 * @param {?Event} event
 */
CalendarNavigationButton.prototype.onRepeatingClick = function(event) {
  this.dispatchEvent(
      CalendarNavigationButton.EventTypeRepeatingButtonClick, this);
  this._timer =
      setTimeout(this.onRepeatingClick, this.reapeatingClicksInterval);
};

/**
 * @constructor
 * @extends View
 * @param {!CalendarPicker} calendarPicker
 */
function CalendarHeaderView(calendarPicker) {
  View.call(
      this,
      createElement('div', CalendarHeaderView.ClassNameCalendarHeaderView));
  this.calendarPicker = calendarPicker;
  this.calendarPicker.on(
      CalendarPicker.EventTypeCurrentMonthChanged, this.onCurrentMonthChanged);

  var titleElement =
      createElement('div', CalendarHeaderView.ClassNameCalendarTitle);
  this.element.appendChild(titleElement);

  /**
   * @type {!MonthPopupButton}
   */
  this.monthPopupButton = new MonthPopupButton(
      this.calendarPicker.calendarTableView.width() -
      CalendarTableView.GetBorderWidth() * 2 -
      CalendarNavigationButton.Width * 3 -
      CalendarNavigationButton.LeftMargin * 2);
  this.monthPopupButton.attachTo(titleElement);

  /**
   * @type {!CalendarNavigationButton}
   * @const
   */
  this._previousMonthButton = new CalendarNavigationButton();
  this._previousMonthButton.attachTo(this);
  this._previousMonthButton.on(
      CalendarNavigationButton.EventTypeButtonClick,
      this.onNavigationButtonClick);
  this._previousMonthButton.on(
      CalendarNavigationButton.EventTypeRepeatingButtonClick,
      this.onNavigationButtonClick);
  this._previousMonthButton.element.setAttribute(
      'aria-label', global.params.axShowPreviousMonth);

  if (!global.params.isFormControlsRefreshEnabled) {
    /**
     * @type {!CalendarNavigationButton}
     * @const
     */
    this._todayButton = new CalendarNavigationButton();
    this._todayButton.attachTo(this);
    this._todayButton.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onNavigationButtonClick);
    this._todayButton.element.classList.add(
        CalendarHeaderView.GetClassNameTodayButton());
    var monthContainingToday = Month.createFromToday();
    this._todayButton.setDisabled(
        monthContainingToday < this.calendarPicker.minimumMonth ||
        monthContainingToday > this.calendarPicker.maximumMonth);
    this._todayButton.element.setAttribute(
        'aria-label', global.params.todayLabel);
  }

  /**
   * @type {!CalendarNavigationButton}
   * @const
   */
  this._nextMonthButton = new CalendarNavigationButton();
  this._nextMonthButton.attachTo(this);
  this._nextMonthButton.on(
      CalendarNavigationButton.EventTypeButtonClick,
      this.onNavigationButtonClick);
  this._nextMonthButton.on(
      CalendarNavigationButton.EventTypeRepeatingButtonClick,
      this.onNavigationButtonClick);
  this._nextMonthButton.element.setAttribute(
      'aria-label', global.params.axShowNextMonth);

  if (global.params.isLocaleRTL) {
    this._nextMonthButton.element.innerHTML =
        CalendarHeaderView.GetBackwardTriangle();
    this._previousMonthButton.element.innerHTML =
        CalendarHeaderView.GetForwardTriangle();
  } else {
    this._nextMonthButton.element.innerHTML =
        CalendarHeaderView.GetForwardTriangle();
    this._previousMonthButton.element.innerHTML =
        CalendarHeaderView.GetBackwardTriangle();
  }
}

CalendarHeaderView.prototype = Object.create(View.prototype);

CalendarHeaderView.Height = 24;
CalendarHeaderView.BottomMargin = 10;
CalendarHeaderView._ForwardTriangle =
    '<svg width=\'4\' height=\'7\'><polygon points=\'0,7 0,0, 4,3.5\' style=\'fill:#6e6e6e;\' /></svg>';
CalendarHeaderView._ForwardTriangleRefresh =
    '<svg width=\"16\" height=\"16\" viewBox=\"0 0 16 16\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\">\
    <path d=\"M15.3516 8.60156L8 15.9531L0.648438 8.60156L1.35156 7.89844L7.5 14.0469V0H8.5V14.0469L14.6484 7.89844L15.3516 8.60156Z\" fill=\"#101010\"/>\
    </svg>'
CalendarHeaderView.GetForwardTriangle = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarHeaderView._ForwardTriangleRefresh;
  }
  return CalendarHeaderView._ForwardTriangle;
};
CalendarHeaderView._BackwardTriangle =
    '<svg width=\'4\' height=\'7\'><polygon points=\'0,3.5 4,7 4,0\' style=\'fill:#6e6e6e;\' /></svg>';
CalendarHeaderView._BackwardTriangleRefresh =
    '<svg width=\"16\" height=\"16\" viewBox=\"0 0 16 16\" fill=\"none\" xmlns=\"http://www.w3.org/2000/svg\">\
    <path d=\"M14.6484 8.10156L8.5 1.95312V16H7.5V1.95312L1.35156 8.10156L0.648438 7.39844L8 0.046875L15.3516 7.39844L14.6484 8.10156Z\" fill=\"#101010\"/>\
    </svg>'
CalendarHeaderView.GetBackwardTriangle = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarHeaderView._BackwardTriangleRefresh;
  }
  return CalendarHeaderView._BackwardTriangle;
};
CalendarHeaderView.ClassNameCalendarHeaderView = 'calendar-header-view';
CalendarHeaderView.ClassNameCalendarTitle = 'calendar-title';
CalendarHeaderView.ClassNameTodayButton = 'today-button';
CalendarHeaderView.ClassNameTodayButtonRefresh = 'today-button-refresh';
CalendarHeaderView.GetClassNameTodayButton = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarHeaderView.ClassNameTodayButtonRefresh;
  }
  return CalendarHeaderView.ClassNameTodayButton;
};

CalendarHeaderView.prototype.onCurrentMonthChanged = function() {
  this.monthPopupButton.setCurrentMonth(this.calendarPicker.currentMonth());
  this._previousMonthButton.setDisabled(
      this.disabled ||
      this.calendarPicker.currentMonth() <= this.calendarPicker.minimumMonth);
  this._nextMonthButton.setDisabled(
      this.disabled ||
      this.calendarPicker.currentMonth() >= this.calendarPicker.maximumMonth);
};

CalendarHeaderView.prototype.onNavigationButtonClick = function(sender) {
  if (sender === this._previousMonthButton)
    this.calendarPicker.setCurrentMonth(
        this.calendarPicker.currentMonth().previous(),
        CalendarPicker.NavigationBehavior.WithAnimation);
  else if (sender === this._nextMonthButton)
    this.calendarPicker.setCurrentMonth(
        this.calendarPicker.currentMonth().next(),
        CalendarPicker.NavigationBehavior.WithAnimation);
  else
    this.calendarPicker.selectRangeContainingDay(Day.createFromToday());
};

/**
 * @param {!boolean} disabled
 */
CalendarHeaderView.prototype.setDisabled = function(disabled) {
  this.disabled = disabled;
  if (global.params.isFormControlsRefreshEnabled) {
    this._previousMonthButton.element.style.visibility =
        this.disabled ? 'hidden' : 'visible';
    this._nextMonthButton.element.style.visibility =
        this.disabled ? 'hidden' : 'visible';
  }

  this.monthPopupButton.element.disabled = this.disabled;
  this._previousMonthButton.setDisabled(
      this.disabled ||
      this.calendarPicker.currentMonth() <= this.calendarPicker.minimumMonth);
  this._nextMonthButton.setDisabled(
      this.disabled ||
      this.calendarPicker.currentMonth() >= this.calendarPicker.maximumMonth);
  if (this._todayButton) {
    var monthContainingToday = Month.createFromToday();
    this._todayButton.setDisabled(
        this.disabled ||
        monthContainingToday < this.calendarPicker.minimumMonth ||
        monthContainingToday > this.calendarPicker.maximumMonth);
  }
};

/**
 * @constructor
 * @extends ListCell
 */
function DayCell() {
  ListCell.call(this);
  this.element.classList.add(DayCell.ClassNameDayCell);
  this.element.style.width = DayCell.GetWidth() + 'px';
  this.element.style.height = DayCell.GetHeight() + 'px';
  this.element.style.lineHeight =
      (DayCell.GetHeight() - DayCell.PaddingSize * 2) + 'px';
  this.element.setAttribute('role', 'gridcell');
  /**
   * @type {?Day}
   */
  this.day = null;
};

DayCell.prototype = Object.create(ListCell.prototype);

DayCell._Width = 34;
DayCell._WidthRefresh = 28;
DayCell.GetWidth = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return DayCell._WidthRefresh;
  }
  return DayCell._Width;
};
DayCell._Height = hasInaccuratePointingDevice() ? 34 : 20;
DayCell._HeightRefresh = 28;
DayCell.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return DayCell._HeightRefresh;
  }
  return DayCell._Height;
};
DayCell.PaddingSize = 1;
DayCell.ClassNameDayCell = 'day-cell';
DayCell.ClassNameHighlighted = 'highlighted';
DayCell.ClassNameDisabled = 'disabled';
DayCell.ClassNameCurrentMonth = 'current-month';
DayCell.ClassNameToday = 'today';

DayCell._recycleBin = [];

DayCell.recycleOrCreate = function() {
  return DayCell._recycleBin.pop() || new DayCell();
};

/**
 * @return {!Array}
 * @override
 */
DayCell.prototype._recycleBin = function() {
  return DayCell._recycleBin;
};

/**
 * @override
 */
DayCell.prototype.throwAway = function() {
  ListCell.prototype.throwAway.call(this);
  this.day = null;
};

/**
 * @param {!boolean} highlighted
 */
DayCell.prototype.setHighlighted = function(highlighted) {
  if (highlighted) {
    this.element.classList.add(DayCell.ClassNameHighlighted);
    this.element.setAttribute('aria-selected', 'true');
  } else {
    this.element.classList.remove(DayCell.ClassNameHighlighted);
    this.element.setAttribute('aria-selected', 'false');
  }
};

/**
 * @param {!boolean} disabled
 */
DayCell.prototype.setDisabled = function(disabled) {
  if (disabled)
    this.element.classList.add(DayCell.ClassNameDisabled);
  else
    this.element.classList.remove(DayCell.ClassNameDisabled);
};

/**
 * @param {!boolean} selected
 */
DayCell.prototype.setIsInCurrentMonth = function(selected) {
  if (selected)
    this.element.classList.add(DayCell.ClassNameCurrentMonth);
  else
    this.element.classList.remove(DayCell.ClassNameCurrentMonth);
};

/**
 * @param {!boolean} selected
 */
DayCell.prototype.setIsToday = function(selected) {
  if (selected)
    this.element.classList.add(DayCell.ClassNameToday);
  else
    this.element.classList.remove(DayCell.ClassNameToday);
};

/**
 * @param {!Day} day
 */
DayCell.prototype.reset = function(day) {
  this.day = day;
  this.element.textContent = localizeNumber(this.day.date.toString());
  this.element.setAttribute('aria-label', this.day.format());
  this.element.id = this.day.toString();
  this.show();
};

/**
 * @constructor
 * @extends ListCell
 */
function WeekNumberCell() {
  ListCell.call(this);
  this.element.classList.add(WeekNumberCell.ClassNameWeekNumberCell);
  this.element.style.width =
      (WeekNumberCell.Width - WeekNumberCell.SeparatorWidth) + 'px';
  this.element.style.height = WeekNumberCell.GetHeight() + 'px';
  this.element.style.lineHeight =
      (WeekNumberCell.GetHeight() - WeekNumberCell.PaddingSize * 2) + 'px';
  /**
   * @type {?Week}
   */
  this.week = null;
};

WeekNumberCell.prototype = Object.create(ListCell.prototype);

WeekNumberCell.Width = 48;
WeekNumberCell._Height = DayCell._Height;
WeekNumberCell._HeightRefresh = DayCell._HeightRefresh;
WeekNumberCell.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return WeekNumberCell._HeightRefresh;
  }
  return WeekNumberCell._Height;
};
WeekNumberCell.SeparatorWidth = 1;
WeekNumberCell.PaddingSize = 1;
WeekNumberCell.ClassNameWeekNumberCell = 'week-number-cell';
WeekNumberCell.ClassNameHighlighted = 'highlighted';
WeekNumberCell.ClassNameDisabled = 'disabled';

WeekNumberCell._recycleBin = [];

/**
 * @return {!Array}
 * @override
 */
WeekNumberCell.prototype._recycleBin = function() {
  return WeekNumberCell._recycleBin;
};

/**
 * @return {!WeekNumberCell}
 */
WeekNumberCell.recycleOrCreate = function() {
  return WeekNumberCell._recycleBin.pop() || new WeekNumberCell();
};

/**
 * @param {!Week} week
 */
WeekNumberCell.prototype.reset = function(week) {
  this.week = week;
  this.element.id = week.toString();
  this.element.setAttribute('role', 'gridcell');
  this.element.setAttribute(
      'aria-label',
      window.pagePopupController.formatWeek(
          week.year, week.week, week.firstDay().format()));
  this.element.textContent = localizeNumber(this.week.week.toString());
  this.show();
};

/**
 * @override
 */
WeekNumberCell.prototype.throwAway = function() {
  ListCell.prototype.throwAway.call(this);
  this.week = null;
};

WeekNumberCell.prototype.setHighlighted = function(highlighted) {
  if (highlighted) {
    this.element.classList.add(WeekNumberCell.ClassNameHighlighted);
    this.element.setAttribute('aria-selected', 'true');
  } else {
    this.element.classList.remove(WeekNumberCell.ClassNameHighlighted);
    this.element.setAttribute('aria-selected', 'false');
  }
};

WeekNumberCell.prototype.setDisabled = function(disabled) {
  if (disabled)
    this.element.classList.add(WeekNumberCell.ClassNameDisabled);
  else
    this.element.classList.remove(WeekNumberCell.ClassNameDisabled);
};

/**
 * @constructor
 * @extends View
 * @param {!boolean} hasWeekNumberColumn
 */
function CalendarTableHeaderView(hasWeekNumberColumn) {
  View.call(this, createElement('div', 'calendar-table-header-view'));
  if (hasWeekNumberColumn) {
    var weekNumberLabelElement =
        createElement('div', 'week-number-label', global.params.weekLabel);
    weekNumberLabelElement.style.width = WeekNumberCell.Width + 'px';
    this.element.appendChild(weekNumberLabelElement);
  }
  for (var i = 0; i < DaysPerWeek; ++i) {
    var weekDayNumber = (global.params.weekStartDay + i) % DaysPerWeek;
    var labelElement = createElement(
        'div', 'week-day-label', global.params.dayLabels[weekDayNumber]);
    labelElement.style.width = DayCell.GetWidth() + 'px';
    this.element.appendChild(labelElement);
    if (getLanguage() === 'ja') {
      if (weekDayNumber === 0)
        labelElement.style.color = 'red';
      else if (weekDayNumber === 6)
        labelElement.style.color = 'blue';
    }
  }
}

CalendarTableHeaderView.prototype = Object.create(View.prototype);

CalendarTableHeaderView._Height = 25;
CalendarTableHeaderView._HeightRefresh = 29;
CalendarTableHeaderView.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarTableHeaderView._HeightRefresh;
  }
  return CalendarTableHeaderView._Height;
};

/**
 * @constructor
 * @extends ListCell
 */
function CalendarRowCell() {
  ListCell.call(this);
  this.element.classList.add(CalendarRowCell.ClassNameCalendarRowCell);
  this.element.style.height = CalendarRowCell.GetHeight() + 'px';
  this.element.setAttribute('role', 'row');

  /**
   * @type {!Array}
   * @protected
   */
  this._dayCells = [];
  /**
   * @type {!number}
   */
  this.row = 0;
  /**
   * @type {?CalendarTableView}
   */
  this.calendarTableView = null;
}

CalendarRowCell.prototype = Object.create(ListCell.prototype);

CalendarRowCell._Height = DayCell._Height;
CalendarRowCell._HeightRefresh = DayCell._HeightRefresh;
CalendarRowCell.GetHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarRowCell._HeightRefresh;
  }
  return CalendarRowCell._Height;
};
CalendarRowCell.ClassNameCalendarRowCell = 'calendar-row-cell';

CalendarRowCell._recycleBin = [];

/**
 * @return {!Array}
 * @override
 */
CalendarRowCell.prototype._recycleBin = function() {
  return CalendarRowCell._recycleBin;
};

/**
 * @param {!number} row
 * @param {!CalendarTableView} calendarTableView
 */
CalendarRowCell.prototype.reset = function(row, calendarTableView) {
  this.row = row;
  this.calendarTableView = calendarTableView;
  if (this.calendarTableView.hasWeekNumberColumn) {
    var middleDay = this.calendarTableView.dayAtColumnAndRow(3, row);
    var week = Week.createFromDay(middleDay);
    this.weekNumberCell = this.calendarTableView.prepareNewWeekNumberCell(week);
    this.weekNumberCell.attachTo(this);
  }
  var day = calendarTableView.dayAtColumnAndRow(0, row);
  for (var i = 0; i < DaysPerWeek; ++i) {
    var dayCell = this.calendarTableView.prepareNewDayCell(day);
    dayCell.attachTo(this);
    this._dayCells.push(dayCell);
    day = day.next();
  }
  this.show();
};

/**
 * @override
 */
CalendarRowCell.prototype.throwAway = function() {
  ListCell.prototype.throwAway.call(this);
  if (this.weekNumberCell)
    this.calendarTableView.throwAwayWeekNumberCell(this.weekNumberCell);
  this._dayCells.forEach(
      this.calendarTableView.throwAwayDayCell, this.calendarTableView);
  this._dayCells.length = 0;
};

/**
 * @constructor
 * @extends ListView
 * @param {!CalendarPicker} calendarPicker
 */
function CalendarTableView(calendarPicker) {
  ListView.call(this);
  this.element.classList.add(CalendarTableView.ClassNameCalendarTableView);
  this.element.tabIndex = 0;

  /**
   * @type {!boolean}
   * @const
   */
  this.hasWeekNumberColumn = calendarPicker.type === 'week';
  /**
   * @type {!CalendarPicker}
   * @const
   */
  this.calendarPicker = calendarPicker;
  /**
   * @type {!Object}
   * @const
   */
  this._dayCells = {};
  var headerView = new CalendarTableHeaderView(this.hasWeekNumberColumn);
  headerView.attachTo(this, this.scrollView);

  if (global.params.isFormControlsRefreshEnabled) {
    /**
     * @type {!CalendarNavigationButton}
     * @const
     */
    var todayButton = new CalendarNavigationButton();
    todayButton.attachTo(this);
    todayButton.on(
        CalendarNavigationButton.EventTypeButtonClick, this.onTodayButtonClick);
    todayButton.element.textContent = global.params.todayLabel;
    todayButton.element.classList.add(
        CalendarHeaderView.GetClassNameTodayButton());
    var monthContainingToday = Month.createFromToday();
    todayButton.setDisabled(
        monthContainingToday < this.calendarPicker.minimumMonth ||
        monthContainingToday > this.calendarPicker.maximumMonth);
    todayButton.element.setAttribute('aria-label', global.params.todayLabel);
  }


  if (this.hasWeekNumberColumn) {
    this.setWidth(DayCell.GetWidth() * DaysPerWeek + WeekNumberCell.Width);
    /**
     * @type {?Array}
     * @const
     */
    this._weekNumberCells = [];
  } else {
    this.setWidth(DayCell.GetWidth() * DaysPerWeek);
  }

  /**
   * @type {!boolean}
   * @protected
   */
  this._ignoreMouseOutUntillNextMouseOver = false;

  this.element.addEventListener('click', this.onClick, false);
  this.element.addEventListener('mouseover', this.onMouseOver, false);
  this.element.addEventListener('mouseout', this.onMouseOut, false);

  // You shouldn't be able to use the mouse wheel to scroll.
  this.scrollView.element.removeEventListener(
      'mousewheel', this.scrollView.onMouseWheel, false);
  // You shouldn't be able to do gesture scroll.
  this.scrollView.element.removeEventListener(
      'touchstart', this.scrollView.onTouchStart, false);
}

CalendarTableView.prototype = Object.create(ListView.prototype);

CalendarTableView._BorderWidth = 1;
CalendarTableView._BorderWidthRefresh = 0;
CalendarTableView.GetBorderWidth = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarTableView._BorderWidthRefresh;
  }
  return CalendarTableView._BorderWidth;
};
CalendarTableView._TodayButtonHeight = 0;
CalendarTableView._TodayButtonHeightRefresh = 28;
CalendarTableView.GetTodayButtonHeight = function() {
  if (global.params.isFormControlsRefreshEnabled) {
    return CalendarTableView._TodayButtonHeightRefresh;
  }
  return CalendarTableView._TodayButtonHeight;
};
CalendarTableView.ClassNameCalendarTableView = 'calendar-table-view';

/**
 * @param {!number} scrollOffset
 * @return {!number}
 */
CalendarTableView.prototype.rowAtScrollOffset = function(scrollOffset) {
  return Math.floor(scrollOffset / CalendarRowCell.GetHeight());
};

/**
 * @param {!number} row
 * @return {!number}
 */
CalendarTableView.prototype.scrollOffsetForRow = function(row) {
  return row * CalendarRowCell.GetHeight();
};

/**
 * @param {?Event} event
 */
CalendarTableView.prototype.onClick = function(event) {
  if (this.hasWeekNumberColumn) {
    var weekNumberCellElement = enclosingNodeOrSelfWithClass(
        event.target, WeekNumberCell.ClassNameWeekNumberCell);
    if (weekNumberCellElement) {
      var weekNumberCell = weekNumberCellElement.$view;
      this.calendarPicker.selectRangeContainingDay(
          weekNumberCell.week.firstDay());
      return;
    }
  }
  var dayCellElement =
      enclosingNodeOrSelfWithClass(event.target, DayCell.ClassNameDayCell);
  if (!dayCellElement)
    return;
  var dayCell = dayCellElement.$view;
  this.calendarPicker.selectRangeContainingDay(dayCell.day);
};

CalendarTableView.prototype.onTodayButtonClick = function(sender) {
  this.calendarPicker.selectRangeContainingDay(Day.createFromToday());
};

/**
 * @param {?Event} event
 */
CalendarTableView.prototype.onMouseOver = function(event) {
  if (this.hasWeekNumberColumn) {
    var weekNumberCellElement = enclosingNodeOrSelfWithClass(
        event.target, WeekNumberCell.ClassNameWeekNumberCell);
    if (weekNumberCellElement) {
      var weekNumberCell = weekNumberCellElement.$view;
      this.calendarPicker.highlightRangeContainingDay(
          weekNumberCell.week.firstDay());
      this._ignoreMouseOutUntillNextMouseOver = false;
      return;
    }
  }
  var dayCellElement =
      enclosingNodeOrSelfWithClass(event.target, DayCell.ClassNameDayCell);
  if (!dayCellElement)
    return;
  var dayCell = dayCellElement.$view;
  this.calendarPicker.highlightRangeContainingDay(dayCell.day);
  this._ignoreMouseOutUntillNextMouseOver = false;
};

/**
 * @param {?Event} event
 */
CalendarTableView.prototype.onMouseOut = function(event) {
  if (this._ignoreMouseOutUntillNextMouseOver)
    return;
  var dayCellElement =
      enclosingNodeOrSelfWithClass(event.target, DayCell.ClassNameDayCell);
  if (!dayCellElement) {
    this.calendarPicker.highlightRangeContainingDay(null);
  }
};

/**
 * @param {!number} row
 * @return {!CalendarRowCell}
 */
CalendarTableView.prototype.prepareNewCell = function(row) {
  var cell = CalendarRowCell._recycleBin.pop() || new CalendarRowCell();
  cell.reset(row, this);
  return cell;
};

/**
 * @return {!number} Height in pixels.
 */
CalendarTableView.prototype.height = function() {
  return this.scrollView.height() + CalendarTableHeaderView.GetHeight() +
      CalendarTableView.GetBorderWidth() * 2 +
      CalendarTableView.GetTodayButtonHeight();
};

/**
 * @param {!number} height Height in pixels.
 */
CalendarTableView.prototype.setHeight = function(height) {
  this.scrollView.setHeight(
      height - CalendarTableHeaderView.GetHeight() -
      CalendarTableView.GetBorderWidth() * 2 -
      CalendarTableView.GetTodayButtonHeight());
  if (global.params.isFormControlsRefreshEnabled) {
    this.element.style.height = height + 'px';
  }
};

/**
 * @param {!Month} month
 * @param {!boolean} animate
 */
CalendarTableView.prototype.scrollToMonth = function(month, animate) {
  var rowForFirstDayInMonth = this.columnAndRowForDay(month.firstDay()).row;
  this.scrollView.scrollTo(
      this.scrollOffsetForRow(rowForFirstDayInMonth), animate);
};

/**
 * @param {!number} column
 * @param {!number} row
 * @return {!Day}
 */
CalendarTableView.prototype.dayAtColumnAndRow = function(column, row) {
  var daysSinceMinimum = row * DaysPerWeek + column +
      global.params.weekStartDay - CalendarTableView._MinimumDayWeekDay;
  return Day.createFromValue(
      daysSinceMinimum * MillisecondsPerDay +
      CalendarTableView._MinimumDayValue);
};

CalendarTableView._MinimumDayValue = Day.Minimum.valueOf();
CalendarTableView._MinimumDayWeekDay = Day.Minimum.weekDay();

/**
 * @param {!Day} day
 * @return {!Object} Object with properties column and row.
 */
CalendarTableView.prototype.columnAndRowForDay = function(day) {
  var daysSinceMinimum =
      (day.valueOf() - CalendarTableView._MinimumDayValue) / MillisecondsPerDay;
  var offset = daysSinceMinimum + CalendarTableView._MinimumDayWeekDay -
      global.params.weekStartDay;
  var row = Math.floor(offset / DaysPerWeek);
  var column = offset - row * DaysPerWeek;
  return {column: column, row: row};
};

CalendarTableView.prototype.updateCells = function() {
  ListView.prototype.updateCells.call(this);

  var selection = this.calendarPicker.selection();
  var firstDayInSelection;
  var lastDayInSelection;
  if (selection) {
    firstDayInSelection = selection.firstDay().valueOf();
    lastDayInSelection = selection.lastDay().valueOf();
  } else {
    firstDayInSelection = Infinity;
    lastDayInSelection = Infinity;
  }
  var highlight = this.calendarPicker.highlight();
  var firstDayInHighlight;
  var lastDayInHighlight;
  if (highlight) {
    firstDayInHighlight = highlight.firstDay().valueOf();
    lastDayInHighlight = highlight.lastDay().valueOf();
  } else {
    firstDayInHighlight = Infinity;
    lastDayInHighlight = Infinity;
  }
  var currentMonth = this.calendarPicker.currentMonth();
  var firstDayInCurrentMonth = currentMonth.firstDay().valueOf();
  var lastDayInCurrentMonth = currentMonth.lastDay().valueOf();
  var activeCell = null;
  for (var dayString in this._dayCells) {
    var dayCell = this._dayCells[dayString];
    var day = dayCell.day;
    dayCell.setIsToday(Day.createFromToday().equals(day));
    dayCell.setSelected(
        day >= firstDayInSelection && day <= lastDayInSelection);
    var isHighlighted = day >= firstDayInHighlight && day <= lastDayInHighlight;
    dayCell.setHighlighted(isHighlighted);
    if (isHighlighted) {
      if (firstDayInHighlight == lastDayInHighlight)
        activeCell = dayCell;
      else if (
          this.calendarPicker.type == 'month' && day == firstDayInHighlight)
        activeCell = dayCell;
    }
    dayCell.setIsInCurrentMonth(
        day >= firstDayInCurrentMonth && day <= lastDayInCurrentMonth);
    dayCell.setDisabled(!this.calendarPicker.isValidDay(day));
  }
  if (this.hasWeekNumberColumn) {
    for (var weekString in this._weekNumberCells) {
      var weekNumberCell = this._weekNumberCells[weekString];
      var week = weekNumberCell.week;
      var isWeekHighlighted = highlight && highlight.equals(week);
      weekNumberCell.setSelected(selection && selection.equals(week));
      weekNumberCell.setHighlighted(isWeekHighlighted);
      if (isWeekHighlighted)
        activeCell = weekNumberCell;
      weekNumberCell.setDisabled(!this.calendarPicker.isValid(week));
    }
  }
  if (activeCell) {
    // Ensure a layoutObject because an element with no layoutObject doesn't post
    // activedescendant events. This shouldn't run in the above |for| loop
    // to avoid CSS transition.
    activeCell.element.offsetLeft;
    this.element.setAttribute('aria-activedescendant', activeCell.element.id);
  }
};

/**
 * @param {!Day} day
 * @return {!DayCell}
 */
CalendarTableView.prototype.prepareNewDayCell = function(day) {
  var dayCell = DayCell.recycleOrCreate();
  dayCell.reset(day);
  if (this.calendarPicker.type == 'month')
    dayCell.element.setAttribute(
        'aria-label', Month.createFromDay(day).toLocaleString());
  this._dayCells[dayCell.day.toString()] = dayCell;
  return dayCell;
};

/**
 * @param {!Week} week
 * @return {!WeekNumberCell}
 */
CalendarTableView.prototype.prepareNewWeekNumberCell = function(week) {
  var weekNumberCell = WeekNumberCell.recycleOrCreate();
  weekNumberCell.reset(week);
  this._weekNumberCells[weekNumberCell.week.toString()] = weekNumberCell;
  return weekNumberCell;
};

/**
 * @param {!DayCell} dayCell
 */
CalendarTableView.prototype.throwAwayDayCell = function(dayCell) {
  delete this._dayCells[dayCell.day.toString()];
  dayCell.throwAway();
};

/**
 * @param {!WeekNumberCell} weekNumberCell
 */
CalendarTableView.prototype.throwAwayWeekNumberCell = function(weekNumberCell) {
  delete this._weekNumberCells[weekNumberCell.week.toString()];
  weekNumberCell.throwAway();
};

/**
 * @constructor
 * @extends View
 * @param {!Object} config
 */
function CalendarPicker(type, config) {
  View.call(this, createElement('div', CalendarPicker.ClassNameCalendarPicker));
  this.element.classList.add(CalendarPicker.ClassNamePreparing);

  /**
   * @type {!string}
   * @const
   */
  this.type = type;
  if (this.type === 'week')
    this._dateTypeConstructor = Week;
  else if (this.type === 'month')
    this._dateTypeConstructor = Month;
  else
    this._dateTypeConstructor = Day;
  /**
   * @type {!Object}
   * @const
   */
  this.config = {};
  this._setConfig(config);
  /**
   * @type {!Month}
   * @const
   */
  this.minimumMonth = Month.createFromDay(this.config.minimum.firstDay());
  /**
   * @type {!Month}
   * @const
   */
  this.maximumMonth = Month.createFromDay(this.config.maximum.lastDay());
  if (global.params.isLocaleRTL)
    this.element.classList.add('rtl');
  /**
   * @type {!CalendarTableView}
   * @const
   */
  this.calendarTableView = new CalendarTableView(this);
  this.calendarTableView.hasNumberColumn = this.type === 'week';
  /**
   * @type {!CalendarHeaderView}
   * @const
   */
  this.calendarHeaderView = new CalendarHeaderView(this);
  this.calendarHeaderView.monthPopupButton.on(
      MonthPopupButton.EventTypeButtonClick, this.onMonthPopupButtonClick);
  /**
   * @type {!MonthPopupView}
   * @const
   */
  this.monthPopupView =
      new MonthPopupView(this.minimumMonth, this.maximumMonth);
  this.monthPopupView.yearListView.on(
      YearListView.EventTypeYearListViewDidSelectMonth,
      this.onYearListViewDidSelectMonth);
  this.monthPopupView.yearListView.on(
      YearListView.EventTypeYearListViewDidHide, this.onYearListViewDidHide);
  this.calendarHeaderView.attachTo(this);
  this.calendarTableView.attachTo(this);
  /**
   * @type {!Month}
   * @protected
   */
  this._currentMonth = new Month(NaN, NaN);
  /**
   * @type {?DateType}
   * @protected
   */
  this._selection = null;
  /**
   * @type {?DateType}
   * @protected
   */
  this._highlight = null;
  this.calendarTableView.element.addEventListener(
      'keydown', this.onCalendarTableKeyDown, false);
  document.body.addEventListener('keydown', this.onBodyKeyDown, false);

  window.addEventListener('resize', this.onWindowResize, false);

  /**
   * @type {!number}
   * @protected
   */
  this._height = -1;

  var initialSelection = parseDateString(config.currentValue);
  if (initialSelection) {
    this.setCurrentMonth(
        Month.createFromDay(initialSelection.middleDay()),
        CalendarPicker.NavigationBehavior.None);
    this.setSelection(initialSelection);
  } else
    this.setCurrentMonth(
        Month.createFromToday(), CalendarPicker.NavigationBehavior.None);
}

CalendarPicker.prototype = Object.create(View.prototype);

CalendarPicker.Padding = 10;
CalendarPicker.BorderWidth = 1;
CalendarPicker.ClassNameCalendarPicker = 'calendar-picker';
CalendarPicker.ClassNamePreparing = 'preparing';
CalendarPicker.EventTypeCurrentMonthChanged = 'currentMonthChanged';
CalendarPicker.commitDelayMs = 100;
CalendarPicker.VisibleRowsRefresh = 6;

/**
 * @param {!Event} event
 */
CalendarPicker.prototype.onWindowResize = function(event) {
  this.element.classList.remove(CalendarPicker.ClassNamePreparing);
  window.removeEventListener('resize', this.onWindowResize, false);
};

/**
 * @param {!YearListView} sender
 */
CalendarPicker.prototype.onYearListViewDidHide = function(sender) {
  this.monthPopupView.hide();
  this.calendarHeaderView.setDisabled(false);
  if (global.params.isFormControlsRefreshEnabled) {
    this.calendarTableView.element.style.visibility = 'visible';
  } else {
    this.adjustHeight();
  }
};

/**
 * @param {!YearListView} sender
 * @param {!Month} month
 */
CalendarPicker.prototype.onYearListViewDidSelectMonth = function(
    sender, month) {
  this.setCurrentMonth(month, CalendarPicker.NavigationBehavior.None);
};

/**
 * @param {!View|Node} parent
 * @param {?View|Node=} before
 * @override
 */
CalendarPicker.prototype.attachTo = function(parent, before) {
  View.prototype.attachTo.call(this, parent, before);
  this.calendarTableView.element.focus();
};

CalendarPicker.prototype.cleanup = function() {
  window.removeEventListener('resize', this.onWindowResize, false);
  this.calendarTableView.element.removeEventListener(
      'keydown', this.onBodyKeyDown, false);
  // Month popup view might be attached to document.body.
  this.monthPopupView.hide();
};

/**
 * @param {?MonthPopupButton} sender
 */
CalendarPicker.prototype.onMonthPopupButtonClick = function(sender) {
  var clientRect = this.calendarTableView.element.getBoundingClientRect();
  var calendarTableRect = new Rectangle(
      clientRect.left + document.body.scrollLeft,
      clientRect.top + document.body.scrollTop, clientRect.width,
      clientRect.height);
  this.monthPopupView.show(this.currentMonth(), calendarTableRect);
  this.calendarHeaderView.setDisabled(true);
  if (global.params.isFormControlsRefreshEnabled) {
    this.calendarTableView.element.style.visibility = 'hidden';
  } else {
    this.adjustHeight();
  }
};

CalendarPicker.prototype._setConfig = function(config) {
  this.config.minimum = (typeof config.min !== 'undefined' && config.min) ?
      parseDateString(config.min) :
      this._dateTypeConstructor.Minimum;
  this.config.maximum = (typeof config.max !== 'undefined' && config.max) ?
      parseDateString(config.max) :
      this._dateTypeConstructor.Maximum;
  this.config.minimumValue = this.config.minimum.valueOf();
  this.config.maximumValue = this.config.maximum.valueOf();
  this.config.step = (typeof config.step !== undefined) ?
      Number(config.step) :
      this._dateTypeConstructor.DefaultStep;
  this.config.stepBase = (typeof config.stepBase !== 'undefined') ?
      Number(config.stepBase) :
      this._dateTypeConstructor.DefaultStepBase;
};

/**
 * @return {!Month}
 */
CalendarPicker.prototype.currentMonth = function() {
  return this._currentMonth;
};

/**
 * @enum {number}
 */
CalendarPicker.NavigationBehavior = {
  None: 0,
  WithAnimation: 1
};

/**
 * @param {!Month} month
 * @param {!CalendarPicker.NavigationBehavior} animate
 */
CalendarPicker.prototype.setCurrentMonth = function(month, behavior) {
  if (month > this.maximumMonth)
    month = this.maximumMonth;
  else if (month < this.minimumMonth)
    month = this.minimumMonth;
  if (this._currentMonth.equals(month))
    return;
  this._currentMonth = month;
  this.calendarTableView.scrollToMonth(
      this._currentMonth,
      behavior === CalendarPicker.NavigationBehavior.WithAnimation);
  this.adjustHeight();
  this.calendarTableView.setNeedsUpdateCells(true);
  this.dispatchEvent(
      CalendarPicker.EventTypeCurrentMonthChanged, {target: this});
};

CalendarPicker.prototype.adjustHeight = function() {
  var rowForFirstDayInMonth =
      this.calendarTableView.columnAndRowForDay(this._currentMonth.firstDay())
          .row;
  var rowForLastDayInMonth =
      this.calendarTableView.columnAndRowForDay(this._currentMonth.lastDay())
          .row;
  var numberOfRows = global.params.isFormControlsRefreshEnabled ?
      CalendarPicker.VisibleRowsRefresh :
      rowForLastDayInMonth - rowForFirstDayInMonth + 1;
  var calendarTableViewHeight = CalendarTableHeaderView.GetHeight() +
      numberOfRows * DayCell.GetHeight() +
      CalendarTableView.GetBorderWidth() * 2 +
      CalendarTableView.GetTodayButtonHeight();
  var height = (this.monthPopupView.isVisible &&
                        !global.params.isFormControlsRefreshEnabled ?
                    YearListView.GetHeight() :
                    calendarTableViewHeight) +
      CalendarHeaderView.Height + CalendarHeaderView.BottomMargin +
      CalendarPicker.Padding * 2 + CalendarPicker.BorderWidth * 2;
  this.setHeight(height);
};

CalendarPicker.prototype.selection = function() {
  return this._selection;
};

CalendarPicker.prototype.highlight = function() {
  return this._highlight;
};

/**
 * @return {!Day}
 */
CalendarPicker.prototype.firstVisibleDay = function() {
  var firstVisibleRow =
      this.calendarTableView.columnAndRowForDay(this.currentMonth().firstDay())
          .row;
  var firstVisibleDay =
      this.calendarTableView.dayAtColumnAndRow(0, firstVisibleRow);
  if (!firstVisibleDay)
    firstVisibleDay = Day.Minimum;
  return firstVisibleDay;
};

/**
 * @return {!Day}
 */
CalendarPicker.prototype.lastVisibleDay = function() {
  var lastVisibleRow =
      this.calendarTableView.columnAndRowForDay(this.currentMonth().lastDay())
          .row;
  if (global.params.isFormControlsRefreshEnabled) {
    lastVisibleRow = this.calendarTableView
                         .columnAndRowForDay(this.currentMonth().firstDay())
                         .row +
        CalendarPicker.VisibleRowsRefresh - 1;
  }
  var lastVisibleDay =
      this.calendarTableView.dayAtColumnAndRow(DaysPerWeek - 1, lastVisibleRow);
  if (!lastVisibleDay)
    lastVisibleDay = Day.Maximum;
  return lastVisibleDay;
};

/**
 * @param {?Day} day
 */
CalendarPicker.prototype.selectRangeContainingDay = function(day) {
  var selection = day ? this._dateTypeConstructor.createFromDay(day) : null;
  this.setSelectionAndCommit(selection);
};

/**
 * @param {?Day} day
 */
CalendarPicker.prototype.highlightRangeContainingDay = function(day) {
  var highlight = day ? this._dateTypeConstructor.createFromDay(day) : null;
  this._setHighlight(highlight);
};

/**
 * Select the specified date.
 * @param {?DateType} dayOrWeekOrMonth
 */
CalendarPicker.prototype.setSelection = function(dayOrWeekOrMonth) {
  if (!this._selection && !dayOrWeekOrMonth)
    return;
  if (this._selection && this._selection.equals(dayOrWeekOrMonth))
    return;
  var firstDayInSelection = dayOrWeekOrMonth.firstDay();
  var lastDayInSelection = dayOrWeekOrMonth.lastDay();
  var candidateCurrentMonth = Month.createFromDay(firstDayInSelection);
  if (this.firstVisibleDay() > lastDayInSelection ||
      this.lastVisibleDay() < firstDayInSelection) {
    // Change current month if the selection is not visible at all.
    this.setCurrentMonth(
        candidateCurrentMonth, CalendarPicker.NavigationBehavior.WithAnimation);
  } else if (
      this.firstVisibleDay() < firstDayInSelection ||
      this.lastVisibleDay() > lastDayInSelection) {
    // If the selection is partly visible, only change the current month if
    // doing so will make the whole selection visible.
    var firstVisibleRow =
        this.calendarTableView
            .columnAndRowForDay(candidateCurrentMonth.firstDay())
            .row;
    var firstVisibleDay =
        this.calendarTableView.dayAtColumnAndRow(0, firstVisibleRow);
    var lastVisibleRow =
        this.calendarTableView
            .columnAndRowForDay(candidateCurrentMonth.lastDay())
            .row;
    var lastVisibleDay = this.calendarTableView.dayAtColumnAndRow(
        DaysPerWeek - 1, lastVisibleRow);
    if (firstDayInSelection >= firstVisibleDay &&
        lastDayInSelection <= lastVisibleDay)
      this.setCurrentMonth(
          candidateCurrentMonth,
          CalendarPicker.NavigationBehavior.WithAnimation);
  }
  this._setHighlight(dayOrWeekOrMonth);
  if (!this.isValid(dayOrWeekOrMonth))
    return;
  this._selection = dayOrWeekOrMonth;
  this.monthPopupView.yearListView.setSelectedMonth(
      Month.createFromDay(dayOrWeekOrMonth.middleDay()));
  this.calendarTableView.setNeedsUpdateCells(true);
};

CalendarPicker.prototype.getSelectedValue = function() {
  return this._selection.toString();
};

/**
 * Select the specified date, commit it, and close the popup.
 * @param {?DateType} dayOrWeekOrMonth
 */
CalendarPicker.prototype.setSelectionAndCommit = function(dayOrWeekOrMonth) {
  this.setSelection(dayOrWeekOrMonth);
  // Redraw the widget immidiately, and wait for some time to give feedback to
  // a user.
  this.element.offsetLeft;

  // CalendarPicker doesn't handle the submission when used for datetime-local.
  if (global.params.isFormControlsRefreshEnabled &&
      this.type == 'datetime-local')
    return;

  var value = this._selection.toString();
  if (CalendarPicker.commitDelayMs == 0) {
    // For testing.
    window.pagePopupController.setValueAndClosePopup(0, value);
  } else if (CalendarPicker.commitDelayMs < 0) {
    // For testing.
    window.pagePopupController.setValue(value);
  } else {
    setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, value);
    }, CalendarPicker.commitDelayMs);
  }
};

/**
 * @param {?DateType} dayOrWeekOrMonth
 */
CalendarPicker.prototype._setHighlight = function(dayOrWeekOrMonth) {
  if (!this._highlight && !dayOrWeekOrMonth)
    return;
  if (!dayOrWeekOrMonth && !this._highlight)
    return;
  if (this._highlight && this._highlight.equals(dayOrWeekOrMonth))
    return;
  this._highlight = dayOrWeekOrMonth;
  this.calendarTableView.setNeedsUpdateCells(true);
};

/**
 * @param {!number} value
 * @return {!boolean}
 */
CalendarPicker.prototype._stepMismatch = function(value) {
  var nextAllowedValue =
      Math.ceil((value - this.config.stepBase) / this.config.step) *
          this.config.step +
      this.config.stepBase;
  return nextAllowedValue >= value + this._dateTypeConstructor.DefaultStep;
};

/**
 * @param {!number} value
 * @return {!boolean}
 */
CalendarPicker.prototype._outOfRange = function(value) {
  return value < this.config.minimumValue || value > this.config.maximumValue;
};

/**
 * @param {!DateType} dayOrWeekOrMonth
 * @return {!boolean}
 */
CalendarPicker.prototype.isValid = function(dayOrWeekOrMonth) {
  var value = dayOrWeekOrMonth.valueOf();
  return dayOrWeekOrMonth instanceof this._dateTypeConstructor &&
      !this._outOfRange(value) && !this._stepMismatch(value);
};

/**
 * @param {!Day} day
 * @return {!boolean}
 */
CalendarPicker.prototype.isValidDay = function(day) {
  return this.isValid(this._dateTypeConstructor.createFromDay(day));
};

/**
 * @param {!DateType} dateRange
 * @return {!boolean} Returns true if the highlight was changed.
 */
CalendarPicker.prototype._moveHighlight = function(dateRange) {
  if (!dateRange)
    return false;
  if (this._outOfRange(dateRange.valueOf()))
    return false;
  if (this.firstVisibleDay() > dateRange.middleDay() ||
      this.lastVisibleDay() < dateRange.middleDay())
    this.setCurrentMonth(
        Month.createFromDay(dateRange.middleDay()),
        CalendarPicker.NavigationBehavior.WithAnimation);
  this._setHighlight(dateRange);
  return true;
};

/**
 * @param {?Event} event
 */
CalendarPicker.prototype.onCalendarTableKeyDown = function(event) {
  var key = event.key;
  var eventHandled = false;
  if (key == 't') {
    this.selectRangeContainingDay(Day.createFromToday());
    eventHandled = true;
  } else if (key == 'PageUp') {
    var previousMonth = this.currentMonth().previous();
    if (previousMonth && previousMonth >= this.config.minimumValue) {
      this.setCurrentMonth(
          previousMonth, CalendarPicker.NavigationBehavior.WithAnimation);
      eventHandled = true;
    }
  } else if (key == 'PageDown') {
    var nextMonth = this.currentMonth().next();
    if (nextMonth && nextMonth >= this.config.minimumValue) {
      this.setCurrentMonth(
          nextMonth, CalendarPicker.NavigationBehavior.WithAnimation);
      eventHandled = true;
    }
  } else if (this._highlight) {
    if (global.params.isLocaleRTL ? key == 'ArrowRight' : key == 'ArrowLeft') {
      eventHandled = this._moveHighlight(this._highlight.previous());
    } else if (key == 'ArrowUp') {
      eventHandled = this._moveHighlight(this._highlight.previous(
          this.type === 'date' || this.type === 'datetime-local' ? DaysPerWeek :
                                                                   1));
    } else if (
        global.params.isLocaleRTL ? key == 'ArrowLeft' : key == 'ArrowRight') {
      eventHandled = this._moveHighlight(this._highlight.next());
    } else if (key == 'ArrowDown') {
      eventHandled = this._moveHighlight(this._highlight.next(
          this.type === 'date' || this.type === 'datetime-local' ? DaysPerWeek :
                                                                   1));
    } else if (key == 'Enter') {
      this.setSelectionAndCommit(this._highlight);
    }
  } else if (
      key == 'ArrowLeft' || key == 'ArrowUp' || key == 'ArrowRight' ||
      key == 'ArrowDown') {
    // Highlight range near the middle.
    this.highlightRangeContainingDay(this.currentMonth().middleDay());
    eventHandled = true;
  }

  if (eventHandled) {
    event.stopPropagation();
    event.preventDefault();
  }
};

/**
 * @return {!number} Width in pixels.
 */
CalendarPicker.prototype.width = function() {
  return this.calendarTableView.width() +
      (CalendarTableView.GetBorderWidth() + CalendarPicker.BorderWidth +
       CalendarPicker.Padding) *
      2;
};

/**
 * @return {!number} Height in pixels.
 */
CalendarPicker.prototype.height = function() {
  return this._height;
};

/**
 * @param {!number} height Height in pixels.
 */
CalendarPicker.prototype.setHeight = function(height) {
  if (this._height === height)
    return;
  this._height = height;
  resizeWindow(this.width(), this._height);
  this.calendarTableView.setHeight(
      this._height - CalendarHeaderView.Height -
      CalendarHeaderView.BottomMargin - CalendarPicker.Padding * 2 -
      CalendarPicker.BorderWidth * 2);
};

/**
 * @param {?Event} event
 */
CalendarPicker.prototype.onBodyKeyDown = function(event) {
  var key = event.key;
  var eventHandled = false;
  var offset = 0;
  switch (key) {
    case 'Escape':
      window.pagePopupController.closePopup();
      eventHandled = true;
      break;
    case 'm':
    case 'M':
      offset = offset || 1;  // Fall-through.
    case 'y':
    case 'Y':
      offset = offset || MonthsPerYear;  // Fall-through.
    case 'd':
    case 'D':
      offset = offset || MonthsPerYear * 10;
      var oldFirstVisibleRow =
          this.calendarTableView
              .columnAndRowForDay(this.currentMonth().firstDay())
              .row;
      this.setCurrentMonth(
          event.shiftKey ? this.currentMonth().previous(offset) :
                           this.currentMonth().next(offset),
          CalendarPicker.NavigationBehavior.WithAnimation);
      var newFirstVisibleRow =
          this.calendarTableView
              .columnAndRowForDay(this.currentMonth().firstDay())
              .row;
      if (this._highlight) {
        var highlightMiddleDay = this._highlight.middleDay();
        this.highlightRangeContainingDay(highlightMiddleDay.next(
            (newFirstVisibleRow - oldFirstVisibleRow) * DaysPerWeek));
      }
      eventHandled = true;
      break;
  }
  if (eventHandled) {
    event.stopPropagation();
    event.preventDefault();
  }
};

if (window.dialogArguments) {
  initialize(dialogArguments);
} else {
  window.addEventListener('message', handleMessage, false);
}
