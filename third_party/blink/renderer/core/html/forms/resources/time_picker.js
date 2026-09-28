'use strict';
// Copyright 2019 The Chromium Authors
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
  };

  /**
   * Advance to the next value offered by the given column, wrapping around at
   * the end of the column's range.
   * @param {TimeColumnType} columnType
   * @param {number} granularity Distance between two consecutive values of the
   *     column, in the column's own units.
   * @param {boolean} hasAMPM
   */
  next = (columnType, granularity, hasAMPM) => {
    const unitsPerColumn = Time.unitsPerColumn(columnType, hasAMPM);
    switch (columnType) {
      case TimeColumnType.HOUR:
        this.hour_ = (this.hour_ + granularity) % unitsPerColumn;
        break;
      case TimeColumnType.MINUTE:
        this.minute_ = (this.minute_ + granularity) % unitsPerColumn;
        break;
      case TimeColumnType.SECOND:
        this.second_ = (this.second_ + granularity) % unitsPerColumn;
        break;
      case TimeColumnType.MILLISECOND:
        this.millisecond_ = (this.millisecond_ + granularity) % unitsPerColumn;
        break;
    }
  };

  /**
   * The value of the given column in the column's own units, without the
   * localization and 12-hour clock adjustments that value() applies.  Hours
   * are folded into the range of the column, so 14:00 reports hour 2 on a
   * 12-hour clock.
   * @param {TimeColumnType} columnType
   * @param {boolean} hasAMPM
   * @return {number}
   */
  unitValue = (columnType, hasAMPM) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return this.hour_ % Time.unitsPerColumn(columnType, hasAMPM);
      case TimeColumnType.MINUTE:
        return this.minute_;
      case TimeColumnType.SECOND:
        return this.second_;
      case TimeColumnType.MILLISECOND:
        return this.millisecond_;
    }
  };

  value = (columnType, hasAMPM) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        const hour = hasAMPM ?
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
    const match = Time.ISOStringRegExp.exec(str);
    if (!match)
      return null;
    const hour = parseInt(match[1], 10);
    const minute = parseInt(match[2], 10);
    let second = 0;
    if (match[3])
      second = parseInt(match[3], 10);
    let millisecond = 0;
    if (match[4])
      millisecond = parseInt(match[4], 10);
    return new Time(hour, minute, second, millisecond);
  };

  static currentTime = () => {
    const currentDate = new Date();
    return new Time(
        currentDate.getHours(), currentDate.getMinutes(),
        currentDate.getSeconds(), currentDate.getMilliseconds());
  };

  /**
   * Number of the column's own units that the column spans, e.g. 60 for the
   * minute column.  This is not the number of cells in the column, which also
   * depends on the granularity imposed by the step attribute.
   * @param {TimeColumnType} columnType
   * @param {boolean} hasAMPM
   * @return {number}
   */
  static unitsPerColumn = (columnType, hasAMPM) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return hasAMPM ? Time.HOUR_UNITS_AMPM : Time.HOUR_UNITS;
      case TimeColumnType.MINUTE:
        return Time.MINUTE_UNITS;
      case TimeColumnType.SECOND:
        return Time.SECOND_UNITS;
      case TimeColumnType.MILLISECOND:
        return Time.MILLISECOND_UNITS;
    }
  };

  /**
   * Length of one of the column's own units, in milliseconds.
   * @param {TimeColumnType} columnType
   * @return {number}
   */
  static millisecondsPerUnit = (columnType) => {
    switch (columnType) {
      case TimeColumnType.HOUR:
        return 60 * 60 * 1000;
      case TimeColumnType.MINUTE:
        return 60 * 1000;
      case TimeColumnType.SECOND:
        return 1000;
      case TimeColumnType.MILLISECOND:
        return 1;
    }
  };

  /**
   * The granularity that the step attribute imposes on the given column,
   * expressed in the column's own units, along with the offset that valid
   * values are aligned to.  A granularity of one means that the column offers
   * every one of its values.
   *
   * This mirrors DateTimeEditBuilder::CreateStep so that the picker offers
   * exactly the values that stepping the in-page control's fields with the
   * up and down arrow keys produces.
   *
   * @param {TimeColumnType} columnType
   * @param {boolean} hasAMPM
   * @return {{granularity: number, stepBase: number}}
   */
  static granularityForColumn = (columnType, hasAMPM) => {
    const defaultGranularity = Time.defaultGranularity(columnType);
    // Killswitch.  Without it each column falls back to the granularity it
    // used before the picker took step into account: one cell per value, and
    // one cell per 100ms for the millisecond column.
    if (!global.params.timePickerHonorsStep) {
      return {granularity: defaultGranularity, stepBase: 0};
    }
    const millisecondsPerUnit = Time.millisecondsPerUnit(columnType);
    const unitsPerColumn = Time.unitsPerColumn(columnType, hasAMPM);
    const millisecondsPerColumn = millisecondsPerUnit * unitsPerColumn;
    let stepInMilliseconds = Number(global.params.step);
    if (!Number.isFinite(stepInMilliseconds) || stepInMilliseconds <= 0) {
      return {granularity: defaultGranularity, stepBase: 0};
    }

    // A step that spans whole columns leaves this column with a single value.
    if (stepInMilliseconds % millisecondsPerColumn === 0) {
      stepInMilliseconds = millisecondsPerColumn;
    }
    // A step that does not divide the column evenly, or that is not a whole
    // number of the column's units, cannot be expressed as a granularity for
    // this column, so the column keeps offering the values it offers by
    // default.
    if (millisecondsPerColumn % stepInMilliseconds !== 0 ||
        stepInMilliseconds % millisecondsPerUnit !== 0) {
      return {granularity: defaultGranularity, stepBase: 0};
    }

    const stepBaseInMilliseconds = Number(global.params.stepBase);
    const stepBase = Number.isFinite(stepBaseInMilliseconds) ?
        Time.wrapToColumn(
            Math.floor(stepBaseInMilliseconds / millisecondsPerUnit),
            unitsPerColumn) :
        0;
    return {
      granularity: Time.coarsenGranularity(
          columnType, stepInMilliseconds / millisecondsPerUnit),
      stepBase: stepBase,
    };
  };

  /**
   * The granularity a column uses when step does not constrain it.  Every
   * column offers each of its values, except the millisecond column, which
   * would need a thousand cells to do so.
   * @param {TimeColumnType} columnType
   * @return {number}
   */
  static defaultGranularity = (columnType) => {
    return columnType === TimeColumnType.MILLISECOND ?
        Time.MILLISECOND_GRANULARITY :
        1;
  };

  /**
   * One cell per millisecond that step allows is far more than the millisecond
   * column can usefully show, so round a fine granularity up to the smallest
   * granularity the column is willing to display.  The candidates all divide
   * the column evenly, and only a multiple of the given granularity is chosen,
   * so every cell remains a value that step allows.
   * @param {TimeColumnType} columnType
   * @param {number} granularity
   * @return {number}
   */
  static coarsenGranularity = (columnType, granularity) => {
    if (columnType !== TimeColumnType.MILLISECOND ||
        granularity >= Time.MILLISECOND_GRANULARITY) {
      return granularity;
    }
    return Time.MILLISECOND_GRANULARITIES.find(
               (candidate) => candidate % granularity === 0) ||
        granularity;
  };

  /**
   * @param {number} value
   * @param {number} unitsPerColumn
   * @return {number} value folded into the range [0, unitsPerColumn).
   */
  static wrapToColumn = (value, unitsPerColumn) => {
    return ((value % unitsPerColumn) + unitsPerColumn) % unitsPerColumn;
  };
}
// See platform/date_components.h.
Time.Minimum = new Time(0, 0, 0, 0);
Time.Maximum = new Time(23, 59, 59, 999);
Time.Maximum_Hour_AMPM = 12;
Time.ISOStringRegExp = /^(\d+):(\d+):?(\d*).?(\d*)/;
// Number of the column's own units that each column spans.
Time.HOUR_UNITS = 24;
Time.HOUR_UNITS_AMPM = 12;
Time.MINUTE_UNITS = 60;
Time.SECOND_UNITS = 60;
Time.MILLISECOND_UNITS = 1000;
// The finest granularity the millisecond column will display, and the
// granularities it will fall back on, which are the divisors of
// MILLISECOND_UNITS that are at least that coarse.
Time.MILLISECOND_GRANULARITY = 100;
Time.MILLISECOND_GRANULARITIES = [100, 125, 200, 250, 500, 1000];

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
 */
class TimePicker extends HTMLElement {
  constructor(config) {
    super();

    this.className = TimePicker.ClassName;
    if (global.params.isBorderTransparent) {
      this.style.borderColor = 'transparent';
    }
    this.initializeFromConfig_(config);

    this.timeColumns_ = new TimeColumns(this);
    this.append(this.timeColumns_);

    if (config.mode == 'time') {
      // TimePicker doesn't handle the submission when used for non-time types.
      this.addEventListener('keydown', this.onKeyDown_);
      this.addEventListener('click', this.onClick_);
    }

    // scrollColumnsToSelectedCells needs offsetTop, so wait for layout.
    runOnceWhenLaidOut((event) => this.onWindowResize_(event));
  };

  initializeFromConfig_ = (config) => {
    const initialSelection = parseDateTimeString(config.currentValue, 'time');
    this.initialSelectedTime_ =
        initialSelection ? initialSelection : Time.currentTime();
    this.hadValidValueWhenOpened_ = (initialSelection != null);
    this.hasSecond_ = config.hasSecond;
    this.hasMillisecond_ = config.hasMillisecond;
    this.hasAMPM_ = config.hasAMPM;
    this.initialFocusedFieldIndex_ = config.focusedFieldIndex || 0;
  };

  onWindowResize_ = (event) => {
    this.timeColumns_.scrollColumnsToSelectedCells();
    if (!this.focusOnFieldIndex(this.initialFocusedFieldIndex_))
      this.timeColumns_.firstChild.focus();
  };

  /** Focus on given index if valid. Return true if so. */
  focusOnFieldIndex = (index) => {
    if (index >= 0 && index < this.timeColumns_.children.length) {
      this.timeColumns_.children[index].focus();
      return true;
    }
    return false
  };

  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        window.pagePopupController.setValueAndClosePopup(
            0, this.selectedValue, /* is_keyboard_event= */ true);
        break;
      case 'Escape':
        if (this.selectedValue ===
            this.initialSelectedTime.toString(
                this.hasSecond, this.hasMillisecond)) {
          window.pagePopupController.closePopup();
        } else {
          this.resetToInitialValue();
          window.pagePopupController.setValue(
              this.hadValidValueWhenOpened ? this.selectedValue : '');
        }
        break;
      case 'ArrowUp':
      case 'ArrowDown':
        window.pagePopupController.setValue(this.selectedValue);
        event.stopPropagation();
        event.preventDefault();
        break;
      case 'Home':
      case 'End':
        window.pagePopupController.setValue(this.selectedValue);
        event.stopPropagation();
        // Prevent an attempt to scroll to the end of
        // of an infinitely looping column.
        event.preventDefault();
        break;
    }
  };

  onClick_ = (event) => {
    window.pagePopupController.setValue(this.selectedValue);
  };

  resetToInitialValue = () => {
    this.timeColumns_.resetToInitialValues();
    this.timeColumns_.scrollColumnsToSelectedCells();
  }

  get selectedValue() {
    return this.timeColumns_.selectedValue().toString(
        this.hasSecond, this.hasMillisecond);
  }

  get initialSelectedTime() {
    return this.initialSelectedTime_;
  }

  get hadValidValueWhenOpened() {
    return this.hadValidValueWhenOpened_;
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
}
TimePicker.ClassName = 'time-picker';
TimePicker.Height = 260;
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

  resetToInitialValues =
      () => {
        Array.prototype.forEach.call(this.children, (column) => {
          column.resetToInitialValue();
        });
      }

  scrollColumnsToSelectedCells = () => {
    this.hourColumn_.scrollToSelectedCell();
    this.minuteColumn_.scrollToSelectedCell();
    if (this.secondColumn_) {
      this.secondColumn_.scrollToSelectedCell();
    }
    if (this.millisecondColumn_) {
      this.millisecondColumn_.scrollToSelectedCell();
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
    this.setAttribute('role', 'listbox');
    if (this.columnType_ === TimeColumnType.HOUR) {
      this.setAttribute('aria-label', global.params.axHourLabel);
    } else if (this.columnType_ === TimeColumnType.MINUTE) {
      this.setAttribute('aria-label', global.params.axMinuteLabel);
    } else if (this.columnType_ === TimeColumnType.SECOND) {
      this.setAttribute('aria-label', global.params.axSecondLabel);
    } else if (this.columnType_ === TimeColumnType.MILLISECOND) {
      this.setAttribute('aria-label', global.params.axMillisecondLabel);
    } else {
      this.setAttribute('aria-label', global.params.axAmPmLabel);
    }

    if (this.columnType_ == TimeColumnType.AMPM) {
      this.createAndInitializeAMPMCells_(timePicker);
    } else {
      this.createAndInitializeCells_(timePicker);
      if (this.isLooping_) {
        this.setupScrollHandler_();
      }
    }

    this.addEventListener('click', this.onClick_);
    this.addEventListener('keydown', this.onKeyDown_);
  };

  createAndInitializeCells_ = (timePicker) => {
    const hasAMPM = timePicker.hasAMPM;
    const currentTime = timePicker.initialSelectedTime.clone();
    const unitsPerColumn = Time.unitsPerColumn(this.columnType_, hasAMPM);
    const {granularity, stepBase} =
        Time.granularityForColumn(this.columnType_, hasAMPM);
    const valuesPerColumn = unitsPerColumn / granularity;

    // The column shows each of its values once.  Scrolling the column loops by
    // rotating those cells, which only works when there are enough of them to
    // cover the visible area plus one offscreen cell above and below it.  A
    // coarse step can leave the column with fewer values than that, and such a
    // column scrolls as an ordinary short list instead.
    const totalCells = valuesPerColumn;
    this.isLooping_ = totalCells >= TimeColumn.MINIMUM_LOOPING_CELLS;

    // The first cell holds the first value at or after the start of the
    // column's range, so a column offering every value still starts at one.
    const firstCellValue = Time.wrapToColumn(
        stepBase + Math.ceil((1 - stepBase) / granularity) * granularity,
        unitsPerColumn);

    // The value of the in-page control is not necessarily one of the values
    // that this column offers, either because it does not match step or
    // because the millisecond column has a granularity of its own.  Keep it
    // reachable by giving the closest cell the exact value of the control
    // instead of the value it would otherwise hold.
    const currentValue = currentTime.unitValue(this.columnType_, hasAMPM);
    const nearestValue = Time.wrapToColumn(
        stepBase +
            Math.round((currentValue - stepBase) / granularity) * granularity,
        unitsPerColumn);

    const time = new Time(
        this.columnType_ === TimeColumnType.HOUR ? firstCellValue : 1,
        this.columnType_ === TimeColumnType.MINUTE ? firstCellValue : 1,
        this.columnType_ === TimeColumnType.SECOND ? firstCellValue : 1,
        this.columnType_ === TimeColumnType.MILLISECOND ? firstCellValue : 100);
    const cells = [];
    let initialCellIndex = -1;
    for (let i = 0; i < totalCells; i++) {
      let value = time.value(this.columnType_, hasAMPM);

      if (time.unitValue(this.columnType_, hasAMPM) === nearestValue) {
        value = currentTime.value(this.columnType_, hasAMPM);
        if (initialCellIndex < 0) {
          initialCellIndex = i;
        }
      }

      const timeCell = new TimeCell(value, localizeNumber(value));
      cells.push(timeCell);

      timeCell.initialOffsetTop = TimeColumn.CELL_HEIGHT * i;
      if (this.isLooping_) {
        timeCell.style.top = `${TimeColumn.SCROLL_OFFSET}px`;
      }

      time.next(this.columnType_, granularity, hasAMPM);
    }
    this.selectedTimeCell = this.initialTimeCell_ = cells[initialCellIndex];
    this.cellsInLayoutOrder = cells;
    this.append(...cells);
  };

  /*
  * Create a scroll handler that implements infinite looping scroll by
  * rotating TimeCells up/down so that there is always at least one cell
  * offscreen in the direction of the scroll.  This activity should be
  * invisible to the user.
  */
  setupScrollHandler_ = () => {
    let lastScrollPosition = 0;
    let upcomingSnapToCellEdge = null;
    let suppressScrollChange = false;
    this.addEventListener('scroll', (event) => {
      const isGoingDown = (this.scrollTop > lastScrollPosition);
      lastScrollPosition = this.scrollTop;

      if (suppressScrollChange) {
        suppressScrollChange = false;
        return;
      }

      // Rotate cells down until there is one cell beyond the bottom
      // of the visible scroller area.
      while (this.cellsInLayoutOrder[this.cellsInLayoutOrder.length - 1]
                     .offsetTop -
                 this.scrollTop - this.clientHeight <
             TimeColumn.CELL_HEIGHT) {
        this.rotateCells_(
            /*topToBottom*/ true);
      }

      // Rotate cells up until there is one cell beyond the top
      // of the visible scroller area.
      while (this.scrollTop - this.cellsInLayoutOrder[0].offsetTop <
             TimeColumn.CELL_HEIGHT * 2) {
        this.rotateCells_(
            /*topToBottom*/ false);
      }

      // Snap the scroll amount to the nearest TimeCell top edge 1 second
      // after the user has stopped scrolling.  This would be done with
      // CSS scroll-snap-align, but it interferes with this scroll handler
      // and causes jittery scrolling.
      window.clearTimeout(upcomingSnapToCellEdge);
      upcomingSnapToCellEdge = window.setTimeout(() => {
        const offset = this.calcOffsetFromCellEdge_(isGoingDown);
        if (offset != 0) {
          suppressScrollChange = true;
          this.scrollTop += offset;
        }
      }, 1000);
    });
  };

  /*
  * Calculate offset form a cell top / bottom edge based on scrolling direction.
  */
  calcOffsetFromCellEdge_ = (isGoingDown) => {
    const offsetFromCellEdge =
        (this.cellsInLayoutOrder[this.cellsInLayoutOrder.length - 1].offsetTop -
         this.scrollTop) %
        TimeColumn.CELL_HEIGHT;
    if (isGoingDown) {
      return offsetFromCellEdge;
    } else {
      if (offsetFromCellEdge != 0) {
        return -(TimeColumn.CELL_HEIGHT - offsetFromCellEdge);
      }
    }
    return 0;
  };

  // Ideally we would have truly infinite scrolling in both directions.
  // However, the platform does not allow scrolling into negative scroll
  // offsets.  So, we start the column at a large positive scroll so that
  // the column will be unlikely to hit the top during normal use.
  static SCROLL_OFFSET = 100000;
  static CELL_HEIGHT = 36;  // Height of one TimeCell, including border

  // A column is 252px tall, so seven cells are visible at a time.  Rotating
  // cells to scroll the column relies on there always being a cell just
  // outside the visible area in both directions, so a column needs at least
  // this many cells before it can loop.
  static MINIMUM_LOOPING_CELLS = 10;

  // Using position:absolute for TimeCells seems like the natural choice,
  // but absolutely positioned children don't cause the TimeColumn scroll
  // container to expand to hold the cells, so they fall off the end of
  // the popup.  Instead, we use relative positioning and use these
  // helpers to convert to an "absolute" position that is easier to reason
  // about when manipulating the layout position of the TimeCells.
  static getCellAbsolutePosition = (cell) => {
    const cellOffset = parseInt(cell.style.top.substring(
        0, cell.style.top.length - 2));  // Chop off the 'px'
    return (cellOffset + cell.initialOffsetTop);
  };
  static setCellAbsolutePosition = (cell, absolutePosition) => {
    cell.style.top = `${absolutePosition - cell.initialOffsetTop}px`;
  };

  // Take the top/bottom TimeCell in this column and move it to the
  // bottom/top.  This should only be done for offscreen cells so that
  // it is invisible to the user -- but it ensures that the cells will
  // always be visible wherever the user scrolls.
  rotateCells_ = (topToBottom) => {
    if (!this.isLooping_) {
      return;
    }
    if (topToBottom) {
      const topCell = this.cellsInLayoutOrder.shift();
      const bottomCell =
          this.cellsInLayoutOrder[this.cellsInLayoutOrder.length - 1];
      const bottomCellAbsoluteOffset =
          TimeColumn.getCellAbsolutePosition(bottomCell);
      TimeColumn.setCellAbsolutePosition(
          topCell, bottomCellAbsoluteOffset + TimeColumn.CELL_HEIGHT);
      this.cellsInLayoutOrder.push(topCell);
    } else {
      const topCell = this.cellsInLayoutOrder[0];
      const bottomCell = this.cellsInLayoutOrder.pop();
      const absoluteTopCellOffset = TimeColumn.getCellAbsolutePosition(topCell);
      TimeColumn.setCellAbsolutePosition(
          bottomCell, absoluteTopCellOffset - TimeColumn.CELL_HEIGHT);
      this.cellsInLayoutOrder.unshift(bottomCell);
    }
  };

  createAndInitializeAMPMCells_ = (timePicker) => {
    const cells = [];
    for (let i = 0; i < 2; i++) {
      const value = global.params.ampmLabels[i];
      const timeCell = new TimeCell(value, value);
      cells.push(timeCell);
    }

    if (timePicker.initialSelectedTime.isAM()) {
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
   * Continuous looping navigation for up/down arrows and scrolling is
   * supported by rotating the layout positions of the TimeCells.  This
   * is done in a scroll event handler and the following keydown handler.
   * Cells are rotated in before they are reached by the visible part of
   * the scroller, so the user just sees an infinitely looping column.
   */
  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'ArrowUp':
        const previousTimeCell = this.selectedTimeCell.previousSibling ?
            this.selectedTimeCell.previousSibling :
            this.lastElementChild;

        if (this.scrollTop === 0 && previousTimeCell.offsetTop <= 0) {
          // If the user somehow made it all the way to the top of the
          // scroller, stop going up and rotating cells into negative
          // offsets.  This should not be a normal scenario.
          break;
        }

        // Ensure that we don't run out of cells ahead of the selected cell in
        // the event that the scroll event handler can't keep up.  This can
        // happen e.g. if the user holds down the arrow key.
        if (this.columnType_ !== TimeColumnType.AMPM &&
            this.selectedTimeCell === this.cellsInLayoutOrder[0]) {
          this.rotateCells_(/*topToBottom*/ false);
        }

        this.selectedTimeCell = previousTimeCell;
        this.selectedTimeCell.scrollIntoViewIfNeeded(false);
        break;
      case 'ArrowDown':
        const nextTimeCell = this.selectedTimeCell.nextSibling ?
            this.selectedTimeCell.nextSibling :
            this.firstElementChild;

        // Ensure that we don't run out of cells ahead of the selected cell in
        // the event that the scroll event handler can't keep up.  This can
        // happen e.g. if the user holds down the arrow key.
        if (this.columnType_ !== TimeColumnType.AMPM &&
            this.selectedTimeCell ===
                this.cellsInLayoutOrder[this.cellsInLayoutOrder.length - 1]) {
          this.rotateCells_(/*topToBottom*/ true);
        }

        this.selectedTimeCell = nextTimeCell;
        this.selectedTimeCell.scrollIntoViewIfNeeded(false);
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
      case 'Home':
        this.setToMinValue();
        this.scrollToSelectedCell();
        break;
      case 'End':
        this.setToMaxValue();
        this.scrollToSelectedCell();
        break;
    }
  };

  scrollToSelectedCell =
      (cell) => {
        if (this.isLooping_) {
          while (this.cellsInLayoutOrder[1] != this.selectedTimeCell) {
            this.rotateCells_(/*topToBottom*/ true);
          }
        }
        // A column that does not loop still holds more cells than fit in the
        // visible area when it has eight or nine of them, so it scrolls to
        // the selected cell as well.  A column whose cells all fit does not
        // scroll, and this leaves it where it is.
        this.scrollTop = this.selectedTimeCell.offsetTop;
      }

  get selectedTimeCell() {
    return this.selectedTimeCell_;
  }

  set selectedTimeCell(timeCell) {
    if (this.selectedTimeCell_) {
      this.selectedTimeCell_.classList.remove('selected');
      this.selectedTimeCell_.removeAttribute('aria-selected');
    }
    this.selectedTimeCell_ = timeCell;
    this.setAttribute('aria-activedescendant', timeCell.id);
    this.selectedTimeCell_.classList.add('selected');
    this.selectedTimeCell_.setAttribute('aria-selected', 'true');
  }

  resetToInitialValue = () => {
    if (this.columnType_ == TimeColumnType.AMPM) {
      this.selectedTimeCell = this.firstChild;
    } else {
      this.selectedTimeCell = this.initialTimeCell_;
    }
  };

  setToMinValue = () => {
    if (this.columnType_ == TimeColumnType.AMPM) {
      this.selectedTimeCell = this.firstChild;
      const isAM = this.selectedTimeCell.textContent ==
          global.params.ampmLabels[Label.AM];
      if (!isAM)
        this.selectedTimeCell = this.lastChild;
    } else {
      this.selectedTimeCell = this.firstChild;
      for (let timeCell of this.children) {
        if (timeCell.value < this.selectedTimeCell.value)
          this.selectedTimeCell = timeCell;
      }
    }
  };

  setToMaxValue = () => {
    if (this.columnType_ == TimeColumnType.AMPM) {
      this.selectedTimeCell = this.firstChild;
      const isAM = this.selectedTimeCell.textContent ==
          global.params.ampmLabels[Label.AM];
      if (isAM)
        this.selectedTimeCell = this.lastChild;
    } else {
      this.selectedTimeCell = this.lastChild;
      for (let timeCell of this.children) {
        if (timeCell.value > this.selectedTimeCell.value)
          this.selectedTimeCell = timeCell;
      }
    }
  };

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

    this.setAttribute('role', 'option');
    this.id = TimeCell.getNextUniqueId();
  };

  static getNextUniqueId() {
    return `timeCell${TimeCell.idCount++}`;
  }

  static idCount = 0;
}
TimeCell.ClassName = 'time-cell';
window.customElements.define('time-cell', TimeCell, {extends: 'li'});
