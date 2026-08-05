/*
 *   This content is licensed according to the W3C Software License at
 *   https://www.w3.org/Consortium/Legal/2015/copyright-software-and-document
 *
 *   File:   DatePickerDialog.js
 */

'use strict';

class DatePickerDialog {
  constructor(cdp) {
    this.buttonLabelChoose = 'Choose Date';
    this.buttonLabelChange = 'Change Date';
    this.dayLabels = [
      'Sunday',
      'Monday',
      'Tuesday',
      'Wednesday',
      'Thursday',
      'Friday',
      'Saturday',
    ];
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

    this.textboxNode = cdp.querySelector('input[type="text"');
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

    this.days = [];

    this.focusDay = new Date();
    this.selectedDay = new Date(0, 0, 1);

    this.lastDate = -1;

    this.isMouseDownOnBackground = false;

    this.textboxNode.addEventListener(
      'blur',
      this.setDateForButtonLabel.bind(this)
    );

    this.buttonNode.addEventListener(
      'keydown',
      this.handleButtonKeydown.bind(this)
    );
    this.buttonNode.addEventListener(
      'click',
      this.handleButtonClick.bind(this)
    );

    this.okButtonNode.addEventListener('click', this.handleOkButton.bind(this));
    this.okButtonNode.addEventListener(
      'keydown',
      this.handleOkButton.bind(this)
    );

    this.cancelButtonNode.addEventListener(
      'click',
      this.handleCancelButton.bind(this)
    );
    this.cancelButtonNode.addEventListener(
      'keydown',
      this.handleCancelButton.bind(this)
    );

    this.prevMonthNode.addEventListener(
      'click',
      this.handlePreviousMonthButton.bind(this)
    );
    this.nextMonthNode.addEventListener(
      'click',
      this.handleNextMonthButton.bind(this)
    );
    this.prevYearNode.addEventListener(
      'click',
      this.handlePreviousYearButton.bind(this)
    );
    this.nextYearNode.addEventListener(
      'click',
      this.handleNextYearButton.bind(this)
    );

    this.prevMonthNode.addEventListener(
      'keydown',
      this.handlePreviousMonthButton.bind(this)
    );
    this.nextMonthNode.addEventListener(
      'keydown',
      this.handleNextMonthButton.bind(this)
    );
    this.prevYearNode.addEventListener(
      'keydown',
      this.handlePreviousYearButton.bind(this)
    );
    this.nextYearNode.addEventListener(
      'keydown',
      this.handleNextYearButton.bind(this)
    );

    document.body.addEventListener(
      'pointerup',
      this.handleBackgroundMouseUp.bind(this),
      true
    );

    // Create Grid of Dates

    this.tbodyNode.innerHTML = '';
    for (let i = 0; i < 6; i++) {
      const row = this.tbodyNode.insertRow(i);
      this.lastRowNode = row;
      for (let j = 0; j < 7; j++) {
        const cell = document.createElement('td');

        cell.tabIndex = -1;
        cell.addEventListener('click', this.handleDayClick.bind(this));
        cell.addEventListener('keydown', this.handleDayKeyDown.bind(this));
        cell.addEventListener('focus', this.handleDayFocus.bind(this));

        cell.textContent = '-1';

        row.appendChild(cell);
        this.days.push(cell);
      }
    }

    this.updateGrid();
    this.close(false);
    this.setDateForButtonLabel();
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
    const fd = this.focusDay;

    this.monthYearNode.textContent =
      this.monthLabels[fd.getMonth()] + ' ' + fd.getFullYear();

    let firstDayOfMonth = new Date(fd.getFullYear(), fd.getMonth(), 1);
    let dayOfWeek = firstDayOfMonth.getDay();

    firstDayOfMonth.setDate(firstDayOfMonth.getDate() - dayOfWeek);

    const d = new Date(firstDayOfMonth);

    for (let i = 0; i < this.days.length; i++) {
      const flag = d.getMonth() != fd.getMonth();
      this.updateDate(
        this.days[i],
        flag,
        d,
        this.isSameDay(d, this.selectedDay)
      );
      d.setDate(d.getDate() + 1);

      // Hide last row if all dates are disabled (e.g. in next month)
      if (i === 35) {
        if (flag) {
          this.lastRowNode.style.visibility = 'hidden';
        } else {
          this.lastRowNode.style.visibility = 'visible';
        }
      }
    }
  }

  updateDate(domNode, disable, day, selected) {
    let d = day.getDate().toString();
    if (day.getDate() <= 9) {
      d = '0' + d;
    }

    let m = day.getMonth() + 1;
    if (day.getMonth() < 9) {
      m = '0' + m;
    }

    domNode.tabIndex = -1;
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
        domNode.tabIndex = 0;
      }
    }
  }

  moveFocusToDay(day) {
    const d = this.focusDay;

    this.focusDay = day;

    if (
      d.getMonth() != this.focusDay.getMonth() ||
      d.getFullYear() != this.focusDay.getFullYear()
    ) {
      this.updateGrid();
    }
    this.setFocusDay();
  }

  setFocusDay(flag) {
    if (typeof flag !== 'boolean') {
      flag = true;
    }

    for (let i = 0; i < this.days.length; i++) {
      const dayNode = this.days[i];
      const day = this.getDayFromDataDateAttribute(dayNode);

      dayNode.tabIndex = -1;
      if (this.isSameDay(day, this.focusDay)) {
        dayNode.tabIndex = 0;
        if (flag) {
          dayNode.focus();
        }
      }
    }
  }

  open() {
    this.dialogNode.style.display = 'block';
    this.dialogNode.style.zIndex = 2;

    this.getDateFromTextbox();
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

    if (flag) {
      this.buttonNode.focus();
    }
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
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() + 1);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToNextWeek() {
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() + 7);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToPreviousDay() {
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() - 1);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToPreviousWeek() {
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() - 7);
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToFirstDayOfWeek() {
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() - d.getDay());
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  moveFocusToLastDayOfWeek() {
    const d = new Date(this.focusDay);
    d.setDate(d.getDate() + (6 - d.getDay()));
    this.lastDate = d.getDate();
    this.moveFocusToDay(d);
  }

  // Day methods

  isDayDisabled(domNode) {
    return domNode.classList.contains('disabled');
  }

  getDayFromDataDateAttribute(domNode) {
    const parts = domNode.getAttribute('data-date').split('-');
    return new Date(parts[0], parseInt(parts[1]) - 1, parts[2]);
  }

  // Textbox methods

  setTextboxDate(domNode) {
    let d = this.focusDay;

    if (domNode) {
      d = this.getDayFromDataDateAttribute(domNode);
      // updated aria-selected
      this.days.forEach((day) =>
        day === domNode
          ? day.setAttribute('aria-selected', 'true')
          : day.removeAttribute('aria-selected')
      );
    }

    this.textboxNode.value =
      d.getMonth() + 1 + '/' + d.getDate() + '/' + d.getFullYear();
    this.setDateForButtonLabel();
  }

  getDateFromTextbox() {
    const parts = this.textboxNode.value.split('/');
    const month = parseInt(parts[0]);
    const day = parseInt(parts[1]);
    let year = parseInt(parts[2]);

    if (
      parts.length === 3 &&
      Number.isInteger(month) &&
      Number.isInteger(day) &&
      Number.isInteger(year)
    ) {
      if (year < 100) {
        year = 2000 + year;
      }
      this.focusDay = new Date(year, month - 1, day);
      this.selectedDay = new Date(this.focusDay);
    } else {
      // If not a valid date (MM/DD/YY) initialize with todays date
      this.focusDay = new Date();
      this.selectedDay = new Date(0, 0, 1);
    }
  }

  setDateForButtonLabel() {
    const parts = this.textboxNode.value.split('/');

    if (
      parts.length === 3 &&
      Number.isInteger(parseInt(parts[0])) &&
      Number.isInteger(parseInt(parts[1])) &&
      Number.isInteger(parseInt(parts[2]))
    ) {
      const day = new Date(
        parseInt(parts[2]),
        parseInt(parts[0]) - 1,
        parseInt(parts[1])
      );

      let label = this.buttonLabelChange;
      label += ', ' + this.dayLabels[day.getDay()];
      label += ' ' + this.monthLabels[day.getMonth()];
      label += ' ' + day.getDate();
      label += ', ' + day.getFullYear();
      this.buttonNode.setAttribute('aria-label', label);
    } else {
      // If not a valid date, initialize with "Choose Date"
      this.buttonNode.setAttribute('aria-label', this.buttonLabelChoose);
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

  // Event handlers

  handleOkButton(event) {
    let flag = false;

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
        this.setTextboxDate();
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

  handleCancelButton(event) {
    let flag = false;

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

  handleNextYearButton(event) {
    let flag = false;

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

  handlePreviousYearButton(event) {
    let flag = false;

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

  handleNextMonthButton(event) {
    let flag = false;

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

  handlePreviousMonthButton(event) {
    let flag = false;

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

  handleDayKeyDown(event) {
    let flag = false;

    switch (event.key) {
      case 'Esc':
      case 'Escape':
        this.close();
        break;

      case ' ':
        this.setTextboxDate(event.currentTarget);
        flag = true;
        break;

      case 'Enter':
        this.setTextboxDate(event.currentTarget);
        this.close();
        flag = true;
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

  handleDayClick(event) {
    if (!this.isDayDisabled(event.currentTarget) && event.which !== 3) {
      this.setTextboxDate(event.currentTarget);
      this.close();
    }

    event.stopPropagation();
    event.preventDefault();
  }

  handleDayFocus() {
    this.setMessage(this.messageCursorKeys);
  }

  handleButtonKeydown(event) {
    if (event.key === 'Enter' || event.key === ' ') {
      this.open();
      this.setFocusDay();

      event.stopPropagation();
      event.preventDefault();
    }
  }

  handleButtonClick(event) {
    if (this.isOpen()) {
      this.close();
    } else {
      this.open();
      this.setFocusDay();
    }

    event.stopPropagation();
    event.preventDefault();
  }

  handleBackgroundMouseUp(event) {
    if (
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
  const datePickers = document.querySelectorAll('.datepicker');

  datePickers.forEach(function (dp) {
    new DatePickerDialog(dp);
  });
});
