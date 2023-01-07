'use strict';
// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='month' />
 */

function initializeMonthPicker(config) {
  global.picker = new MonthPicker(config);
  main.append(global.picker);
  main.style.border = '1px solid #bfbfbf';
  if (global.params.isBorderTransparent) {
    main.style.borderColor = 'transparent';
  }
  main.style.height = (MonthPicker.HEIGHT - 2) + 'px';
  main.style.width = (MonthPicker.WIDTH - 2) + 'px';
  resizeWindow(MonthPicker.WIDTH, MonthPicker.HEIGHT);
}

/**
 * MonthPicker: Custom element providing a month picker implementation.
 *              MonthPicker contains 2 parts:
 *                - year list view
 *                - today button
 */
class MonthPicker extends HTMLElement {
  constructor(config) {
    super();

    this.initializeFromConfig_(config);

    this.yearListView_ =
        new YearListView(this.minimumMonth_, this.maximumMonth_, config);
    this.append(this.yearListView_.element);
    this.initializeYearListView_();

    this.clearButton_ = new ClearButton();
    this.append(this.clearButton_.element);
    this.initializeClearButton_();

    this.todayButton_ = new CalendarNavigationButton();
    this.append(this.todayButton_.element);
    this.initializeTodayButton_();

    window.addEventListener('resize', this.onWindowResize_);
    this.addEventListener('keydown', this.onKeyDown_);
  }

  initializeFromConfig_ = (config) => {
    const minimum = (typeof config.min !== 'undefined' && config.min) ?
        parseDateString(config.min) :
        Month.Minimum;
    const maximum = (typeof config.max !== 'undefined' && config.max) ?
        parseDateString(config.max) :
        Month.Maximum;
    this.minimumMonth_ = Month.createFromDay(minimum.firstDay());
    this.maximumMonth_ = Month.createFromDay(maximum.lastDay());
  };

  initializeYearListView_ = () => {
    this.yearListView_.setWidth(MonthPicker.YEAR_WIDTH);
    this.yearListView_.setHeight(MonthPicker.YEAR_HEIGHT);
    if (global.params.isLocaleRTL) {
      this.yearListView_.element.style.right = MonthPicker.YEAR_PADDING + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.right =
          MonthPicker.YEAR_WIDTH + 'px';
    } else {
      this.yearListView_.element.style.left = MonthPicker.YEAR_PADDING + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.left =
          MonthPicker.YEAR_WIDTH + 'px';
    }
    this.yearListView_.element.style.top = MonthPicker.YEAR_PADDING + 'px';

    let yearForInitialScroll = this.selectedMonth ?
        this.selectedMonth.year - 1 :
        Month.createFromToday().year - 1;
    this.yearListView_.scrollToRow(yearForInitialScroll, false);
    this.yearListView_.selectWithoutAnimating(yearForInitialScroll);

    this.yearListView_.on(
        YearListView.EventTypeYearListViewDidSelectMonth,
        this.onYearListViewDidSelectMonth_);
  };

  onYearListViewDidSelectMonth_ = (sender, month) => {
    const selectedValue = month.toString();
    window.pagePopupController.setValueAndClosePopup(0, selectedValue);
  };

  initializeClearButton_ = () => {
    this.clearButton_.element.textContent = global.params.clearLabel;
    this.clearButton_.element.setAttribute(
        'aria-label', global.params.clearLabel);
    this.clearButton_.on(
        ClearButton.EventTypeButtonClick, this.onClearButtonClick_);
  };

  onClearButtonClick_ = () => {
    window.pagePopupController.setValueAndClosePopup(0, '');
  };

  initializeTodayButton_ = () => {
    this.todayButton_.element.textContent = global.params.todayLabel;
    this.todayButton_.element.setAttribute(
        'aria-label', global.params.todayLabel);
    this.todayButton_.element.classList.add(
        MonthPicker.CLASS_NAME_TODAY_BUTTON);
    const monthContainingToday = Month.createFromToday();
    this.todayButton_.setDisabled(
        !this.yearListView_.isValid(monthContainingToday));
    this.todayButton_.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onTodayButtonClick_);
  };

  onTodayButtonClick_ = (sender) => {
    const selectedValue = Month.createFromToday().toString();
    window.pagePopupController.setValueAndClosePopup(0, selectedValue);
  };

  onKeyDown_ = (event) => {
    switch (event.key) {
      case 'Enter':
        // Don't do anything here if user has hit Enter on 'Clear' or
        // 'This month' buttons. We'll handle that respectively in
        // this.onClearButtonClick_ and this.onTodayButtonClick_.
        if (!event.target.matches(
                '.calendar-navigation-button, .clear-button')) {
          if (this.selectedMonth) {
            window.pagePopupController.setValueAndClosePopup(
                0, this.selectedMonth.toString());
          } else {
            window.pagePopupController.closePopup();
          }
        }
        break;
      case 'Escape':
        if (!this.selectedMonth ||
            (this.selectedMonth.equals(this.initialSelectedMonth))) {
          window.pagePopupController.closePopup();
        } else {
          this.resetToInitialValue_();
          window.pagePopupController.setValue(
              this.hadValidValueWhenOpened ?
                  this.initialSelectedMonth.toString() :
                  '');
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
        if (this.selectedMonth) {
          window.pagePopupController.setValue(this.selectedMonth.toString());
        }
        break;
    }
  };

  resetToInitialValue_ = () => {
    this.yearListView_.setSelectedMonthAndUpdateView(this.initialSelectedMonth);
  };

  onWindowResize_ = (event) => {
    window.removeEventListener('resize', this.onWindowResize_);
    this.yearListView_.element.focus();
  };

  get selectedMonth() {
    return this.yearListView_._selectedMonth;
  };

  get initialSelectedMonth() {
    return this.yearListView_._initialSelectedMonth;
  };

  get hadValidValueWhenOpened() {
    return this.yearListView_._hadValidValueWhenOpened;
  };
}
MonthPicker.WIDTH = 232;
MonthPicker.YEAR_WIDTH = 194;
MonthPicker.YEAR_HEIGHT = 128;
MonthPicker.YEAR_PADDING = 12;
MonthPicker.HEIGHT = 182;
MonthPicker.CLASS_NAME_TODAY_BUTTON = 'today-button';
window.customElements.define('month-picker', MonthPicker);
