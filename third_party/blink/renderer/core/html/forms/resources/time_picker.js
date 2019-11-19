'use strict';
// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='time' />
 */

function initializeTimePicker(config) {
  const timePicker = new TimePicker(config);
  global.picker = timePicker;
  main.append(timePicker);
  resizeWindow(timePicker.width, timePicker.height);
}

/**
 * Supported time column types.
 * @enum {number}
 */
const TimeColumnType = {
  UNDEFINED: 0,
  HOUR: 1,
  MINUTE: 2,
  SECOND: 3,
  MILLISECOND: 4,
  AMPM: 5,
};


/**
 * Supported label types.
 * @enum {number}
 */
const Label = {
  AM: 0,
  PM: 1,
};

/**
 * @param {string} dateTimeString
 * @param {string} mode - used to differentiate between date and time for datetime-local
 * @return {?Day|Week|Month|Time}
 */
function parseDateTimeString(dateTimeString, mode) {
  const time = Time.parse(dateTimeString);
  if (time)
    return time;
  const dateTime = DateTime.parse(dateTimeString);
  if (dateTime) {
    if (mode == 'date') {
      return dateTime.date;
    } else if (mode == 'time') {
      return dateTime.time;
    }
  }
  return parseDateString(dateTimeString);
}

class Time {
  constructor(hour, minute, second, millisecond) {
    this.hour_ = hour;
    this.minute_ = minute;
    this.second_ = second;
    this.millisecond_ = millisecond;
  }

  next = (columnType) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        this.hour_ = (this.hour_ + 1) % Time.HOUR_VALUES;
        break;
      case TimeColumnType.MINUTE:
        this.minute_ = (this.minute_ + 1) % Time.MINUTE_VALUES;
        break;
      case TimeColumnType.SECOND:
        this.second_ = (this.second_ + 1) % Time.SECOND_VALUES;
        break;
      case TimeColumnType.MILLISECOND:
        // TODO(https://crbug.com/1008294): Use increments of 1 instead of 100 for milliseconds.
        // support 100, 200, 300... for milliseconds
        this.millisecond_ =
            (Math.round(this.millisecond_ / 100) * 100 + 100) % 1000;
        break;
    }
  };

  value = (columnType, hasAMPM) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        let hour = hasAMPM ?
            (this.hour_ % Time.Maximum_Hour_AMPM || Time.Maximum_Hour_AMPM) :
            this.hour_;
        return hour.toString().padStart(2, '0');
      case TimeColumnType.MINUTE:
        return this.minute_.toString().padStart(2, '0');
      case TimeColumnType.SECOND:
        return this.second_.toString().padStart(2, '0');
      case TimeColumnType.MILLISECOND:
        return this.millisecond_.toString().padStart(3, '0');
    }
  };

  toString = (hasSecond, hasMillisecond) => {
    let value = `${this.value(TimeColumnType.HOUR)}:${
        this.value(TimeColumnType.MINUTE)}`;
    if (hasSecond) {
      value += `:${this.value(TimeColumnType.SECOND)}`;
    }
    if (hasMillisecond) {
      value += `.${this.value(TimeColumnType.MILLISECOND)}`;
    }
    return value;
  };

  clone =
      () => {
        return new Time(
            this.hour_, this.minute_, this.second_, this.millisecond_);
      }

  isAM = () => {
    return this.hour_ < Time.Maximum_Hour_AMPM;
  };

  static parse = (str) => {
    var match = Time.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    var hour = parseInt(match[1], 10);
    var minute = parseInt(match[2], 10);
    var second = 0;
    if (match[3])
      second = parseInt(match[3], 10);
    var millisecond = 0;
    if (match[4])
      millisecond = parseInt(match[4], 10);
    return new Time(hour, minute, second, millisecond);
  };

  static currentTime = () => {
    var currentDate = new Date();
    return new Time(
        currentDate.getHours(), currentDate.getMinutes(),
        currentDate.getSeconds(), currentDate.getMilliseconds());
  };

  static numberOfValues = (columnType, hasAMPM) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return hasAMPM ? Time.HOUR_VALUES_AMPM : Time.HOUR_VALUES;
      case TimeColumnType.MINUTE:
        return Time.MINUTE_VALUES;
      case TimeColumnType.SECOND:
        return Time.SECOND_VALUES;
      case TimeColumnType.MILLISECOND:
        return Time.MILLISECOND_VALUES;
    }
  };
}
// See platform/date_components.h.
Time.Minimum = new Time(0, 0, 0, 0);
Time.Maximum = new Time(23, 59, 59, 999);
Time.Maximum_Hour_AMPM = 12;
Time.ISOStringRegExp = /^(\d+):(\d+):?(\d*).?(\d*)/;
// Number of values for each column.
Time.HOUR_VALUES = 24;
Time.HOUR_VALUES_AMPM = 12;
Time.MINUTE_VALUES = 60;
Time.SECOND_VALUES = 60;
Time.MILLISECOND_VALUES = 10;

class DateTime {
  constructor(date, time) {
    this.date_ = date;
    this.time_ = time;
  };

  static parse = (str) => {
    const match = DateTime.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    const year = parseInt(match[1], 10);
    const month = parseInt(match[2], 10) - 1;
    const day = parseInt(match[3], 10);
    const date = new Day(year, month, day);

    const hour = parseInt(match[4], 10);
    const minute = parseInt(match[5], 10);
    let second = 0;
    if (match[6])
      second = parseInt(match[6], 10);
    let millisecond = 0;
    if (match[7])
      millisecond = parseInt(match[7], 10);
    const time = new Time(hour, minute, second, millisecond);

    return new DateTime(date, time);
  };

  get date() {
    return this.date_;
  }

  get time() {
    return this.time_;
  }
}
DateTime.ISOStringRegExp = /^(\d+)-(\d+)-(\d+)T(\d+):(\d+):?(\d*).?(\d*)/;

/**
 * TimePicker: Custom element providing a time picker implementation.
 *             TimePicker contains 2 parts:
 *                 - column container
 *                 - submission controls
 */
class TimePicker extends HTMLElement {
  constructor(config) {
    super();

    this.className = TimePicker.ClassName;
    this.initializeFromConfig_(config);

    this.timeColumns_ = new TimeColumns(this);
    this.append(this.timeColumns_);

    if (config.mode == 'time') {
      // TimePicker doesn't handle the submission when used for non-time types.
      this.submissionControls_ = new SubmissionControls(
          this.onSubmitButtonClick_, this.onCancelButtonClick_);
      this.append(this.submissionControls_);
      this.addEventListener('keydown', this.onKeyDown_);
    }

    window.addEventListener('resize', this.onWindowResize_, {once: true});
  };

  initializeFromConfig_ = (config) => {
    const initialSelection = parseDateTimeString(config.currentValue, 'time');
    this.selectedTime_ =
        initialSelection ? initialSelection : Time.currentTime();
    this.hasSecond_ = config.hasSecond;
    this.hasMillisecond_ = config.hasMillisecond;
    this.hasAMPM_ = config.hasAMPM;
  };

  onSubmitButtonClick_ = () => {
    const selectedValue = this.selectedValue;
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  };

  onCancelButtonClick_ = () => {
    window.pagePopupController.closePopup();
  };

  onWindowResize_ = (event) => {
    // Scroll columns to the second half to allow scrolling up.
    this.timeColumns_.scrollColumnsToMiddle();
    this.timeColumns_.firstChild.focus();
  };

  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        this.submissionControls_.submitButton.click();
        break;
      case 'Escape':
        this.submissionControls_.cancelButton.click();
        break;
    }
  };

  get selectedValue() {
    return this.timeColumns_.selectedValue().toString(
        this.hasSecond, this.hasMillisecond);
  }

  get selectedTime() {
    return this.selectedTime_;
  }

  get hasSecond() {
    return this.hasSecond_;
  }

  get hasMillisecond() {
    return this.hasMillisecond_;
  }

  get hasAMPM() {
    return this.hasAMPM_;
  }

  get width() {
    return this.timeColumns_.width;
  }

  get height() {
    return TimePicker.Height;
  }

  get timeColumns() {
    return this.timeColumns_;
  }

  get submissionControls() {
    return this.submissionControls_;
  }
}
TimePicker.ClassName = 'time-picker';
TimePicker.Height = 300;
TimePicker.ColumnWidth = 56;
TimePicker.BorderWidth = 1;
window.customElements.define('time-picker', TimePicker);

/**
 * TimeColumns: Columns container that provides functionality for creating
 *              the required columns and for updating the selected value.
 */
class TimeColumns extends HTMLElement {
  constructor(timePicker) {
    super();

    this.className = TimeColumns.ClassName;
    this.hourColumn_ = new TimeColumn(TimeColumnType.HOUR, timePicker);
    this.width_ = TimePicker.BorderWidth * 2 + TimeColumns.Margin * 2;
    this.minuteColumn_ = new TimeColumn(TimeColumnType.MINUTE, timePicker);
    if (timePicker.hasAMPM) {
      this.ampmColumn_ = new TimeColumn(TimeColumnType.AMPM, timePicker);
    }
    if (timePicker.hasAMPM && global.params.isAMPMFirst) {
      this.append(this.ampmColumn_, this.hourColumn_, this.minuteColumn_);
      this.width_ += 3 * TimePicker.ColumnWidth;
    } else {
      this.append(this.hourColumn_, this.minuteColumn_);
      this.width_ += 2 * TimePicker.ColumnWidth;
    }
    if (timePicker.hasSecond) {
      this.secondColumn_ = new TimeColumn(TimeColumnType.SECOND, timePicker);
      this.append(this.secondColumn_);
      this.width_ += TimePicker.ColumnWidth;
    }
    if (timePicker.hasMillisecond) {
      this.millisecondColumn_ =
          new TimeColumn(TimeColumnType.MILLISECOND, timePicker);
      this.append(this.millisecondColumn_);
      this.width_ += TimePicker.ColumnWidth;
    }
    if (timePicker.hasAMPM && !global.params.isAMPMFirst) {
      this.append(this.ampmColumn_);
      this.width_ += TimePicker.ColumnWidth;
    }
  };

  get width() {
    return this.width_;
  }

  selectedValue = () => {
    let hour = parseInt(this.hourColumn_.selectedTimeCell.value, 10);
    const minute = parseInt(this.minuteColumn_.selectedTimeCell.value, 10);
    const second = this.secondColumn_ ?
        parseInt(this.secondColumn_.selectedTimeCell.value, 10) :
        0;
    const millisecond = this.millisecondColumn_ ?
        parseInt(this.millisecondColumn_.selectedTimeCell.value, 10) :
        0;
    if (this.ampmColumn_) {
      const isAM = this.ampmColumn_.selectedTimeCell.textContent ==
          global.params.ampmLabels[Label.AM];
      if (isAM && hour == Time.Maximum_Hour_AMPM) {
        hour = 0;
      } else if (!isAM && hour != Time.Maximum_Hour_AMPM) {
        hour += Time.Maximum_Hour_AMPM;
      }
    }
    return new Time(hour, minute, second, millisecond);
  };

  scrollColumnsToMiddle = () => {
    this.hourColumn_.scrollTop = this.hourColumn_.scrollHeight / 2;
    this.minuteColumn_.scrollTop = this.minuteColumn_.scrollHeight / 2;
    if (this.secondColumn_) {
      this.secondColumn_.scrollTop = this.secondColumn_.scrollHeight / 2;
    }
    if (this.millisecondColumn_) {
      this.millisecondColumn_.scrollTop =
          this.millisecondColumn_.scrollHeight / 2;
    }
  }
}
TimeColumns.ClassName = 'time-columns';
TimeColumns.Margin = 1;
window.customElements.define('time-columns', TimeColumns);

/**
 * TimeColumn: Column that contains all values available for a time column type.
 */
class TimeColumn extends HTMLUListElement {
  constructor(columnType, timePicker) {
    super();

    this.className = TimeColumn.ClassName;
    this.tabIndex = 0;
    this.columnType_ = columnType;
    if (this.columnType_ == TimeColumnType.AMPM) {
      this.createAndInitializeAMPMCells_(timePicker);
    } else {
      this.createAndInitializeCells_(timePicker);
    }

    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
  };

  createAndInitializeCells_ = (timePicker) => {
    const totalCells = Time.numberOfValues(this.columnType_, timePicker.hasAMPM);
    let currentTime = timePicker.selectedTime.clone();
    let cells = [];
    let duplicateCells = [];
    // In order to support a continuous looping navigation for up/down arrows,
    // the initial list of cells is doubled and middleTimeCell is kept
    // to inform where the duplicated cells begin.
    for (let i = 0; i < totalCells; i++) {
      let value = currentTime.value(this.columnType_, timePicker.hasAMPM);
      let timeCell = new TimeCell(value, localizeNumber(value));
      let duplicatedTimeCell = new TimeCell(value, localizeNumber(value));
      cells.push(timeCell);
      duplicateCells.push(duplicatedTimeCell);
      currentTime.next(this.columnType_);
    }
    this.selectedTimeCell = duplicateCells[0];
    this.middleTimeCell_ = duplicateCells[0];
    this.append(...cells, ...duplicateCells);
  };

  createAndInitializeAMPMCells_ = (timePicker) => {
    let cells = [];
    for (let i = 0; i < 2; i++) {
      let value = global.params.ampmLabels[i];
      let timeCell = new TimeCell(value, value);
      cells.push(timeCell);
    }

    if (timePicker.selectedTime.isAM()) {
      this.append(cells[Label.AM], cells[Label.PM]);
      this.selectedTimeCell = cells[Label.AM];
    } else {
      this.append(cells[Label.PM], cells[Label.AM]);
      this.selectedTimeCell = cells[Label.PM];
    }
  };

  onClick_ = (event) => {
    this.selectedTimeCell = event.target;
  };

  /**
   * Continuous looping navigation for up/down arrows is supported by:
   *   - moving for ArrowUp to previous cell and for topmost cell which
   * has no previous, we are moving to the last cell from the first list
   *   - moving for ArrowDown to next cell and for the last duplicated cell
   * which has no next, we are moving to the first cell from the duplicated list
   */
  onKeyDown_ = (event) => {
    let eventHandled = false;
    switch (event.key) {
      case 'ArrowUp':
        const previousTimeCell = this.selectedTimeCell.previousSibling;
        if (previousTimeCell) {
          this.selectedTimeCell = previousTimeCell;
          previousTimeCell.scrollIntoViewIfNeeded(false);
        } else if (this.columnType != TimeColumnType.AMPM) {
          // move from the topmost cell to the last cell (the last cell is
          // the first one before the duplicated list).
          this.selectedTimeCell = this.middleTimeCell.previousSibling;
          this.selectedTimeCell.scrollIntoView();
        }
        eventHandled = true;
        break;
      case 'ArrowDown':
        const nextTimeCell = this.selectedTimeCell.nextSibling;
        if (nextTimeCell) {
          this.selectedTimeCell = nextTimeCell;
          nextTimeCell.scrollIntoViewIfNeeded(false);
        } else if (this.columnType != TimeColumnType.AMPM) {
          // move from the last duplicated cell to the first cell
          // of the duplicated list.
          this.selectedTimeCell = this.middleTimeCell;
          this.selectedTimeCell.scrollIntoView(false);
        }
        eventHandled = true;
        break;
      case 'ArrowLeft':
        const previousTimeColumn = this.previousSibling;
        if (previousTimeColumn) {
          previousTimeColumn.focus();
        }
        break;
      case 'ArrowRight':
        const nextTimeColumn = this.nextSibling;
        if (nextTimeColumn) {
          nextTimeColumn.focus();
        }
        break;
    }

    if (eventHandled) {
      event.stopPropagation();
      event.preventDefault();
    }
  };

  get selectedTimeCell() {
    return this.selectedTimeCell_;
  }

  set selectedTimeCell(timeCell) {
    if (this.selectedTimeCell_) {
      this.selectedTimeCell_.classList.remove('selected');
    }
    this.selectedTimeCell_ = timeCell;
    this.selectedTimeCell_.classList.add('selected');
  }

  get middleTimeCell() {
    return this.middleTimeCell_;
  }

  get columnType() {
    return this.columnType_;
  }
}
TimeColumn.ClassName = 'time-column';
window.customElements.define('time-column', TimeColumn, {extends: 'ul'});

/**
 * TimeCell: List item with a custom look that displays a time value.
 */
class TimeCell extends HTMLLIElement {
  constructor(value, localizedValue) {
    super();

    this.className = TimeCell.ClassName;
    this.textContent = localizedValue;
    this.value = value;
  };
}
TimeCell.ClassName = 'time-cell';
window.customElements.define('time-cell', TimeCell, {extends: 'li'});

/**
 * SubmissionControls: Provides functionality to submit or discard a change.
 */
class SubmissionControls extends HTMLElement {
  constructor(submitCallback, cancelCallback) {
    super();

    const padding = document.createElement('span');
    padding.setAttribute('id', 'submission-controls-padding');
    this.append(padding);

    this.className = SubmissionControls.ClassName;

    this.submitButton_ = new SubmissionButton(
        submitCallback,
        '<svg width="14" height="10" viewBox="0 0 14 10" fill="none" ' +
            'xmlns="http://www.w3.org/2000/svg"><path d="M13.3516 ' +
            '1.35156L5 9.71094L0.648438 5.35156L1.35156 4.64844L5 ' +
            '8.28906L12.6484 0.648438L13.3516 1.35156Z" fill="black"/></svg>');
    this.cancelButton_ = new SubmissionButton(
        cancelCallback,
        '<svg width="14" height="14" viewBox="0 0 14 14" fill="none" ' +
            'xmlns="http://www.w3.org/2000/svg"><path d="M7.71094 7L13.1016 ' +
            '12.3984L12.3984 13.1016L7 7.71094L1.60156 13.1016L0.898438 ' +
            '12.3984L6.28906 7L0.898438 1.60156L1.60156 0.898438L7 ' +
            '6.28906L12.3984 0.898438L13.1016 1.60156L7.71094 7Z" ' +
            'fill="black"/></svg>');
    this.append(this.submitButton_, this.cancelButton_);
  }

  get submitButton() {
    return this.submitButton_;
  }

  get cancelButton() {
    return this.cancelButton_;
  }
}
SubmissionControls.ClassName = 'submission-controls';
window.customElements.define('submission-controls', SubmissionControls);

/**
 * SubmissionButton: Button with a custom look that can be clicked for
 *                   a submission action.
 */
class SubmissionButton extends HTMLButtonElement {
  constructor(clickCallback, htmlString) {
    super();

    this.className = SubmissionButton.ClassName;
    this.innerHTML = htmlString;
    this.addEventListener('click', clickCallback);
  }
}
SubmissionButton.ClassName = 'submission-button';
window.customElements.define(
    'submission-button', SubmissionButton, {extends: 'button'});
