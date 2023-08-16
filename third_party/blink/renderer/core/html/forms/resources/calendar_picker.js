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
const WeekDay = {
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
const global = {
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
    isBorderTransparent: false,
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
  const locale = getLocale();
  const result = locale.match(/^([a-z]+)/);
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
  const newDate = new Date(0);
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
  const month = Month.parse(dateString);
  if (month)
    return month;
  const week = Week.parse(dateString);
  if (week)
    return week;
  return Day.parse(dateString);
}

/**
 * @const
 * @type {number}
 */
const DaysPerWeek = 7;

/**
 * @const
 * @type {number}
 */
const MonthsPerYear = 12;

/**
 * @const
 * @type {number}
 */
const MillisecondsPerDay = 24 * 60 * 60 * 1000;

/**
 * @const
 * @type {number}
 */
const MillisecondsPerWeek = DaysPerWeek * MillisecondsPerDay;

// ----------------------------------------------------------------

/**
 * The base class of Day, Week, and Month.
 */
class DateType {
  constructor() {
  }
}

// ----------------------------------------------------------------

class Day extends DateType {
  /**
   * @param {!number} year
   * @param {!number} month
   * @param {!number} date
   */
  constructor(year, month, date) {
    super();
    const dateObject = createUTCDate(year, month, date);
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
  }

  /** @const */
  static ISOStringRegExp = /^(\d+)-(\d+)-(\d+)/;

  /**
   * @param {!string} str
   * @return {?Day}
   */
  static parse(str) {
    const match = Day.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    const year = parseInt(match[1], 10);
    const month = parseInt(match[2], 10) - 1;
    const date = parseInt(match[3], 10);
    return new Day(year, month, date);
  }

  /**
   * @param {!number} value
   * @return {!Day}
   */
  static createFromValue(millisecondsSinceEpoch) {
    return Day.createFromDate(new Date(millisecondsSinceEpoch))
  }

  /**
   * @param {!Date} date
   * @return {!Day}
   */
  static createFromDate(date) {
    if (isNaN(date.valueOf()))
      throw 'Invalid date';
    return new Day(
        date.getUTCFullYear(), date.getUTCMonth(), date.getUTCDate());
  }

  /**
   * @param {!Day} day
   * @return {!Day}
   */
  static createFromDay(day) {
    return day;
  }

  /**
   * @return {!Day}
   */
  static createFromToday() {
    const now = new Date();
    return new Day(now.getFullYear(), now.getMonth(), now.getDate());
  }

  /**
   * @param {!DateType} other
   * @return {!boolean}
   */
  equals(other) {
    return other instanceof Day && this.year === other.year &&
        this.month === other.month && this.date === other.date;
  }

  /**
   * @param {!number=} offset
   * @return {!Day}
   */
  previous(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Day(this.year, this.month, this.date - offset);
  }

  /**
   * @param {!number=} offset
   * @return {!Day}
   */
  next(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Day(this.year, this.month, this.date + offset);
  }

  /**
   * @return {!Day}
   */
  nextHome() {
    if (this.date !== 1)
      return new Day(this.year, this.month, 1);
    return new Day(this.year, this.month - 1, 1);
  }

  /**
   * @return {!Day}
   */
  nextEnd() {
    let tomorrow = this.next();
    if (tomorrow.month === this.month)
      return new Day(this.year, this.month + 1, 1).previous();
    return new Day(tomorrow.year, tomorrow.month + 1, 1).previous();
  }

  /**
   * Given that 'this' is the Nth day of the month, returns the Nth
   * day of the month that is specified by the parameter.
   * Clips the date if necessary, e.g. if 'this' Day is October 31st and
   * the parameter is a November, returns November 30th.
   * @param {!Month} month
   * @return {!Day}
   */
  thisRangeInMonth(month) {
    const newDate = month.startDate();
    const originalMonthInt = newDate.getUTCMonth();
    newDate.setUTCDate(this.date);
    if (newDate.getUTCMonth() != originalMonthInt) {
      newDate.setUTCDate(0);
    }
    return Day.createFromDate(newDate);
  }

  /**
   * @param {!Month} month
   * @return {!boolean}
   */
  overlapsMonth(month) {
    return (month.firstDay() <= this && month.lastDay() >= this);
  }

  /**
   * @param {!Month} month
   * @return {!boolean}
   */
  isFullyContainedInMonth(month) {
    return (month.firstDay() <= this && month.lastDay() >= this);
  }

  /**
   * @return {!Date}
   */
  startDate() {
    return createUTCDate(this.year, this.month, this.date);
  }

  /**
   * @return {!Date}
   */
  endDate() {
    return createUTCDate(this.year, this.month, this.date + 1);
  }

  /**
   * @return {!Day}
   */
  firstDay() {
    return this;
  }

  /**
   * @return {!Day}
   */
  middleDay() {
    return this;
  }

  /**
   * @return {!Day}
   */
  lastDay() {
    return this;
  }

  /**
   * @return {!number}
   */
  valueOf() {
    return createUTCDate(this.year, this.month, this.date).getTime();
  }

  /**
   * @return {!WeekDay}
   */
  weekDay() {
    return createUTCDate(this.year, this.month, this.date).getUTCDay();
  }

  /**
   * @return {!string}
   */
  toString() {
    let yearString = String(this.year);
    if (yearString.length < 4)
      yearString = ('000' + yearString).substr(-4, 4);
    return yearString + '-' + ('0' + (this.month + 1)).substr(-2, 2) + '-' +
        ('0' + this.date).substr(-2, 2);
  }

  /**
   * @return {!string}
   */
  format() {
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
  }

  // See platform/text/date_components.h.
  /** @const */
  static Minimum = Day.createFromValue(-62135596800000.0);
  /** @const */
  static Maximum = Day.createFromValue(8640000000000000.0);

  // See core/html/forms/date_input_type.cc.
  /** @const */
  static DefaultStep = 86400000;
  /** @const */
  static DefaultStepBase = 0;
}

// ----------------------------------------------------------------

class Week extends DateType {
  /**
   * @param {!number} year
   * @param {!number} week
   */
  constructor(year, week) {
    super();
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
      const normalizedWeek = Week.createFromDay(this.firstDay());
      this.year = normalizedWeek.year;
      this.week = normalizedWeek.week;
    }
  }

  static ISOStringRegExp = /^(\d+)-[wW](\d+)$/;

  // See platform/text/date_components.h.
  static Minimum = new Week(1, 1);
  static Maximum = new Week(275760, 37);

  // See core/html/forms/week_input_type.cc.
  static DefaultStep = 604800000;
  static DefaultStepBase = -259200000;

  static EpochWeekDay = createUTCDate(1970, 0, 0).getUTCDay();

  /**
   * @param {!string} str
   * @return {?Week}
   */
  static parse(str) {
    const match = Week.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    const year = parseInt(match[1], 10);
    const week = parseInt(match[2], 10);
    return new Week(year, week);
  }

  /**
   * @param {!number} millisecondsSinceEpoch
   * @return {!Week}
   */
  static createFromValue(millisecondsSinceEpoch) {
    return Week.createFromDate(new Date(millisecondsSinceEpoch))
  }

  /**
   * @param {!Date} date
   * @return {!Week}
   */
  static createFromDate(date) {
    if (isNaN(date.valueOf()))
      throw 'Invalid date';
    let year = date.getUTCFullYear();
    if (year <= Week.Maximum.year &&
        Week.weekOneStartDateForYear(year + 1).getTime() <= date.getTime())
      year++;
    else if (
        year > 1 &&
        Week.weekOneStartDateForYear(year).getTime() > date.getTime())
      year--;
    const week = 1 +
        Week._numberOfWeeksSinceDate(Week.weekOneStartDateForYear(year), date);
    return new Week(year, week);
  }

  /**
   * @param {!Day} day
   * @return {!Week}
   */
  static createFromDay(day) {
    let year = day.year;
    if (year <= Week.Maximum.year &&
        Week.weekOneStartDayForYear(year + 1) <= day)
      year++;
    else if (year > 1 && Week.weekOneStartDayForYear(year) > day)
      year--;
    const week = Math.floor(
        1 +
        (day.valueOf() - Week.weekOneStartDayForYear(year).valueOf()) /
            MillisecondsPerWeek);
    return new Week(year, week);
  }

  /**
   * @return {!Week}
   */
  static createFromToday() {
    const now = new Date();
    return Week.createFromDate(
        createUTCDate(now.getFullYear(), now.getMonth(), now.getDate()));
  }

  /**
   * @param {!number} year
   * @return {!Date}
   */
  static weekOneStartDateForYear(year) {
    if (year < 1)
      return createUTCDate(1, 0, 1);
    // The week containing January 4th is week one.
    const yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
    return createUTCDate(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
  }

  /**
   * @param {!number} year
   * @return {!Day}
   */
  static weekOneStartDayForYear(year) {
    if (year < 1)
      return Day.Minimum;
    // The week containing January 4th is week one.
    const yearStartDay = createUTCDate(year, 0, 4).getUTCDay();
    return new Day(year, 0, 4 - (yearStartDay + 6) % DaysPerWeek);
  }

  /**
   * @param {!number} year
   * @return {!number}
   */
  static numberOfWeeksInYear(year) {
    if (year < 1 || year > Week.Maximum.year)
      return 0;
    else if (year === Week.Maximum.year)
      return Week.Maximum.week;
    return Week._numberOfWeeksSinceDate(
        Week.weekOneStartDateForYear(year),
        Week.weekOneStartDateForYear(year + 1));
  }

  /**
   * @param {!Date} baseDate
   * @param {!Date} date
   * @return {!number}
   */
  static _numberOfWeeksSinceDate(baseDate, date) {
    return Math.floor(
        (date.getTime() - baseDate.getTime()) / MillisecondsPerWeek);
  }

  /**
   * @param {!DateType} other
   * @return {!boolean}
   */
  equals(other) {
    return other instanceof Week && this.year === other.year &&
        this.week === other.week;
  }

  /**
   * @param {!number=} offset
   * @return {!Week}
   */
  previous(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Week(this.year, this.week - offset);
  }

  /**
   * @param {!number=} offset
   * @return {!Week}
   */
  next(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Week(this.year, this.week + offset);
  }

  /**
   * @return {!Week}
   */
  nextHome() {
    // Go back weeks until we find the one that is the first week of a month. Do
    // that by finding the first day in the current week, then go back a day. We
    // want the first week of the month for that day.
    const desiredDay = this.firstDay().previous();
    desiredDay.date = 1;
    return Week.createFromDay(desiredDay);
  }

  /**
   * @return {!Week}
   */
  nextEnd() {
    // Go forward weeks until we find the one that is the last week of a month. Do
    // that by finding the week containing the last day of the month for the day
    // following the last day included in the current week.
    let desiredDay = this.lastDay().next();
    desiredDay = new Day(desiredDay.year, desiredDay.month + 1, 1).previous();
    return Week.createFromDay(desiredDay);
  }

  /**
   * Given that 'this' is the Nth week of the month, returns
   * the Week that is the Nth week in the month specified
   * by the parameter.
   * Clips the date if necessary, e.g. if 'this' is the 5th week
   * of a month that has 5 weeks and the parameter month only has
   * 4 weeks, returns the 4th week of that month.
   * @param {!Month} month
   * @return {!Week}
   */
  thisRangeInMonth(month) {
    const firstDateInCurrentMonth = this.startDate();
    firstDateInCurrentMonth.setUTCDate(1);

    const offsetInOriginalMonth =
        Week._numberOfWeeksSinceDate(firstDateInCurrentMonth, this.startDate());

    // Determine the first Monday in the new month (the week control shows weeks
    // starting on Monday).
    const firstWeekStartInNewMonth = month.startDate();
    firstWeekStartInNewMonth.setUTCDate(
        1 +
        ((DaysPerWeek + 1 - firstWeekStartInNewMonth.getUTCDay()) %
         DaysPerWeek));


    // Find the Nth Monday in the month where N == offsetInOriginalMonth.
    firstWeekStartInNewMonth.setUTCDate(
        firstWeekStartInNewMonth.getUTCDate() +
        (DaysPerWeek * offsetInOriginalMonth));

    if (firstWeekStartInNewMonth.getUTCMonth() != month.month) {
      // If we overshot into the next month (can happen if we were
      // on the 5th week of the old month), go back to the last week
      // of the target month.
      firstWeekStartInNewMonth.setUTCDate(
          firstWeekStartInNewMonth.getUTCDate() - DaysPerWeek);
    }

    return Week.createFromDate(firstWeekStartInNewMonth);
  }

  /**
   * @param {!Month} month
   * @return {!boolean}
   */
  overlapsMonth(month) {
    return (
        month.firstDay() <= this.lastDay() &&
        month.lastDay() >= this.firstDay());
  }

  /**
   * @param {!Month} month
   * @return {!boolean}
   */
  isFullyContainedInMonth(month) {
    return (
        month.firstDay() <= this.firstDay() &&
        month.lastDay() >= this.lastDay());
  }

  /**
   * @return {!Date}
   */
  startDate() {
    const weekStartDate = Week.weekOneStartDateForYear(this.year);
    weekStartDate.setUTCDate(weekStartDate.getUTCDate() + (this.week - 1) * 7);
    return weekStartDate;
  }

  /**
   * @return {!Date}
   */
  endDate() {
    if (this.equals(Week.Maximum))
      return Day.Maximum.startDate();
    return this.next().startDate();
  }

  /**
   * @return {!Day}
   */
  firstDay() {
    const weekOneStartDay = Week.weekOneStartDayForYear(this.year);
    return weekOneStartDay.next((this.week - 1) * DaysPerWeek);
  }

  /**
   * @return {!Day}
   */
  middleDay() {
    return this.firstDay().next(3);
  }

  /**
   * @return {!Day}
   */
  lastDay() {
    if (this.equals(Week.Maximum))
      return Day.Maximum;
    return this.next().firstDay().previous();
  }

  /**
   * @return {!number}
   */
  valueOf() {
    return this.firstDay().valueOf() - createUTCDate(1970, 0, 1).getTime();
  }

  /**
   * @return {!string}
   */
  toString() {
    let yearString = String(this.year);
    if (yearString.length < 4)
      yearString = ('000' + yearString).substr(-4, 4);
    return yearString + '-W' + ('0' + this.week).substr(-2, 2);
  }
}

// ----------------------------------------------------------------

class Month extends DateType {
  /**
   * @param {!number} year
   * @param {!number} month
   */
  constructor(year, month) {
    super();
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
  }

  static ISOStringRegExp = /^(\d+)-(\d+)$/;

  // See platform/text/date_components.h.
  static Minimum = new Month(1, 0);
  static Maximum = new Month(275760, 8);

  // See core/html/forms/month_input_type.cc.
  static DefaultStep = 1;
  static DefaultStepBase = 0;

  /**
   * @param {!string} str
   * @return {?Month}
   */
  static parse(str) {
    const match = Month.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    const year = parseInt(match[1], 10);
    const month = parseInt(match[2], 10) - 1;
    return new Month(year, month);
  }

  /**
   * @param {!number} value
   * @return {!Month}
   */
  static createFromValue(monthsSinceEpoch) {
    return new Month(1970, monthsSinceEpoch)
  }

  /**
   * @param {!Date} date
   * @return {!Month}
   */
  static createFromDate(date) {
    if (isNaN(date.valueOf()))
      throw 'Invalid date';
    return new Month(date.getUTCFullYear(), date.getUTCMonth());
  }

  /**
   * @param {!Day} day
   * @return {!Month}
   */
  static createFromDay(day) {
    return new Month(day.year, day.month);
  }

  /**
   * @return {!Month}
   */
  static createFromToday() {
    const now = new Date();
    return new Month(now.getFullYear(), now.getMonth());
  }

  /**
   * @param {!Month} other
   * @return {!boolean}
   */
  equals(other) {
    return other instanceof Month && this.year === other.year &&
        this.month === other.month;
  }

  /**
   * @param {!number=} offset
   * @return {!Month}
   */
  previous(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Month(this.year, this.month - offset);
  }

  /**
   * @param {!number=} offset
   * @return {!Month}
   */
  next(offset) {
    if (typeof offset === 'undefined')
      offset = 1;
    return new Month(this.year, this.month + offset);
  }

  /**
   * @return {!Month}
   */
  nextHome() {
    if (this.month !== 0)
      return new Month(this.year, 0);
    return new Month(this.year - 1, 0);
  }

  /**
   * @return {!Month}
   */
  nextEnd() {
    if (this.month !== MonthsPerYear - 1)
      return new Month(this.year, MonthsPerYear - 1);
    return new Month(this.year + 1, MonthsPerYear - 1);
  }

  /**
   * @return {!Date}
   */
  startDate() {
    return createUTCDate(this.year, this.month, 1);
  }

  /**
   * @return {!Date}
   */
  endDate() {
    if (this.equals(Month.Maximum))
      return Day.Maximum.startDate();
    return this.next().startDate();
  }

  /**
   * @return {!Day}
   */
  firstDay() {
    return new Day(this.year, this.month, 1);
  }

  /**
   * @return {!Day}
   */
  middleDay() {
    return new Day(this.year, this.month, this.month === 1 ? 14 : 15);
  }

  /**
   * @return {!Day}
   */
  lastDay() {
    if (this.equals(Month.Maximum))
      return Day.Maximum;
    return this.next().firstDay().previous();
  }

  /**
   * @return {!number}
   */
  valueOf() {
    return (this.year - 1970) * MonthsPerYear + this.month;
  }

  /**
   * @return {!string}
   */
  toString() {
    let yearString = String(this.year);
    if (yearString.length < 4)
      yearString = ('000' + yearString).substr(-4, 4);
    return yearString + '-' + ('0' + (this.month + 1)).substr(-2, 2);
  }

  /**
   * @return {!string}
   */
  toLocaleString() {
    if (global.params.locale === 'ja')
      return '' + this.year + '\u5e74' +
          formatJapaneseImperialEra(this.year, this.month) + ' ' +
          (this.month + 1) + '\u6708';
    return window.pagePopupController.formatMonth(this.year, this.month);
  }

  /**
   * @return {!string}
   */
  toShortLocaleString() {
    return window.pagePopupController.formatShortMonth(this.year, this.month);
  }
}

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
  let name;
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
  const main = $('main');
  main.innerHTML = '';
  main.className = '';
};

function openSuggestionPicker() {
  closePicker();
  global.picker = new SuggestionPicker($('main'), global.params);
};

function openCalendarPicker() {
  closePicker();
  if (global.params.mode == 'month') {
    return initializeMonthPicker(global.params);
  } else if (global.params.mode == 'time') {
    return initializeTimePicker(global.params);
  } else if (global.params.mode == 'datetime-local') {
    return initializeDateTimeLocalPicker(global.params);
  }

  global.picker = new CalendarPicker(global.params.mode, global.params);
  global.picker.attachTo($('main'));
};

// Parameter t should be a number between 0 and 1.
const AnimationTimingFunction = {
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

// ----------------------------------------------------------------

class AnimationManager extends EventEmitter {
  constructor() {
    super();

    this._isRunning = false;
    this._runningAnimatorCount = 0;
    this._runningAnimators = {};
    this._animationFrameCallbackBound = this._animationFrameCallback.bind(this);
  }

  static EventTypeAnimationFrameWillFinish = 'animationFrameWillFinish';

  _startAnimation() {
    if (this._isRunning)
      return;
    this._isRunning = true;
    window.requestAnimationFrame(this._animationFrameCallbackBound);
  }

  _stopAnimation() {
    if (!this._isRunning)
      return;
    this._isRunning = false;
  }

  /**
   * @param {!Animator} animator
   */
  add(animator) {
    if (this._runningAnimators[animator.id])
      return;
    this._runningAnimators[animator.id] = animator;
    this._runningAnimatorCount++;
    if (this._needsTimer())
      this._startAnimation();
  }

  /**
   * @param {!Animator} animator
   */
  remove(animator) {
    if (!this._runningAnimators[animator.id])
      return;
    delete this._runningAnimators[animator.id];
    this._runningAnimatorCount--;
    if (!this._needsTimer())
      this._stopAnimation();
  }

  _animationFrameCallback(now) {
    if (this._runningAnimatorCount > 0) {
      for (let id in this._runningAnimators) {
        this._runningAnimators[id].onAnimationFrame(now);
      }
    }
    this.dispatchEvent(AnimationManager.EventTypeAnimationFrameWillFinish);
    if (this._isRunning)
      window.requestAnimationFrame(this._animationFrameCallbackBound);
  }

  /**
   * @return {!boolean}
   */
  _needsTimer() {
    return this._runningAnimatorCount > 0 ||
        this.hasListener(AnimationManager.EventTypeAnimationFrameWillFinish);
  }

  /**
   * @param {!string} type
   * @param {!Function} callback
   * @override
   */
  on(type, callback) {
    EventEmitter.prototype.on.call(this, type, callback);
    if (this._needsTimer())
      this._startAnimation();
  }

  /**
   * @param {!string} type
   * @param {!Function} callback
   * @override
   */
  removeListener(type, callback) {
    EventEmitter.prototype.removeListener.call(this, type, callback);
    if (!this._needsTimer())
      this._stopAnimation();
  }

  static shared = new AnimationManager();
}

// ----------------------------------------------------------------

class Animator extends EventEmitter {
  constructor() {
    super();
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

  static _lastId = 0;

  static EventTypeDidAnimationStop = 'didAnimationStop';

  /**
   * @return {!boolean}
   */
  isRunning() {
    return this._isRunning;
  }

  start() {
    this._lastStepTime = performance.now();
    this._isRunning = true;
    AnimationManager.shared.add(this);
  }

  stop() {
    if (!this._isRunning)
      return;
    this._isRunning = false;
    AnimationManager.shared.remove(this);
    this.dispatchEvent(Animator.EventTypeDidAnimationStop, this);
  }

  /**
   * @param {!number} now
   */
  onAnimationFrame(now) {
    this._lastStepTime = now;
    this.step(this);
  }
}

// ----------------------------------------------------------------

class TransitionAnimator extends Animator {
  constructor() {
    super();
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

  /**
   * @param {!number} value
   */
  setFrom(value) {
    this._from = value;
    this._delta = this._to - this._from;
  }

  start() {
    console.assert(isFinite(this.duration));
    this.progress = 0.0;
    this.currentValue = this._from;
    super.start();
  }

  /**
   * @param {!number} value
   */
  setTo(value) {
    this._to = value;
    this._delta = this._to - this._from;
  }

  /**
   * @param {!number} now
   */
  onAnimationFrame(now) {
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
  }
}

// ----------------------------------------------------------------

class FlingGestureAnimator extends Animator {
  /**
   * @param {!number} initialVelocity
   * @param {!number} initialValue
   */
  constructor(initialVelocity, initialValue) {
    super();
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
    let startVelocity = Math.abs(this.initialVelocity);
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

  // Velocity is subject to exponential decay. These parameters are coefficients
  // that determine the curve.
  static _P0 = -5707.62;
  static _P1 = 0.172;
  static _P2 = 0.0037;

  /**
   * @param {!number} t
   */
  _valueAtTime(t) {
    return (
        FlingGestureAnimator._P0 * Math.exp(-FlingGestureAnimator._P2 * t) -
        FlingGestureAnimator._P1 * t - FlingGestureAnimator._P0);
  }

  /**
   * @param {!number} t
   */
  _velocityAtTime(t) {
    return (
        -FlingGestureAnimator._P0 * FlingGestureAnimator._P2 *
            Math.exp(-FlingGestureAnimator._P2 * t) -
        FlingGestureAnimator._P1);
  }

  /**
   * @param {!number} v
   */
  _timeAtVelocity(v) {
    return (
        -Math.log(
            (v + FlingGestureAnimator._P1) /
            (-FlingGestureAnimator._P0 * FlingGestureAnimator._P2)) /
        FlingGestureAnimator._P2);
  }

  start() {
    this._lastStepTime = performance.now();
    super.start();
  }

  /**
   * @param {!number} now
   */
  onAnimationFrame(now) {
    this._elapsedTime += now - this._lastStepTime;
    this._lastStepTime = now;
    if (this._elapsedTime + this._timeOffset >= this.duration) {
      this.stop();
      return;
    }
    let position = this._valueAtTime(this._elapsedTime + this._timeOffset) -
        this._positionOffset;
    if (this.initialVelocity < 0)
      position = -position;
    this.currentValue = position + this.initialValue;
    this.step(this);
  }
}

// ----------------------------------------------------------------

class View extends EventEmitter {
  /**
   * @param {?Element} element
   * View adds itself as a property on the element so we can access it from Event.target.
   */
  constructor(element) {
    super();
    /**
     * @type {Element}
     * @const
     */
    this.element = element || createElement('div');
    this.element.$view = this;
    this.bindCallbackMethods();
  }

  /**
   * @param {!Element} ancestorElement
   * @return {?Object}
   */
  offsetRelativeTo(ancestorElement) {
    let x = 0;
    let y = 0;
    let element = this.element;
    while (element) {
      x += element.offsetLeft || 0;
      y += element.offsetTop || 0;
      element = element.offsetParent;
      if (element === ancestorElement)
        return {x: x, y: y};
    }
    return null;
  }

  /**
   * @param {!View|Node} parent
   * @param {?View|Node=} before
   */
  attachTo(parent, before) {
    if (parent instanceof View)
      return this.attachTo(parent.element, before);
    if (typeof before === 'undefined')
      before = null;
    if (before instanceof View)
      before = before.element;
    parent.insertBefore(this.element, before);
  }

  bindCallbackMethods() {
    for (let methodName in this) {
      if (!/^on[A-Z]/.test(methodName))
        continue;
      if (this.hasOwnProperty(methodName))
        continue;
      let method = this[methodName];
      if (!(method instanceof Function))
        continue;
      this[methodName] = method.bind(this);
    }
  }
}

// ----------------------------------------------------------------

class ScrollView extends View {
  /**
   * @extends View
   */
  constructor() {
    super(createElement('div', ScrollView.ClassNameScrollView));
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

    this._onWindowTouchMoveBound = this.onWindowTouchMove.bind(this);
    this._onWindowTouchEndBound = this.onWindowTouchEnd.bind(this);
    this._onFlingGestureAnimatorStepBound =
        this.onFlingGestureAnimatorStep.bind(this);

    this.element.addEventListener(
        'mousewheel', this.onMouseWheel.bind(this), false);
    this.element.addEventListener(
        'touchstart', this.onTouchStart.bind(this), false);

    /**
     * The content offset is partitioned so the it can go beyond the CSS limit
     * of 33554433px.
     * @type {number}
     * @protected
     */
    this._partitionNumber = 0;
  }

  static PartitionHeight = 100000;
  static ClassNameScrollView = 'scroll-view';
  static ClassNameScrollViewContent = 'scroll-view-content';

  /**
   * @param {!Event} event
   */
  onTouchStart(event) {
    const touch = event.touches[0];
    this._lastTouchPosition = touch.clientY;
    this._lastTouchVelocity = 0;
    this._lastTouchTimeStamp = event.timeStamp;
    if (this._scrollAnimator)
      this._scrollAnimator.stop();
    window.addEventListener('touchmove', this._onWindowTouchMoveBound);
    window.addEventListener('touchend', this._onWindowTouchEndBound);
  }

  /**
   * @param {!Event} event
   */
  onWindowTouchMove(event) {
    const touch = event.touches[0];
    const deltaTime = event.timeStamp - this._lastTouchTimeStamp;
    const deltaY = this._lastTouchPosition - touch.clientY;
    this.scrollBy(deltaY, false);
    this._lastTouchVelocity = deltaY / deltaTime;
    this._lastTouchPosition = touch.clientY;
    this._lastTouchTimeStamp = event.timeStamp;
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * @param {!Event} event
   */
  onWindowTouchEnd(event) {
    if (Math.abs(this._lastTouchVelocity) > 0.01) {
      this._scrollAnimator = new FlingGestureAnimator(
          this._lastTouchVelocity, this._contentOffset);
      this._scrollAnimator.step = this._onFlingGestureAnimatorStepBound;
      this._scrollAnimator.start();
    }
    window.removeEventListener('touchmove', this._onWindowTouchMoveBound);
    window.removeEventListener('touchend', this._onWindowTouchEndBound);
  }

  /**
   * @param {!Animator} animator
   */
  onFlingGestureAnimatorStep(animator) {
    this.scrollTo(animator.currentValue, false);
  }

  /**
   * @return {!Animator}
   */
  scrollAnimator() {
    return this._scrollAnimator;
  }

  /**
   * @param {!number} width
   */
  setWidth(width) {
    console.assert(isFinite(width));
    if (this._width === width)
      return;
    this._width = width;
    this.element.style.width = this._width + 'px';
  }

  /**
   * @return {!number}
   */
  width() {
    return this._width;
  }

  /**
   * @param {!number} height
   */
  setHeight(height) {
    console.assert(isFinite(height));
    if (this._height === height)
      return;
    this._height = height;
    this.element.style.height = height + 'px';
    if (this.delegate)
      this.delegate.scrollViewDidChangeHeight(this);
  }

  /**
   * @return {!number}
   */
  height() {
    return this._height;
  }

  /**
   * @param {!Animator} animator
   */
  onScrollAnimatorStep(animator) {
    this.setContentOffset(animator.currentValue);
  }

  /**
   * @param {!number} offset
   * @param {?boolean} animate
   */
  scrollTo(offset, animate) {
    console.assert(isFinite(offset));
    if (!animate) {
      this.setContentOffset(offset);
      return;
    }
    if (this._scrollAnimator)
      this._scrollAnimator.stop();
    this._scrollAnimator = new TransitionAnimator();
    this._scrollAnimator.step = this.onScrollAnimatorStep.bind(this);
    this._scrollAnimator.setFrom(this._contentOffset);
    this._scrollAnimator.setTo(offset);
    this._scrollAnimator.duration = 300;
    this._scrollAnimator.start();
  }

  /**
   * @param {!number} offset
   * @param {?boolean} animate
   */
  scrollBy(offset, animate) {
    this.scrollTo(this._contentOffset + offset, animate);
  }

  /**
   * @return {!number}
   */
  contentOffset() {
    return this._contentOffset;
  }

  /**
   * @param {?Event} event
   */
  onMouseWheel(event) {
    this.setContentOffset(this._contentOffset - event.wheelDelta / 30);
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * @param {!number} value
   */
  setContentOffset(value) {
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
  }

  _updateScrollContent() {
    const newPartitionNumber =
        Math.floor(this._contentOffset / ScrollView.PartitionHeight);
    const partitionChanged = this._partitionNumber !== newPartitionNumber;
    this._partitionNumber = newPartitionNumber;
    this.contentElement.style.webkitTransform = 'translate(0, ' +
        -this.contentPositionForContentOffset(this._contentOffset) + 'px)';
    if (this.delegate && partitionChanged)
      this.delegate.scrollViewDidChangePartition(this);
  }

  /**
   * @param {!View|Node} parent
   * @param {?View|Node=} before
   * @override
   */
  attachTo(parent, before) {
    View.prototype.attachTo.call(this, parent, before);
    this._updateScrollContent();
  }

  /**
   * @param {!number} offset
   */
  contentPositionForContentOffset(offset) {
    return offset - this._partitionNumber * ScrollView.PartitionHeight;
  }
}

// ----------------------------------------------------------------

class ListCell extends View {
  /**
   * @extends View
   */
  constructor() {
    super(createElement('div', ListCell.ClassNameListCell));
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

  static DefaultRecycleBinLimit = 64;
  static ClassNameListCell = 'list-cell';
  static ClassNameHidden = 'hidden';

  /**
   * @return {!Array} An array to keep thrown away cells.
   */
  _recycleBin() {
    console.assert(
        false,
        'NOT REACHED: ListCell.prototype._recycleBin needs to be overridden.');
    return [];
  }

  throwAway() {
    this.hide();
    const limit = typeof this.constructor.RecycleBinLimit === 'undefined' ?
        ListCell.DefaultRecycleBinLimit :
        this.constructor.RecycleBinLimit;
    const recycleBin = this._recycleBin();
    if (recycleBin.length < limit)
      recycleBin.push(this);
  }

  show() {
    this.element.classList.remove(ListCell.ClassNameHidden);
  }

  hide() {
    this.element.classList.add(ListCell.ClassNameHidden);
  }

  /**
   * @return {!number} Width in pixels.
   */
  width() {
    return this._width;
  }

  /**
   * @param {!number} width Width in pixels.
   */
  setWidth(width) {
    if (this._width === width)
      return;
    this._width = width;
    this.element.style.width = this._width + 'px';
  }

  /**
   * @return {!number} Position in pixels.
   */
  position() {
    return this._position;
  }

  /**
   * @param {!number} y Position in pixels.
   */
  setPosition(y) {
    if (this._position === y)
      return;
    this._position = y;
    this.element.style.webkitTransform =
        'translate(0, ' + this._position + 'px)';
  }

  /**
   * @param {!boolean} selected
   */
  setSelected(selected) {
    if (this._selected === selected)
      return;
    this._selected = selected;
    if (this._selected) {
      this.element.classList.add('selected');
      this.element.setAttribute('aria-selected', true);
    } else {
      this.element.classList.remove('selected');
      this.element.setAttribute('aria-selected', false);
    }
  }
}

// ----------------------------------------------------------------

class ListView extends View {
  constructor() {
    super(createElement('div', ListView.ClassNameListView));
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

    this.element.addEventListener('click', this.onClick.bind(this), false);

    /**
     * @type {!boolean}
     * @private
     */
    this._needsUpdateCells = false;
  }

  static NoSelection = -1;
  static ClassNameListView = 'list-view';

  onAnimationFrameWillFinish() {
    if (this._needsUpdateCells)
      this.updateCells();
  }

  /**
   * @param {!boolean} needsUpdateCells
   */
  setNeedsUpdateCells(needsUpdateCells) {
    if (this._needsUpdateCells === needsUpdateCells)
      return;
    this._needsUpdateCells = needsUpdateCells;
    if (this._needsUpdateCells)
      AnimationManager.shared.on(
          AnimationManager.EventTypeAnimationFrameWillFinish,
          this.onAnimationFrameWillFinish.bind(this));
    else
      AnimationManager.shared.removeListener(
          AnimationManager.EventTypeAnimationFrameWillFinish,
          this.onAnimationFrameWillFinish.bind(this));
  }

  /**
   * @param {!number} row
   * @return {?ListCell}
   */
  cellAtRow(row) {
    return this._cells[row];
  }

  /**
   * @param {!number} offset Scroll offset in pixels.
   * @return {!number}
   */
  rowAtScrollOffset(offset) {
    console.assert(
        false,
        'NOT REACHED: ListView.prototype.rowAtScrollOffset needs to be overridden.');
    return 0;
  }

  /**
   * @param {!number} row
   * @return {!number} Scroll offset in pixels.
   */
  scrollOffsetForRow(row) {
    console.assert(
        false,
        'NOT REACHED: ListView.prototype.scrollOffsetForRow needs to be overridden.');
    return 0;
  }

  /**
   * @param {!number} row
   * @return {!ListCell}
   */
  addCellIfNecessary(row) {
    let cell = this._cells[row];
    if (cell)
      return cell;
    cell = this.prepareNewCell(row);

    // Ensure that the DOM tree positions of the rows are in increasing
    // chronological order.  This is needed for correct application of
    // the :hover selector for the week control, which spans across multiple
    // calendar rows.
    const rowIndices = Object.keys(this._cells);
    const shouldPrepend = (rowIndices.length) > 0 && (row < rowIndices[0]);
    cell.attachTo(
        this.scrollView.contentElement,
        shouldPrepend ? this.scrollView.contentElement.firstElementChild :
                        undefined);

    cell.setWidth(this._width);
    cell.setPosition(this.scrollView.contentPositionForContentOffset(
        this.scrollOffsetForRow(row)));
    this._cells[row] = cell;
    return cell;
  }

  /**
   * @param {!number} row
   * @return {!ListCell}
   */
  prepareNewCell(row) {
    console.assert(
        false,
        'NOT REACHED: ListView.prototype.prepareNewCell should be overridden.');
    return new ListCell();
  }

  /**
   * @param {!ListCell} cell
   */
  throwAwayCell(cell) {
    delete this._cells[cell.row];
    cell.throwAway();
  }

  /**
   * @return {!number}
   */
  firstVisibleRow() {
    return this.rowAtScrollOffset(this.scrollView.contentOffset());
  }

  /**
   * @return {!number}
   */
  lastVisibleRow() {
    return this.rowAtScrollOffset(
        this.scrollView.contentOffset() + this.scrollView.height() - 1);
  }

  /**
   * @param {!ScrollView} scrollView
   */
  scrollViewDidChangeContentOffset(scrollView) {
    this.setNeedsUpdateCells(true);
  }

  /**
   * @param {!ScrollView} scrollView
   */
  scrollViewDidChangeHeight(scrollView) {
    this.setNeedsUpdateCells(true);
  }

  /**
   * @param {!ScrollView} scrollView
   */
  scrollViewDidChangePartition(scrollView) {
    this.setNeedsUpdateCells(true);
  }

  updateCells() {
    const firstVisibleRow = this.firstVisibleRow();
    const lastVisibleRow = this.lastVisibleRow();
    console.assert(firstVisibleRow <= lastVisibleRow);
    for (let c in this._cells) {
      const cell = this._cells[c];
      if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
        this.throwAwayCell(cell);
    }
    for (let i = firstVisibleRow; i <= lastVisibleRow; ++i) {
      const cell = this._cells[i];
      if (cell)
        cell.setPosition(this.scrollView.contentPositionForContentOffset(
            this.scrollOffsetForRow(cell.row)));
      else
        this.addCellIfNecessary(i);
    }
    this.setNeedsUpdateCells(false);
  }

  /**
   * @return {!number} Width in pixels.
   */
  width() {
    return this._width;
  }

  /**
   * @param {!number} width Width in pixels.
   */
  setWidth(width) {
    if (this._width === width)
      return;
    this._width = width;
    this.scrollView.setWidth(this._width);
    for (let c in this._cells) {
      this._cells[c].setWidth(this._width);
    }
    this.element.style.width = this._width + 'px';
    this.setNeedsUpdateCells(true);
  }

  /**
   * @return {!number} Height in pixels.
   */
  height() {
    return this.scrollView.height();
  }

  /**
   * @param {!number} height Height in pixels.
   */
  setHeight(height) {
    this.scrollView.setHeight(height);
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    const clickedCellElement =
        enclosingNodeOrSelfWithClass(event.target, ListCell.ClassNameListCell);
    if (!clickedCellElement)
      return;
    const clickedCell = clickedCellElement.$view;
    if (clickedCell.row !== this.selectedRow)
      this.select(clickedCell.row);
  }

  /**
   * @param {!number} row
   */
  select(row) {
    if (this.selectedRow === row)
      return;
    this.deselect();
    if (row === ListView.NoSelection)
      return;
    this.selectedRow = row;
    const selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
      selectedCell.setSelected(true);
  }

  deselect() {
    if (this.selectedRow === ListView.NoSelection)
      return;
    const selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
      selectedCell.setSelected(false);
    this.selectedRow = ListView.NoSelection;
  }

  /**
   * @param {!number} row
   * @param {!boolean} animate
   */
  scrollToRow(row, animate) {
    this.scrollView.scrollTo(this.scrollOffsetForRow(row), animate);
  }
}

// ----------------------------------------------------------------

class ScrubbyScrollBar extends View {
  /**
   * @extends View
   * @param {!ScrollView} scrollView
   */
  constructor(scrollView) {
    super(createElement('div', ScrubbyScrollBar.ClassNameScrubbyScrollBar));
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

    /**
     * @type {?function}
     * @protected
     */
    this._mouseUpEventListener = null;
    /**
     * @type {?function}
     * @protected
     */
    this._mouseMoveEventListener = null;
    /**
     * @type {?function}
     * @protected
     */
    this._touchEndEventListener = null;
    /**
     * @type {?function}
     * @protected
     */
    this._touchMoveEventListener = null;

    this.element.addEventListener(
        'mousedown', this.onMouseDown.bind(this), false);
    this.element.addEventListener(
        'touchstart', this.onTouchStart.bind(this), false);
  }

  static ScrollInterval = 16;
  static ThumbMargin = 2;
  static ThumbHeight = 30;
  static ClassNameScrubbyScrollBar = 'scrubby-scroll-bar';
  static ClassNameScrubbyScrollThumb = 'scrubby-scroll-thumb';

  /**
   * @param {?Event} event
   */
  onTouchStart(event) {
    const touch = event.touches[0];
    this._setThumbPositionFromEventPosition(touch.clientY);
    if (this._thumbStyleTopAnimator)
      this._thumbStyleTopAnimator.stop();
    this._timer = setInterval(
        this.onScrollTimer.bind(this), ScrubbyScrollBar.ScrollInterval);
    this._touchMoveEventListener = this.onWindowTouchMove.bind(this);
    window.addEventListener('touchmove', this._touchMoveEventListener, false);
    this._touchEndEventListener = this.onWindowTouchEnd.bind(this);
    window.addEventListener('touchend', this._touchEndEventListener, false);
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * @param {?Event} event
   */
  onWindowTouchMove(event) {
    const touch = event.touches[0];
    this._setThumbPositionFromEventPosition(touch.clientY);
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * @param {?Event} event
   */
  onWindowTouchEnd(event) {
    this._thumbStyleTopAnimator = new TransitionAnimator();
    this._thumbStyleTopAnimator.step =
        this.onThumbStyleTopAnimationStep.bind(this);
    this._thumbStyleTopAnimator.setFrom(this.thumb.offsetTop);
    this._thumbStyleTopAnimator.setTo((this._height - this._thumbHeight) / 2);
    this._thumbStyleTopAnimator.timingFunction =
        AnimationTimingFunction.EaseInOut;
    this._thumbStyleTopAnimator.duration = 100;
    this._thumbStyleTopAnimator.start();

    window.removeEventListener(
        'touchmove', this._touchMoveEventListener, false);
    window.removeEventListener('touchend', this._touchEndEventListener, false);
    clearInterval(this._timer);
  }

  /**
   * @return {!number} Height of the view in pixels.
   */
  height() {
    return this._height;
  }

  /**
   * @param {!number} height Height of the view in pixels.
   */
  setHeight(height) {
    if (this._height === height)
      return;
    this._height = height;
    this.element.style.height = this._height + 'px';
    this.thumb.style.top = (this._height - this._thumbHeight) / 2 + 'px';
    this._thumbPosition = 0;
  }

  /**
   * @param {!number} height Height of the scroll bar thumb in pixels.
   */
  setThumbHeight(height) {
    if (this._thumbHeight === height)
      return;
    this._thumbHeight = height;
    this.thumb.style.height = this._thumbHeight + 'px';
    this.thumb.style.top = (this._height - this._thumbHeight) / 2 + 'px';
    this._thumbPosition = 0;
  }

  /**
   * @param {number} position
   */
  _setThumbPositionFromEventPosition(position) {
    const thumbMin = ScrubbyScrollBar.ThumbMargin;
    const thumbMax =
        this._height - this._thumbHeight - ScrubbyScrollBar.ThumbMargin * 2;
    const y = position - this.element.getBoundingClientRect().top -
        this.element.clientTop + this.element.scrollTop;
    let thumbPosition = y - this._thumbHeight / 2;
    thumbPosition = Math.max(thumbPosition, thumbMin);
    thumbPosition = Math.min(thumbPosition, thumbMax);
    this.thumb.style.top = thumbPosition + 'px';
    this._thumbPosition =
        1.0 - ((thumbPosition - thumbMin) / (thumbMax - thumbMin)) * 2;
  }

  /**
   * @param {?Event} event
   */
  onMouseDown(event) {
    this._setThumbPositionFromEventPosition(event.clientY);

    this._mouseMoveEventListener = this.onWindowMouseMove.bind(this);
    window.addEventListener('mousemove', this._mouseMoveEventListener, false);
    this._mouseUpEventListener = this.onWindowMouseUp.bind(this);
    window.addEventListener('mouseup', this._mouseUpEventListener, false);
    if (this._thumbStyleTopAnimator)
      this._thumbStyleTopAnimator.stop();
    this._timer = setInterval(
        this.onScrollTimer.bind(this), ScrubbyScrollBar.ScrollInterval);
    event.stopPropagation();
    event.preventDefault();
  }

  /**
   * @param {?Event} event
   */
  onWindowMouseMove(event) {
    this._setThumbPositionFromEventPosition(event.clientY);
  }

  /**
   * @param {?Event} event
   */
  onWindowMouseUp(event) {
    this._thumbStyleTopAnimator = new TransitionAnimator();
    this._thumbStyleTopAnimator.step =
        this.onThumbStyleTopAnimationStep.bind(this);
    this._thumbStyleTopAnimator.setFrom(this.thumb.offsetTop);
    this._thumbStyleTopAnimator.setTo((this._height - this._thumbHeight) / 2);
    this._thumbStyleTopAnimator.timingFunction =
        AnimationTimingFunction.EaseInOut;
    this._thumbStyleTopAnimator.duration = 100;
    this._thumbStyleTopAnimator.start();

    window.removeEventListener(
        'mousemove', this._mouseMoveEventListener, false);
    window.removeEventListener('mouseup', this._mouseUpEventListener, false);
    clearInterval(this._timer);
  }

  /**
   * @param {!Animator} animator
   */
  onThumbStyleTopAnimationStep(animator) {
    this.thumb.style.top = animator.currentValue + 'px';
  }

  onScrollTimer() {
    let scrollAmount = Math.pow(this._thumbPosition, 2) * 10;
    if (this._thumbPosition > 0)
      scrollAmount = -scrollAmount;
    this.scrollView.scrollBy(scrollAmount, false);
  }
}

// ----------------------------------------------------------------

// Mixin containing utilities for identifying and navigating between
// valid day/week/month ranges.
function dateRangeManagerMixin(baseClass) {
  class DateRangeManager extends baseClass {
    _setValidDateConfig(config) {
      this.config = {};

      this.config.minimum = (typeof config.min !== 'undefined' && config.min) ?
          parseDateString(config.min) :
          this._dateTypeConstructor.Minimum;
      this.config.maximum = (typeof config.max !== 'undefined' && config.max) ?
          parseDateString(config.max) :
          this._dateTypeConstructor.Maximum;
      this.config.minimumValue = this.config.minimum.valueOf();
      this.config.maximumValue = this.config.maximum.valueOf();
      this.config.step = (typeof config.step !== 'undefined') ?
          Number(config.step) :
          this._dateTypeConstructor.DefaultStep;
      this.config.stepBase = (typeof config.stepBase !== 'undefined') ?
          Number(config.stepBase) :
          this._dateTypeConstructor.DefaultStepBase;
    }

    _isValidForStep(value) {
      // nextAllowedValue is the time closest (looking forward) to value that is
      // within the interval specified by the step and the stepBase.  This may
      // be equal to value.
      const nextAllowedValue =
          (Math.ceil((value - this.config.stepBase) / this.config.step) *
           this.config.step) +
          this.config.stepBase;
      // If the nextAllowedValue is between value and the next nearest possible
      // time for this control type (determined by adding the smallest time
      // interval, given by DefaultStep, to value) then we consider it to be
      // valid.
      return nextAllowedValue < (value + this._dateTypeConstructor.DefaultStep);
    }

    /**
     * @param {!number} value
     * @return {!boolean}
     */
    _outOfRange(value) {
      return value < this.config.minimumValue ||
          value > this.config.maximumValue;
    }

    /**
     * @param {!DateType} dayOrWeekOrMonth
     * @return {!boolean}
     */
    isValid(dayOrWeekOrMonth) {
      const value = dayOrWeekOrMonth.valueOf();
      return dayOrWeekOrMonth instanceof this._dateTypeConstructor &&
          !this._outOfRange(value) && this._isValidForStep(value);
    }

    /**
     * @param {!DayOrWeekOrMonth} dayOrWeekOrMonth
     * @return {?DayOrWeekOrMonth}
     */
    getNearestValidRangeLookingForward(dayOrWeekOrMonth) {
      if (dayOrWeekOrMonth < this.config.minimumValue) {
        // Performance optimization: avoid wasting lots of time in the below
        // loop if dayOrWeekOrMonth is significantly less than the min.
        dayOrWeekOrMonth =
            this._dateTypeConstructor.createFromValue(this.config.minimumValue);
      }

      while (!this.isValid(dayOrWeekOrMonth) &&
             dayOrWeekOrMonth < this.config.maximumValue) {
        dayOrWeekOrMonth = dayOrWeekOrMonth.next();
      }

      return this.isValid(dayOrWeekOrMonth) ? dayOrWeekOrMonth : null;
    }

    /**
     * @param {!DayOrWeekOrMonth} dayOrWeekOrMonth
     * @return {?DayOrWeekOrMonth}
     */
    getNearestValidRangeLookingBackward(dayOrWeekOrMonth) {
      if (dayOrWeekOrMonth > this.config.maximumValue) {
        // Performance optimization: avoid wasting lots of time in the below
        // loop if dayOrWeekOrMonth is significantly greater than the max.
        dayOrWeekOrMonth =
            this._dateTypeConstructor.createFromValue(this.config.maximumValue);
      }

      while (!this.isValid(dayOrWeekOrMonth) &&
             dayOrWeekOrMonth > this.config.minimumValue) {
        dayOrWeekOrMonth = dayOrWeekOrMonth.previous();
      }

      return this.isValid(dayOrWeekOrMonth) ? dayOrWeekOrMonth : null;
    }

    /**
     * @param {!DayOrWeekOrMonth} dayOrWeekOrMonth
     * @param {!boolean} lookForwardFirst
     * @return {?DayOrWeekOrMonth}
     */
    getNearestValidRange(dayOrWeekOrMonth, lookForwardFirst) {
      let result = null;
      if (lookForwardFirst) {
        if (!(result =
                  this.getNearestValidRangeLookingForward(dayOrWeekOrMonth))) {
          result = this.getNearestValidRangeLookingBackward(dayOrWeekOrMonth);
        }
      } else {
        if (!(result =
                  this.getNearestValidRangeLookingBackward(dayOrWeekOrMonth))) {
          result = this.getNearestValidRangeLookingForward(dayOrWeekOrMonth);
        }
      }

      return result;
    }

    /**
     * @param {!Day} day
     * @param {!boolean} lookForwardFirst
     * @return {?DayOrWeekOrMonth}
     */
    getValidRangeNearestToDay(day, lookForwardFirst) {
      const dayOrWeekOrMonth = this._dateTypeConstructor.createFromDay(day);
      return this.getNearestValidRange(dayOrWeekOrMonth, lookForwardFirst);
    }
  }
  return DateRangeManager;
}

// ----------------------------------------------------------------

class YearListCell extends ListCell {
  /**
   * @param {!Array} shortMonthLabels
   */
  constructor(shortMonthLabels) {
    super();
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
    const monthChooserElement =
        createElement('div', YearListCell.ClassNameMonthChooser);
    for (let r = 0; r < YearListCell.ButtonRows; ++r) {
      const buttonsRow =
          createElement('div', YearListCell.ClassNameMonthButtonsRow);
      buttonsRow.setAttribute('role', 'row');
      for (let c = 0; c < YearListCell.ButtonColumns; ++c) {
        const month = c + r * YearListCell.ButtonColumns;
        const button = createElement(
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

  static _Height = hasInaccuratePointingDevice() ? 31 : 25;
  static _Height = 25;
  static GetHeight() {
    return YearListCell._Height;
  }
  static BorderBottomWidth = 1;
  static ButtonRows = 3;
  static ButtonColumns = 4;
  static _SelectedHeight = 128;
  static GetSelectedHeight() {
    return YearListCell._SelectedHeight;
  }
  static ClassNameYearListCell = 'year-list-cell';
  static ClassNameLabel = 'label';
  static ClassNameMonthChooser = 'month-chooser';
  static ClassNameMonthButtonsRow = 'month-buttons-row';
  static ClassNameMonthButton = 'month-button';
  static ClassNameHighlighted = 'highlighted';
  static ClassNameSelected = 'selected';
  static ClassNameToday = 'today';

  static _recycleBin = [];

  /**
   * @return {!Array}
   * @override
   */
  _recycleBin() {
    return YearListCell._recycleBin;
  }

  /**
   * @param {!number} row
   */
  reset(row) {
    this.row = row;
    this.label.textContent = row + 1;
    for (let i = 0; i < this.monthButtons.length; ++i) {
      this.monthButtons[i].classList.remove(YearListCell.ClassNameHighlighted);
      this.monthButtons[i].classList.remove(YearListCell.ClassNameSelected);
      this.monthButtons[i].classList.remove(YearListCell.ClassNameToday);
    }
    this.show();
  }

  /**
   * @return {!number} The height in pixels.
   */
  height() {
    return this._height;
  }

  /**
   * @param {!number} height Height in pixels.
   */
  setHeight(height) {
    if (this._height === height)
      return;
    this._height = height;
    this.element.style.height = this._height + 'px';
  }
}

// ----------------------------------------------------------------

// clang-format off
class YearListView extends dateRangeManagerMixin(ListView) {
  // clang-format on
  /**
   * @param {!Month} minimumMonth
   * @param {!Month} maximumMonth
   */
  constructor(minimumMonth, maximumMonth, config) {
    super();
    this.element.classList.add('year-list-view');

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

    this.element.addEventListener('keydown', this.onKeyDown.bind(this));

    if (config && config.mode == 'month') {
      this.type = 'month';
      this._dateTypeConstructor = Month;

      this._setValidDateConfig(config);

      this._hadValidValueWhenOpened = false;
      const initialSelection = parseDateString(config.currentValue);
      if (initialSelection) {
        this._hadValidValueWhenOpened = this.isValid(initialSelection);
        this._selectedMonth = this.getNearestValidRange(
            initialSelection, /*lookForwardFirst*/ true);
      } else {
        // Ensure that the next month closest to today is selected to start with
        // so that the user can simply submit the popup to choose it.
        this._selectedMonth = this.getValidRangeNearestToDay(
            this._dateTypeConstructor.createFromToday(),
            /*lookForwardFirst*/ true);
      }

      this._initialSelectedMonth = this._selectedMonth;
    } else {
      // This is a month switcher menu embedded in another calendar control.
      // Set up our config so that getNearestValidRangeLookingForward(Backward)
      // when called on this YearListView will navigate by month.
      this.config = {};
      this.config.minimumValue = minimumMonth;
      this.config.maximumValue = maximumMonth;
      this.config.step = Month.DefaultStep;
      this.config.stepBase = Month.DefaultStepBase;
      this._dateTypeConstructor = Month;
    }
  }

  static _VisibleYears = 4;
  static _Height = YearListCell._SelectedHeight - 1 +
      YearListView._VisibleYears * YearListCell._Height;
  static GetHeight() {
    return YearListView._Height;
  };
  static EventTypeYearListViewDidHide = 'yearListViewDidHide';
  static EventTypeYearListViewDidSelectMonth = 'yearListViewDidSelectMonth';

  /**
   * @param {!number} width Width in pixels.
   * @override
   */
  setWidth(width) {
    super.setWidth(width - this.scrubbyScrollBar.element.offsetWidth);
    this.element.style.width = width + 'px';
  }

  /**
   * @param {!number} height Height in pixels.
   * @override
   */
  setHeight(height) {
    super.setHeight(height);
    this.scrubbyScrollBar.setHeight(height);
  }

  /**
   * @enum {number}
   */
  static RowAnimationDirection = {Opening: 0, Closing: 1};

  /**
   * @param {!number} row
   * @param {!YearListView.RowAnimationDirection} direction
   */
  _animateRow(row, direction) {
    let fromValue = direction === YearListView.RowAnimationDirection.Closing ?
        YearListCell.GetSelectedHeight() :
        YearListCell.GetHeight();
    const oldAnimator = this._runningAnimators[row];
    if (oldAnimator) {
      oldAnimator.stop();
      fromValue = oldAnimator.currentValue;
    }
    const cell = this.cellAtRow(row);
    const animator = new TransitionAnimator();
    animator.step = this.onCellHeightAnimatorStep.bind(this);
    animator.setFrom(fromValue);
    animator.setTo(
        direction === YearListView.RowAnimationDirection.Opening ?
            YearListCell.GetSelectedHeight() :
            YearListCell.GetHeight());
    animator.timingFunction = AnimationTimingFunction.EaseInOut;
    animator.duration = 300;
    animator.row = row;
    animator.on(
        Animator.EventTypeDidAnimationStop,
        this.onCellHeightAnimatorDidStop.bind(this));
    this._runningAnimators[row] = animator;
    this._animatingRows.push(row);
    this._animatingRows.sort();
    animator.start();
  }

  /**
   * @param {?Animator} animator
   */
  onCellHeightAnimatorDidStop(animator) {
    delete this._runningAnimators[animator.row];
    const index = this._animatingRows.indexOf(animator.row);
    this._animatingRows.splice(index, 1);
  }

  /**
   * @param {!Animator} animator
   */
  onCellHeightAnimatorStep(animator) {
    const cell = this.cellAtRow(animator.row);
    if (cell)
      cell.setHeight(animator.currentValue);
    this.updateCells();
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    const oldSelectedRow = this.selectedRow;
    super.onClick(event);
    const year = this.selectedRow + 1;
    if (this.selectedRow !== oldSelectedRow) {
      // Always start with first month when changing the year.
      const month = new Month(year, 0);
      this.scrollView.scrollTo(
          this.selectedRow * YearListCell.GetHeight(), true);
    } else {
      const monthButton = enclosingNodeOrSelfWithClass(
          event.target, YearListCell.ClassNameMonthButton);
      if (!monthButton || monthButton.getAttribute('aria-disabled') == 'true')
        return;
      const month = parseInt(monthButton.dataset.month, 10);
      this.dispatchEvent(
          YearListView.EventTypeYearListViewDidSelectMonth, this,
          new Month(year, month));
    }
  }

  /**
   * @param {!number} scrollOffset
   * @return {!number}
   * @override
   */
  rowAtScrollOffset(scrollOffset) {
    let remainingOffset = scrollOffset;
    let lastAnimatingRow = 0;
    const rowsWithIrregularHeight = this._animatingRows.slice();
    if (this.selectedRow > -1 && !this._runningAnimators[this.selectedRow]) {
      rowsWithIrregularHeight.push(this.selectedRow);
      rowsWithIrregularHeight.sort();
    }
    for (let i = 0; i < rowsWithIrregularHeight.length; ++i) {
      const row = rowsWithIrregularHeight[i];
      const animator = this._runningAnimators[row];
      const rowHeight =
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
  }

  /**
   * @param {!number} row
   * @return {!number}
   * @override
   */
  scrollOffsetForRow(row) {
    let scrollOffset = row * YearListCell.GetHeight();
    for (let i = 0; i < this._animatingRows.length; ++i) {
      const animatingRow = this._animatingRows[i];
      if (animatingRow >= row)
        break;
      const animator = this._runningAnimators[animatingRow];
      scrollOffset += animator.currentValue - YearListCell.GetHeight();
    }
    if (this.selectedRow > -1 && this.selectedRow < row &&
        !this._runningAnimators[this.selectedRow]) {
      scrollOffset +=
          YearListCell.GetSelectedHeight() - YearListCell.GetHeight();
    }
    return scrollOffset;
  }

  /**
   * @param {!number} row
   * @return {!YearListCell}
   * @override
   */
  prepareNewCell(row) {
    const cell = YearListCell._recycleBin.pop() ||
        new YearListCell(global.params.shortMonthLabels);
    cell.reset(row);
    cell.setSelected(this.selectedRow === row);
    for (let i = 0; i < cell.monthButtons.length; ++i) {
      const month = new Month(row + 1, i);
      cell.monthButtons[i].id = month.toString();
      if (this.type === 'month') {
        cell.monthButtons[i].setAttribute(
            'aria-disabled', this.isValid(month) ? 'false' : 'true');
      } else {
        cell.monthButtons[i].setAttribute(
            'aria-disabled',
            this._minimumMonth > month || this._maximumMonth < month ? 'true' :
                                                                       'false');
      }
      cell.monthButtons[i].setAttribute('aria-label', month.toLocaleString());
      cell.monthButtons[i].setAttribute('aria-selected', false);
    }
    if (this._selectedMonth && (this._selectedMonth.year - 1) === row) {
      const monthButton = cell.monthButtons[this._selectedMonth.month];
      monthButton.classList.add(YearListCell.ClassNameSelected);
      this.element.setAttribute('aria-activedescendant', monthButton.id);
      monthButton.setAttribute('aria-selected', true);
    }
    const todayMonth = Month.createFromToday();
    if ((todayMonth.year - 1) === row) {
      const monthButton = cell.monthButtons[todayMonth.month];
      monthButton.classList.add(YearListCell.ClassNameToday);
    }

    const animator = this._runningAnimators[row];
    if (animator)
      cell.setHeight(animator.currentValue);
    else if (row === this.selectedRow)
      cell.setHeight(YearListCell.GetSelectedHeight());
    else
      cell.setHeight(YearListCell.GetHeight());
    return cell;
  }

  /**
   * @override
   */
  updateCells() {
    const firstVisibleRow = this.firstVisibleRow();
    const lastVisibleRow = this.lastVisibleRow();
    console.assert(firstVisibleRow <= lastVisibleRow);
    for (let c in this._cells) {
      const cell = this._cells[c];
      if (cell.row < firstVisibleRow || cell.row > lastVisibleRow)
        this.throwAwayCell(cell);
    }
    for (let i = firstVisibleRow; i <= lastVisibleRow; ++i) {
      const cell = this._cells[i];
      if (cell)
        cell.setPosition(this.scrollView.contentPositionForContentOffset(
            this.scrollOffsetForRow(cell.row)));
      else
        this.addCellIfNecessary(i);
    }
    this.setNeedsUpdateCells(false);
  }

  /**
   * @override
   */
  deselect() {
    if (this.selectedRow === ListView.NoSelection)
      return;
    const selectedCell = this._cells[this.selectedRow];
    if (selectedCell)
      selectedCell.setSelected(false);
    this._animateRow(
        this.selectedRow, YearListView.RowAnimationDirection.Closing);
    this.selectedRow = ListView.NoSelection;
    this.setNeedsUpdateCells(true);
  }

  deselectWithoutAnimating() {
    if (this.selectedRow === ListView.NoSelection)
      return;
    const selectedCell = this._cells[this.selectedRow];
    if (selectedCell) {
      selectedCell.setSelected(false);
      selectedCell.setHeight(YearListCell.GetHeight());
    }
    this.selectedRow = ListView.NoSelection;
    this.setNeedsUpdateCells(true);
  }

  /**
   * @param {!number} row
   * @override
   */
  select(row) {
    if (this.selectedRow === row)
      return;
    this.deselect();
    if (row === ListView.NoSelection)
      return;
    this.selectedRow = row;
    if (this.selectedRow !== ListView.NoSelection) {
      const selectedCell = this._cells[this.selectedRow];
      this._animateRow(
          this.selectedRow, YearListView.RowAnimationDirection.Opening);
      if (selectedCell)
        selectedCell.setSelected(true);
    }
    this.setNeedsUpdateCells(true);
  }

  /**
   * @param {!number} row
   */
  selectWithoutAnimating(row) {
    if (this.selectedRow === row)
      return;
    this.deselectWithoutAnimating();
    if (row === ListView.NoSelection)
      return;
    this.selectedRow = row;
    if (this.selectedRow !== ListView.NoSelection) {
      const selectedCell = this._cells[this.selectedRow];
      if (selectedCell) {
        selectedCell.setSelected(true);
        selectedCell.setHeight(YearListCell.GetSelectedHeight());
      }
    }
    this.setNeedsUpdateCells(true);
  }

  /**
   * @param {!Month} month
   * @return {?HTMLDivElement}
   */
  buttonForMonth(month) {
    if (!month)
      return null;
    const row = month.year - 1;
    const cell = this.cellAtRow(row);
    if (!cell)
      return null;
    return cell.monthButtons[month.month];
  }

  dehighlightMonth() {
    if (!this.highlightedMonth)
      return;
    const monthButton = this.buttonForMonth(this.highlightedMonth);
    if (monthButton) {
      monthButton.classList.remove(YearListCell.ClassNameHighlighted);
    }
    this.highlightedMonth = null;
    this.element.removeAttribute('aria-activedescendant');
  }

  /**
   * @param {!Month} month
   */
  highlightMonth(month) {
    if (this.highlightedMonth && this.highlightedMonth.equals(month))
      return;
    this.dehighlightMonth();
    this.highlightedMonth = month;
    if (!this.highlightedMonth)
      return;
    const monthButton = this.buttonForMonth(this.highlightedMonth);
    if (monthButton) {
      monthButton.classList.add(YearListCell.ClassNameHighlighted);
      this.element.setAttribute('aria-activedescendant', monthButton.id);
    }
  }

  setSelectedMonth(month) {
    const oldMonthButton = this.buttonForMonth(this._selectedMonth);
    if (oldMonthButton) {
      oldMonthButton.classList.remove(YearListCell.ClassNameSelected);
      oldMonthButton.setAttribute('aria-selected', false);
    }

    this._selectedMonth = month;

    const newMonthButton = this.buttonForMonth(this._selectedMonth);
    if (newMonthButton) {
      newMonthButton.classList.add(YearListCell.ClassNameSelected);
      this.element.setAttribute('aria-activedescendant', newMonthButton.id);
      newMonthButton.setAttribute('aria-selected', true);
    }
  }

  setSelectedMonthAndUpdateView(month) {
    this.setSelectedMonth(month);

    this.select(this._selectedMonth.year - 1);

    this.scrollView.scrollTo(this.selectedRow * YearListCell.GetHeight(), true);
  }

  showSelectedMonth() {
    const monthButton = this.buttonForMonth(this._selectedMonth);
    if (monthButton) {
      monthButton.classList.add(YearListCell.ClassNameSelected);
    }
  }

  /**
   * @param {!Month} month
   */
  show(month) {
    this._ignoreMouseOutUntillNextMouseOver = true;

    this.scrollToRow(month.year - 1, false);
    this.selectWithoutAnimating(month.year - 1);
    this.showSelectedMonth();
  }

  hide() {
    this.dispatchEvent(YearListView.EventTypeYearListViewDidHide, this);
  }

  /**
   * @param {!Month} month
   */
  _moveHighlightTo(month) {
    this.highlightMonth(month);
    this.select(this.highlightedMonth.year - 1);
    this.scrollView.scrollTo(this.selectedRow * YearListCell.GetHeight(), true);
    return true;
  }

  /**
   * @param {?Event} event
   */
  onKeyDown(event) {
    const key = event.key;
    let eventHandled = false;
    if (this._selectedMonth) {
      if (global.params.isLocaleRTL ? key == 'ArrowRight' :
                                      key == 'ArrowLeft') {
        const newSelection = this.getNearestValidRangeLookingBackward(
            this._selectedMonth.previous());
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'ArrowUp') {
        const newSelection = this.getNearestValidRangeLookingBackward(
            this._selectedMonth.previous(YearListCell.ButtonColumns));
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (
          global.params.isLocaleRTL ? key == 'ArrowLeft' :
                                      key == 'ArrowRight') {
        const newSelection =
            this.getNearestValidRangeLookingForward(this._selectedMonth.next());
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'ArrowDown') {
        const newSelection = this.getNearestValidRangeLookingForward(
            this._selectedMonth.next(YearListCell.ButtonColumns));
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'PageUp') {
        const newSelection = this.getNearestValidRangeLookingBackward(
            this._selectedMonth.previous(MonthsPerYear));
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'PageDown') {
        const newSelection = this.getNearestValidRangeLookingForward(
            this._selectedMonth.next(MonthsPerYear));
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'Home') {
        const newMonth = this._selectedMonth.month === 0 ?
            new Month(this._selectedMonth.year - 1, 0) :
            new Month(this._selectedMonth.year, 0);
        const newSelection = this.getNearestValidRangeLookingBackward(newMonth);
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (key == 'End') {
        const lastMonthNum = MonthsPerYear - 1;
        const newMonth = this._selectedMonth.month === lastMonthNum ?
            new Month(this._selectedMonth.year + 1, lastMonthNum) :
            new Month(this._selectedMonth.year, lastMonthNum);
        const newSelection = this.getNearestValidRangeLookingForward(newMonth);
        if (newSelection) {
          this.setSelectedMonthAndUpdateView(newSelection);
        }
      } else if (this.type !== 'month') {
        if (key == 'Enter') {
          this.dispatchEvent(
              YearListView.EventTypeYearListViewDidSelectMonth, this,
              this._selectedMonth);
        } else if (key == 'Escape') {
          this.hide();
          eventHandled = true;
        }
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
  }
}

// ----------------------------------------------------------------

class MonthPopupView extends View {
  /**
   * @param {!Month} minimumMonth
   * @param {!Month} maximumMonth
   */
  constructor(minimumMonth, maximumMonth) {
    super(createElement('div', MonthPopupView.ClassNameMonthPopupView));

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

    this.element.addEventListener('click', this.onClick.bind(this), false);
  }

  static ClassNameMonthPopupView = 'month-popup-view';

  show(initialMonth, calendarTableRect) {
    this.isVisible = true;
    if (global.params.mode == 'datetime-local') {
      // Place the month popup under the datetimelocal-picker element so that
      // the datetimelocal-picker element receives its keyboard and click
      // events. For other calendar control types, these events are handled via
      // the body element.
      document.querySelector('datetimelocal-picker').appendChild(this.element);
    } else {
      document.body.appendChild(this.element);
    }
    this.yearListView.setWidth(calendarTableRect.width - 2);
    this.yearListView.setHeight(YearListView.GetHeight());
    if (global.params.isLocaleRTL)
      this.yearListView.element.style.right = calendarTableRect.x + 'px';
    else
      this.yearListView.element.style.left = calendarTableRect.x + 'px';
    this.yearListView.element.style.top = calendarTableRect.y + 'px';
    this.yearListView.show(initialMonth);
    this.yearListView.element.focus();
  }

  hide() {
    if (!this.isVisible)
      return;
    this.isVisible = false;
    this.element.parentNode.removeChild(this.element);
    this.yearListView.hide();
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    if (event.target !== this.element)
      return;
    this.hide();
  }
}

// ----------------------------------------------------------------

class MonthPopupButton extends View {
  /**
   * @extends View
   * @param {!number} maxWidth Maximum width in pixels.
   */
  constructor(maxWidth) {
    super(createElement('button', MonthPopupButton.ClassNameMonthPopupButton));
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

    this.element.addEventListener('click', this.onClick.bind(this), false);
  }

  static ClassNameMonthPopupButton = 'month-popup-button';
  static ClassNameMonthPopupButtonLabel = 'month-popup-button-label';
  static ClassNameDisclosureTriangle = 'disclosure-triangle';
  static EventTypeButtonClick = 'buttonClick';

  /**
   * @param {!number} maxWidth Maximum available width in pixels.
   * @return {!boolean}
   */
  _shouldUseShortMonth(maxWidth) {
    document.body.appendChild(this.element);
    let month = Month.Maximum;
    for (let i = 0; i < MonthsPerYear; ++i) {
      this.labelElement.textContent = month.toLocaleString();
      if (this.element.offsetWidth > maxWidth)
        return true;
      month = month.previous();
    }
    document.body.removeChild(this.element);
    return false;
  }

  /**
   * @param {!Month} month
   */
  setCurrentMonth(month) {
    this.labelElement.textContent = this._useShortMonth ?
        month.toShortLocaleString() :
        month.toLocaleString();
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    this.dispatchEvent(MonthPopupButton.EventTypeButtonClick, this);
  }
}

// ----------------------------------------------------------------

class ClearButton extends View {
  /**
   * @extends View
   */
  constructor() {
    super(createElement('button', ClearButton.ClassNameClearButton));
    this.element.addEventListener('click', this.onClick.bind(this), false);
  }

  static ClassNameClearButton = 'clear-button';
  static EventTypeButtonClick = 'buttonClick';

  /**
   * @param {?Event} event
   */
  onClick(event) {
    this.dispatchEvent(ClearButton.EventTypeButtonClick, this);
  }
}

// ----------------------------------------------------------------

class CalendarNavigationButton extends View {
  /**
   * @extends View
   */
  constructor() {
    super(createElement(
        'button', CalendarNavigationButton.ClassNameCalendarNavigationButton));
    /**
     * @type {number} Threshold for starting repeating clicks in milliseconds.
     */
    this.repeatingClicksStartingThreshold =
        CalendarNavigationButton.DefaultRepeatingClicksStartingThreshold;
    /**
     * @type {number} Interval between repeating clicks in milliseconds.
     */
    this.repeatingClicksInterval =
        CalendarNavigationButton.DefaultRepeatingClicksInterval;
    /**
     * @type {?number} The ID for the timeout that triggers the repeating clicks.
     */
    this._timer = null;

    this._onWindowMouseUpBound = this.onWindowMouseUp.bind(this);

    this.element.addEventListener('click', this.onClick.bind(this), false);
    this.element.addEventListener(
        'mousedown', this.onMouseDown.bind(this), false);
    this.element.addEventListener(
        'touchstart', this.onTouchStart.bind(this), false);
  }

  static DefaultRepeatingClicksStartingThreshold = 600;
  static DefaultRepeatingClicksInterval = 300;
  static LeftMargin = 4;
  static Width = 24;
  static ClassNameCalendarNavigationButton = 'calendar-navigation-button';
  static EventTypeButtonClick = 'buttonClick';
  static EventTypeRepeatingButtonClick = 'repeatingButtonClick';

  /**
   * @param {!boolean} disabled
   */
  setDisabled(disabled) {
    this.element.disabled = disabled;
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    this.dispatchEvent(CalendarNavigationButton.EventTypeButtonClick, this);
  }

  /**
   * @param {?Event} event
   */
  onTouchStart(event) {
    if (this._timer !== null)
      return;
    this._timer = setTimeout(
        this.onRepeatingClick.bind(this),
        this.repeatingClicksStartingThreshold);
    window.addEventListener(
        'touchend', this.onWindowTouchEnd.bind(this), false);
  }

  /**
   * @param {?Event} event
   */
  onWindowTouchEnd(event) {
    if (this._timer === null)
      return;
    clearTimeout(this._timer);
    this._timer = null;
    window.removeEventListener('touchend', this._onWindowMouseUpBound);
  }

  /**
   * @param {?Event} event
   */
  onMouseDown(event) {
    if (this._timer !== null)
      return;
    this._timer = setTimeout(
        this.onRepeatingClick.bind(this),
        this.repeatingClicksStartingThreshold);
    window.addEventListener('mouseup', this._onWindowMouseUpBound);
  }

  /**
   * @param {?Event} event
   */
  onWindowMouseUp(event) {
    if (this._timer === null)
      return;
    clearTimeout(this._timer);
    this._timer = null;
    window.removeEventListener('mouseup', this._onWindowMouseUpBound);
  }

  /**
   * @param {?Event} event
   */
  onRepeatingClick(event) {
    this.dispatchEvent(
        CalendarNavigationButton.EventTypeRepeatingButtonClick, this);
    this._timer = setTimeout(
        this.onRepeatingClick.bind(this), this.repeatingClicksInterval);
  }
}

// ----------------------------------------------------------------

/**
 * @param {!Day} day
 * @param {!Day} minDay
 * @param {!Day} maxDay
 * @return {boolean}
 */
function isDayOutsideOfRange(day, minDay, maxDay) {
  return day < minDay || maxDay < day;
}

/**
 * @param {!Week} week
 * @param {!Week} minWeek
 * @param {!Week} maxWeek
 * @return {boolean}
 */
function isWeekOutsideOfRange(week, minWeek, maxWeek) {
  return week < minWeek || maxWeek < week;
}

// ----------------------------------------------------------------

class CalendarHeaderView extends View {
  /**
   * @extends View
   * @param {!CalendarPicker} calendarPicker
   */
  constructor(calendarPicker) {
    super(createElement('div', CalendarHeaderView.ClassNameCalendarHeaderView));
    this.calendarPicker = calendarPicker;
    this.calendarPicker.on(
        CalendarPicker.EventTypeCurrentMonthChanged,
        this.onCurrentMonthChanged.bind(this));

    const titleElement =
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
        this.onNavigationButtonClick.bind(this));
    this._previousMonthButton.on(
        CalendarNavigationButton.EventTypeRepeatingButtonClick,
        this.onNavigationButtonClick.bind(this));
    this._previousMonthButton.element.setAttribute(
        'aria-label', global.params.axShowPreviousMonth);
    this._previousMonthButton.element.setAttribute(
        'title', global.params.axShowPreviousMonth);

    /**
     * @type {!CalendarNavigationButton}
     * @const
     */
    this._nextMonthButton = new CalendarNavigationButton();
    this._nextMonthButton.attachTo(this);
    this._nextMonthButton.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onNavigationButtonClick.bind(this));
    this._nextMonthButton.on(
        CalendarNavigationButton.EventTypeRepeatingButtonClick,
        this.onNavigationButtonClick.bind(this));
    this._nextMonthButton.element.setAttribute(
        'aria-label', global.params.axShowNextMonth);
    this._nextMonthButton.element.setAttribute(
        'title', global.params.axShowNextMonth);

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

  static Height = 24;
  static BottomMargin = 10;
  static ClassNameCalendarNavigationButtonIcon = 'navigation-button-icon';
  static _ForwardTriangle = `<svg class="${
      CalendarHeaderView
          .ClassNameCalendarNavigationButtonIcon}" width="16" height="16" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
    <path class="${
      CalendarHeaderView
          .ClassNameCalendarNavigationButtonIcon}" d="M15.3516 8.60156L8 15.9531L0.648438 8.60156L1.35156 7.89844L7.5 14.0469V0H8.5V14.0469L14.6484 7.89844L15.3516 8.60156Z" fill="#101010"/>
    </svg>`;
  static GetForwardTriangle() {
    return CalendarHeaderView._ForwardTriangle;
  }
  static _BackwardTriangle = `<svg class="${
      CalendarHeaderView
          .ClassNameCalendarNavigationButtonIcon}" width="16" height="16" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
    <path class="${
      CalendarHeaderView
          .ClassNameCalendarNavigationButtonIcon}" d="M14.6484 8.10156L8.5 1.95312V16H7.5V1.95312L1.35156 8.10156L0.648438 7.39844L8 0.046875L15.3516 7.39844L14.6484 8.10156Z" fill="#101010"/>
    </svg>`;
  static GetBackwardTriangle() {
    return CalendarHeaderView._BackwardTriangle;
  }
  static ClassNameCalendarHeaderView = 'calendar-header-view';
  static ClassNameCalendarTitle = 'calendar-title';
  static ClassNameTodayButton = 'today-button';
  static GetClassNameTodayButton() {
    return CalendarHeaderView.ClassNameTodayButton;
  }

  onCurrentMonthChanged() {
    this.monthPopupButton.setCurrentMonth(this.calendarPicker.currentMonth());
    this._previousMonthButton.setDisabled(
        this.disabled ||
        this.calendarPicker.currentMonth() <= this.calendarPicker.minimumMonth);
    this._nextMonthButton.setDisabled(
        this.disabled ||
        this.calendarPicker.currentMonth() >= this.calendarPicker.maximumMonth);
  }

  onNavigationButtonClick(sender) {
    if (sender === this._previousMonthButton) {
      this.calendarPicker.setCurrentMonth(
          this.calendarPicker.currentMonth().previous(),
          CalendarPicker.NavigationBehavior.WithAnimation);
      this.calendarPicker.ensureSelectionIsWithinCurrentMonth();
    } else if (sender === this._nextMonthButton) {
      this.calendarPicker.setCurrentMonth(
          this.calendarPicker.currentMonth().next(),
          CalendarPicker.NavigationBehavior.WithAnimation);
      this.calendarPicker.ensureSelectionIsWithinCurrentMonth();
    } else
      this.calendarPicker.selectRangeContainingDay(Day.createFromToday());
  }

  /**
   * @param {!boolean} disabled
   */
  setDisabled(disabled) {
    this.disabled = disabled;
    this._previousMonthButton.element.style.visibility =
        this.disabled ? 'hidden' : 'visible';
    this._nextMonthButton.element.style.visibility =
        this.disabled ? 'hidden' : 'visible';
    this.monthPopupButton.element.disabled = this.disabled;
    this._previousMonthButton.setDisabled(
        this.disabled ||
        this.calendarPicker.currentMonth() <= this.calendarPicker.minimumMonth);
    this._nextMonthButton.setDisabled(
        this.disabled ||
        this.calendarPicker.currentMonth() >= this.calendarPicker.maximumMonth);
    if (this._todayButton) {
      if (this.disabled) {
        this._todayButton.setDisabled(true);
      } else if (this.calendarPicker.type === 'week') {
        this._todayButton.setDisabled(isWeekOutsideOfRange(
            Week.createFromToday(), this.calendarPicker.config.minimum,
            this.calendarPicker.config.maximum));
      } else {
        this._todayButton.setDisabled(isDayOutsideOfRange(
            Day.createFromToday(), this.calendarPicker.config.minimum,
            this.calendarPicker.config.maximum));
      }
    }
  }
}

// ----------------------------------------------------------------

class DayCell extends ListCell {
  constructor() {
    super();
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
  }

  static _Width = 28;
  static GetWidth() {
    return DayCell._Width;
  }
  static _Height = 28;
  static GetHeight() {
    return DayCell._Height;
  }
  static PaddingSize = 1;
  static ClassNameDayCell = 'day-cell';
  static ClassNameHighlighted = 'highlighted';
  static ClassNameDisabled = 'disabled';
  static ClassNameCurrentMonth = 'current-month';
  static ClassNameToday = 'today';

  static _recycleBin = [];

  static recycleOrCreate() {
    return DayCell._recycleBin.pop() || new DayCell();
  }

  /**
   * @return {!Array}
   * @override
   */
  _recycleBin() {
    return DayCell._recycleBin;
  }

  /**
   * @override
   */
  throwAway() {
    super.throwAway();
    this.day = null;
  }

  /**
   * @param {!boolean} highlighted
   */
  setHighlighted(highlighted) {
    if (highlighted) {
      this.element.classList.add(DayCell.ClassNameHighlighted);
      this.element.setAttribute('aria-selected', 'true');
    } else {
      this.element.classList.remove(DayCell.ClassNameHighlighted);
      this.element.setAttribute('aria-selected', 'false');
    }
  }

  /**
   * @param {!boolean} disabled
   */
  setDisabled(disabled) {
    if (disabled)
      this.element.classList.add(DayCell.ClassNameDisabled);
    else
      this.element.classList.remove(DayCell.ClassNameDisabled);
  }

  /**
   * @param {!boolean} selected
   */
  setIsInCurrentMonth(selected) {
    if (selected)
      this.element.classList.add(DayCell.ClassNameCurrentMonth);
    else
      this.element.classList.remove(DayCell.ClassNameCurrentMonth);
  }

  /**
   * @param {!boolean} selected
   */
  setIsToday(selected) {
    if (selected)
      this.element.classList.add(DayCell.ClassNameToday);
    else
      this.element.classList.remove(DayCell.ClassNameToday);
  }

  /**
   * @param {!Day} day
   */
  reset(day) {
    this.day = day;
    this.element.textContent = localizeNumber(this.day.date.toString());
    this.element.setAttribute('aria-label', this.day.format());
    this.element.id = this.day.toString();
    this.show();
  }
}

// ----------------------------------------------------------------

class WeekNumberCell extends ListCell {
  constructor() {
    super();
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
  }

  static Width = 48;
  static _Height = DayCell._Height;
  static GetHeight() {
    return WeekNumberCell._Height;
  }
  static SeparatorWidth = 1;
  static PaddingSize = 1;
  static ClassNameWeekNumberCell = 'week-number-cell';
  static ClassNameHighlighted = 'highlighted';
  static ClassNameDisabled = 'disabled';

  static _recycleBin = [];

  /**
   * @return {!Array}
   * @override
   */
  _recycleBin() {
    return WeekNumberCell._recycleBin;
  }

  /**
   * @return {!WeekNumberCell}
   */
  static recycleOrCreate() {
    return WeekNumberCell._recycleBin.pop() || new WeekNumberCell();
  }

  /**
   * @param {!Week} week
   */
  reset(week) {
    this.week = week;
    this.element.id = week.toString();
    this.element.setAttribute('role', 'gridcell');
    this.element.setAttribute(
        'aria-label',
        window.pagePopupController.formatWeek(
            week.year, week.week, week.firstDay().format()));
    this.element.textContent = localizeNumber(this.week.week.toString());
    this.show();
  }

  /**
   * @override
   */
  throwAway() {
    super.throwAway();
    this.week = null;
  }

  setHighlighted(highlighted) {
    if (highlighted) {
      this.element.classList.add(WeekNumberCell.ClassNameHighlighted);
      this.element.setAttribute('aria-selected', 'true');
    } else {
      this.element.classList.remove(WeekNumberCell.ClassNameHighlighted);
      this.element.setAttribute('aria-selected', 'false');
    }
  }

  setDisabled(disabled) {
    if (disabled)
      this.element.classList.add(WeekNumberCell.ClassNameDisabled);
    else
      this.element.classList.remove(WeekNumberCell.ClassNameDisabled);
  }
}

// ----------------------------------------------------------------

class CalendarTableHeaderView extends View {
  /**
   * @param {!boolean} hasWeekNumberColumn
   */
  constructor(hasWeekNumberColumn) {
    super(createElement('div', 'calendar-table-header-view'));
    if (hasWeekNumberColumn) {
      const weekNumberLabelElement =
          createElement('div', 'week-number-label', global.params.weekLabel);
      weekNumberLabelElement.style.width = WeekNumberCell.Width + 'px';
      this.element.appendChild(weekNumberLabelElement);
    }
    for (let i = 0; i < DaysPerWeek; ++i) {
      const weekDayNumber = (global.params.weekStartDay + i) % DaysPerWeek;
      const labelElement = createElement(
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

  static _Height = 29;
  static GetHeight() {
    return CalendarTableHeaderView._Height;
  }
}

// ----------------------------------------------------------------

class CalendarRowCell extends ListCell {
  constructor() {
    super();

    this.element.classList.add(CalendarRowCell.ClassNameCalendarRowCell);
    if (global.params.weekStartDay === WeekDay.Sunday) {
      this.element.classList.add(CalendarRowCell.ClassNameWeekStartsOnSunday);
    }
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

  static _Height = DayCell._Height;
  static GetHeight() {
    return CalendarRowCell._Height;
  }
  static ClassNameCalendarRowCell = 'calendar-row-cell';
  static ClassNameWeekStartsOnSunday = 'week-starts-on-sunday';

  static _recycleBin = [];

  /**
   * @return {!Array}
   * @override
   */
  _recycleBin() {
    return CalendarRowCell._recycleBin;
  }

  /**
   * @param {!number} row
   * @param {!CalendarTableView} calendarTableView
   */
  reset(row, calendarTableView) {
    this.row = row;
    this.calendarTableView = calendarTableView;
    if (this.calendarTableView.hasWeekNumberColumn) {
      const middleDay = this.calendarTableView.dayAtColumnAndRow(3, row);
      const week = Week.createFromDay(middleDay);
      this.weekNumberCell =
          this.calendarTableView.prepareNewWeekNumberCell(week);
      this.weekNumberCell.attachTo(this);
    }
    let day = calendarTableView.dayAtColumnAndRow(0, row);
    for (let i = 0; i < DaysPerWeek; ++i) {
      const dayCell = this.calendarTableView.prepareNewDayCell(day);
      dayCell.attachTo(this);
      this._dayCells.push(dayCell);
      day = day.next();
    }
    this.show();
  }

  /**
   * @override
   */
  throwAway() {
    super.throwAway();
    if (this.weekNumberCell)
      this.calendarTableView.throwAwayWeekNumberCell(this.weekNumberCell);
    this._dayCells.forEach(
        this.calendarTableView.throwAwayDayCell, this.calendarTableView);
    this._dayCells.length = 0;
  }
}

// ----------------------------------------------------------------

class CalendarTableView extends ListView {
  /**
   * @param {!CalendarPicker} calendarPicker
   */
  constructor(calendarPicker) {
    super();
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
    const headerView = new CalendarTableHeaderView(this.hasWeekNumberColumn);
    headerView.attachTo(this, this.scrollView);

    /**
     * @type {!ClearButton}
     * @const
     */
    const clearButton = new ClearButton();
    clearButton.attachTo(this);
    clearButton.on(
        ClearButton.EventTypeButtonClick, this.onClearButtonClick.bind(this));
    clearButton.element.textContent = global.params.clearLabel;
    clearButton.element.setAttribute('aria-label', global.params.clearLabel);

    /**
     * @type {!CalendarNavigationButton}
     * @const
     */
    const todayButton = new CalendarNavigationButton();
    todayButton.attachTo(this);
    todayButton.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onTodayButtonClick.bind(this));
    todayButton.element.textContent = global.params.todayLabel;
    todayButton.element.classList.add(
        CalendarHeaderView.GetClassNameTodayButton());
    if (this.calendarPicker.type === 'week') {
      todayButton.setDisabled(isWeekOutsideOfRange(
          Week.createFromToday(), this.calendarPicker.config.minimum,
          this.calendarPicker.config.maximum));
    } else {
      todayButton.setDisabled(isDayOutsideOfRange(
          Day.createFromToday(), this.calendarPicker.config.minimum,
          this.calendarPicker.config.maximum));
    }
    todayButton.element.setAttribute('aria-label', global.params.todayLabel);

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

    // You shouldn't be able to use the mouse wheel to scroll.
    this.scrollView.element.removeEventListener(
        'mousewheel', this.scrollView.onMouseWheel, false);
    // You shouldn't be able to do gesture scroll.
    this.scrollView.element.removeEventListener(
        'touchstart', this.scrollView.onTouchStart, false);
  }

  static _BorderWidth = 0;
  static GetBorderWidth() {
    return CalendarTableView._BorderWidth;
  }
  static _TodayButtonHeight = 28;
  static GetTodayButtonHeight() {
    return CalendarTableView._TodayButtonHeight;
  }
  static ClassNameCalendarTableView = 'calendar-table-view';

  /**
   * @param {!number} scrollOffset
   * @return {!number}
   */
  rowAtScrollOffset(scrollOffset) {
    return Math.floor(scrollOffset / CalendarRowCell.GetHeight());
  }

  /**
   * @param {!number} row
   * @return {!number}
   */
  scrollOffsetForRow(row) {
    return row * CalendarRowCell.GetHeight();
  }

  /**
   * @param {?Event} event
   */
  onClick(event) {
    if (this.hasWeekNumberColumn) {
      const weekNumberCellElement = enclosingNodeOrSelfWithClass(
          event.target, WeekNumberCell.ClassNameWeekNumberCell);
      if (weekNumberCellElement) {
        const weekNumberCell = weekNumberCellElement.$view;
        this.calendarPicker.selectRangeContainingDay(
            weekNumberCell.week.firstDay());
        return;
      }
    }
    const dayCellElement =
        enclosingNodeOrSelfWithClass(event.target, DayCell.ClassNameDayCell);
    if (!dayCellElement)
      return;
    const dayCell = dayCellElement.$view;
    this.calendarPicker.selectRangeContainingDay(dayCell.day);
  }

  onClearButtonClick() {
    window.pagePopupController.setValueAndClosePopup(0, '');
  }

  onTodayButtonClick(sender) {
    this.calendarPicker.selectRangeContainingDay(Day.createFromToday());
  }

  /**
   * @param {!number} row
   * @return {!CalendarRowCell}
   */
  prepareNewCell(row) {
    const cell = CalendarRowCell._recycleBin.pop() || new CalendarRowCell();
    cell.reset(row, this);
    return cell;
  }

  /**
   * @return {!number} Height in pixels.
   */
  height() {
    return this.scrollView.height() + CalendarTableHeaderView.GetHeight() +
        CalendarTableView.GetBorderWidth() * 2 +
        CalendarTableView.GetTodayButtonHeight();
  }

  /**
   * @param {!number} height Height in pixels.
   */
  setHeight(height) {
    this.scrollView.setHeight(
        height - CalendarTableHeaderView.GetHeight() -
        CalendarTableView.GetBorderWidth() * 2 -
        CalendarTableView.GetTodayButtonHeight());
    this.element.style.height = height + 'px';
  }

  /**
   * @param {!Month} month
   * @param {!boolean} animate
   */
  scrollToMonth(month, animate) {
    const rowForFirstDayInMonth = this.columnAndRowForDay(month.firstDay()).row;
    this.scrollView.scrollTo(
        this.scrollOffsetForRow(rowForFirstDayInMonth), animate);
  }

  /**
   * @param {!number} column
   * @param {!number} row
   * @return {!Day}
   */
  dayAtColumnAndRow(column, row) {
    const daysSinceMinimum = row * DaysPerWeek + column +
        global.params.weekStartDay - CalendarTableView._MinimumDayWeekDay;
    return Day.createFromValue(
        daysSinceMinimum * MillisecondsPerDay +
        CalendarTableView._MinimumDayValue);
  }

  static _MinimumDayValue = Day.Minimum.valueOf();
  static _MinimumDayWeekDay = Day.Minimum.weekDay();

  /**
   * @param {!Day} day
   * @return {!Object} Object with properties column and row.
   */
  columnAndRowForDay(day) {
    const daysSinceMinimum =
        (day.valueOf() - CalendarTableView._MinimumDayValue) /
        MillisecondsPerDay;
    const offset = daysSinceMinimum + CalendarTableView._MinimumDayWeekDay -
        global.params.weekStartDay;
    const row = Math.floor(offset / DaysPerWeek);
    const column = offset - row * DaysPerWeek;
    return {column: column, row: row};
  }

  updateCells() {
    super.updateCells();

    const selection = this.calendarPicker.selection();
    let firstDayInSelection;
    let lastDayInSelection;
    if (selection) {
      firstDayInSelection = selection.firstDay().valueOf();
      lastDayInSelection = selection.lastDay().valueOf();
    } else {
      firstDayInSelection = Infinity;
      lastDayInSelection = Infinity;
    }
    const highlight = this.calendarPicker.highlight();
    let firstDayInHighlight;
    let lastDayInHighlight;
    if (highlight) {
      firstDayInHighlight = highlight.firstDay().valueOf();
      lastDayInHighlight = highlight.lastDay().valueOf();
    } else {
      firstDayInHighlight = Infinity;
      lastDayInHighlight = Infinity;
    }
    const currentMonth = this.calendarPicker.currentMonth();
    const firstDayInCurrentMonth = currentMonth.firstDay().valueOf();
    const lastDayInCurrentMonth = currentMonth.lastDay().valueOf();
    let activeCell = null;
    for (let dayString in this._dayCells) {
      const dayCell = this._dayCells[dayString];
      const day = dayCell.day;
      dayCell.setIsToday(Day.createFromToday().equals(day));
      const isSelected =
          (day >= firstDayInSelection && day <= lastDayInSelection);
      dayCell.setSelected(isSelected);
      if (isSelected && firstDayInSelection == lastDayInSelection) {
        activeCell = dayCell;
      }
      dayCell.setIsInCurrentMonth(
          day >= firstDayInCurrentMonth && day <= lastDayInCurrentMonth);
      dayCell.setDisabled(!this.calendarPicker.isValidDay(day));
    }
    if (this.hasWeekNumberColumn) {
      for (let weekString in this._weekNumberCells) {
        const weekNumberCell = this._weekNumberCells[weekString];
        const week = weekNumberCell.week;
        const isSelected = (selection && selection.equals(week));
        weekNumberCell.setSelected(isSelected);
        if (isSelected) {
          activeCell = weekNumberCell;
        }
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
  }

  /**
   * @param {!Day} day
   * @return {!DayCell}
   */
  prepareNewDayCell(day) {
    const dayCell = DayCell.recycleOrCreate();
    dayCell.reset(day);
    if (this.calendarPicker.type == 'month')
      dayCell.element.setAttribute(
          'aria-label', Month.createFromDay(day).toLocaleString());
    this._dayCells[dayCell.day.toString()] = dayCell;
    return dayCell;
  }

  /**
   * @param {!Week} week
   * @return {!WeekNumberCell}
   */
  prepareNewWeekNumberCell(week) {
    const weekNumberCell = WeekNumberCell.recycleOrCreate();
    weekNumberCell.reset(week);
    this._weekNumberCells[weekNumberCell.week.toString()] = weekNumberCell;
    return weekNumberCell;
  }

  /**
   * @param {!DayCell} dayCell
   */
  throwAwayDayCell(dayCell) {
    delete this._dayCells[dayCell.day.toString()];
    dayCell.throwAway();
  }

  /**
   * @param {!WeekNumberCell} weekNumberCell
   */
  throwAwayWeekNumberCell(weekNumberCell) {
    delete this._weekNumberCells[weekNumberCell.week.toString()];
    weekNumberCell.throwAway();
  }
}

// ----------------------------------------------------------------

// clang-format off
class CalendarPicker extends dateRangeManagerMixin(View) {
  // clang-format on
  /**
   * @param {!Object} config
   */
  constructor(type, config) {
    super(createElement('div', CalendarPicker.ClassNameCalendarPicker));
    this.element.classList.add(CalendarPicker.ClassNamePreparing);
    if (global.params.isBorderTransparent) {
      this.element.style.borderColor = 'transparent';
    }
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

    this._setValidDateConfig(config);

    if (this.type === 'week') {
      this.element.classList.add(CalendarPicker.ClassNameWeekPicker);
    }

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
        MonthPopupButton.EventTypeButtonClick,
        this.onMonthPopupButtonClick.bind(this));
    /**
     * @type {!MonthPopupView}
     * @const
     */
    this.monthPopupView =
        new MonthPopupView(this.minimumMonth, this.maximumMonth);
    this.monthPopupView.yearListView.on(
        YearListView.EventTypeYearListViewDidSelectMonth,
        this.onYearListViewDidSelectMonth.bind(this));
    this.monthPopupView.yearListView.on(
        YearListView.EventTypeYearListViewDidHide,
        this.onYearListViewDidHide.bind(this));
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

    this.calendarTableView.element.addEventListener(
        'keydown', this.onCalendarTableKeyDown.bind(this));

    document.body.addEventListener('click', this.onBodyClick.bind(this));
    this._onBodyKeyDownBound = this.onBodyKeyDown.bind(this);
    document.body.addEventListener('keydown', this._onBodyKeyDownBound);

    this._onWindowResizeBound = this.onWindowResize.bind(this);
    window.addEventListener('resize', this._onWindowResizeBound);

    /**
     * @type {!number}
     * @protected
     */
    this._height = -1;

    this._hadValidValueWhenOpened = false;

    const initialSelection = parseDateString(config.currentValue);
    if (initialSelection) {
      this.setCurrentMonth(
          Month.createFromDay(initialSelection.middleDay()),
          CalendarPicker.NavigationBehavior.None);

      this._hadValidValueWhenOpened = this.isValid(initialSelection);
      this.setSelection(this.getNearestValidRange(
          initialSelection, /*lookForwardFirst*/ true));
    } else {
      this.setCurrentMonth(
          Month.createFromToday(), CalendarPicker.NavigationBehavior.None);

      // Ensure that the next date closest to today is selected to start with
      // so that the user can simply submit the popup to choose it.
      this.setSelection(this.getValidRangeNearestToDay(
          this._dateTypeConstructor.createFromToday(),
          /*lookForwardFirst*/ true));
    }

    /**
     * @type {?DateType}
     * @protected
     */
    this._initialSelection = this._selection;
  }

  static Padding = 10;
  static BorderWidth = 1;
  static ClassNameCalendarPicker = 'calendar-picker';
  static ClassNameWeekPicker = 'week-picker';
  static ClassNamePreparing = 'preparing';
  static EventTypeCurrentMonthChanged = 'currentMonthChanged';
  static commitDelayMs = 100;
  static VisibleRows = 6;

  /**
   * @param {!Event} event
   */
  onWindowResize(event) {
    this.element.classList.remove(CalendarPicker.ClassNamePreparing);
    window.removeEventListener('resize', this._onWindowResizeBound);
  }

  resetToInitialValue() {
    this.setSelection(this._initialSelection);
  }

  /**
   * @param {!YearListView} sender
   */
  onYearListViewDidHide(sender) {
    this.monthPopupView.hide();
    this.calendarHeaderView.setDisabled(false);
    this.calendarTableView.element.style.visibility = 'visible';
    this.calendarTableView.element.focus();
  }

  /**
   * @param {!YearListView} sender
   * @param {!Month} month
   */
  onYearListViewDidSelectMonth(sender, month) {
    this.setCurrentMonth(month, CalendarPicker.NavigationBehavior.None);

    this.ensureSelectionIsWithinCurrentMonth();
    this.onYearListViewDidHide();
  }

  /**
   * @param {!View|Node} parent
   * @param {?View|Node=} before
   * @override
   */
  attachTo(parent, before) {
    View.prototype.attachTo.call(this, parent, before);
    this.calendarTableView.element.focus();
  }

  cleanup() {
    window.removeEventListener('resize', this._onWindowResizeBound);
    this.calendarTableView.element.removeEventListener(
        'keydown', this._onBodyKeyDownBound);
    // Month popup view might be attached to document.body.
    this.monthPopupView.hide();
  }

  /**
   * @param {?MonthPopupButton} sender
   */
  onMonthPopupButtonClick(sender) {
    const clientRect = this.calendarTableView.element.getBoundingClientRect();
    const calendarTableRect = new Rectangle(
        clientRect.left + document.body.scrollLeft,
        clientRect.top + document.body.scrollTop, clientRect.width,
        clientRect.height);
    this.monthPopupView.show(this.currentMonth(), calendarTableRect);
    this.calendarHeaderView.setDisabled(true);
    this.calendarTableView.element.style.visibility = 'hidden';
  }

  /**
   * @return {!Month}
   */
  currentMonth() {
    return this._currentMonth;
  }

  /**
   * @enum {number}
   */
  static NavigationBehavior = {None: 0, WithAnimation: 1};

  /**
   * @param {!Month} month
   * @param {!CalendarPicker.NavigationBehavior} animate
   */
  setCurrentMonth(month, behavior) {
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
  }

  adjustHeight() {
    const numberOfRows = CalendarPicker.VisibleRows;
    const calendarTableViewHeight = CalendarTableHeaderView.GetHeight() +
        numberOfRows * DayCell.GetHeight() +
        CalendarTableView.GetBorderWidth() * 2 +
        CalendarTableView.GetTodayButtonHeight();
    const height = calendarTableViewHeight + CalendarHeaderView.Height +
        CalendarHeaderView.BottomMargin + CalendarPicker.Padding * 2 +
        CalendarPicker.BorderWidth * 2;
    this.setHeight(height);
  }

  selection() {
    return this._selection;
  }

  highlight() {
    return this._highlight;
  }

  /**
   * @return {!Day}
   */
  firstVisibleDay() {
    const firstVisibleRow =
        this.calendarTableView
            .columnAndRowForDay(this.currentMonth().firstDay())
            .row;
    let firstVisibleDay =
        this.calendarTableView.dayAtColumnAndRow(0, firstVisibleRow);
    if (!firstVisibleDay)
      firstVisibleDay = Day.Minimum;
    return firstVisibleDay;
  }

  /**
   * @return {!Day}
   */
  lastVisibleDay() {
    let lastVisibleRow =
        this.calendarTableView.columnAndRowForDay(this.currentMonth().lastDay())
            .row;
    lastVisibleRow = this.calendarTableView
                         .columnAndRowForDay(this.currentMonth().firstDay())
                         .row +
        CalendarPicker.VisibleRows - 1;
    let lastVisibleDay = this.calendarTableView.dayAtColumnAndRow(
        DaysPerWeek - 1, lastVisibleRow);
    if (!lastVisibleDay)
      lastVisibleDay = Day.Maximum;
    return lastVisibleDay;
  }

  /**
   * @param {?Day} day
   */
  selectRangeContainingDay(day) {
    const selection = day ? this._dateTypeConstructor.createFromDay(day) : null;
    this.setSelectionAndCommit(selection);
  }

  /**
   * @param {?Day} day
   */
  highlightRangeContainingDay(day) {
    const highlight = day ? this._dateTypeConstructor.createFromDay(day) : null;
    this._setHighlight(highlight);
  }

  /**
   * Select the specified date.
   * @param {?DateType} dayOrWeekOrMonth
   */
  setSelection(dayOrWeekOrMonth) {
    if (!this._selection && !dayOrWeekOrMonth)
      return;
    if (this._selection && this._selection.equals(dayOrWeekOrMonth))
      return;
    if (this._selection && !dayOrWeekOrMonth) {
      this._selection = null;
      return;
    }
    const firstDayInSelection = dayOrWeekOrMonth.firstDay();
    const lastDayInSelection = dayOrWeekOrMonth.lastDay();
    const candidateCurrentMonth = Month.createFromDay(firstDayInSelection);
    if (this.firstVisibleDay() > lastDayInSelection ||
        this.lastVisibleDay() < firstDayInSelection) {
      // Change current month if the selection is not visible at all.
      this.setCurrentMonth(
          candidateCurrentMonth,
          CalendarPicker.NavigationBehavior.WithAnimation);
    } else if (
        this.firstVisibleDay() < firstDayInSelection ||
        this.lastVisibleDay() > lastDayInSelection) {
      // If the selection is partly visible, only change the current month if
      // doing so will make the whole selection visible.
      const firstVisibleRow =
          this.calendarTableView
              .columnAndRowForDay(candidateCurrentMonth.firstDay())
              .row;
      const firstVisibleDay =
          this.calendarTableView.dayAtColumnAndRow(0, firstVisibleRow);
      const lastVisibleRow =
          this.calendarTableView
              .columnAndRowForDay(candidateCurrentMonth.lastDay())
              .row;
      const lastVisibleDay = this.calendarTableView.dayAtColumnAndRow(
          DaysPerWeek - 1, lastVisibleRow);
      if (firstDayInSelection >= firstVisibleDay &&
          lastDayInSelection <= lastVisibleDay)
        this.setCurrentMonth(
            candidateCurrentMonth,
            CalendarPicker.NavigationBehavior.WithAnimation);
    }
    if (!this.isValid(dayOrWeekOrMonth))
      return;
    this._selection = dayOrWeekOrMonth;
    this.monthPopupView.yearListView.setSelectedMonth(
        Month.createFromDay(dayOrWeekOrMonth.middleDay()));
    this.calendarTableView.setNeedsUpdateCells(true);
  }

  getSelectedValue() {
    return this._selection.toString();
  }

  /**
   * Select the specified date, commit it, and close the popup.
   * @param {?DateType} dayOrWeekOrMonth
   */
  setSelectionAndCommit(dayOrWeekOrMonth) {
    this.setSelection(dayOrWeekOrMonth);
    // Redraw the widget immediately, and wait for some time to give feedback to
    // a user.
    this.element.offsetLeft;

    // CalendarPicker doesn't handle the submission when used for
    // datetime-local.
    if (this.type == 'datetime-local')
      return;

    const value = this._selection.toString();
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
  }

  /**
   * @param {?DateType} dayOrWeekOrMonth
   */
  _setHighlight(dayOrWeekOrMonth) {
    if (!this._highlight && !dayOrWeekOrMonth)
      return;
    if (!dayOrWeekOrMonth && !this._highlight)
      return;
    if (this._highlight && this._highlight.equals(dayOrWeekOrMonth))
      return;
    this._highlight = dayOrWeekOrMonth;
    this.calendarTableView.setNeedsUpdateCells(true);
  }

  /**
   * @param {!Day} day
   * @return {!boolean}
   */
  isValidDay(day) {
    return this.isValid(this._dateTypeConstructor.createFromDay(day));
  }

  /**
   * If the selection is not inside the month currently shown in the control,
   * adjust the selection so that it is within the current month.
   * The new selection value is determined in the following manner:
   * 1) If the old selection is on the Nth day of the month, try to place it
   * on the Nth day of the new month.
   * 2) If the Nth day of the new month is not valid, choose the closest
   * valid date that is within the new month.
   * 3) If the next and previous valid date are equidistant and both within
   * the new month, arbitrarily choose the older date.
   */
  ensureSelectionIsWithinCurrentMonth() {
    if (!this._selection)
      return;
    if (this._selection.isFullyContainedInMonth(this.currentMonth()))
      return;

    let newSelection = null;
    const currentRangeInNewMonth =
        this._selection.thisRangeInMonth(this.currentMonth());

    if (this.isValid(currentRangeInNewMonth)) {
      newSelection = currentRangeInNewMonth;
    } else {
      const validRangeLookingBackward =
          this.getNearestValidRangeLookingBackward(currentRangeInNewMonth);
      const validRangeLookingForward =
          this.getNearestValidRangeLookingForward(currentRangeInNewMonth);
      if (validRangeLookingBackward && validRangeLookingForward) {
        const newMonthIsForwardOfSelection =
            (currentRangeInNewMonth.firstDay() > this._selection.firstDay());
        const [validRangeInDirectionOfAdvancement, validRangeAgainstDirectionOfAdvancement] =
            newMonthIsForwardOfSelection ?
            [validRangeLookingForward, validRangeLookingBackward] :
            [validRangeLookingBackward, validRangeLookingForward];

        if (!validRangeAgainstDirectionOfAdvancement.overlapsMonth(
                this.currentMonth())) {
          // If the range going against our direction of movement is not
          // entirely within the new month, go with the range in the
          // other direction to ensure we that we don't backtrack.
          newSelection = validRangeInDirectionOfAdvancement;
        } else if (!validRangeInDirectionOfAdvancement.overlapsMonth(
                       this.currentMonth())) {
          newSelection = validRangeAgainstDirectionOfAdvancement;
        } else {
          // If both of the ranges are in the new month, select the closest one
          // to the target date in the new month.
          const diffFromForwardRange = Math.abs(
              currentRangeInNewMonth.valueOf() -
              validRangeLookingForward.valueOf());
          const diffFromBackwardRange = Math.abs(
              currentRangeInNewMonth.valueOf() -
              validRangeLookingBackward.valueOf());
          if (diffFromForwardRange < diffFromBackwardRange) {
            newSelection = validRangeLookingForward;
          } else {  // In a tie, arbitrarily choose older date
            newSelection = validRangeLookingBackward;
          }
        }
      } else if (!validRangeLookingForward) {
        newSelection = validRangeLookingBackward;
      } else {  // !validRangeLookingBackward
        newSelection = validRangeLookingForward;
      }  // No additional clause because they can't both be null; we have a
         // selection so there's at least one valid date.
    }

    if (newSelection) {
      this.setSelection(newSelection);
    }
  }

  /**
   * @param {!DateType} dateRange
   * @return {!boolean} Returns true if the highlight was changed.
   */
  _moveHighlight(dateRange) {
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
  }

  /**
   * @param {?Event} event
   */
  onCalendarTableKeyDown(event) {
    const key = event.key;
    if (!event.target.matches('.today-button, .clear-button') &&
        this._selection) {
      switch (key) {
        case 'PageUp':
          const previousMonth = this.currentMonth().previous();
          if (previousMonth && previousMonth >= this.config.minimumValue) {
            this.setCurrentMonth(
                previousMonth, CalendarPicker.NavigationBehavior.WithAnimation);
            this.ensureSelectionIsWithinCurrentMonth();
          }
          break;
        case 'PageDown':
          const nextMonth = this.currentMonth().next();
          if (nextMonth && nextMonth >= this.config.minimumValue) {
            this.setCurrentMonth(
                nextMonth, CalendarPicker.NavigationBehavior.WithAnimation);
            this.ensureSelectionIsWithinCurrentMonth();
          }
          break;
        case 'ArrowUp':
        case 'ArrowDown':
        case 'ArrowLeft':
        case 'ArrowRight':
          const upOrDownArrowStepSize =
              this.type === 'date' || this.type === 'datetime-local' ?
              DaysPerWeek :
              1;
          if (global.params.isLocaleRTL ? key == 'ArrowRight' :
                                          key == 'ArrowLeft') {
            const newSelection = this.getNearestValidRangeLookingBackward(
                this._selection.previous());
            if (newSelection) {
              this.setSelection(newSelection);
            }
          } else if (key == 'ArrowUp') {
            const newSelection = this.getNearestValidRangeLookingBackward(
                this._selection.previous(upOrDownArrowStepSize));
            if (newSelection) {
              this.setSelection(newSelection);
            }
          } else if (
              global.params.isLocaleRTL ? key == 'ArrowLeft' :
                                          key == 'ArrowRight') {
            const newSelection =
                this.getNearestValidRangeLookingForward(this._selection.next());
            if (newSelection) {
              this.setSelection(newSelection);
            }
          } else if (key == 'ArrowDown') {
            const newSelection = this.getNearestValidRangeLookingForward(
                this._selection.next(upOrDownArrowStepSize));
            if (newSelection) {
              this.setSelection(newSelection);
            }
          }
          break;
        case 'Home': {
          const newSelection = this.getNearestValidRangeLookingBackward(
              this._selection.nextHome());
          if (newSelection) {
            this.setSelection(newSelection);
          }
          break;
        }
        case 'End': {
          const newSelection = this.getNearestValidRangeLookingForward(
              this._selection.nextEnd());
          if (newSelection) {
            this.setSelection(newSelection);
          }
          break;
        }
      }
    }
    // else if there is no selection it must be the case that there are no
    // valid values (because min >= max).  Otherwise we would have set the
    // selection during initialization.  In this case there's nothing to do.
  }

  /**
   * @return {!number} Width in pixels.
   */
  width() {
    return this.calendarTableView.width() +
        (CalendarTableView.GetBorderWidth() + CalendarPicker.BorderWidth +
         CalendarPicker.Padding) *
        2;
  }

  /**
   * @return {!number} Height in pixels.
   */
  height() {
    return this._height;
  }

  /**
   * @param {!number} height Height in pixels.
   */
  setHeight(height) {
    if (this._height === height)
      return;
    this._height = height;
    resizeWindow(this.width(), this._height);
    this.calendarTableView.setHeight(
        this._height - CalendarHeaderView.Height -
        CalendarHeaderView.BottomMargin - CalendarPicker.Padding * 2 -
        CalendarPicker.BorderWidth * 2);
  }

  /**
   * @param {?Event} event
   */
  onBodyClick(event) {
    if (this.type !== 'datetime-local') {
      if (event.target.matches(
              '.calendar-navigation-button, ' +
              '.navigation-button-icon, .month-button')) {
        window.pagePopupController.setValue(this.getSelectedValue());
      }
    }
  }

  /**
   * @param {?Event} event
   */
  onBodyKeyDown(event) {
    const key = event.key;
    switch (key) {
      case 'Escape':
        // The datetime-local control handles submission/cancellation at
        // the top level, so if we're in a datetime-local let event bubble
        // up instead of handling it here.
        if (this.type !== 'datetime-local') {
          if (!this._selection ||
              (this._selection.equals(this._initialSelection))) {
            window.pagePopupController.closePopup();
          } else {
            this.resetToInitialValue();
            window.pagePopupController.setValue(
                this._hadValidValueWhenOpened ?
                    this._initialSelection.toString() :
                    '');
          }
        }
        break;
      case 'ArrowUp':
      case 'ArrowDown':
      case 'ArrowLeft':
      case 'ArrowRight':
      case 'PageUp':
      case 'PageDown':
      case 'Home':
      case 'End':
        if (this.type !== 'datetime-local' &&
            event.target.matches('.calendar-table-view') && this._selection) {
          window.pagePopupController.setValue(this.getSelectedValue());
        }
        break;
      case 'Enter':
        // Submit the popup for an Enter keypress except when the user is
        // hitting Enter to activate the month switcher button, Clear button,
        // Today button, or previous/next month arrows.
        if (this.type !== 'datetime-local') {
          if (!event.target.matches(
                  '.calendar-navigation-button, .clear-button, ' +
                  '.month-popup-button, .year-list-view')) {
            if (this._selection) {
              window.pagePopupController.setValueAndClosePopup(
                  0, this.getSelectedValue());
            } else {
              // If there is no selection it must be the case that there are no
              // valid values (because min >= max).  There's nothing useful to
              // do with the popup in this case so just close on Enter.
              window.pagePopupController.closePopup();
            }
          } else if (event.target.matches(
                         '.calendar-navigation-button, .year-list-view')) {
            // Navigating with the previous/next arrows may change selection,
            // so push this change to the in-page control but don't
            // close the popup.
            window.pagePopupController.setValue(this.getSelectedValue());
          }
        }
        break;
    }
  }
}

// ----------------------------------------------------------------

if (window.dialogArguments) {
  initialize(dialogArguments);
} else {
  window.addEventListener('message', handleMessage, false);
}

// Necessary for some web tests.
window.AnimationManager = AnimationManager;
window.CalendarTableHeaderView = CalendarTableHeaderView;
window.CalendarPicker = CalendarPicker;
window.Day = Day;
window.DayCell = DayCell;
window.Month = Month;
window.Week = Week;
window.WeekNumberCell = WeekNumberCell;
window.global = global;
