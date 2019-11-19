'use strict';
// Copyright (C) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picker used by <input type='month' />
 */

function initializeMonthPicker(config) {
  global.picker = new MonthPicker(config);
  main.append(global.picker);
  main.style.border = '1px solid transparent';
  main.style.height = (MonthPicker.Height - 2) + 'px';
  main.style.width = (MonthPicker.Width - 2) + 'px';
  resizeWindow(MonthPicker.Width, MonthPicker.Height);
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
        new YearListView(this.minimumMonth_, this.maximumMonth_);
    this.append(this.yearListView_.element);
    this.initializeYearListView_();

    this.todayButton_ = new CalendarNavigationButton();
    this.append(this.todayButton_.element);
    this.initializeTodayButton_();

    window.addEventListener('resize', this.onWindowResize_);
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

    const initialSelection = parseDateString(config.currentValue);
    const initialSelectedMonth = initialSelection ?
        Month.createFromDay(initialSelection.middleDay()) :
        Month.createFromToday();
    this.initialValidSelection_ = false;
    if (initialSelectedMonth < this.minimumMonth_) {
      this.selectedMonth_ = this.minimumMonth_;
    } else if (initialSelectedMonth > this.maximumMonth_) {
      this.selectedMonth_ = this.maximumMonth_;
    } else {
      this.selectedMonth_ = initialSelectedMonth;
      this.initialValidSelection_ = initialSelection != null;
    }
  };

  initializeYearListView_ = () => {
    this.yearListView_.setWidth(MonthPicker.YearWidth);
    this.yearListView_.setHeight(MonthPicker.YearHeight);
    if (global.params.isLocaleRTL) {
      this.yearListView_.element.style.right = MonthPicker.YearPadding + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.right =
          MonthPicker.YearWidth + 'px';
    } else {
      this.yearListView_.element.style.left = MonthPicker.YearPadding + 'px';
      this.yearListView_.scrubbyScrollBar.element.style.left =
          MonthPicker.YearWidth + 'px';
    }
    this.yearListView_.element.style.top = MonthPicker.YearPadding + 'px';
    if (this.initialValidSelection_) {
      this.yearListView_.setSelectedMonth(this.selectedMonth_);
    }
    this.yearListView_.show(this.selectedMonth_);
    this.yearListView_.on(
        YearListView.EventTypeYearListViewDidSelectMonth,
        this.onYearListViewDidSelectMonth_);
    this.yearListView_.on(
        YearListView.EventTypeYearListViewDidHide, this.onYearListViewDidHide_);
  };

  onYearListViewDidHide_ = (sender) => {
    const selectedValue = this.selectedMonth_.toString();
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  };

  onYearListViewDidSelectMonth_ = (sender, month) => {
    this.selectedMonth_ = month;
  };

  initializeTodayButton_ = () => {
    this.todayButton_.element.textContent = global.params.todayLabel;
    this.todayButton_.element.setAttribute(
        'aria-label', global.params.todayLabel);
    this.todayButton_.element.classList.add(MonthPicker.ClassNameTodayButton);
    const monthContainingToday = Month.createFromToday();
    this.todayButton_.setDisabled(
        monthContainingToday < this.minimumMonth_ ||
        monthContainingToday > this.maximumMonth_);
    this.todayButton_.on(
        CalendarNavigationButton.EventTypeButtonClick,
        this.onTodayButtonClick_);
  };

  onTodayButtonClick_ = (sender) => {
    const selectedValue = Month.createFromToday().toString();
    window.setTimeout(function() {
      window.pagePopupController.setValueAndClosePopup(0, selectedValue);
    }, 100);
  };

  onWindowResize_ = (event) => {
    window.removeEventListener('resize', this.onWindowResize_);
    this.yearListView_.element.focus();
  };
}
MonthPicker.Width = 232;
MonthPicker.YearWidth = 194;
MonthPicker.YearHeight = 128;
MonthPicker.YearPadding = 12;
MonthPicker.Height = 182;
MonthPicker.ClassNameTodayButton = 'today-button-refresh';
window.customElements.define('month-picker', MonthPicker);
