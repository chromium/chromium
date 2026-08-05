/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   ComboboxDatePicker.js
 */

'use strict';

class ComboboxDatePicker {
  constructor(cdp) {
    this.buttonLabel = 'Date';
    this.monthLabels = [
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

    this.messageCursorKeys = 'Cursor keys can navigate dates';
    this.lastMessage = '';

    this.comboboxNode = cdp.querySelector('input[type="text"]');
    this.buttonNode = cdp.querySelector('.group button');
    this.dialogNode = cdp.querySelector('[role="dialog"]');
    this.messageNode = this.dialogNode.querySelector('.dialog-message');

    this.monthYearNode = this.dialogNode.querySelector('.month-year');

    this.prevYearNode = this.dialogNode.querySelector('.prev-year');
    this.prevMonthNode = this.dialogNode.querySelector('.prev-month');
    this.nextMonthNode = this.dialogNode.querySelector('.next-month');
    this.nextYearNode = this.dialogNode.querySelector('.next-year');

    this.okButtonNode = this.dialogNode.querySelector('button[value="ok"]');
    this.cancelButtonNode = this.dialogNode.querySelector(
      'button[value="cancel"]'
    );

    this.tbodyNode = this.dialogNode.querySelector('table.dates tbody');

    this.lastRowNode = null;
    this.lastDate = -1;

    this.days = [];

    this.focusDay = new Date();
    this.selectedDay = new Date(0, 0, 1);

    this.isMouseDownOnBackground = false;

    this.comboboxNode.addEventListener(
      'keydown',
      this.onComboboxKeyDown.bind(this)
    );
    this.comboboxNode.addEventListener(
      'click',
      this.onComboboxClick.bind(this)
    );
    this.comboboxNode.addEventListener(
      'focus',
      this.onComboboxFocus.bind(this)
    );
    this.comboboxNode.addEventListener('blur', this.onComboboxBlur.bind(this));

    this.buttonNode.addEventListener(
      'keydown',
      this.onButtonKeyDown.bind(this)
    );
    this.buttonNode.addEventListener('click', this.onButtonClick.bind(this));

    this.okButtonNode.addEventListener('click', this.onOkButton.bind(this));
    this.okButtonNode.addEventListener('keydown', this.onOkButton.bind(this));

    this.cancelButtonNode.addEventListener(
      'click',
      this.onCancelButton.bind(this)
    );
    this.cancelButtonNode.addEventListener(
      'keydown',
      this.onCancelButton.bind(this)
    );

    this.prevMonthNode.addEventListener(
      'click',
      this.onPreviousMonthButton.bind(this)
    );
    this.nextMonthNode.addEventListener(
      'click',
      this.onNextMonthButton.bind(this)
    );
    this.prevYearNode.addEventListener(
      'click',
      this.onPreviousYearButton.bind(this)
    );
    this.nextYearNode.addEventListener(
      'click',
      this.onNextYearButton.bind(this)
    );

    this.prevMonthNode.addEventListener(
      'keydown',
      this.onPreviousMonthButton.bind(this)
    );
    this.nextMonthNode.addEventListener(
      'keydown',
      this.onNextMonthButton.bind(this)
    );
    this.prevYearNode.addEventListener(
      'keydown',
      this.onPreviousYearButton.bind(this)
    );
    this.nextYearNode.addEventListener(
      'keydown',
      this.onNextYearButton.bind(this)
    );

    document.body.addEventListener(
      'mouseup',
      this.onBackgroundMouseUp.bind(this),
      true
    );

    // Create Grid of Dates

    this.tbodyNode.innerHTML = '';
    for (var i = 0; i < 6; i++) {
      var row = this.tbodyNode.insertRow(i);
      this.lastRowNode = row;
      for (var j = 0; j < 7; j++) {
        var cell = document.createElement('td');

        cell.setAttribute('tabindex', '-1');
        cell.addEventListener('click', this.onDayClick.bind(this));
        cell.addEventListener('keydown', this.onDayKeyDown.bind(this));
        cell.addEventListener('focus', this.onDayFocus.bind(this));

        cell.textContent = '-1';

        row.appendChild(cell);
        this.days.push(cell);
      }
    }

    this.updateGrid();
    this.close(false);
  }

  isSameDay(day1, day2) {
    return (
      day1.getFullYear() == day2.getFullYear() &&
      day1.getMonth() == day2.getMonth() &&
      day1.getDate() == day2.getDate()
    );
  }

  isNotSameMonth(day1, day2) {
    return (
      day1.getFullYear() != day2.getFullYear() ||
      day1.getMonth() != day2.getMonth()
    );
  }

  updateGrid() {
    var i, flag;
    var fd = this.focusDay;

    this.monthYearNode.textContent =
      this.monthLabels[fd.getMonth()] + ' ' + fd.getFullYear();

    var firstDayOfMonth = new Date(fd.getFullYear(), fd.getMonth(), 1);
    var dayOfWeek = firstDayOfMonth.getDay();

    firstDayOfMonth.setDate(firstDayOfMonth.getDate() - dayOfWeek);

    var d = new Date(firstDayOfMonth);

    for (i = 0; i < this.days.length; i++) {
      flag = d.getMonth() != fd.getMonth();
      this.updateDate(
        this.days[i],
        flag,
        d,
        this.isSameDay(d, this.selectedDay)
      );
      d.setDate(d.getDate() + 1);

      // Hide last row if all disabled dates
      if (i === 35) {
        if (flag) {
          this.lastRowNode.style.visibility = 'hidden';
        } else {
          this.lastRowNode.style.visibility = 'visible';
        }
      }
    }
  }

  setFocusDay(flag) {
    if (typeof flag !== 'boolean') {
      flag = true;
    }

    var fd = this.focusDay;
    var getDayFromDataDateAttribute = this.getDayFromDataDateAttribute;

    function checkDay(domNode) {
      var d = getDayFromDataDateAttribute(domNode);

      domNode.setAttribute('tabindex', '-1');
      if (this.isSameDay(d, fd)) {
        domNode.setAttribute('tabindex', '0');
        if (flag) {
          domNode.focus();
        }
      }
    }

    this.days.forEach(checkDay.bind(this));
  }

  open() {
    this.dialogNode.style.display = 'block';
    this.dialogNode.style.zIndex = 2;

    this.comboboxNode.setAttribute('aria-expanded', 'true');
    this.buttonNode.classList.add('open');
    this.getDateFromCombobox();
    this.updateGrid();
    this.lastDate = this.focusDay.getDate();
  }

  isOpen() {
    return window.getComputedStyle(this.dialogNode).display !== 'none';
  }

  close(flag) {
    if (typeof flag !== 'boolean') {
      // Default is to move focus to combobox
      flag = true;
    }

    this.setMessage('');
    this.dialogNode.style.display = 'none';
    this.comboboxNode.setAttribute('aria-expanded', 'false');
    this.buttonNode.classList.remove('open');

    if (flag) {
      this.comboboxNode.focus();
    }
  }

  onOkButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Tab':
            if (!event.shiftKey) {
              this.prevYearNode.focus();
              flag = true;
            }
            break;

          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          default:
            break;
        }
        break;

      case 'click':
        this.setComboboxDate();
        this.close();
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onCancelButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          default:
            break;
        }
        break;

      case 'click':
        this.close();
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onNextYearButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          case 'Enter':
            this.moveToNextYear();
            this.setFocusDay(false);
            flag = true;
            break;
        }

        break;

      case 'click':
        this.moveToNextYear();
        this.setFocusDay(false);
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onPreviousYearButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Enter':
            this.moveToPreviousYear();
            this.setFocusDay(false);
            flag = true;
            break;

          case 'Tab':
            if (event.shiftKey) {
              this.okButtonNode.focus();
              flag = true;
            }
            break;

          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          default:
            break;
        }

        break;

      case 'click':
        this.moveToPreviousYear();
        this.setFocusDay(false);
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onNextMonthButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          case 'Enter':
            this.moveToNextMonth();
            this.setFocusDay(false);
            flag = true;
            break;
        }

        break;

      case 'click':
        this.moveToNextMonth();
        this.setFocusDay(false);
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onPreviousMonthButton(event) {
    var flag = false;

    switch (event.type) {
      case 'keydown':
        switch (event.key) {
          case 'Esc':
          case 'Escape':
            this.close();
            flag = true;
            break;

          case 'Enter':
            this.moveToPreviousMonth();
            this.setFocusDay(false);
            flag = true;
            break;
        }

        break;

      case 'click':
        this.moveToPreviousMonth();
        this.setFocusDay(false);
        flag = true;
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  moveFocusToDay(day) {
    var d = this.focusDay;

    this.focusDay = day;

    if (
      d.getMonth() != this.focusDay.getMonth() ||
      d.getYear() != this.focusDay.getYear()
    ) {
      this.updateGrid();
    }
    this.setFocusDay();
  }

  changeMonth(currentDate, numMonths) {
    const getDays = (year, month) => new Date(year, month, 0).getDate();

    const isPrev = numMonths < 0;
    const numYears = Math.trunc(Math.abs(numMonths) / 12);
    numMonths = Math.abs(numMonths) % 12;

    const newYear = isPrev
      ? currentDate.getFullYear() - numYears
      : currentDate.getFullYear() + numYears;

    const newMonth = isPrev
      ? currentDate.getMonth() - numMonths
      : currentDate.getMonth() + numMonths;

    const newDate = new Date(newYear, newMonth, 1);

    const daysInMonth = getDays(newDate.getFullYear(), newDate.getMonth() + 1);

    // If lastDat is not initialized set to current date
    this.lastDate = this.lastDate ? this.lastDate : currentDate.getDate();

    if (this.lastDate > daysInMonth) {
      newDate.setDate(daysInMonth);
    } else {
      newDate.setDate(this.lastDate);
    }

    return newDate;
  }

  moveToNextYear() {
    this.focusDay = this.changeMonth(this.focusDay, 12);
    this.updateGrid();
  }

  moveToPreviousYear() {
    this.focusDay = this.changeMonth(this.focusDay, -12);
    this.updateGrid();
  }

  moveToNextMonth() {
    this.focusDay = this.changeMonth(this.focusDay, 1);
    this.updateGrid();
  }

  moveToPreviousMonth() {
    this.focusDay = this.changeMonth(this.focusDay, -1);
    this.updateGrid();
  }

  moveFocusToNextDay() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() + 1);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToNextWeek() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() + 7);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToPreviousDay() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() - 1);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToPreviousWeek() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() - 7);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToFirstDayOfWeek() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() - d.getDay());
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToLastDayOfWeek() {
    var d = new Date(this.focusDay);
    d.setDate(d.getDate() + (6 - d.getDay()));
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  // Day methods

  isDayDisabled(domNode) {
    return domNode.classList.contains('disabled');
  }

  getDayFromDataDateAttribute(domNode) {
    var parts = domNode.getAttribute('data-date').split('-');
    return new Date(parts[0], parseInt(parts[1]) - 1, parts[2]);
  }

  updateDate(domNode, disable, day, selected) {
    var d = day.getDate().toString();
    if (day.getDate() <= 9) {
      d = '0' + d;
    }

    var m = day.getMonth() + 1;
    if (day.getMonth() < 9) {
      m = '0' + m;
    }

    domNode.setAttribute('tabindex', '-1');
    domNode.removeAttribute('aria-selected');
    domNode.setAttribute('data-date', day.getFullYear() + '-' + m + '-' + d);

    if (disable) {
      domNode.classList.add('disabled');
      domNode.textContent = '';
    } else {
      domNode.classList.remove('disabled');
      domNode.textContent = day.getDate();
      if (selected) {
        domNode.setAttribute('aria-selected', 'true');
        domNode.setAttribute('tabindex', '0');
      }
    }
  }

  updateSelected(domNode) {
    for (var i = 0; i < this.days.length; i++) {
      var day = this.days[i];
      if (day === domNode) {
        day.setAttribute('aria-selected', 'true');
      } else {
        day.removeAttribute('aria-selected');
      }
    }
  }

  onDayKeyDown(event) {
    var flag = false;

    switch (event.key) {
      case 'Esc':
      case 'Escape':
        this.close();
        break;

      case ' ':
        this.updateSelected(event.currentTarget);
        this.setComboboxDate(event.currentTarget);
        flag = true;
        break;

      case 'Enter':
        this.setComboboxDate(event.currentTarget);
        this.close();
        break;

      case 'Tab':
        this.cancelButtonNode.focus();
        if (event.shiftKey) {
          this.nextYearNode.focus();
        }
        this.setMessage('');
        flag = true;
        break;

      case 'Right':
      case 'ArrowRight':
        this.moveFocusToNextDay();
        flag = true;
        break;

      case 'Left':
      case 'ArrowLeft':
        this.moveFocusToPreviousDay();
        flag = true;
        break;

      case 'Down':
      case 'ArrowDown':
        this.moveFocusToNextWeek();
        flag = true;
        break;

      case 'Up':
      case 'ArrowUp':
        this.moveFocusToPreviousWeek();
        flag = true;
        break;

      case 'PageUp':
        if (event.shiftKey) {
          this.moveToPreviousYear();
        } else {
          this.moveToPreviousMonth();
        }
        this.setFocusDay();
        flag = true;
        break;

      case 'PageDown':
        if (event.shiftKey) {
          this.moveToNextYear();
        } else {
          this.moveToNextMonth();
        }
        this.setFocusDay();
        flag = true;
        break;

      case 'Home':
        this.moveFocusToFirstDayOfWeek();
        flag = true;
        break;

      case 'End':
        this.moveFocusToLastDayOfWeek();
        flag = true;
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onDayClick(event) {
    if (!this.isDayDisabled(event.currentTarget)) {
      this.setComboboxDate(event.currentTarget);
      this.close();
    }

    event.stopPropagation();
    event.preventDefault();
  }

  onDayFocus() {
    this.setMessage(this.messageCursorKeys);
  }

  // Combobox methods

  setComboboxDate(domNode) {
    var d = this.focusDay;

    if (domNode) {
      d = this.getDayFromDataDateAttribute(domNode);
    }

    this.comboboxNode.value =
      d.getMonth() + 1 + '/' + d.getDate() + '/' + d.getFullYear();
  }

  getDateFromCombobox() {
    var parts = this.comboboxNode.value.split('/');

    if (
      parts.length === 3 &&
      Number.isInteger(parseInt(parts[0])) &&
      Number.isInteger(parseInt(parts[1])) &&
      Number.isInteger(parseInt(parts[2]))
    ) {
      this.focusDay = new Date(
        parseInt(parts[2]),
        parseInt(parts[0]) - 1,
        parseInt(parts[1])
      );
      this.selectedDay = new Date(this.focusDay);
    } else {
      // If not a valid date (MM/DD/YY) initialize with today's date
      this.focusDay = new Date();
      this.selectedDay = new Date(0, 0, 1);
    }
  }

  setMessage(str) {
    function setMessageDelayed() {
      this.messageNode.textContent = str;
    }

    if (str !== this.lastMessage) {
      setTimeout(setMessageDelayed.bind(this), 200);
      this.lastMessage = str;
    }
  }

  onComboboxKeyDown(event) {
    var flag = false;

    if (event.ctrlKey || event.shiftKey) {
      return;
    }

    switch (event.key) {
      case 'Down':
      case 'ArrowDown':
        this.open();
        this.setFocusDay();
        flag = true;
        break;

      case 'Esc':
      case 'Escape':
        if (this.isOpen()) {
          this.close(false);
        } else {
          this.comboboxNode.value = '';
        }
        this.option = null;
        flag = true;
        break;

      case 'Tab':
        this.close(false);
        break;

      default:
        break;
    }

    if (flag) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  onComboboxClick(event) {
    if (this.isOpen()) {
      this.close(false);
    } else {
      this.open();
    }

    event.stopPropagation();
    event.preventDefault();
  }

  onComboboxFocus(event) {
    event.currentTarget.parentNode.classList.add('focus');
  }

  onComboboxBlur(event) {
    event.currentTarget.parentNode.classList.remove('focus');
  }

  onButtonKeyDown(event) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.open();
      this.setFocusDay();

      event.stopPropagation();
      event.preventDefault();
    }
  }

  onButtonClick(event) {
    if (this.isOpen()) {
      this.close();
    } else {
      this.open();
      this.setFocusDay();
    }

    event.stopPropagation();
    event.preventDefault();
  }

  onBackgroundMouseUp(event) {
    if (
      !this.comboboxNode.contains(event.target) &&
      !this.buttonNode.contains(event.target) &&
      !this.dialogNode.contains(event.target)
    ) {
      if (this.isOpen()) {
        this.close(false);
        event.stopPropagation();
        event.preventDefault();
      }
    }
  }
}

// Initialize menu button date picker

window.addEventListener('load', function () {
  var comboboxDatePickers = document.querySelectorAll('.combobox-datepicker');
  comboboxDatePickers.forEach(function (dp) {
    new ComboboxDatePicker(dp);
  });
});
