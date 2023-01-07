function currentMonth() {
    return internals.pageinternals.pagePopupWindow.global.picker.currentMonth().toString();
}

function availableDayCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pageinternals.pagePopupWindow.document.querySelectorAll(".day-cell:not(.disabled):not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".day-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedDayCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".day-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function availableWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".week-number-cell.highlighted:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function selectedWeekNumberCells() {
    skipAnimation();
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".week-number-cell.selected:not(.hidden)"), function(element) {
        return element.$view.day.toString();
    }).sort().join();
}

function highlightedValue() {
    var highlight = internals.pagePopupWindow.global.picker.highlight();
    if (highlight)
        return highlight.toString();
    return null;
}

function selectedValue() {
    var selection = internals.pagePopupWindow.global.picker.selection();
    if (selection)
        return selection.toString();
    return null;
}

function skipAnimation() {
    internals.pagePopupWindow.AnimationManager.shared._animationFrameCallback(Infinity);
}

function hoverOverDayCellAt(column, row) {
  skipAnimation();
  const calendarTableView = internals.pagePopupWindow.global.picker.datePicker ?
      internals.pagePopupWindow.global.picker.datePicker.calendarTableView :
      internals.pagePopupWindow.global.picker.calendarTableView;
  var offset = cumulativeOffset(calendarTableView.element);
  var x = offset[0];
  var y = offset[1];
  if (calendarTableView.hasWeekNumberColumn)
    x += internals.pagePopupWindow.WeekNumberCell.Width;
  x += (column + 0.5) * internals.pagePopupWindow.DayCell.GetWidth();
  y += (row + 0.5) * internals.pagePopupWindow.DayCell.GetHeight() +
      internals.pagePopupWindow.CalendarTableHeaderView.GetHeight();
  eventSender.mouseMoveTo(x, y);
};

function clickDayCellAt(column, row) {
  hoverOverDayCellAt(column, row);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function hoverOverTimeCellAt(column, row) {
  const timeCellWidth = 56;
  const timeCellHeight = 36;

  skipAnimation();
  const timeColumns = internals.pagePopupWindow.global.picker.timePicker ?
      internals.pagePopupWindow.global.picker.timePicker.timeColumns :
      internals.pagePopupWindow.global.picker.timeColumns;
  var offset = cumulativeOffset(timeColumns);
  var x = offset[0];
  var y = offset[1];
  x += (column + 0.5) * timeCellWidth;
  y += (row + 0.5) * timeCellHeight;
  eventSender.mouseMoveTo(x, y);
}

function clickTimeCellAt(column, row) {
  hoverOverTimeCellAt(column, row);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function highlightedMonthButton() {
    skipAnimation();
    var year = internals.pagePopupWindow.global.picker.monthPopupView.yearListView.selectedRow + 1;
    return Array.prototype.map.call(internals.pagePopupWindow.document.querySelectorAll(".month-button.highlighted"), function(element) {
        return new internals.pagePopupWindow.Month(year, Number(element.dataset.month)).toString();
    }).sort().join();
}

function skipAnimationAndGetPositionOfMonthPopupButton() {
    skipAnimation();
    const calendarHeaderView = internals.pagePopupWindow.global.picker.datePicker ?
        internals.pagePopupWindow.global.picker.datePicker.calendarHeaderView :
        internals.pagePopupWindow.global.picker.calendarHeaderView;
    var buttonElement = calendarHeaderView.monthPopupButton.element;
    var offset = cumulativeOffset(buttonElement);
    return {x: offset[0] + buttonElement.offsetWidth / 2, y: offset[1] + buttonElement.offsetHeight / 2};
}

function hoverOverMonthPopupButton() {
    var position = skipAnimationAndGetPositionOfMonthPopupButton();
    eventSender.mouseMoveTo(position.x, position.y);
}

function clickMonthPopupButton() {
    hoverOverMonthPopupButton();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfPrevNextMonthButton(buttonIndex) {
  skipAnimation();
  var prevNextMonthButton = internals.pagePopupWindow.global.picker.element ?
      internals.pagePopupWindow.global.picker.element.querySelectorAll(
          '.calendar-navigation-button')[buttonIndex] :
      internals.pagePopupWindow.global.picker.querySelectorAll(
          '.calendar-navigation-button')[buttonIndex];
  prevNextMonthButton.foo;
  var offset = cumulativeOffset(prevNextMonthButton);
  return {
    x: offset[0] + prevNextMonthButton.offsetWidth / 2,
    y: offset[1] + prevNextMonthButton.offsetHeight / 2
  };
}

function hoverOverPrevNextMonthButton(buttonIndex) {
  var position = skipAnimationAndGetPositionOfPrevNextMonthButton(buttonIndex);
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickPrevMonthButton() {
  hoverOverPrevNextMonthButton(/*buttonIndex*/ 0);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function clickNextMonthButton() {
  hoverOverPrevNextMonthButton(/*buttonIndex*/ 1);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfButton(selector) {
  skipAnimation();
  const calendarTableView = internals.pagePopupWindow.global.picker.datePicker ?
      internals.pagePopupWindow.global.picker.datePicker.calendarTableView :
      internals.pagePopupWindow.global.picker.calendarTableView;
  var buttonElement =
      calendarTableView.element.querySelector(selector);
  var offset = cumulativeOffset(buttonElement);
  return {
    x: offset[0] + buttonElement.offsetWidth / 2,
    y: offset[1] + buttonElement.offsetHeight / 2
  };
}

function hoverOverClearButton() {
  var position = skipAnimationAndGetPositionOfButton('.clear-button');
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickClearButton() {
  hoverOverClearButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function hoverOverTodayButton() {
  var position = skipAnimationAndGetPositionOfButton('.today-button');
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickTodayButton() {
  hoverOverTodayButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfGlobalPickerButton(selector) {
  skipAnimation();
  const button = internals.pagePopupWindow.global.picker.querySelector(selector);
  var offset = cumulativeOffset(button);
  return {
    x: offset[0] + button.offsetWidth / 2,
    y: offset[1] + button.offsetHeight / 2
  };
}

function hoverOverClearMonthButton() {
  var position =
      skipAnimationAndGetPositionOfGlobalPickerButton('.clear-button');
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickClearMonthButton() {
  hoverOverClearMonthButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function hoverOverThisMonthButton() {
  var position =
      skipAnimationAndGetPositionOfGlobalPickerButton('.today-button');
  eventSender.mouseMoveTo(position.x, position.y);
}

function clickThisMonthButton() {
  hoverOverThisMonthButton();
  eventSender.mouseDown();
  eventSender.mouseUp();
}

function clickYearListCell(year) {
    skipAnimation();
    var row = year - 1;
    var rowCell = internals.pagePopupWindow.global.picker.monthPopupView.yearListView.cellAtRow(row);

    var rowScrollOffset = internals.pagePopupWindow.global.picker.monthPopupView.yearListView.scrollOffsetForRow(row);
    var scrollOffset = internals.pagePopupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var scrollViewOffset = cumulativeOffset(internals.pagePopupWindow.global.picker.monthPopupView.yearListView.scrollView.element);
    var rowCellCenterX = scrollViewOffset[0] + rowCell.element.offsetWidth / 2;
    var rowCellCenterY = scrollViewOffset[1] + rowOffsetFromViewportTop + rowCell.element.offsetHeight / 2;
    eventSender.mouseMoveTo(rowCellCenterX, rowCellCenterY);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function skipAnimationAndGetPositionOfMonthButton(year, month, inMonthPicker) {
    let yearListView = inMonthPicker ? internals.pagePopupWindow.global.picker.yearListView_
                                     : internals.pagePopupWindow.global.picker.monthPopupView.yearListView;
    skipAnimation();
    var row = year - 1;
    var rowCell = yearListView.cellAtRow(row);
    var rowScrollOffset = yearListView.scrollOffsetForRow(row);
    var scrollOffset = yearListView.scrollView.contentOffset();
    var rowOffsetFromViewportTop = rowScrollOffset - scrollOffset;

    var button = yearListView.buttonForMonth(new internals.pagePopupWindow.Month(year, month));
    var buttonOffset = cumulativeOffset(button);
    var rowCellOffset = cumulativeOffset(rowCell.element);
    var buttonOffsetRelativeToRowCell = [buttonOffset[0] - rowCellOffset[0], buttonOffset[1] - rowCellOffset[1]];

    var scrollViewOffset = cumulativeOffset(yearListView.scrollView.element);
    var buttonCenterX = scrollViewOffset[0] + buttonOffsetRelativeToRowCell[0] + button.offsetWidth / 2;
    var buttonCenterY = scrollViewOffset[1] + buttonOffsetRelativeToRowCell[1] + rowOffsetFromViewportTop + button.offsetHeight / 2;
    return {x: buttonCenterX, y: buttonCenterY};
}

function hoverOverMonthButton(year, month) {
    var position = skipAnimationAndGetPositionOfMonthButton(year, month, false);
    eventSender.mouseMoveTo(position.x, position.y);
}

function clickMonthButton(year, month) {
    hoverOverMonthButton(year, month);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function hoverOverMonthButtonForMonthPicker(year, month) {
    var position = skipAnimationAndGetPositionOfMonthButton(year, month, true);
    eventSender.mouseMoveTo(position.x, position.y);
}

var lastYearListViewScrollOffset = NaN;
function checkYearListViewScrollOffset() {
    skipAnimation();
    var scrollOffset = internals.pagePopupWindow.global.picker.monthPopupView.yearListView.scrollView.contentOffset();
    var change = lastYearListViewScrollOffset - scrollOffset;
    lastYearListViewScrollOffset = scrollOffset;
    return change;
}

function isCalendarTableScrollingWithAnimation() {
    var animator = internals.pagePopupWindow.global.picker.calendarTableView.scrollView.scrollAnimator();
    if (!animator)
        return false;
    return animator.isRunning();
}

function removeCommitDelay() {
    internals.pagePopupWindow.CalendarPicker.commitDelayMs = 0;
}

function setNoCloseOnCommit() {
    internals.pagePopupWindow.CalendarPicker.commitDelayMs = -1;
}
