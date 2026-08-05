/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   datepicker-spinbuttons.js
 */

'use strict';

/* global SpinButtonDate */

var DatePickerSpinButtons = function (domNode) {
  this.domNode = domNode;
  this.monthNode = domNode.querySelector('.spinbutton.month');
  this.dayNode = domNode.querySelector('.spinbutton.day');
  this.yearNode = domNode.querySelector('.spinbutton.year');
  this.dateNode = domNode.querySelector('.date');

  this.valuesWeek = [
    'Sunday',
    'Monday',
    'Tuesday',
    'Wednesday',
    'Thursday',
    'Friday',
    'Saturday',
  ];
  this.valuesDay = [
    '',
    'first',
    'second',
    'third',
    'fourth',
    'fifth',
    'sixth',
    'seventh',
    'eighth',
    'ninth',
    'tenth',
    'eleventh',
    'twelfth',
    'thirteenth',
    'fourteenth',
    'fifteenth',
    'sixteenth',
    'seventeenth',
    'eighteenth',
    'nineteenth',
    'twentieth',
    'twenty first',
    'twenty second',
    'twenty third',
    'twenty fourth',
    'twenty fifth',
    'twenty sixth',
    'twenty seventh',
    'twenty eighth',
    'twenty ninth',
    'thirtieth',
    'thirty first',
  ];
  this.valuesMonth = [
    'January',
    'February',
    'March',
    'April',
    'May',
    'June',
    'July',
    'August',
    'September',
    'October',
    'November',
    'December',
  ];
};

// Initialize slider
DatePickerSpinButtons.prototype.init = function () {
  this.spinbuttonDay = new SpinButtonDate(
    this.dayNode,
    null,
    this.updateDay.bind(this)
  );
  this.spinbuttonDay.init();

  this.spinbuttonMonth = new SpinButtonDate(
    this.monthNode,
    this.valuesMonth,
    this.updateMonth.bind(this)
  );
  this.spinbuttonMonth.init();

  this.spinbuttonYear = new SpinButtonDate(
    this.yearNode,
    null,
    this.updateYear.bind(this)
  );
  this.spinbuttonYear.init();
  this.spinbuttonYear.noWrap();

  this.minYear = this.spinbuttonYear.getValueMin();
  this.maxYear = this.spinbuttonYear.getValueMax();

  this.currentDate = new Date();

  this.day = this.currentDate.getDate();
  this.month = this.currentDate.getMonth();
  this.year = this.currentDate.getFullYear();
  this.daysInMonth = this.getDaysInMonth(this.year, this.month);

  this.spinbuttonDay.setValue(this.day, false);
  this.spinbuttonMonth.setValue(this.month, false);
  this.spinbuttonYear.setValue(this.year, false);

  this.updateSpinButtons();
};

DatePickerSpinButtons.prototype.getDaysInMonth = function (year, month) {
  return new Date(year, month + 1, 0).getDate();
};

DatePickerSpinButtons.prototype.updateDay = function (day) {
  this.day = day;
  this.updateSpinButtons();
};

DatePickerSpinButtons.prototype.updateMonth = function (month) {
  this.month = month;
  this.updateSpinButtons();
};

DatePickerSpinButtons.prototype.updateYear = function (year) {
  this.year = year;
  this.updateSpinButtons();
};

DatePickerSpinButtons.prototype.updatePreviousDayMonthAndYear = function () {
  this.previousYear = this.year - 1;

  this.previousMonth = this.month ? this.month - 1 : 11;

  this.previousDay = this.day - 1;
  if (this.previousDay < 1) {
    this.previousDay = this.getDaysInMonth(this.year, this.month);
  }
};

DatePickerSpinButtons.prototype.updateNextDayMonthAndYear = function () {
  this.nextYear = this.year + 1;
  this.nextMonth = this.month >= 11 ? 0 : this.month + 1;

  this.nextDay = this.day + 1;
  var maxDay = this.getDaysInMonth(this.year, this.month);
  if (this.nextDay > maxDay) {
    this.nextDay = 1;
  }
};

DatePickerSpinButtons.prototype.updateSpinButtons = function () {
  this.daysInMonth = this.getDaysInMonth(this.year, this.month);
  this.spinbuttonDay.setValueMax(this.daysInMonth);
  if (this.day > this.daysInMonth) {
    this.spinbuttonDay.setValue(this.daysInMonth);
    return;
  }

  this.updatePreviousDayMonthAndYear();
  this.updateNextDayMonthAndYear();

  this.spinbuttonDay.setValueText(this.valuesDay[this.day]);

  this.spinbuttonDay.setPreviousValue(this.previousDay);
  this.spinbuttonMonth.setPreviousValue(this.previousMonth);
  this.spinbuttonYear.setPreviousValue(this.previousYear);

  this.spinbuttonDay.setNextValue(this.nextDay);
  this.spinbuttonMonth.setNextValue(this.nextMonth);
  this.spinbuttonYear.setNextValue(this.nextYear);

  this.currentDate = new Date(
    this.year + '-' + (this.month + 1) + '-' + this.day
  );

  this.dateNode.innerHTML =
    'current value is ' +
    this.valuesWeek[this.currentDate.getDay()] +
    ', ' +
    this.spinbuttonMonth.getValueText() +
    ' ' +
    this.spinbuttonDay.getValueText() +
    ', ' +
    this.spinbuttonYear.getValue();
};

// Initialize menu button date picker

window.addEventListener('load', function () {
  var spinButtons = document.querySelectorAll('.datepicker-spinbuttons');

  spinButtons.forEach(function (spinButton) {
    var datepicker = new DatePickerSpinButtons(spinButton);
    datepicker.init();
  });
});
